#include <stdio.h>
#include <stdlib.h>
#include "./libs/sod/sod.h"

int main(void)
{
	const char *zFile = "./images/test4.jpg";
	/*
	 * By default, RealNets are designed to process video streams thanks
	 * to their very fast processing speed. However, for the sake of simplicity
	 * we'll stick with images for this programming intro to RealNets.
	 */
	sod_realnet *pNet;
	int i,rc;
	rc = sod_realnet_create(&pNet);
	if (rc != SOD_OK) return rc;

	/* ==== MODELİ BELLEĞE YÜKLE ==== */
	FILE *fp = fopen("./models/bardak_detector.realnet", "rb");
	if (!fp) {
		puts("Bardak tespit modeli açılamadı");
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	void *buffer = malloc(size);
	if (!buffer) {
		puts("Model için bellek ayrılamadı");
		fclose(fp);
		return -1;
	}
	fread(buffer, 1, size, fp);
	fclose(fp);

	sod_realnet_model_handle hModel;

	rc = sod_realnet_load_model_from_mem(pNet, buffer, (unsigned int)size, &hModel);
	if (rc != SOD_OK) {
		puts("Bardak tespit modeli yüklenemedi");
		free(buffer);
		return rc;
	}
	puts("Bardak tespit modeli başarıyla yüklendi");

	/* Load the target image in grayscale colorspace */
	sod_img img = sod_img_load_grayscale(zFile);
	if (img.data == 0) {
		puts("Görüntü dosyası yüklenemedi");
		free(buffer);
		return 0;
	}
	/* Load a full color copy of the target image so we draw boxes */
	sod_img color = sod_img_load_color(zFile);

	/* convert the grayscale image to blob. */
	unsigned char *zBlob = sod_image_to_blob(img);

	/* Bounding boxes array */
	sod_box *aBoxes;
	int nbox;
	/* 
	 * Perform Real-Time detection on this blob
	 */
	rc = sod_realnet_detect(pNet, zBlob, img.w, img.h, &aBoxes, &nbox);
	if (rc != SOD_OK) {
		puts("Bardak tespiti sırasında hata oluştu");
		free(buffer);
		return rc;
	}

	/* Consume result */
	printf("=== TESPİT SONUÇLARI ===\n");
	printf("Toplam tespit sayısı: %d\n", nbox);
	
	if (nbox == 0) {
		printf("UYARI: Hiç tespit yapılamadı!\n");
		printf("Olası nedenler:\n");
		printf("- Model dosyası yanlış veya bozuk\n");
		printf("- Görüntüde bardak bulunmuyor\n");
		printf("- Model bu tip görüntülerle eğitilmemiş\n");
	}
	
	int detected_cups = 0;
	for (i = 0; i < nbox; i++) {
		printf("Tespit #%d: x:%d y:%d w:%d h:%d güven:%f sınıf:'%s'\n", 
			i+1, aBoxes[i].x, aBoxes[i].y, aBoxes[i].w, aBoxes[i].h, 
			aBoxes[i].score, aBoxes[i].zName ? aBoxes[i].zName : "bilinmeyen");
		
		/* Eşik değeri 0.0 - tüm tespitleri kabul et */
		if (aBoxes[i].score >= 0.0) {
			detected_cups++;
			/* Draw a blue box around detected object */
			sod_image_draw_bbox_width(color, aBoxes[i], 3, 0., 150., 255.); // Mavi renk
		}
	}
	
	printf("Eşik değeri üstündeki tespit sayısı: %d\n", detected_cups);

	/* Save the detection result */
	sod_img_save_as_png(color, "./bardak_tespit_sonuc.png");
	printf("Tespit sonucu './bardak_tespit_sonuc.png' dosyasına kaydedildi.\n");

	/* cleanup */
	sod_free_image(img);
	sod_free_image(color);
	sod_image_free_blob(zBlob);
	sod_realnet_destroy(pNet);
	free(buffer);
	
	return 0;
}