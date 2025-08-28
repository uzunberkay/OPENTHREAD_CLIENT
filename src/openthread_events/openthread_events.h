#ifndef OPENTHREAD_EVENTS_H
#define OPENTHREAD_EVENTS_H

#include <zephyr/kernel.h>
#include <openthread/instance.h>
#include <openthread/thread.h>
#include <openthread/coap.h>
#include <zephyr/net/openthread.h>
#include<openthread/commissioner.h>
#include<openthread/thread_ftd.h>
#include <openthread/ip6.h>  
#include"../utils/utils.h"

/**
 * @brief OpenThread olay is parcacigi yigin boyutu (bayt).
 */
#define OPENTHREAD_EVENTS_THREAD_STACK_SIZE   2048
/**
 * @brief OpenThread olay is parcacigi onceligi.
 */
#define OPENTHREAD_EVENTS_THREAD_PRIORITY      7


/**
 * @brief Kaynak URI yolu.
 */
typedef enum {
    DEVICE_DATA = 0, /*!< Cihaz verisi */
    HEALTH = 1       /*!< Sağlık bilgisi */
} coap_uri_path_t;


/**
 * @brief URI path enumunu stringe çevirir.
 * @param uri_path CoAP URI path değeri.
 * @return Sabit C string işaretçisi.
 */
const char* get_uri_path(coap_uri_path_t uri_path);

#endif