/*
 * SIM800C.h - Arduino UNO GSM Shield Kütüphanesi
 * SIM800C modülü ile temel iletişim fonksiyonları
 * 
 * Pin Bağlantıları:
 *   Arduino Pin 2 -> SIM800C TX
 *   Arduino Pin 3 -> SIM800C RX
 *   Arduino Pin 9 -> SIM800C Boot/Power Key
 * 
 * GitHub: https://github.com/hiberblsm/arduino-uno-gsm-shield
 */

#ifndef SIM800C_H
#define SIM800C_H

#include <Arduino.h>
#include <SoftwareSerial.h>

// Varsayılan pin tanımları
#define SIM800_TX_PIN   2
#define SIM800_RX_PIN   3
#define SIM800_BOOT_PIN 9

// Varsayılan baud rate
#define SIM800_BAUD     9600

// Zaman aşımı değerleri (ms)
#define AT_TIMEOUT       3000
#define GPRS_TIMEOUT    15000
#define HTTP_TIMEOUT    20000
#define MQTT_TIMEOUT    10000
#define BOOT_TIMEOUT    10000

// MQTT sabitler
#define MQTT_CONNECT     0x10
#define MQTT_CONNACK     0x20
#define MQTT_PUBLISH     0x30
#define MQTT_SUBSCRIBE   0x82
#define MQTT_SUBACK      0x90
#define MQTT_PINGREQ     0xC0
#define MQTT_PINGRESP    0xD0
#define MQTT_DISCONNECT  0xE0

class SIM800C {
public:
    // Kurucu: pin numaraları
    SIM800C(uint8_t txPin = SIM800_TX_PIN,
            uint8_t rxPin = SIM800_RX_PIN,
            uint8_t bootPin = SIM800_BOOT_PIN);

    // Başlatma & güç
    bool begin(long baud = SIM800_BAUD);
    void powerOn();
    void powerOff();
    void hardReset();

    // Temel AT komutları
    String sendAT(const String &cmd, uint32_t timeoutMs = AT_TIMEOUT);
    bool sendATExpect(const String &cmd, const String &expected, uint32_t timeoutMs = AT_TIMEOUT);
    bool waitForResponse(const String &expected, uint32_t timeoutMs = AT_TIMEOUT);
    String waitForData(const String &trigger, uint32_t timeoutMs = AT_TIMEOUT);  // trigger gelene kadar oku
    int  waitHttpActionCode(uint32_t timeoutMs = HTTP_TIMEOUT); // heap-free URC parser
    void clearBuffer();

    // Modem bilgileri
    String getIMEI();
    String getIMSI();
    String getOperator();
    int    getSignalQuality();  // CSQ değeri (0-31, 99=bilinmiyor)
    String getSIMStatus();
    bool   isRegistered();

    // GPRS bağlantı
    bool initGPRS(const String &apn, const String &user = "", const String &pass = "");
    bool closeGPRS();
    bool isGPRSConnected();
    String getLocalIP();

    // HTTP fonksiyonlar
    bool httpGET(const String &url, String &response, int &httpCode);
    bool httpPOST(const String &url, const String &contentType,
                  const String &data, String &response, int &httpCode);

    // TCP fonksiyonlar
    bool tcpConnect(const String &host, uint16_t port);
    bool tcpSend(const String &data);
    String tcpReceive(uint32_t timeoutMs = 5000);
    bool tcpClose();
    bool isTCPConnected();

    // UDP fonksiyonlar
    bool udpStart(const String &host, uint16_t port);
    bool udpSend(const String &data);
    String udpReceive(uint32_t timeoutMs = 5000);
    bool udpClose();

    // MQTT fonksiyonlar (TCP üzerinden raw paketler)
    bool mqttConnect(const String &broker, uint16_t port,
                     const String &clientId,
                     const String &user = "", const String &pass = "");
    bool mqttPublish(const String &topic, const String &payload);
    bool mqttSubscribe(const String &topic);
    String mqttLoop(uint32_t timeoutMs = 1000);  // gelen mesajları oku
    bool mqttPing();
    bool mqttDisconnect();

    // SMS fonksiyonlar
    bool   smsSend(const String &number, const String &message);
    String smsRead(int index);                        // AT+CMGR - ham yanıt
    bool   smsDelete(int index);                      // AT+CMGD=index,0
    bool   smsDeleteAll();                            // AT+CMGD=1,4 (tümü)
    String smsList(const String &status = "ALL");     // AT+CMGL
    bool   smsSetIncoming(bool enable);               // AT+CNMI URC aç/kapat
    String smsWaitIncoming(uint32_t timeoutMs = 30000); // +CMT: URC bekle

    // Yardımcı
    void debugPrint(const String &msg);
    void setDebug(bool enabled);

private:
    SoftwareSerial *_serial;
    uint8_t _bootPin;
    bool _debugEnabled;

    // MQTT dahili
    bool _mqttConnected;
    String _mqttBroker;
    uint16_t _mqttPort;

    // Dahili yardımcılar
    bool _startTCP(const String &host, uint16_t port);
    void _sendMQTTPacket(uint8_t *packet, size_t len);
    void _encodeRemainingLength(uint8_t *buf, size_t &idx, uint32_t len);
    // Heap kullanmadan serial'i tara (sabit 64 byte sliding window)
    bool _scanFor(const char *trigger, uint32_t timeoutMs);
};

#endif // SIM800C_H
