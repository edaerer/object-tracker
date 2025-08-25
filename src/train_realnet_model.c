/**
 * sod realnet modellerini CPU üzerinde eğitir
 * modeller CNN tabanlı
 * gcc -D SOD_ENABLE_NET_TRAIN sod.c main.c -lm
 **/
#include <stdio.h>
#ifndef SOD_ENABLE_NET_TRAIN
#define SOD_ENABLE_NET_TRAIN // bu satır sod_realnet_trainer değişkeni için gerekli!!
#endif
#define SOD_IMPLEMENTATION
#include "../libs/sod/sod.h"

static void my_log_consumer(const char *zMsg, size_t n, void *pUser);

int main(void) {
    const char *zTrainFile = "../train_data/train.txt";
    sod_realnet_trainer *pNet;

    int rc;
    rc = sod_realnet_train_init(&pNet);
    if (rc != SOD_OK) return rc;

    rc = sod_realnet_train_config(pNet, SOD_REALNET_TR_LOG_CALLBACK, my_log_consumer, 0);
    if (rc != SOD_OK) return rc;

	rc = sod_realnet_train_config(pNet, SOD_REALNET_TR_OUTPUT_MODEL, "./bardak_detector.realnet");
	if (rc != SOD_OK) return rc;

    rc = sod_realnet_train_start(pNet, zTrainFile);

    sod_realnet_train_release(pNet);

    return rc;
}

static void my_log_consumer(const char *zMsg, size_t n, void *pUser){
    (void)pUser;
    fwrite(zMsg, 1, n, stdout);
    fputc('\n', stdout);
}