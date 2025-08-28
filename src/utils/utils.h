#ifndef L_UTILS_H
#define L_UTILS_H


/**
 * @brief Donanimdan benzersiz kimlik (MAC benzeri) baslangic adresi.
 * Bu makro, nRF SoC uzerindeki `NRF_FICR->DEVICEID` adresini gosterir ve
 * cihazin benzersiz tanimlayicisina erismek icin kullanilir.
 */
#define P_DEVICE_MAC_ADDR NRF_FICR->DEVICEID

/**
 * @brief Genel maksimum tekrar deneme sayisi.
 */
#define MAX_ATTEMPTS 3
/**
 * @brief Genel tekrar deneme gecikmesi (ms).
 */
#define RETRY_DELAY_MS 100

/**
 * @brief Genel tampon boyutu (bayt).
 */
#define BUFFER_SIZE 256

/**
 * @brief Örnek sunucu IPv6 adresi (string).
 */
#define SERVER_IPV6_ADDR   "fdde:ad00:beef:0000:9da4:4886:f1a9:824e" 


#endif // L_UTILS_H