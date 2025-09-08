#ifndef ONNX_ABSTRACTION_H
#define ONNX_ABSTRACTION_H

/*
  Basit ONNX Runtime C sarmalayıcısı
  - Model yükleme
  - Tek görüntü (RGB uint8 HWC) üzerinde ileri besleme (predict)
  - Sınıf-bilinçli NMS
  - Kaynakları serbest bırakma

  Derleme Örneği (Linux):
    gcc -std=c11 -O3 main.c onnx.c -o detect \
        -I/path/to/onnxruntime/include -L/path/to/onnxruntime/lib -lonnxruntime -lm

  Not:
    - Görüntü verisi RGB 8-bit HWC varsayılıyor.
    - Model giriş formatı NCHW float32 [1,3,H,W].
    - Çıktı formatı [N, 4 + K] veya [1, N, 4 + K] (YOLO benzeri) varsayımı.
    - Çıktı satırı: [x1,y1,x2,y2, score_class0, score_class1, ...].
*/

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tespit (Detection) yapısı */
typedef struct {
    float x1, y1, x2, y2;  /* Piksel cinsinden (predict döndüğünde orijinal görüntü ölçeğine dönüştürülmüş) */
    float score;           /* Seçilen sınıfın skoru */
    int   cls;             /* Sınıf indeksi */
} OnnxDet;

/* Hata kodları */
enum {
    ONNX_OK = 0,
    ONNX_ERR_GENERAL = -1,
    ONNX_ERR_MODEL = -2,
    ONNX_ERR_MEMORY = -3,
    ONNX_ERR_RUNTIME = -4,
    ONNX_ERR_INVALID_ARG = -5
};

/* Konfigürasyon */
typedef struct {
    int intra_threads;    /* Varsayılan 8 */
    int inter_threads;    /* Varsayılan 1 */
    float score_thresh;   /* Varsayılan 0.30f */
    float nms_iou_thresh; /* Varsayılan 0.45f */
    int   verbose;        /* >0 ise bazı loglar yazar */
} OnnxConfig;

/* İçsel durum (kullanıcı doğrudan alanları kullanmamalı) */
typedef struct {
    /* ONNX Runtime ham pointerları */
    const struct OrtApi* api;
    struct OrtEnv* env;
    struct OrtSessionOptions* session_opts;
    struct OrtSession* session;
    struct OrtMemoryInfo* mem_info;
    struct OrtAllocator* allocator;

    /* Model giriş/çıkış adları (tek giriş, tek ana çıktı varsayımı) */
    char* input_name;
    char* output_name;

    /* Giriş boyutları */
    int64_t in_n;
    int64_t in_c;
    int64_t in_h;
    int64_t in_w;

    /* Konfigürasyon */
    OnnxConfig cfg;
} OnnxDetector;

/* Varsayılan konfigürasyon döndürür */
OnnxConfig onnx_default_config(void);

/* Yükleme:
   - detector: sıfırlanmış (memset 0) bir yapı
   - model_path: .onnx dosya yolu
   - cfg: NULL ise default uygulanır
   Dönüş: 0 (başarılı) veya negatif hata kodu
*/
int onnx_load_model(OnnxDetector* detector, const char* model_path, const OnnxConfig* cfg);

/* Tek görüntü üzerinde tahmin:
   - img_rgb: H x W x 3, uint8 RGB
   - w,h: giriş görüntü boyutu
   - out_dets: çıktı tespit dizisi pointer'ı (malloc ile ayrılır) -> kullanıcı onnx_free_detections ile serbest bırakır
   - out_count: tespit sayısı
   Dönüş: 0 veya hata kodu
*/
int onnx_predict(const OnnxDetector* detector,
                 const uint8_t* img_rgb, int w, int h,
                 OnnxDet** out_dets, int* out_count);

/* Kaynakları serbest bırak */
void onnx_destroy(OnnxDetector* detector);

/* Tespit dizisini serbest bırak */
void onnx_free_detections(OnnxDet* dets);

/* Versiyon bilgisini (derleme zamanı ORT_API_VERSION makrosu + runtime yoklamadan) yazı olarak döndürür.
   NOT: Statik string döndürür, free yapmayın. */
const char* onnx_runtime_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* ONNX_ABSTRACTION_H */
