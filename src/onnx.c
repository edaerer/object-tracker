#include "onnx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "onnx/onnxruntime_c_api.h"

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

/* IoU */
static float iou_raw(const RawDet* a, const RawDet* b) {
    float xx1 = fmaxf(a->x1, b->x1);
    float yy1 = fmaxf(a->y1, b->y1);
    float xx2 = fminf(a->x2, b->x2);
    float yy2 = fminf(a->y2, b->y2);
    float w = fmaxf(0.f, xx2 - xx1);
    float h = fmaxf(0.f, yy2 - yy1);
    float inter = w * h;
    float areaA = fmaxf(0.f, a->x2 - a->x1) * fmaxf(0.f, a->y2 - a->y1);
    float areaB = fmaxf(0.f, b->x2 - b->x1) * fmaxf(0.f, b->y2 - b->y1);
    return inter / (areaA + areaB - inter + EPS_F);
}

static void nms_one_class(RawDet* dets, int count, float iou_thr, RawDet* out, int* out_count) {
    int* sup = (int*)calloc(count, sizeof(int));
    if(!sup) return;
    for (int i=0;i<count;i++) {
        if (sup[i]) continue;
        out[(*out_count)++] = dets[i];
        for (int j=i+1;j<count;j++) {
            if (sup[j]) continue;
            if (iou_raw(&dets[i], &dets[j]) > iou_thr) sup[j] = 1;
        }
    }
    free(sup);
}

/* Basit selection sort (skora göre azalan) */
static void sort_by_score(RawDet* dets, int n) {
    for (int i=0;i<n-1;i++) {
        int mx=i;
        for (int j=i+1;j<n;j++)
            if (dets[j].score > dets[mx].score) mx=j;
        if (mx!=i) {
            RawDet tmp=dets[i]; dets[i]=dets[mx]; dets[mx]=tmp;
        }
    }
}

static int class_aware_nms(const float* preds, int rows, int cols,
                           float score_thr, float iou_thr,
                           RawDet** out_ptr, int* out_count) {
    int num_cls = cols - 4;
    int capacity = rows * num_cls;
    if (capacity > MAX_DETECTIONS_TEMP) capacity = MAX_DETECTIONS_TEMP;

    RawDet* accum = (RawDet*)malloc(sizeof(RawDet)*capacity);
    if(!accum) return ONNX_ERR_MEMORY;
    int total=0;

    RawDet* buf = (RawDet*)malloc(sizeof(RawDet)*rows);
    RawDet* nms_tmp = (RawDet*)malloc(sizeof(RawDet)*rows);
    if(!buf || !nms_tmp) {
        free(accum); free(buf); free(nms_tmp);
        return ONNX_ERR_MEMORY;
    }

    for (int c=0; c<num_cls; c++) {
        int bc=0;
        for (int r=0; r<rows; r++) {
            const float* p = preds + r*cols;
            float s = p[4+c];
            if (s >= score_thr) {
                buf[bc].x1 = p[0];
                buf[bc].y1 = p[1];
                buf[bc].x2 = p[2];
                buf[bc].y2 = p[3];
                buf[bc].score = s;
                buf[bc].cls = c;
                bc++;
            }
        }
        if (!bc) continue;
        sort_by_score(buf, bc);
        int after=0;
        nms_one_class(buf, bc, iou_thr, nms_tmp, &after);
        for (int i=0;i<after;i++) {
            if (total < capacity) accum[total++] = nms_tmp[i];
        }
    }

    free(buf); free(nms_tmp);
    *out_ptr = accum;
    *out_count = total;
    return ONNX_OK;
}

/* Resize (nearest) */
static void resize_nearest(const uint8_t* src, int sw, int sh,
                           uint8_t* dst, int dw, int dh) {
    for (int y=0; y<dh; y++) {
        int sy = (int)((float)y * sh / dh);
        if (sy>=sh) sy=sh-1;
        for (int x=0; x<dw; x++) {
            int sx = (int)((float)x * sw / dw);
            if (sx>=sw) sx=sw-1;
            const uint8_t* sp = src + (size_t)(sy*sw+sx)*3;
            uint8_t* dp = dst + (size_t)(y*dw+x)*3;
            dp[0]=sp[0]; dp[1]=sp[1]; dp[2]=sp[2];
        }
    }
}

/* HWC -> CHW float norm */
static void hwc_to_chw_norm(const uint8_t* in, float* out, int W, int H) {
    int stride = W*H;
    for(int y=0;y<H;y++) {
        for(int x=0;x<W;x++) {
            const uint8_t* p = in + (size_t)(y*W+x)*3;
            out[0*stride + y*W + x] = p[0]/255.0f;
            out[1*stride + y*W + x] = p[1]/255.0f;
            out[2*stride + y*W + x] = p[2]/255.0f;
        }
    }
}

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

    uint8_t* resized = NULL;
    const uint8_t* use_img = img_rgb;
    if (w != target_w || h != target_h) {
        resized = (uint8_t*)malloc((size_t)target_w * target_h * 3);
        if (!resized) return ONNX_ERR_MEMORY;
        resize_nearest(img_rgb, w, h, resized, target_w, target_h);
        use_img = resized;
    }

    size_t tensor_elems = (size_t)detector->in_c * target_h * target_w;
    float* input_buf = (float*)malloc(sizeof(float)*tensor_elems);
    if(!input_buf) {
        free(resized);
        return ONNX_ERR_MEMORY;
    }
    hwc_to_chw_norm(use_img, input_buf, target_w, target_h);

    int64_t shape[4] = {1, 3, detector->in_h, detector->in_w};
    OrtValue* input_tensor = NULL;
    OrtStatus* st = detector->api->CreateTensorWithDataAsOrtValue(
        detector->mem_info,
        input_buf,
        sizeof(float)*tensor_elems,
        shape, 4,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
        &input_tensor
    );
    if (st) {
        const char* msg = detector->api->GetErrorMessage(st);
        fprintf(stderr, "Tensor oluşturma hatası: %s\n", msg?msg:"(null)");
        detector->api->ReleaseStatus(st);
        free(resized);
        free(input_buf);
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
        const char* msg = detector->api->GetErrorMessage(st);
        fprintf(stderr, "Run hatası: %s\n", msg?msg:"(null)");
        detector->api->ReleaseStatus(st);
        detector->api->ReleaseValue(input_tensor);
        free(resized);
        free(input_buf);
        return ONNX_ERR_RUNTIME;
    }

    int is_tensor=0;
    detector->api->IsTensor(output_tensor, &is_tensor);
    if (!is_tensor) {
        fprintf(stderr, "Çıktı tensor değil.\n");
        detector->api->ReleaseValue(output_tensor);
        detector->api->ReleaseValue(input_tensor);
        free(resized);
        free(input_buf);
        return ONNX_ERR_MODEL;
    }

    OrtTensorTypeAndShapeInfo* oinfo=NULL;
    detector->api->GetTensorTypeAndShape(output_tensor, &oinfo);
    size_t odim_count=0;
    detector->api->GetDimensionsCount(oinfo, &odim_count);

    int64_t odims[8];
    if (odim_count > 8) odim_count = 8; /* güvenlik */
    detector->api->GetDimensions(oinfo, odims, odim_count);

    int rows=0, cols=0;
    if (odim_count==3) { /* [1, N, 4+K] */
        rows = (int)odims[1];
        cols = (int)odims[2];
    } else if (odim_count==2) { /* [N, 4+K] */
        rows = (int)odims[0];
        cols = (int)odims[1];
    } else {
        fprintf(stderr, "Beklenmeyen çıktı boyutu (dim=%zu)\n", odim_count);
        detector->api->ReleaseTensorTypeAndShapeInfo(oinfo);
        detector->api->ReleaseValue(output_tensor);
        detector->api->ReleaseValue(input_tensor);
        free(resized);
        free(input_buf);
        return ONNX_ERR_MODEL;
    }

    float* out_data = NULL;
    detector->api->GetTensorMutableData(output_tensor, (void**)&out_data);

    RawDet* raw = NULL;
    int raw_count=0;
    int rc = class_aware_nms(out_data, rows, cols,
                             detector->cfg.score_thresh,
                             detector->cfg.nms_iou_thresh,
                             &raw, &raw_count);
    if (rc != ONNX_OK) {
        detector->api->ReleaseTensorTypeAndShapeInfo(oinfo);
        detector->api->ReleaseValue(output_tensor);
        detector->api->ReleaseValue(input_tensor);
        free(resized);
        free(input_buf);
        return rc;
    }

    /* Normalize mı? */
    int normalized = 0;
    if (raw_count>0) {
        float mx=0.f;
        for (int i=0;i<raw_count;i++) {
            if (raw[i].x1>mx) mx=raw[i].x1;
            if (raw[i].y1>mx) mx=raw[i].y1;
            if (raw[i].x2>mx) mx=raw[i].x2;
            if (raw[i].y2>mx) mx=raw[i].y2;
        }
        normalized = (mx<=1.2f);
    }

    OnnxDet* dets = NULL;
    if (raw_count>0) {
        dets = (OnnxDet*)malloc(sizeof(OnnxDet)*raw_count);
        if(!dets) {
            free(raw);
            detector->api->ReleaseTensorTypeAndShapeInfo(oinfo);
            detector->api->ReleaseValue(output_tensor);
            detector->api->ReleaseValue(input_tensor);
            free(resized);
            free(input_buf);
            return ONNX_ERR_MEMORY;
        }
        for (int i=0;i<raw_count;i++) {
            float x1=raw[i].x1, y1=raw[i].y1, x2=raw[i].x2, y2=raw[i].y2;
            if (normalized) {
                x1 *= (float)w; x2 *= (float)w;
                y1 *= (float)h; y2 *= (float)h;
            }
            if (x1<0) x1=0; if (y1<0) y1=0;
            if (x2>w-1) x2=(float)(w-1);
            if (y2>h-1) y2=(float)(h-1);
            dets[i].x1 = x1;
            dets[i].y1 = y1;
            dets[i].x2 = x2;
            dets[i].y2 = y2;
            dets[i].score = raw[i].score;
            dets[i].cls = raw[i].cls;
        }
    }

    *out_dets = dets;
    *out_count = raw_count;

    free(raw);
    detector->api->ReleaseTensorTypeAndShapeInfo(oinfo);
    detector->api->ReleaseValue(output_tensor);
    detector->api->ReleaseValue(input_tensor);
    free(resized);
    free(input_buf);

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
    /* Derleme zamanı API versiyonunu döndürüyoruz. Runtime kontrolü için
       OrtGetApiBase()->GetVersion() eklenebilir (bazı sürümlerde mevcut). */
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
