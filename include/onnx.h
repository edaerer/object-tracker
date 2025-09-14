#ifndef ONNX_ABSTRACTION_H
#define ONNX_ABSTRACTION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimal ONNX Runtime C wrapper API
 *  - Model load/unload
 *  - Inference with RGB8 (HWC) input sized w x h
 *  - Post-processing returns pixel-space boxes & scores
 */

/* Forward-declare ONNX Runtime types so this header
 * does not require including ORT headers at call sites. */
typedef struct OrtApi OrtApi;
typedef struct OrtEnv OrtEnv;
typedef struct OrtSessionOptions OrtSessionOptions;
typedef struct OrtSession OrtSession;
typedef struct OrtAllocator OrtAllocator;
typedef struct OrtMemoryInfo OrtMemoryInfo;
typedef struct OrtValue OrtValue;
typedef struct OrtStatus OrtStatus;
typedef struct OrtTensorTypeAndShapeInfo OrtTensorTypeAndShapeInfo;

/* Public detection result */
typedef struct {
    float x1, y1, x2, y2;  /* pixel coords */
    float score;           /* confidence */
    int   cls;             /* class id */
} OnnxDet;

/* Config */
typedef struct {
    int   intra_threads;     /* intra-op threads */
    int   inter_threads;     /* inter-op threads */
    float score_thresh;      /* pre-NMS score threshold */
    float nms_iou_thresh;    /* NMS IoU threshold */
    int   verbose;           /* 0/1 logging */
} OnnxConfig;

/* Status codes */
typedef enum {
    ONNX_OK = 0,
    ONNX_ERR_INVALID_ARG = -1,
    ONNX_ERR_MODEL       = -2,
    ONNX_ERR_MEMORY      = -3,
    ONNX_ERR_RUNTIME     = -4
} OnnxStatus;

/* Detector instance */
typedef struct {
    const OrtApi* api;             /* ORT API table */
    OrtEnv*            env;
    OrtSessionOptions* session_opts;
    OrtSession*        session;
    OrtAllocator*      allocator;
    OrtMemoryInfo*     mem_info;

    char* input_name;              /* owned by ORT allocator */
    char* output_name;             /* owned by ORT allocator */

    int64_t in_n, in_c, in_h, in_w; /* expected input NCHW */

    OnnxConfig cfg;                /* runtime config */
} OnnxDetector;

/* Defaults */
OnnxConfig onnx_default_config(void);

/* Load model from path */
int onnx_load_model(OnnxDetector* detector, const char* model_path, const OnnxConfig* cfg);

/* Inference on RGB8 HWC image of size w x h */
int onnx_predict(const OnnxDetector* detector,
                 const uint8_t* img_rgb, int w, int h,
                 OnnxDet** out_dets, int* out_count);

/* Cleanup */
void onnx_destroy(OnnxDetector* detector);
void onnx_free_detections(OnnxDet* dets);

/* Build/runtime ORT version string (static) */
const char* onnx_runtime_version_string(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ONNX_ABSTRACTION_H */
