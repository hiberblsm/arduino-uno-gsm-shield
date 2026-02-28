/*
 * 02_HTTPTest.ino
 * Arduino UNO + SIM800C — HTTP GET / POST Testi
 * 
 * Bu sketch SIM800C modülü ile HTTP isteklerini test eder:
 *   - GPRS bağlantısı kurma
 *   - Token alma (POST /token)
 *   - HTTP POST ile veri gönderme (/ingest)
 *   - Mesajları görüntüleme (GET /messages)
 * 
 * Test Sunucusu: test.hibersoft.com.tr:2884
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
const char* APN      = "internet";       // Turkcell: "internet", Vodafone: "internet", Turk Telekom: "tt"
const char* APN_USER = "";
const char* APN_PASS = "";

// HiberSoft Test Sunucusu
const char* SERVER     = "test.hibersoft.com.tr";
const int   HTTP_PORT  = 2884;
const char* API_KEY    = "PUBLIC_TEST_2026_ESP_ARDUINO";

// Token (runtime'da alinacak)
String authToken = "";
// =====================

void printSeparator() {
    Serial.println(F("========================================"));
}

// Token alma fonksiyonu - POST /token
bool getToken() {
    Serial.println(F("[TOKEN] Token aliniyor..."));

    // HTTP servisini baslat
    gsm.sendAT("AT+HTTPTERM", 2000);
    delay(500);

    if (!gsm.sendATExpect("AT+HTTPINIT", "OK")) return false;
    if (!gsm.sendATExpect("AT+HTTPPARA=\"CID\",1", "OK")) return false;

    // URL
    String url = "http://" + String(SERVER) + ":" + String(HTTP_PORT) + "/token";
    if (!gsm.sendATExpect("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK")) return false;

    // API Key header - USERDATA ile eklenir
    String headerData = "x-api-key: " + String(API_KEY) + "\r\n";
    if (!gsm.sendATExpect("AT+HTTPPARA=\"USERDATA\",\"" + headerData + "\"", "OK")) return false;

    // Content-Type bos body icin
    if (!gsm.sendATExpect("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK")) return false;

    // Bos body gonder (POST /token body gerektirmez ama HTTPDATA gerekli)
    String emptyBody = "{}";
    String dataCmd = "AT+HTTPDATA=" + String(emptyBody.length()) + ",10000";
    String resp = gsm.sendAT(dataCmd, 5000);
    if (resp.indexOf("DOWNLOAD") >= 0) {
        gsm.sendAT(emptyBody, 3000);
    }

    // POST istegi
    resp = gsm.sendAT("AT+HTTPACTION=1", 20000);
    delay(3000);

    // Sonucu bekle
    resp += gsm.sendAT("", 5000);

    int idx = resp.indexOf("+HTTPACTION:");
    int httpCode = 0;
    if (idx >= 0) {
        int c1 = resp.indexOf(',', idx);
        int c2 = resp.indexOf(',', c1 + 1);
        httpCode = resp.substring(c1 + 1, c2).toInt();
    }

    if (httpCode == 200) {
        // Yaniti oku
        resp = gsm.sendAT("AT+HTTPREAD", 10000);
        Serial.print(F("[TOKEN] Yanit: "));
        Serial.println(resp);

        // Token'i ayristir: "token":"XXXXX"
        int tStart = resp.indexOf("\"token\":\"");
        if (tStart >= 0) {
            tStart += 9;
            int tEnd = resp.indexOf('"', tStart);
            authToken = resp.substring(tStart, tEnd);
            Serial.print(F("[TOKEN] Token: "));
            Serial.println(authToken);
        }
    }

    gsm.sendAT("AT+HTTPTERM");

    if (authToken.length() > 0) {
        Serial.println(F("[PASS] Token alindi!"));
        return true;
    }
    Serial.println(F("[FAIL] Token alinamadi!"));
    return false;
}

// Veri gonderme - POST /ingest
bool sendData(const String &deviceId, float temp, float humidity) {
    Serial.println(F("[INGEST] Veri gonderiliyor..."));

    gsm.sendAT("AT+HTTPTERM", 2000);
    delay(500);

    if (!gsm.sendATExpect("AT+HTTPINIT", "OK")) return false;
    if (!gsm.sendATExpect("AT+HTTPPARA=\"CID\",1", "OK")) return false;

    String url = "http://" + String(SERVER) + ":" + String(HTTP_PORT) + "/ingest";
    if (!gsm.sendATExpect("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK")) return false;

    // Token header
    String headerData = "x-api-key: " + authToken + "\r\n";
    if (!gsm.sendATExpect("AT+HTTPPARA=\"USERDATA\",\"" + headerData + "\"", "OK")) return false;
    if (!gsm.sendATExpect("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK")) return false;

    // JSON body
    String body = "{\"deviceId\":\"" + deviceId + "\",\"data\":{\"temp\":" +
                  String(temp, 1) + ",\"humidity\":" + String(humidity, 1) + "}}";
    Serial.print(F("  Body: "));
    Serial.println(body);

    String dataCmd = "AT+HTTPDATA=" + String(body.length()) + ",10000";
    String resp = gsm.sendAT(dataCmd, 5000);
    if (resp.indexOf("DOWNLOAD") >= 0) {
        gsm.sendAT(body, 3000);
    }

    // POST
    resp = gsm.sendAT("AT+HTTPACTION=1", 20000);
    delay(3000);
    resp += gsm.sendAT("", 5000);

    int idx = resp.indexOf("+HTTPACTION:");
    int httpCode = 0;
    if (idx >= 0) {
        int c1 = resp.indexOf(',', idx);
        int c2 = resp.indexOf(',', c1 + 1);
        httpCode = resp.substring(c1 + 1, c2).toInt();
    }

    Serial.print(F("  HTTP Code: "));
    Serial.println(httpCode);

    if (httpCode == 200 || httpCode == 201) {
        resp = gsm.sendAT("AT+HTTPREAD", 10000);
        Serial.print(F("  Yanit: "));
        Serial.println(resp);
        Serial.println(F("[PASS] Veri gonderildi!"));
    } else {
        Serial.println(F("[FAIL] Veri gonderilemedi!"));
    }

    gsm.sendAT("AT+HTTPTERM");
    return (httpCode >= 200 && httpCode < 300);
}

// Mesajlari goruntule - GET /messages
bool getMessages() {
    Serial.println(F("[MSG] Mesajlar aliniyor..."));

    String response;
    int httpCode;

    gsm.sendAT("AT+HTTPTERM", 2000);
    delay(500);

    if (!gsm.sendATExpect("AT+HTTPINIT", "OK")) return false;
    if (!gsm.sendATExpect("AT+HTTPPARA=\"CID\",1", "OK")) return false;

    String url = "http://" + String(SERVER) + ":" + String(HTTP_PORT) + "/messages";
    if (!gsm.sendATExpect("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK")) return false;

    String headerData = "x-api-key: " + authToken + "\r\n";
    if (!gsm.sendATExpect("AT+HTTPPARA=\"USERDATA\",\"" + headerData + "\"", "OK")) return false;

    // GET
    gsm.sendAT("AT+HTTPACTION=0", 20000);
    delay(3000);
    String resp = gsm.sendAT("", 5000);

    resp = gsm.sendAT("AT+HTTPREAD", 10000);
    Serial.println(F("  --- MESAJLAR ---"));
    Serial.println(resp);
    Serial.println(F("  --- BITIS ---"));

    gsm.sendAT("AT+HTTPTERM");
    return true;
}

void setup() {
    Serial.begin(9600);
    while (!Serial);

    printSeparator();
    Serial.println(F("  Arduino UNO + SIM800C HTTP Test"));
    Serial.println(F("  HiberSoft Test Sunucusu"));
    printSeparator();
    Serial.println();

    // [1] Modem baslat
    Serial.println(F("[1/6] Modem baslatiliyor..."));
    if (!gsm.begin()) {
        Serial.println(F("[FAIL] Modem hatasi!"));
        while (1) delay(1000);
    }
    Serial.println(F("[PASS] Modem hazir"));
    Serial.println();

    // [2] Network bekle
    Serial.println(F("[2/6] Network bekleniyor..."));
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
    Serial.print(F("[PASS] Network OK | CSQ: "));
    Serial.println(gsm.getSignalQuality());
    Serial.println();

    // [3] GPRS baglan
    Serial.println(F("[3/6] GPRS baglaniliyor..."));
    Serial.print(F("  APN: "));
    Serial.println(APN);

    // Bearer ayarla (HTTP servisi icin)
    gsm.sendAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
    gsm.sendAT("AT+SAPBR=3,1,\"APN\",\"" + String(APN) + "\"");
    if (String(APN_USER).length() > 0) {
        gsm.sendAT("AT+SAPBR=3,1,\"USER\",\"" + String(APN_USER) + "\"");
        gsm.sendAT("AT+SAPBR=3,1,\"PWD\",\"" + String(APN_PASS) + "\"");
    }
    gsm.sendAT("AT+SAPBR=1,1", 15000);
    delay(2000);

    String bearerResp = gsm.sendAT("AT+SAPBR=2,1");
    Serial.print(F("[PASS] GPRS: "));
    Serial.println(bearerResp);
    Serial.println();

    // [4] Token al
    Serial.println(F("[4/6] Token aliniyor..."));
    Serial.print(F("  API Key: "));
    Serial.println(API_KEY);
    if (!getToken()) {
        Serial.println(F("[FAIL] Token alinamadi! Devam ediliyor..."));
    }
    Serial.println();

    // [5] Veri gonder
    Serial.println(F("[5/6] Test verisi gonderiliyor..."));
    sendData("arduino-uno-http", 24.5, 60.2);
    Serial.println();

    // [6] Mesajlari goruntule
    Serial.println(F("[6/6] Mesajlar kontrol ediliyor..."));
    delay(2000);
    getMessages();
    Serial.println();

    // Temizlik
    gsm.sendAT("AT+SAPBR=0,1", 5000);

    printSeparator();
    Serial.println(F("  HTTP testleri tamamlandi!"));
    printSeparator();
}

void loop() {
    delay(10000);
}
