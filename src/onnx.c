#include "onnx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "onnxruntime_c_api.h"

/* ---------------- Dahili Yardımcılar ---------------- */

#define MAX_DETECTIONS_TEMP 65536  /* Güvenlik üst limiti */
#define EPS_F 1e-9f

#ifndef STR_HELPER
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#endif

/* Basit log makrosu */
#define LOG_IF(detector, ...) \
    do { if ((detector)->cfg.verbose) { fprintf(stderr, __VA_ARGS__); } } while(0)

/* Hata gömme makrosu */
#define ORT_CALL(detector, expr)                                                      \
    do {                                                                             \
        OrtStatus* _st = (expr);                                                     \
        if (_st) {                                                                   \
            const char* _msg = (detector)->api->GetErrorMessage(_st);                \
            fprintf(stderr, "ONNXRuntime Hatası: %s\n", _msg ? _msg : "(null)");     \
            (detector)->api->ReleaseStatus(_st);                                     \
            return ONNX_ERR_RUNTIME;                                                \
        }                                                                            \
    } while(0)

typedef struct {
    float x1,y1,x2,y2;
    float score;
    int cls;
} RawDet;

/* ---------------- Dış API Uygulamaları ---------------- */

OnnxConfig onnx_default_config(void) {
    OnnxConfig c;
    c.intra_threads = 8;
    c.inter_threads = 1;
    c.score_thresh = 0.30f;
    c.nms_iou_thresh = 0.45f;
    c.verbose = 0;
    return c;
}

int onnx_load_model(OnnxDetector* detector, const char* model_path, const OnnxConfig* cfg) {
    if (!detector || !model_path) return ONNX_ERR_INVALID_ARG;
    memset(detector, 0, sizeof(*detector));

    detector->api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (!detector->api) return ONNX_ERR_RUNTIME;

    if (cfg) detector->cfg = *cfg;
    else detector->cfg = onnx_default_config();

    ORT_CALL(detector, detector->api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "onnxdet", &detector->env));
    ORT_CALL(detector, detector->api->CreateSessionOptions(&detector->session_opts));
    ORT_CALL(detector, detector->api->SetIntraOpNumThreads(detector->session_opts, detector->cfg.intra_threads));
    ORT_CALL(detector, detector->api->SetInterOpNumThreads(detector->session_opts, detector->cfg.inter_threads));
    ORT_CALL(detector, detector->api->SetSessionGraphOptimizationLevel(detector->session_opts, ORT_ENABLE_ALL));

    ORT_CALL(detector, detector->api->CreateSession(detector->env, model_path, detector->session_opts, &detector->session));
    ORT_CALL(detector, detector->api->GetAllocatorWithDefaultOptions(&detector->allocator));
    ORT_CALL(detector, detector->api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &detector->mem_info));

    size_t in_count=0;
    ORT_CALL(detector, detector->api->SessionGetInputCount(detector->session, &in_count));
    if (in_count < 1) {
        LOG_IF(detector, "Modelde giriş yok\n");
        return ONNX_ERR_MODEL;
    }

    ORT_CALL(detector, detector->api->SessionGetInputName(detector->session, 0, detector->allocator, &detector->input_name));

    /* Girdi shape al */
    OrtTypeInfo* ti = NULL;
    ORT_CALL(detector, detector->api->SessionGetInputTypeInfo(detector->session, 0, &ti));
    const OrtTensorTypeAndShapeInfo* tshape = NULL;
    ORT_CALL(detector, detector->api->CastTypeInfoToTensorInfo(ti, &tshape));

    size_t dim_count=0;
    ORT_CALL(detector, detector->api->GetDimensionsCount(tshape, &dim_count));
    if (dim_count != 4) {
        LOG_IF(detector, "Beklenmeyen giriş boyutu (4 değil)\n");
        detector->api->ReleaseTypeInfo(ti);
        return ONNX_ERR_MODEL;
    }
    int64_t dims[4];
    ORT_CALL(detector, detector->api->GetDimensions(tshape, dims, 4));
    detector->in_n = dims[0];
    detector->in_c = dims[1];
    detector->in_h = dims[2];
    detector->in_w = dims[3];

    detector->api->ReleaseTypeInfo(ti);

    if (detector->in_n != 1 || detector->in_c != 3) {
        LOG_IF(detector, "Şu an sadece N=1, C=3 destekleniyor\n");
        return ONNX_ERR_MODEL;
    }

    /* Çıkış adı (tek bir ana tensör varsayımı) */
    size_t out_count=0;
    ORT_CALL(detector, detector->api->SessionGetOutputCount(detector->session, &out_count));
    if (out_count < 1) {
        LOG_IF(detector, "Modelde çıktı yok\n");
        return ONNX_ERR_MODEL;
    }
    ORT_CALL(detector, detector->api->SessionGetOutputName(detector->session, 0, detector->allocator, &detector->output_name));

    LOG_IF(detector, "Model yüklendi: %s\n", model_path);
    LOG_IF(detector, "Input name: %s  Shape: [1,3,%lld,%lld]\n",
          detector->input_name, (long long)detector->in_h, (long long)detector->in_w);
    LOG_IF(detector, "Output name: %s\n", detector->output_name);

    return ONNX_OK;
}

int onnx_predict(const OnnxDetector* detector,
                 const uint8_t* img_rgb, int w, int h,
                 OnnxDet** out_dets, int* out_count) {
    if (!detector || !img_rgb || !out_dets || !out_count) return ONNX_ERR_INVALID_ARG;
    if (!detector->session) return ONNX_ERR_MODEL;

    *out_dets = NULL;
    *out_count = 0;

    int target_w = (int)detector->in_w;
    int target_h = (int)detector->in_h;

    size_t tensor_elems = (size_t)detector->in_c * detector->in_h * detector->in_w;
    int64_t shape[4] = {1, 3, detector->in_h, detector->in_w};

    OrtValue* input_tensor = NULL;
    OrtStatus* st = detector->api->CreateTensorWithDataAsOrtValue(
        detector->mem_info,
        (void*)img_rgb,
        sizeof(float) * tensor_elems,
        shape, 4,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
        &input_tensor
    );
    if (st) {
        fprintf(stderr, "Tensor oluşturma hatası: %s\n", detector->api->GetErrorMessage(st));
        detector->api->ReleaseStatus(st);
        return ONNX_ERR_RUNTIME;
    }

    const char* input_names[1]  = { detector->input_name };
    const char* output_names[1] = { detector->output_name };
    OrtValue* output_tensor = NULL;
    st = detector->api->Run(
        detector->session, NULL,
        input_names, (const OrtValue* const*)&input_tensor, 1,
        output_names, 1,
        &output_tensor
    );
    if (st) {
        fprintf(stderr, "Run hatası: %s\n", detector->api->GetErrorMessage(st));
        detector->api->ReleaseStatus(st);
        detector->api->ReleaseValue(input_tensor);
        return ONNX_ERR_RUNTIME;
    }

    int is_tensor = 0;
    detector->api->IsTensor(output_tensor, &is_tensor);
    if (!is_tensor) {
        fprintf(stderr, "Çıktı tensor değil.\n");
        detector->api->ReleaseValue(output_tensor);
        detector->api->ReleaseValue(input_tensor);
        return ONNX_ERR_MODEL;
    }

    OrtTensorTypeAndShapeInfo* oinfo = NULL;
    detector->api->GetTensorTypeAndShape(output_tensor, &oinfo);
    int64_t odims[3] = {0};
    size_t odim_count = 0;
    detector->api->GetDimensionsCount(oinfo, &odim_count);
    detector->api->GetDimensions(oinfo, odims, odim_count);

    // Beklenen: [1, num_det, 6]
    int num_det = (int)odims[1];
    int elem_per_det = (int)odims[2];

    float* out_data = NULL;
    detector->api->GetTensorMutableData(output_tensor, (void**)&out_data);

    OnnxDet* dets = (OnnxDet*)malloc(sizeof(OnnxDet) * num_det);
    if (!dets) {
        detector->api->ReleaseTensorTypeAndShapeInfo(oinfo);
        detector->api->ReleaseValue(output_tensor);
        detector->api->ReleaseValue(input_tensor);
        return ONNX_ERR_MEMORY;
    }

    for (int i = 0; i < num_det; ++i) {
        float x1 = out_data[i * elem_per_det + 0];
        float y1 = out_data[i * elem_per_det + 1];
        float x2 = out_data[i * elem_per_det + 2];
        float y2 = out_data[i * elem_per_det + 3];
        float score = out_data[i * elem_per_det + 4];
        int cls = (int)out_data[i * elem_per_det + 5];

        dets[i].x1 = x1;
        dets[i].y1 = y1;
        dets[i].x2 = x2;
        dets[i].y2 = y2;
        dets[i].score = score;
        dets[i].cls = cls;
    }

    *out_dets = dets;
    *out_count = num_det;

    detector->api->ReleaseTensorTypeAndShapeInfo(oinfo);
    detector->api->ReleaseValue(output_tensor);
    detector->api->ReleaseValue(input_tensor);

    return ONNX_OK;
}

void onnx_destroy(OnnxDetector* detector) {
    if (!detector) return;
    if (detector->allocator) {
        if (detector->output_name) detector->allocator->Free(detector->allocator, detector->output_name);
        if (detector->input_name)  detector->allocator->Free(detector->allocator, detector->input_name);
    }
    if (detector->mem_info) detector->api->ReleaseMemoryInfo(detector->mem_info);
    if (detector->session) detector->api->ReleaseSession(detector->session);
    if (detector->session_opts) detector->api->ReleaseSessionOptions(detector->session_opts);
    if (detector->env) detector->api->ReleaseEnv(detector->env);

    memset(detector, 0, sizeof(*detector));
}

void onnx_free_detections(OnnxDet* dets) {
    free(dets);
}

const char* onnx_runtime_version_string(void) {
#if defined(ORT_API_VERSION)
#   if ORT_API_VERSION == 1
    return "ONNX Runtime API v1";
#   else
    return "ONNX Runtime API v" STR(ORT_API_VERSION);
#   endif
#else
    return "ONNX Runtime API (version macro not found)";
#endif
}
