/*
 * 03_TCPTest.ino
 * Arduino UNO + SIM800C — TCP Soket Testi
 * 
 * Bu sketch SIM800C modülü ile TCP baglantisini test eder:
 *   - GPRS bağlantısı
 *   - TCP soket oluşturma
 *   - Sunucuya veri gönderme
 *   - Yanıt alma
 * 
 * Bağlantılar:
 *   Arduino Pin 2 -> SIM800C TX
 *   Arduino Pin 3 -> SIM800C RX
 *   Arduino Pin 9 -> SIM800C Boot/Power Key
 * 
 * Serial Monitor: 9600 baud
 */

#include <SIM800C.h>

SIM800C gsm(2, 3, 9);

// ====== AYARLAR ======
const char* APN       = "internet";
const char* APN_USER  = "";
const char* APN_PASS  = "";

// TCP test sunucusu (httpbin.org port 80)
const char* TCP_HOST  = "httpbin.org";
const uint16_t TCP_PORT = 80;
// =====================

void printSeparator() {
    Serial.println(F("========================================"));
}

void setup() {
    Serial.begin(9600);
    while (!Serial);

    printSeparator();
    Serial.println(F("  Arduino UNO + SIM800C TCP Test"));
    Serial.println(F("  HiberSoft GSM Shield Library"));
    printSeparator();
    Serial.println();

    // Modem başlat
    Serial.println(F("[1/5] Modem baslatiliyor..."));
    if (!gsm.begin()) {
        Serial.println(F("[FAIL] Modem hatasi!"));
        while (1) delay(1000);
    }
    Serial.println(F("[PASS] Modem hazir"));
    Serial.println();

    // Network bekle
    Serial.println(F("[2/5] Network bekleniyor..."));
    int retries = 0;
    while (!gsm.isRegistered() && retries < 30) {
        delay(1000);
        retries++;
        Serial.print('.');
    }
    Serial.println();
    if (!gsm.isRegistered()) {
        Serial.println(F("[FAIL] Network kaydi yok!"));
        while (1) delay(1000);
    }
    Serial.println(F("[PASS] Network OK"));
    Serial.println();

    // GPRS
    Serial.println(F("[3/5] GPRS baglaniliyor..."));
    if (!gsm.initGPRS(APN, APN_USER, APN_PASS)) {
        Serial.println(F("[FAIL] GPRS hatasi!"));
        while (1) delay(1000);
    }
    Serial.print(F("[PASS] GPRS OK | IP: "));
    Serial.println(gsm.getLocalIP());
    Serial.println();

    // TCP bağlan
    Serial.println(F("[4/5] TCP baglanti kuruluyor..."));
    Serial.print(F("  Sunucu: "));
    Serial.print(TCP_HOST);
    Serial.print(F(":"));
    Serial.println(TCP_PORT);

    if (!gsm.tcpConnect(TCP_HOST, TCP_PORT)) {
        Serial.println(F("[FAIL] TCP baglanti hatasi!"));
        gsm.closeGPRS();
        while (1) delay(1000);
    }
    Serial.println(F("[PASS] TCP baglanildi!"));
    Serial.println();

    // HTTP isteği gönder (raw TCP üzerinden)
    Serial.println(F("[5/5] TCP uzerinden HTTP GET gonderiyor..."));
    String httpReq = "GET /ip HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n";

    if (gsm.tcpSend(httpReq)) {
        Serial.println(F("[PASS] Veri gonderildi!"));
        Serial.println(F("  Yanit bekleniyor..."));
        delay(2000);

        String response = gsm.tcpReceive(10000);
        if (response.length() > 0) {
            Serial.println(F("  --- YANIT ---"));
            Serial.println(response);
            Serial.println(F("  --- BITIS ---"));
            Serial.println(F("[PASS] TCP veri alindi!"));
        } else {
            Serial.println(F("[FAIL] Yanit alinamadi!"));
        }
    } else {
        Serial.println(F("[FAIL] TCP gonderim hatasi!"));
    }
    Serial.println();

    // Temizlik
    gsm.tcpClose();
    gsm.closeGPRS();

    printSeparator();
    Serial.println(F("  TCP testleri tamamlandi!"));
    printSeparator();
}

void loop() {
    delay(10000);
}
