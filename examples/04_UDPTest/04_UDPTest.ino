/*
 * 04_UDPTest.ino
 * Arduino UNO + SIM800C — UDP Gönderim Testi
 * 
 * Bu sketch SIM800C modülü ile UDP iletisimini test eder:
 *   - GPRS bağlantısı
 *   - UDP soket (test.hibersoft.com.tr:2886)
 *   - JSON formatında veri gönderme
 * 
 * UDP Format: Tek JSON paket
 * Auth: JSON içinde "token" alanı
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
const char* APN      = "internet";
const char* APN_USER = "";
const char* APN_PASS = "";

// HiberSoft Test Sunucusu
const char* UDP_HOST = "test.hibersoft.com.tr";
const int   UDP_PORT = 2886;

// Token
const char* API_KEY  = "PUBLIC_TEST_2026_ESP_ARDUINO";
String authToken = "";
// =====================

void printSeparator() {
    Serial.println(F("========================================"));
}

// HTTP uzerinden token al
bool getToken() {
    Serial.println(F("[TOKEN] Token aliniyor (HTTP)..."));

    gsm.sendAT("AT+HTTPTERM", 2000);
    delay(500);

    if (!gsm.sendATExpect("AT+HTTPINIT", "OK")) return false;
    if (!gsm.sendATExpect("AT+HTTPPARA=\"CID\",1", "OK")) return false;

    String url = "http://" + String(UDP_HOST) + ":2884/token";
    if (!gsm.sendATExpect("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK")) return false;

    String headerData = "x-api-key: " + String(API_KEY) + "\r\n";
    if (!gsm.sendATExpect("AT+HTTPPARA=\"USERDATA\",\"" + headerData + "\"", "OK")) return false;
    if (!gsm.sendATExpect("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK")) return false;

    String emptyBody = "{}";
    String dataCmd = "AT+HTTPDATA=" + String(emptyBody.length()) + ",10000";
    String resp = gsm.sendAT(dataCmd, 5000);
    if (resp.indexOf("DOWNLOAD") >= 0) {
        gsm.sendAT(emptyBody, 3000);
    }

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

    if (httpCode == 200) {
        resp = gsm.sendAT("AT+HTTPREAD", 10000);
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
    return (authToken.length() > 0);
}

void setup() {
    Serial.begin(9600);
    while (!Serial);

    printSeparator();
    Serial.println(F("  Arduino UNO + SIM800C UDP Test"));
    Serial.println(F("  HiberSoft Test Sunucusu"));
    printSeparator();
    Serial.println();

    // [1] Modem
    Serial.println(F("[1/5] Modem baslatiliyor..."));
    if (!gsm.begin()) {
        Serial.println(F("[FAIL] Modem hatasi!"));
        while (1) delay(1000);
    }
    Serial.println(F("[PASS] Modem hazir"));
    Serial.println();

    // [2] Network
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
    Serial.print(F("[PASS] Network OK | CSQ: "));
    Serial.println(gsm.getSignalQuality());
    Serial.println();

    // [3] GPRS
    Serial.println(F("[3/5] GPRS baglaniliyor..."));
    if (!gsm.initGPRS(APN, APN_USER, APN_PASS)) {
        Serial.println(F("[FAIL] GPRS hatasi!"));
        while (1) delay(1000);
    }
    Serial.print(F("[PASS] IP: "));
    Serial.println(gsm.getLocalIP());
    Serial.println();

    // [4] Token al
    Serial.println(F("[4/5] Token aliniyor..."));
    gsm.sendAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
    gsm.sendAT("AT+SAPBR=3,1,\"APN\",\"" + String(APN) + "\"");
    gsm.sendAT("AT+SAPBR=1,1", 15000);
    delay(2000);

    if (!getToken()) {
        Serial.println(F("[FAIL] Token alinamadi!"));
    } else {
        Serial.println(F("[PASS] Token alindi!"));
    }
    Serial.println();

    // UDP icin GPRS'i yeniden kur
    gsm.closeGPRS();
    delay(1000);
    gsm.sendAT("AT+SAPBR=0,1", 5000);
    delay(1000);
    gsm.initGPRS(APN, APN_USER, APN_PASS);
    delay(1000);

    // [5] UDP test
    Serial.println(F("[5/5] UDP testi basliyor..."));
    Serial.print(F("  Sunucu: "));
    Serial.print(UDP_HOST);
    Serial.print(F(":"));
    Serial.println(UDP_PORT);

    if (!gsm.udpStart(UDP_HOST, UDP_PORT)) {
        Serial.println(F("[FAIL] UDP baglanti hatasi!"));
        gsm.closeGPRS();
        while (1) delay(1000);
    }
    Serial.println(F("[PASS] UDP soket hazir!"));
    Serial.println();

    // Veri gonder - Test 1
    Serial.println(F("--- UDP Mesaj 1: Sensor Verisi ---"));
    String json1 = "{\"token\":\"" + authToken + "\","
                   "\"deviceId\":\"arduino-uno-udp\","
                   "\"data\":{\"rssi\":" + String(-113 + gsm.getSignalQuality() * 2) +
                   ",\"temp\":22.5}}";
    Serial.print(F("  Veri: "));
    Serial.println(json1);

    if (gsm.udpSend(json1)) {
        Serial.println(F("[PASS] UDP mesaj 1 gonderildi!"));
    } else {
        Serial.println(F("[FAIL] UDP gonderim hatasi!"));
    }
    Serial.println();
    delay(2000);

    // Veri gonder - Test 2
    Serial.println(F("--- UDP Mesaj 2: Durum Raporu ---"));
    String json2 = "{\"token\":\"" + authToken + "\","
                   "\"deviceId\":\"arduino-uno-udp\","
                   "\"data\":{\"status\":\"online\",\"uptime\":";
    json2 += String(millis() / 1000);
    json2 += "}}";
    Serial.print(F("  Veri: "));
    Serial.println(json2);

    if (gsm.udpSend(json2)) {
        Serial.println(F("[PASS] UDP mesaj 2 gonderildi!"));
    } else {
        Serial.println(F("[FAIL] UDP gonderim hatasi!"));
    }
    Serial.println();
    delay(2000);

    // Veri gonder - Test 3
    Serial.println(F("--- UDP Mesaj 3: Konum Bilgisi ---"));
    String json3 = "{\"token\":\"" + authToken + "\","
                   "\"deviceId\":\"arduino-uno-udp\","
                   "\"data\":{\"lat\":41.0082,\"lon\":28.9784,\"label\":\"Istanbul\"}}";
    Serial.print(F("  Veri: "));
    Serial.println(json3);

    if (gsm.udpSend(json3)) {
        Serial.println(F("[PASS] UDP mesaj 3 gonderildi!"));
    } else {
        Serial.println(F("[FAIL] UDP gonderim hatasi!"));
    }
    Serial.println();

    // Temizlik
    gsm.udpClose();
    gsm.closeGPRS();

    printSeparator();
    Serial.println(F("  UDP testleri tamamlandi!"));
    Serial.println(F("  Mesajlari gormek icin:"));
    Serial.println(F("  curl http://test.hibersoft.com.tr:2884/messages"));
    printSeparator();
}

void loop() {
    delay(10000);
}
