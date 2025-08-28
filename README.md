# OPENTHREAD_CLIENT

Kısaca: Zephyr RTOS üzerinde nRF52840 ile **OpenThread MTD** çalıştıran bir **CoAP istemcisidir**.  
Belirlenen sunucuya periyodik CoAP mesajları gönderir ve cihaz rol değişimlerini loglar.

Sunucu tarafı proje: `OPENTHREAD_SERVER` (CoAP isteklerini karşılar, mesajları kuyruklar ve loglar).

### Örnek Log
```text
[00:00:05.123] <inf> openthread_events: Sunucuya CoAP mesajı gönderildi
[00:00:10.567] <inf> openthread_events: Rol değişikliği: CHILD -> ROUTER
