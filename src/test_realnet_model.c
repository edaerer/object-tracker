#ifdef TEST_REALNET_MODEL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <darknet/darknet.h>

// En yüksek tespitleri çiz
void draw_top_detections(image im, void* sorted_dets_ptr, int count) {
    typedef struct {
        int index;
        int class_id;
        float confidence;
        box bbox;
    } sorted_detection;
    
    sorted_detection* sorted_dets = (sorted_detection*)sorted_dets_ptr;
    
    printf("Çizim başlıyor: %d tespit, resim %dx%dx%d\n", count, im.w, im.h, im.c);
    
    if (!im.data || im.w <= 0 || im.h <= 0 || im.c != 3) {
        printf("Geçersiz resim verisi!\n");
        return;
    }
    
    // Farklı renkler (RGB)
    float colors[10][3] = {
        {1.0, 0.0, 0.0},  // Kırmızı
        {0.0, 1.0, 0.0},  // Yeşil
        {0.0, 0.0, 1.0},  // Mavi
        {1.0, 1.0, 0.0},  // Sarı
        {1.0, 0.0, 1.0},  // Magenta
        {0.0, 1.0, 1.0},  // Cyan
        {1.0, 0.5, 0.0},  // Turuncu
        {0.5, 0.0, 1.0},  // Mor
        {0.0, 0.5, 0.0},  // Koyu yeşil
        {0.5, 0.5, 0.5}   // Gri
    };
    
    for (int i = 0; i < count; i++) {
        box b = sorted_dets[i].bbox;
        
        // Bounding box koordinatları
        int left  = (int)((b.x - b.w/2.) * im.w);
        int right = (int)((b.x + b.w/2.) * im.w);
        int top   = (int)((b.y - b.h/2.) * im.h);
        int bot   = (int)((b.y + b.h/2.) * im.h);
        
        // Güvenli sınır kontrolü
        if (left < 0) left = 0;
        if (right >= im.w) right = im.w - 1;
        if (top < 0) top = 0;
        if (bot >= im.h) bot = im.h - 1;
        
        if (left >= right || top >= bot) continue;
        
        printf("Kutu %d çiziliyor: Sınıf=%d, Güven=%.3f%%, Box=(%d,%d,%d,%d)\n", 
               i+1, sorted_dets[i].class_id, sorted_dets[i].confidence*100, left, top, right, bot);
        
        // Renk seç
        float* color = colors[i % 10];
        int thickness = 3;
        
        // Üst çizgi
        for (int x = left; x <= right; x++) {
            for (int t = 0; t < thickness; t++) {
                int y = top + t;
                if (x >= 0 && x < im.w && y >= 0 && y < im.h) {
                    int idx = y * im.w + x;
                    if (idx >= 0 && idx < im.w * im.h) {
                        im.data[0 * im.h * im.w + idx] = color[0];  // R
                        im.data[1 * im.h * im.w + idx] = color[1];  // G
                        im.data[2 * im.h * im.w + idx] = color[2];  // B
                    }
                }
            }
        }
        
        // Alt çizgi
        for (int x = left; x <= right; x++) {
            for (int t = 0; t < thickness; t++) {
                int y = bot - t;
                if (x >= 0 && x < im.w && y >= 0 && y < im.h) {
                    int idx = y * im.w + x;
                    if (idx >= 0 && idx < im.w * im.h) {
                        im.data[0 * im.h * im.w + idx] = color[0];
                        im.data[1 * im.h * im.w + idx] = color[1];
                        im.data[2 * im.h * im.w + idx] = color[2];
                    }
                }
            }
        }
        
        // Sol çizgi
        for (int y = top; y <= bot; y++) {
            for (int t = 0; t < thickness; t++) {
                int x = left + t;
                if (x >= 0 && x < im.w && y >= 0 && y < im.h) {
                    int idx = y * im.w + x;
                    if (idx >= 0 && idx < im.w * im.h) {
                        im.data[0 * im.h * im.w + idx] = color[0];
                        im.data[1 * im.h * im.w + idx] = color[1];
                        im.data[2 * im.h * im.w + idx] = color[2];
                    }
                }
            }
        }
        
        // Sağ çizgi
        for (int y = top; y <= bot; y++) {
            for (int t = 0; t < thickness; t++) {
                int x = right - t;
                if (x >= 0 && x < im.w && y >= 0 && y < im.h) {
                    int idx = y * im.w + x;
                    if (idx >= 0 && idx < im.w * im.h) {
                        im.data[0 * im.h * im.w + idx] = color[0];
                        im.data[1 * im.h * im.w + idx] = color[1];
                        im.data[2 * im.h * im.w + idx] = color[2];
                    }
                }
            }
        }
    }
    
    printf("Toplam %d kutu çizildi\n", count);
}

// Güvenli manuel çizim fonksiyonu
void manuel_draw_boxes(image im, detection *dets, int num, float thresh, int classes) {
    printf("Çizim başlıyor: %d tespit, resim %dx%dx%d\n", num, im.w, im.h, im.c);
    
    if (!im.data || im.w <= 0 || im.h <= 0 || im.c != 3) {
        printf("Geçersiz resim verisi!\n");
        return;
    }
    
    int drawn_count = 0;
    for (int i = 0; i < num && drawn_count < 10; ++i) {  // Maksimum 10 kutu çiz
        if (!dets[i].prob) continue;
        
        char labelstr[4096] = {0};
        int class_id = -1;
        float prob = 0;
        
        // En yüksek olasılığa sahip sınıfı bul
        for (int j = 0; j < classes; ++j) {
            if (dets[i].prob[j] > thresh && dets[i].prob[j] > prob) {
                prob = dets[i].prob[j];
                class_id = j;
            }
        }
        
        if (class_id >= 0) {
            // Bounding box koordinatlarını hesapla
            box b = dets[i].bbox;
            int left  = (int)((b.x - b.w/2.) * im.w);
            int right = (int)((b.x + b.w/2.) * im.w);
            int top   = (int)((b.y - b.h/2.) * im.h);
            int bot   = (int)((b.y + b.h/2.) * im.h);
            
            // Güvenli sınır kontrolü
            if (left < 0) left = 0;
            if (right >= im.w) right = im.w - 1;
            if (top < 0) top = 0;
            if (bot >= im.h) bot = im.h - 1;
            
            // Geçerli kutu kontrolü
            if (left >= right || top >= bot) continue;
            
            printf("Kutu %d çiziliyor: Sınıf=%d, Güven=%.3f, Box=(%d,%d,%d,%d)\n", 
                   drawn_count+1, class_id, prob, left, top, right, bot);
            
            // Basit kırmızı dikdörtgen çizimi - sadece köşeleri
            int corner_size = 10; // Köşe boyutu
            
            // Sol üst köşe
            for (int x = left; x < left + corner_size && x < right && x < im.w; x++) {
                for (int y = top; y < top + 2 && y < bot && y < im.h; y++) {
                    int idx = y * im.w + x;
                    if (idx >= 0 && idx < im.w * im.h) {
                        im.data[0 * im.h * im.w + idx] = 1.0f;  // R
                        im.data[1 * im.h * im.w + idx] = 0.0f;  // G  
                        im.data[2 * im.h * im.w + idx] = 0.0f;  // B
                    }
                }
            }
            
            // Sağ alt köşe
            for (int x = right - corner_size; x <= right && x >= left && x >= 0; x++) {
                for (int y = bot - 2; y <= bot && y >= top && y >= 0; y++) {
                    int idx = y * im.w + x;
                    if (idx >= 0 && idx < im.w * im.h) {
                        im.data[0 * im.h * im.w + idx] = 1.0f;  // R
                        im.data[1 * im.h * im.w + idx] = 0.0f;  // G
                        im.data[2 * im.h * im.w + idx] = 0.0f;  // B
                    }
                }
            }
            
            drawn_count++;
        }
    }
    
    printf("Toplam %d kutu çizildi\n", drawn_count);
}

static int last_yolo_classes(network *net) {
    for (int i = net->n - 1; i >= 0; --i)
        if (net->layers[i].type == YOLO) return net->layers[i].classes;
    return -1;
}

int main(void) {
    // Eşik değerlerini ayarla
    const float thresh = 0.001f;  // Çok daha düşük eşik
    const float hier = 0.5f;
    const float nms = 0.45f;     // NMS değeri ekle
    
    // Ağı yükle
    network *net = load_network("../config/yolov3.cfg", "../weights/yolov3-tiny.weights", 0);
    set_batch_network(net, 1);
    
    // Resmi yükle ve boyutlandır
    image im = load_image_color("../dataset/test/test4.jpg", 0, 0);
    printf("Resim boyutu: %d x %d x %d\n", im.w, im.h, im.c);
    
    image sz = letterbox_image(im, net->w, net->h);
    printf("Ağ girdi boyutu: %d x %d\n", net->w, net->h);
    
    // Tahmin yap
    network_predict_image(net, sz);
    
    int nboxes = 0;
    detection *dets = get_network_boxes(net, im.w, im.h, thresh, hier, 0, 1, &nboxes);
    
    int classes = last_yolo_classes(net);
    if (classes <= 0) { 
        fprintf(stderr, "YOLO katmani yok!\n"); 
        goto cleanup; 
    }
    printf("Sınıf sayısı: %d\n", classes);
    
    // Ham tespit sayısını göster
    printf("Ham tespit sayısı: %d\n", nboxes);
    
    // En yüksek güven skorlarını bul
    float max_scores[10] = {0}; // En yüksek 10 skor
    int max_classes[10] = {-1};
    int max_indices[10] = {-1};
    
    for (int i = 0; i < nboxes; ++i) {
        if (dets[i].prob) {
            for (int c = 0; c < classes; ++c) {
                float score = dets[i].prob[c];
                if (score > 0) {
                    // En düşük maksimum skoru bul
                    int min_idx = 0;
                    for (int j = 1; j < 10; ++j) {
                        if (max_scores[j] < max_scores[min_idx]) min_idx = j;
                    }
                    // Eğer bu skor daha yüksekse, değiştir
                    if (score > max_scores[min_idx]) {
                        max_scores[min_idx] = score;
                        max_classes[min_idx] = c;
                        max_indices[min_idx] = i;
                    }
                }
            }
        }
    }
    
    printf("En yüksek 10 güven skoru:\n");
    for (int i = 0; i < 10; ++i) {
        if (max_scores[i] > 0) {
            printf("  %d. Tespit %d, Sınıf %d: %.6f\n", i+1, max_indices[i], max_classes[i], max_scores[i]);
        }
    }
    
    // Eşik öncesi tüm tespitleri kontrol et
    int kept_before_nms = 0;
    for (int i = 0; i < nboxes; ++i) {
        for (int c = 0; c < classes; ++c) {
            if (dets[i].prob && dets[i].prob[c] >= thresh) { 
                kept_before_nms++; 
                printf("Tespit %d: Sınıf %d, Güven: %.6f\n", i, c, dets[i].prob[c]);
                break; 
            }
        }
    }
    printf("NMS öncesi tespit: %d (eşik>=%.3f)\n", kept_before_nms, thresh);
    
    // NMS uygula
    if (nms > 0) do_nms_sort(dets, nboxes, classes, nms);
    
    // NMS sonrası say
    int kept_after_nms = 0;
    for (int i = 0; i < nboxes; ++i) {
        for (int c = 0; c < classes; ++c) {
            if (dets[i].prob && dets[i].prob[c] >= thresh) { 
                kept_after_nms++; 
                break; 
            }
        }
    }
    printf("NMS sonrası tespit: %d\n", kept_after_nms);
    
    // Tespitleri çiz - En yüksek 10 güven skoru
    if (kept_after_nms > 0) {
        printf("\n=== EN YÜKSEK 10 TESPİT ===\n");
        
        // Tüm tespitleri güven skoruna göre sırala
        typedef struct {
            int index;
            int class_id;
            float confidence;
            box bbox;
        } sorted_detection;
        
        sorted_detection sorted_dets[nboxes];
        int valid_count = 0;
        
        // Geçerli tespitleri topla
        for (int i = 0; i < nboxes; ++i) {
            if (!dets[i].prob) continue;
            
            float max_prob = 0;
            int best_class = -1;
            
            for (int c = 0; c < classes; ++c) {
                if (dets[i].prob[c] > max_prob) {
                    max_prob = dets[i].prob[c];
                    best_class = c;
                }
            }
            
            if (best_class >= 0 && max_prob >= thresh) {
                sorted_dets[valid_count].index = i;
                sorted_dets[valid_count].class_id = best_class;
                sorted_dets[valid_count].confidence = max_prob;
                sorted_dets[valid_count].bbox = dets[i].bbox;
                valid_count++;
            }
        }
        
        // Basit sıralama (bubble sort) - güven skoruna göre azalan
        for (int i = 0; i < valid_count - 1; i++) {
            for (int j = 0; j < valid_count - i - 1; j++) {
                if (sorted_dets[j].confidence < sorted_dets[j + 1].confidence) {
                    sorted_detection temp = sorted_dets[j];
                    sorted_dets[j] = sorted_dets[j + 1];
                    sorted_dets[j + 1] = temp;
                }
            }
        }
        
        // En yüksek 10'u yazdır ve çiz
        //int draw_count = (valid_count < 10) ? valid_count : 10;
        
        printf("En yüksek %d tespit:\n", 2);
        for (int i = 0; i < 2; i++) {
            printf("%d. Sınıf %d: %.3f%% güven\n", 
                   i+1, sorted_dets[i].class_id, sorted_dets[i].confidence*100);
        }
        
        // En yüksek 10'u çiz
        printf("\nÇizim başlıyor...\n");
        draw_top_detections(im, sorted_dets, 2);
    }
    
    save_image(im, "out");
    printf("Kaydedildi: out.jpg / out.png\n");
    
cleanup:
    if (dets) free_detections(dets, nboxes);
    free_image(sz); 
    free_image(im); 
    free_network(net);
    return 0;
}
#endif