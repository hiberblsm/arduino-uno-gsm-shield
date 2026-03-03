/*
 * 02_HTTPTest.ino
 * Arduino UNO + SIM800C — HTTP GET / POST Testi
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
// APN: Turkcell/Vodafone: "internet", Turk Telekom: "tt"
const char APN_STR[] = "internet";

// HiberSoft Test Sunucusu - Flash'ta sakla
const char URL_TOKEN[]    PROGMEM = "http://test.hibersoft.com.tr:2884/token";
const char URL_INGEST[]   PROGMEM = "http://test.hibersoft.com.tr:2884/ingest";
const char URL_MESSAGES[] PROGMEM = "http://test.hibersoft.com.tr:2884/messages";

// Token (runtime'da alinacak)
char authToken[64] = "";

// =====================

static bool httpSetUrl(const char* urlPROGMEM) {
    char url[60]; strcpy_P(url, urlPROGMEM);
    char cmd[92]; snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
    return gsm.sendATExpect(cmd, "OK");
}

static bool httpSetUserdata(const char* token) {
    char cmd[115]; snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"USERDATA\",\"x-api-key: %s\"", token);
    return gsm.sendATExpect(cmd, "OK");
}

static bool httpInit() {
    gsm.sendAT("AT+HTTPTERM", 2000); delay(500);
    if (!gsm.sendATExpect("AT+HTTPINIT", "OK", 3000)) return false;
    return gsm.sendATExpect("AT+HTTPPARA=\"CID\",1", "OK");
}

static int waitHttpAction(uint8_t method) {
    char cmd[22]; snprintf(cmd, sizeof(cmd), "AT+HTTPACTION=%d", method);
    gsm.sendAT(cmd, 3000);          // OK gel, kes
    // +HTTPACTION: URC heap-free char-level parser ile yakala
    return gsm.waitHttpActionCode(28000);
}

// ==================== TOKEN ====================
bool getToken() {
    Serial.println(F("[TOKEN] Token aliniyor..."));
    if (!httpInit()) { Serial.println(F("  [ERR] HTTPINIT fail")); return false; }
    if (!httpSetUrl(URL_TOKEN)) { gsm.sendAT("AT+HTTPTERM"); return false; }
    gsm.sendATExpect("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK");

    String resp = gsm.sendAT("AT+HTTPDATA=2,5000", 5000);
    if (resp.indexOf(F("DOWNLOAD")) >= 0) gsm.sendAT("{}", 2000);

    int code = waitHttpAction(1);
    Serial.print(F("  HTTP ")); Serial.println(code);

    if (code == 200) {
        resp = gsm.sendAT("AT+HTTPREAD", 8000);
        int s = resp.indexOf("\"token\":\"");
        if (s >= 0) {
            s += 9;
            int e = resp.indexOf('"', s);
            if (e > s) resp.substring(s, e).toCharArray(authToken, sizeof(authToken));
        }
        Serial.print(F("  Token: ")); Serial.println(authToken);
    }
    gsm.sendAT("AT+HTTPTERM");

    if (authToken[0] != '\0') { Serial.println(F("[PASS] Token alindi!")); return true; }
    Serial.println(F("[FAIL] Token alinamadi!")); return false;
}

// ==================== INGEST ====================
bool sendData(float temp, float hum) {
    Serial.println(F("[INGEST] Veri gonderiliyor..."));
    if (!httpInit()) { Serial.println(F("  [ERR] HTTPINIT fail")); return false; }
    if (!httpSetUrl(URL_INGEST))     { gsm.sendAT("AT+HTTPTERM"); return false; }
    if (!httpSetUserdata(authToken)) { gsm.sendAT("AT+HTTPTERM"); return false; }
    gsm.sendATExpect("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK");

    // JSON body - dtostrf ile float formatla (AVR snprintf %f desteklemez)
    char tempStr[8], humStr[8];
    dtostrf(temp, 4, 1, tempStr);
    dtostrf(hum,  4, 1, humStr);
    char body[80];
    snprintf(body, sizeof(body),
             "{\"deviceId\":\"arduino-uno\",\"data\":{\"temp\":%s,\"humi\":%s}}",
             tempStr, humStr);
    Serial.print(F("  Body: ")); Serial.println(body);

    char dcmd[28]; snprintf(dcmd, sizeof(dcmd), "AT+HTTPDATA=%d,5000", (int)strlen(body));
    String resp = gsm.sendAT(dcmd, 5000);
    if (resp.indexOf(F("DOWNLOAD")) >= 0) gsm.sendAT(body, 2000);

    int code = waitHttpAction(1);
    Serial.print(F("  HTTP ")); Serial.println(code);

    if (code >= 200 && code < 300) {
        resp = gsm.sendAT("AT+HTTPREAD", 5000);
        Serial.print(F("  Yanit: ")); Serial.println(resp);
        Serial.println(F("[PASS] Veri gonderildi!"));
    } else { Serial.println(F("[FAIL] Gonderilemedi!")); }

    gsm.sendAT("AT+HTTPTERM");
    return (code >= 200 && code < 300);
}

// ==================== MESSAGES ====================
bool getMessages() {
    Serial.println(F("[MSG] Mesajlar aliniyor..."));
    if (!httpInit()) { Serial.println(F("  [ERR] HTTPINIT fail")); return false; }
    if (!httpSetUrl(URL_MESSAGES))   { gsm.sendAT("AT+HTTPTERM"); return false; }
    if (!httpSetUserdata(authToken)) { gsm.sendAT("AT+HTTPTERM"); return false; }

    int code = waitHttpAction(0);
    Serial.print(F("  HTTP ")); Serial.println(code);

    String resp = gsm.sendAT("AT+HTTPREAD", 8000);
    Serial.println(F("--- MESAJLAR ---"));
    Serial.println(resp);
    Serial.println(F("--- BITIS ---"));

    gsm.sendAT("AT+HTTPTERM");
    return true;
}

// ==================== SETUP ====================
void setup() {
    Serial.begin(9600);
    while (!Serial);

    Serial.println(F("========================================"));
    Serial.println(F("  Arduino UNO + SIM800C HTTP Test"));
    Serial.println(F("  HiberSoft Test Sunucusu"));
    Serial.println(F("========================================"));

    // [1] Modem
    Serial.println(F("[1/6] Modem baslatiliyor..."));
    if (!gsm.begin()) { Serial.println(F("[FAIL] Modem hatasi!")); while (1) delay(1000); }
    Serial.println(F("[PASS] Modem hazir"));
    gsm.setDebug(false);  // HTTP testinde debug kapat, SoftwareSerial bufferini koru

    // Takili kalabilecek her seyi temizle
    Serial.println(F("[INIT] Onceki oturum temizleniyor..."));
    gsm.sendAT("AT+HTTPTERM", 2000);
    gsm.sendAT("AT+CIPSHUT",  5000);
    gsm.sendAT("AT+SAPBR=0,1", 5000);
    delay(1000);
    Serial.println(F("[INIT] Temizlendi"));

    // [2] Network
    Serial.println(F("[2/6] Network bekleniyor..."));
    for (int i = 0; i < 30 && !gsm.isRegistered(); i++) { delay(1000); Serial.print('.'); }
    Serial.println();
    if (!gsm.isRegistered()) { Serial.println(F("[FAIL] Network yok!")); while (1) delay(1000); }
    Serial.print(F("[PASS] Network OK | CSQ: ")); Serial.println(gsm.getSignalQuality());

    // [3] GPRS - Bearer (char dizisi ile String yok)
    Serial.println(F("[3/6] GPRS baglaniliyor..."));
    {
        char cmd[50];
        gsm.sendAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
        snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,\"APN\",\"%s\"", APN_STR);
        gsm.sendAT(cmd);
        gsm.sendAT("AT+SAPBR=1,1", 15000);
        delay(2000);
        String r = gsm.sendAT("AT+SAPBR=2,1");
        Serial.print(F("  IP: ")); Serial.println(r);
    }
    Serial.println(F("[PASS] GPRS hazir"));

    // [4] Token
    Serial.println(F("[4/6] Token aliniyor..."));
    if (!getToken()) Serial.println(F("[WARN] Token yok, devam..."));

    // [5] Veri gonder
    Serial.println(F("[5/6] Veri gonderiliyor..."));
    sendData(24.5, 60.2);

    // [6] Mesajlar
    Serial.println(F("[6/6] Mesajlar kontrol ediliyor..."));
    delay(1500);
    getMessages();

    gsm.sendAT("AT+SAPBR=0,1", 5000);
    Serial.println(F("========================================"));
    Serial.println(F("  HTTP testleri tamamlandi!"));
    Serial.println(F("========================================"));
}

void loop() { delay(10000); }
