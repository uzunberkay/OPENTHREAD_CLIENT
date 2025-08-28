#include "openthread_events.h"
#include <zephyr/logging/log.h>
#include<zephyr/init.h>

LOG_MODULE_REGISTER(openthread_events, LOG_LEVEL_DBG);


static otError openthread_coap_send_message(const coap_uri_path_t uri_path,const uint8_t* msg , uint16_t *buffer_index, const otCoapType a_type, const otCoapCode a_code ,const char* ipaddr);
static void state_changed_callback(uint32_t flags, void *context);

otExtAddress device_mac_addr = {0};

                   
/**
 * @brief  Cihazın benzersiz MAC adresini (DEVICEID üzerinden) verilen buffer'a kopyalar.
 *
 * Bu fonksiyon, cihazın donanımsal olarak tanımlı olan DEVICEID bilgisini alır
 * ve @p mac_addr ile gosterilen buffer'a kopyalar. Eger belirtilen uzunluk
 * (len), DEVICEID boyutunu aşarsa, kopyalama boyutu DEVICEID boyutuna
 * sınırlandırılır.
 *
 * @param[out] mac_addr  Cihaz MAC adresinin yazilacagi hedef buffer.
 * @param[in]  len      Kopyalanacak byte sayısı.
 *
 * @note  Eğer @p len, DEVICEID boyutundan büyükse, fonksiyon uyarı logu yazar
 *        ve kopyalanan uzunluğu DEVICEID boyutuyla sınırlar.
 */
static void write_device_mac_addr(uint8_t* mac_addr , uint8_t len)
{
    if(len > (uint8_t)sizeof(NRF_FICR->DEVICEID))
    {
        LOG_WRN("Uzunluk DEVICEID boyutunu asiyor");
        len = (uint8_t)sizeof(NRF_FICR->DEVICEID);
    }
    LOG_DBG("NRF_FICR = %d", NRF_FICR->DEVICEID[0]);
    volatile uint8_t* src = (volatile uint8_t*)P_DEVICE_MAC_ADDR;
    uint8_t* dest = mac_addr;
    while(len--) *dest++ = *src++; 
}



/**
 * @brief  OpenThread yığınını başlatır ve cihazın genişletilmiş (extended) adresini ayarlar.
 *
 * Bu fonksiyon, cihazın donanımsal MAC adresini alır, OpenThread
 * örneğine (otInstance) genişletilmiş adres olarak tanımlar ve
 * ardından OpenThread protokolünü başlatır.
 *
 * Fonksiyon, hem genişletilmiş adres ayarlama hem de OpenThread
 * başlatma işlemlerinde hata alındığında, belirli sayıda (MAX_ATTEMPTS)
 * tekrar denemesi yapar. İşlemler başarısız olursa hata kodunu döner.
 *
 * @return
 *  - OT_ERROR_NONE  : Başarılı
 *  - Diğer otError  : Extended address ayarlama veya OpenThread başlatma sırasında hata
 *
 * @note
 * - Adres ayarlama veya başlatma başarısız olursa, her deneme arasında
 *   @p RETRY_DELAY_MS kadar beklenir.
 * - Mutex kilitleme ve açma işlemleri otContext üzerinden yapılır.
 * - Başarısız durumlarda loglama ile hata bilgisi kullanıcıya iletilir.
 */
static otError openthread_init(void)
{
    otInstance* instance = openthread_get_default_instance();
    otError error = OT_ERROR_NONE;
    struct openthread_context *ot_context = openthread_get_default_context();
    write_device_mac_addr(device_mac_addr.m8, (uint8_t)OT_EXT_ADDRESS_SIZE);
    openthread_api_mutex_lock(ot_context);
    uint8_t retry_count = 0;
    do
    {
         error = otLinkSetExtendedAddress(instance, &device_mac_addr);
         if(error == OT_ERROR_NONE)
         {
            LOG_INF("Extended adres ayarlandi: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                    device_mac_addr.m8[0], device_mac_addr.m8[1], device_mac_addr.m8[2], device_mac_addr.m8[3],
                    device_mac_addr.m8[4], device_mac_addr.m8[5], device_mac_addr.m8[6], device_mac_addr.m8[7]);
            break;
         }
        LOG_WRN("Extended adres ayarlanamadi, tekrar deneniyor... (%d/%d)", retry_count+1, MAX_ATTEMPTS);
        k_msleep(RETRY_DELAY_MS);
    } while (++retry_count < MAX_ATTEMPTS);
    if(error != OT_ERROR_NONE)
    {
        LOG_ERR("Extended adres ayarlanamadi, hata: %d", error);
        openthread_api_mutex_unlock(ot_context);
        return error;
    }
    openthread_api_mutex_unlock(ot_context);
    retry_count = 0;
     do{
        error = openthread_start(ot_context);
        if(error == OT_ERROR_NONE)
        {
            LOG_INF("OpenThread baslatildi.");
            break;
        }
        LOG_INF("OpenThread baslatilamadi, tekrar deneniyor... (%d/%d)", retry_count+1, MAX_ATTEMPTS);
        k_msleep(RETRY_DELAY_MS);

     }while(++retry_count < MAX_ATTEMPTS );
    if(error != OT_ERROR_NONE)
    {
        LOG_ERR("OpenThread baslatma hatasi: %d", error);
    }

    return error ;
}

/**
 * @brief  OpenThread üzerinde CoAP sunucusunu başlatır ve temel resource'u ekler.
 *
 * Bu fonksiyon, OpenThread instance'ını alır ve belirtilen portta
 * CoAP sunucusunu başlatmaya çalışır. Başlatma işlemi başarısız olursa
 * belirli sayıda (MAX_ATTEMPTS) tekrar denenir. Başarıyla başlatılırsa,
 * "Hlth" URI path'ine sahip temel resource eklenir.
 *
 * @return
 *  - OT_ERROR_NONE  : CoAP sunucusu başarıyla başlatıldı.
 *  - Diğer otError  : Başlatma işlemi sırasında hata oluştu.
 *
 * @note
 * - Retry sırasında her deneme arasında @p RETRY_DELAY_MS kadar beklenir.
 * - Fonksiyon, resource eklerken static otCoapResource kullanır.
 * - Başlatma başarısız olursa hata logu yazılır.
 */
static otError coap_init(void)
{
    otInstance* instance = openthread_get_default_instance();
    otError error = OT_ERROR_NONE;
    uint8_t retry_count = 0;
    do{
         error = otCoapStart(instance, OT_DEFAULT_COAP_PORT);
         if(error == OT_ERROR_NONE)
         {
            LOG_INF("CoAP sunucusu %d portunda baslatildi", OT_DEFAULT_COAP_PORT);
            break;
         }
         k_msleep(RETRY_DELAY_MS);
         LOG_WRN("CoAP sunucusu baslatilamadi, tekrar deneniyor... (%d/%d)", retry_count+1, MAX_ATTEMPTS);
    

    }while(retry_count++ < MAX_ATTEMPTS);
    return error ;
}


/**
 * @brief  OpenThread cihaz rolü değişikliklerini işleyen callback fonksiyonu.
 *
 * Bu fonksiyon, OpenThread state değişikliklerini dinler ve özellikle
 * cihaz rolündeki değişiklikleri loglar.
 *
 * @param flags    State değişiklik bayrakları (OpenThread flags).
 * @param context  Callback context, otInstance pointer.
 *
 * @note
 * - Sadece OT_CHANGED_THREAD_ROLE bayrağı işlenir.
 * - Rol değişiklikleri log seviyesinde bilgi mesajı olarak gösterilir.
 * - Rol türleri: DISABLED, DETACHED, CHILD, ROUTER, LEADER, UNKNOWN.
 */
static void state_changed_callback(uint32_t flags, void *context)
{
    otInstance* instance = (otInstance*)context;
    if(flags & OT_CHANGED_THREAD_ROLE)
    {
        otDeviceRole role = otThreadGetDeviceRole(instance);
        switch(role)
        {
            case OT_DEVICE_ROLE_DISABLED:
                LOG_INF("Cihaz rolu: DISABLED");
                break;
            case OT_DEVICE_ROLE_DETACHED:
                LOG_INF("Cihaz rolu: DETACHED");
                break;
            case OT_DEVICE_ROLE_CHILD:
                LOG_INF("Cihaz rolu: CHILD");
                break;
            case OT_DEVICE_ROLE_ROUTER:
                LOG_INF("Cihaz rolu: ROUTER");
                break;
            case OT_DEVICE_ROLE_LEADER:
                LOG_INF("Cihaz rolu: LEADER");
                break;
            default:
                LOG_INF("Cihaz rolu: UNKNOWN");
                break;
        }
    }   
}

/**
 * @brief  Sistem ve OpenThread altyapısını başlatır.
 *
 * Bu fonksiyon, OpenThread cihazının çalışması için gerekli tüm
 * başlangıç işlemlerini sırasıyla gerçekleştirir:
 * 1. OpenThread yığınını başlatır (`openthread_init()`).
 * 2. CoAP sunucusunu başlatır ve temel resource'u ekler (`coap_init()`).
 * 3. Statik IPv6 adresi ekler (`add_ipv6_address()`).
 * 4. OpenThread state değişiklikleri için callback fonksiyonunu kaydeder.
 * 5. Komşu ve IPv6 adres değişiklikleri için callback fonksiyonlarını kaydeder.
 *
 * Her adımda hata oluşursa, fonksiyon hata kodunu döndürür ve log yazar.
 *
 * @param dev  Kullanılmayan cihaz parametresi (ARG_UNUSED).
 *
 * @return
 *  - 0 (int)           : Tüm başlatma işlemleri başarılı.
 *  - Diğer otError kodu : Başlatma sırasında oluşan hata.
 *
 * @note
 * - Fonksiyon, cihazın OpenThread instance'ının daha önce oluşturulmuş
 *   olmasını gerektirir.
 * - Callback fonksiyonları cihazın durumunu ve ağ olaylarını takip etmek
 *   için gereklidir.
 */
static int system_init(const struct device *dev)
{
    ARG_UNUSED(dev);
    otInstance* instance = openthread_get_default_instance();
    otError error = openthread_init();
    if(error != OT_ERROR_NONE)
    {
        LOG_ERR("OpenThread ilklendirme basarisiz, hata: %d", error);
        return (int)error ;
    } 
    error = coap_init();
    if(error != OT_ERROR_NONE)
    {
        LOG_ERR("CoAP ilklendirme basarisiz, hata: %d", error);
        return (int)error ;
    }
    error = otSetStateChangedCallback(instance, state_changed_callback, instance);
    if(error != OT_ERROR_NONE)
    {
        LOG_ERR("State degisiklik callback kaydi basarisiz, hata: %d", error);
        return (int)error ;
    }
    return (int)error ;
}

SYS_INIT(system_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);






/**
 * @brief Verilen URI path türüne karşılık gelen string ifadesini döndürür.
 *
 * Bu fonksiyon, uygulamada kullanılan CoAP URI path enum değerini (coap_uri_path_t)
 * string karşılığına çevirir. Eğer bilinmeyen bir değer verilirse "unknown" döndürür.
 *
 * @param[in] uri_path  CoAP URI path türü (DEVICE_DATA, HEALTH, vb.).
 *
 * @return 
 * - "data"  -> DEVICE_DATA için
 * - "Hlth"  -> HEALTH için
 * - "unknown" -> Tanımsız değerler için
 *
 * @note Dönüş değeri sabit bir string işaretçisidir, kopyalanmadan doğrudan kullanılabilir.
 */
const char* get_uri_path(coap_uri_path_t uri_path)
{
    switch(uri_path)
    {
        case DEVICE_DATA:
            return "data";
        case HEALTH:
            return "Hlth";
        default:
            return "unknown";
    }
}



/**
 * @brief CoAP isteği oluşturur ve belirtilen IPv6 adrese gönderir.
 *
 * Bu fonksiyon, verilen URI path'e ve CoAP tür/kod bilgisine göre bir CoAP
 * mesajı hazırlar, payload'ı ekler ve hedef IPv6 adrese isteği yollar. Hedef
 * adres multicast ya da unicast olabilir; geçerli bir IPv6 string'i
 * beklenmektedir.
 *
 * @param[in]  uri_path      CoAP URI path türü (ör. DEVICE_DATA, HEALTH).
 * @param[in]  msg           Gönderilecek payload verisi için buffer işaretçisi.
 * @param[in,out] buffer_index  Gönderilecek payload uzunluğu (byte). Eğer
 *                              BUFFER_SIZE değerinden büyükse BUFFER_SIZE'a
 *                              kırpılır. İşlem sonrası aynı değeri korur.
 * @param[in]  a_type        CoAP mesaj tipi (ör. OT_COAP_TYPE_NON_CONFIRMABLE).
 * @param[in]  a_code        CoAP metod kodu (ör. OT_COAP_CODE_PUT, OT_COAP_CODE_POST).
 * @param[in]  ipaddr        Hedef IPv6 adresi (string, ör. "ff03::1" ya da unicast adres).
 *
 * @retval OT_ERROR_NONE           İstek başarıyla gönderildi.
 * @retval OT_ERROR_INVALID_ARGS   Geçersiz parametre ya da IP adresi.
 * @retval OT_ERROR_INVALID_STATE  OpenThread instance alınamadı.
 * @retval Diğer otError           Mesaj hazırlama/gönderme sırasında oluşan hatalar.
 */
static otError openthread_coap_send_message(const coap_uri_path_t uri_path,const uint8_t* msg , uint16_t *buffer_index, const otCoapType a_type, const otCoapCode a_code ,const char* ipaddr)
{
    LOG_DBG("CoAP mesaji hazirlaniyor");
    if (msg == NULL || buffer_index == NULL || *buffer_index == 0 ) {
        LOG_ERR("Gecersiz parametre: msg/buffer_index hatali");
        return OT_ERROR_INVALID_ARGS;
    }

    otMessage *message = NULL;
    otMessageInfo message_info;
    otInstance *instance = openthread_get_default_instance();

    if (instance == NULL) {
        LOG_ERR("OpenThread instance alinamadi");
        return OT_ERROR_INVALID_STATE;
    }
    uint8_t payload[BUFFER_SIZE] = {0};
    *buffer_index = *buffer_index > BUFFER_SIZE ? *buffer_index = BUFFER_SIZE : *buffer_index;
    memcpy(payload,msg,*buffer_index);
    message = otCoapNewMessage(instance, NULL);
    if (message == NULL) {
        LOG_ERR("CoAP istegi icin mesaj olusturulamadi");
        goto exit;
    }

    otCoapMessageInit(message, a_type, a_code);

    otError error = otCoapMessageAppendUriPathOptions(message, get_uri_path(uri_path));
    if (error != OT_ERROR_NONE) {
        LOG_ERR("URI path eklenemedi, hata: %d", error);
        goto exit;
    }


    error = otCoapMessageSetPayloadMarker(message);
    if (error != OT_ERROR_NONE) {
        LOG_ERR("Payload marker eklenemedi, hata: %d", error);
        goto exit;
    }

    error = otMessageAppend(message, payload, *buffer_index);
    if (error != OT_ERROR_NONE) {
        LOG_ERR("Payload CoAP mesajina eklenemedi, hata: %d", error);
        goto exit;
    }
    LOG_DBG("Payload CoAP mesajina eklendi");

    memset(&message_info, 0, sizeof(message_info));
    const char *destination_ip = ipaddr;
    if (!destination_ip) {
        LOG_ERR("Hedef IP adresi gecersiz");
        error = OT_ERROR_INVALID_ARGS;
        goto exit;
    }

    error = otIp6AddressFromString(destination_ip, &message_info.mPeerAddr);
    if (error != OT_ERROR_NONE) {
        LOG_ERR("Hedef IP adresi olusturulamadi, hata: %d", error);
        goto exit;
    }

    message_info.mPeerPort = OT_DEFAULT_COAP_PORT;
    error = otCoapSendRequest(instance, message, &message_info, NULL, NULL);
    if (error != OT_ERROR_NONE) {
        LOG_ERR("CoAP istegi gonderilemedi, hata: %d", error);
        goto exit;
    }
    LOG_INF("CoAP mesaji gonderildi");
    return OT_ERROR_NONE;

exit:
    if (message) {
        LOG_ERR("Mesaj gonderimi basarisiz, kaynaklar serbest birakiliyor");
        otMessageFree(message);

    }
    return error;
}


void openthread_event_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);
    const char* msg = "Merhaba OpenThread";
    uint16_t buffer_index = strlen(msg);
    while (1)
    {
        openthread_coap_send_message(DEVICE_DATA,(const uint8_t *)msg, &buffer_index, OT_COAP_TYPE_NON_CONFIRMABLE,OT_COAP_CODE_PUT,SERVER_IPV6_ADDR);
        k_msleep(1000);
    }
}


K_THREAD_DEFINE(openthread_thread, OPENTHREAD_EVENTS_THREAD_STACK_SIZE,
               openthread_event_thread, NULL,
               NULL, NULL,
               OPENTHREAD_EVENTS_THREAD_PRIORITY, 0, 0);