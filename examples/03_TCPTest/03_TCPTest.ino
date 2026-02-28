/*
 * 03_TCPTest.ino
 * Arduino UNO + SIM800C — TCP Soket Testi
 * 
 * Bu sketch SIM800C modülü ile TCP bağlantısını test eder:
 *   - GPRS bağlantısı
 *   - TCP soket oluşturma (test.hibersoft.com.tr:2885)
 *   - JSON formatında veri gönderme
 *   - Yanıt okuma
 * 
 * TCP Format: Her mesaj bir JSON satırı olmalı ve \n ile bitmeli
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
const char* TCP_HOST = "test.hibersoft.com.tr";
const int   TCP_PORT = 2885;

// Token (HTTP uzerinden alinacak)
const char* API_KEY  = "PUBLIC_TEST_2026_ESP_ARDUINO";
String authToken = "";
// =====================

void printSeparator() {
    Serial.println(F("========================================"));
}

// HTTP uzerinden token al (port 2884)
bool getToken() {
    Serial.println(F("[TOKEN] Token aliniyor (HTTP)..."));

    gsm.sendAT("AT+HTTPTERM", 2000);
    delay(500);

    // Bearer zaten ayarli olmali
    if (!gsm.sendATExpect("AT+HTTPINIT", "OK")) return false;
    if (!gsm.sendATExpect("AT+HTTPPARA=\"CID\",1", "OK")) return false;

    String url = "http://" + String(TCP_HOST) + ":2884/token";
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
    Serial.println(F("  Arduino UNO + SIM800C TCP Test"));
    Serial.println(F("  HiberSoft Test Sunucusu"));
    printSeparator();
    Serial.println();

    // [1] Modem
    Serial.println(F("[1/6] Modem baslatiliyor..."));
    if (!gsm.begin()) {
        Serial.println(F("[FAIL] Modem hatasi!"));
        while (1) delay(1000);
    }
    Serial.println(F("[PASS] Modem hazir"));
    Serial.println();

    // [2] Network
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

    // [3] GPRS
    Serial.println(F("[3/6] GPRS baglaniliyor..."));
    if (!gsm.initGPRS(APN, APN_USER, APN_PASS)) {
        Serial.println(F("[FAIL] GPRS hatasi!"));
        while (1) delay(1000);
    }
    Serial.print(F("[PASS] IP: "));
    Serial.println(gsm.getLocalIP());
    Serial.println();

    // [4] Token al (HTTP bearer ayarla)
    Serial.println(F("[4/6] Token aliniyor..."));
    gsm.sendAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
    gsm.sendAT("AT+SAPBR=3,1,\"APN\",\"" + String(APN) + "\"");
    gsm.sendAT("AT+SAPBR=1,1", 15000);
    delay(2000);

    if (!getToken()) {
        Serial.println(F("[FAIL] Token alinamadi!"));
        Serial.println(F("[INFO] Token olmadan devam ediliyor..."));
    } else {
        Serial.println(F("[PASS] Token alindi!"));
    }
    Serial.println();

    // GPRS'i TCP modu icin yeniden kur
    gsm.closeGPRS();
    delay(1000);
    gsm.sendAT("AT+SAPBR=0,1", 5000);
    delay(1000);
    gsm.initGPRS(APN, APN_USER, APN_PASS);
    delay(1000);

    // [5] TCP baglan
    Serial.println(F("[5/6] TCP baglanti kuruluyor..."));
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

    // [6] JSON veri gonder
    Serial.println(F("[6/6] TCP uzerinden veri gonderiliyor..."));

    // Format: JSON satiri + \n
    String jsonData = "{\"token\":\"" + authToken + "\","
                      "\"deviceId\":\"arduino-uno-tcp\","
                      "\"data\":{\"voltage\":3.3,\"csq\":" +
                      String(gsm.getSignalQuality()) + "}}\n";

    Serial.print(F("  Veri: "));
    Serial.println(jsonData);

    if (gsm.tcpSend(jsonData)) {
        Serial.println(F("[PASS] TCP veri gonderildi!"));

        // Yanit bekle
        Serial.println(F("  Yanit bekleniyor..."));
        delay(2000);
        String response = gsm.tcpReceive(10000);
        if (response.length() > 0) {
            Serial.println(F("  --- YANIT ---"));
            Serial.println(response);
            Serial.println(F("  --- BITIS ---"));
            Serial.println(F("[PASS] TCP yanit alindi!"));
        } else {
            Serial.println(F("[INFO] Sunucu yanit gondermiyor (normal olabilir)"));
        }
    } else {
        Serial.println(F("[FAIL] TCP gonderim hatasi!"));
    }
    Serial.println();

    // Ikinci mesaj
    Serial.println(F("--- Ikinci TCP mesaji ---"));
    jsonData = "{\"token\":\"" + authToken + "\","
               "\"deviceId\":\"arduino-uno-tcp\","
               "\"data\":{\"msg\":\"Merhaba HiberSoft!\"}}\n";

    if (gsm.tcpSend(jsonData)) {
        Serial.println(F("[PASS] 2. mesaj gonderildi!"));
        delay(2000);
        String r = gsm.tcpReceive(5000);
        if (r.length() > 0) Serial.println(r);
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
