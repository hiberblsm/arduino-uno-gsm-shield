/*
 * 02_HTTPTest.ino
 * Arduino UNO + SIM800C — HTTP GET / POST Testi
 * 
 * Bu sketch SIM800C modülü ile HTTP isteklerini test eder:
 *   - GPRS bağlantısı kurma
 *   - HTTP GET isteği
 *   - HTTP POST isteği (JSON)
 *   - Yanıt okuma
 * 
 * Bağlantılar:
 *   Arduino Pin 2 -> SIM800C TX
 *   Arduino Pin 3 -> SIM800C RX
 *   Arduino Pin 9 -> SIM800C Boot/Power Key
 * 
 * Serial Monitor: 9600 baud
 * 
 * NOT: APN ayarlarini kendi operatorunuze gore degistirin!
 */

#include <SIM800C.h>

SIM800C gsm(2, 3, 9);

// ====== AYARLAR — BURADAN DEĞİŞTİRİN ======
const char* APN  = "internet";       // Turkcell: "internet", Vodafone: "internet", Turk Telekom: "tt"
const char* APN_USER = "";           // Genellikle bos
const char* APN_PASS = "";           // Genellikle bos

// Test URL'leri
const char* TEST_GET_URL  = "http://httpbin.org/ip";
const char* TEST_POST_URL = "http://httpbin.org/post";
// =============================================

void printSeparator() {
    Serial.println(F("========================================"));
}

void setup() {
    Serial.begin(9600);
    while (!Serial);

    printSeparator();
    Serial.println(F("  Arduino UNO + SIM800C HTTP Test"));
    Serial.println(F("  HiberSoft GSM Shield Library"));
    printSeparator();
    Serial.println();

    // Modemi başlat
    Serial.println(F("[1/5] Modem baslatiliyor..."));
    if (!gsm.begin()) {
        Serial.println(F("[FAIL] Modem baslatma hatasi!"));
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
        Serial.println(F("[FAIL] Network kaydi basarisiz!"));
        while (1) delay(1000);
    }

    int csq = gsm.getSignalQuality();
    Serial.print(F("[PASS] Network OK | CSQ: "));
    Serial.println(csq);
    Serial.println();

    // GPRS bağlan
    Serial.println(F("[3/5] GPRS baglaniliyor..."));
    Serial.print(F("  APN: "));
    Serial.println(APN);
    if (!gsm.initGPRS(APN, APN_USER, APN_PASS)) {
        Serial.println(F("[FAIL] GPRS baglanti hatasi!"));
        while (1) delay(1000);
    }
    String ip = gsm.getLocalIP();
    Serial.print(F("[PASS] GPRS OK | IP: "));
    Serial.println(ip);
    Serial.println();

    // HTTP GET testi
    Serial.println(F("[4/5] HTTP GET testi..."));
    Serial.print(F("  URL: "));
    Serial.println(TEST_GET_URL);

    String response;
    int httpCode;
    if (gsm.httpGET(TEST_GET_URL, response, httpCode)) {
        Serial.print(F("  HTTP Code: "));
        Serial.println(httpCode);
        Serial.print(F("  Yanit: "));
        Serial.println(response);
        Serial.println(F("[PASS] HTTP GET basarili!"));
    } else {
        Serial.print(F("  HTTP Code: "));
        Serial.println(httpCode);
        Serial.println(F("[FAIL] HTTP GET basarisiz!"));
    }
    Serial.println();

    // HTTP POST testi
    Serial.println(F("[5/5] HTTP POST testi..."));
    Serial.print(F("  URL: "));
    Serial.println(TEST_POST_URL);

    String postData = "{\"device\":\"arduino-uno\",\"module\":\"sim800c\",\"test\":true}";
    Serial.print(F("  Data: "));
    Serial.println(postData);

    response = "";
    httpCode = 0;
    if (gsm.httpPOST(TEST_POST_URL, "application/json", postData, response, httpCode)) {
        Serial.print(F("  HTTP Code: "));
        Serial.println(httpCode);
        Serial.print(F("  Yanit: "));
        Serial.println(response);
        Serial.println(F("[PASS] HTTP POST basarili!"));
    } else {
        Serial.print(F("  HTTP Code: "));
        Serial.println(httpCode);
        Serial.println(F("[FAIL] HTTP POST basarisiz!"));
    }
    Serial.println();

    // Temizlik
    gsm.closeGPRS();

    printSeparator();
    Serial.println(F("  HTTP testleri tamamlandi!"));
    printSeparator();
}

void loop() {
    // Bos — tek seferlik test
    delay(10000);
}
