/*
 * 04_UDPTest.ino
 * Arduino UNO + SIM800C — UDP Gönderim Testi
 *
 * Test akisi:
 *   1) CIP GPRS ac
 *   2) SAPBR bearer ile HTTP token al, kapat
 *   3) UDP soket (port 2886) ile JSON paketler gonder
 *
 * Baglantilar:
 *   Arduino Pin 2 -> SIM800C TX
 *   Arduino Pin 3 -> SIM800C RX
 *   Arduino Pin 9 -> SIM800C Boot/Power Key
 *
 * Serial Monitor: 9600 baud
 */

#include <SIM800C.h>
#include <avr/pgmspace.h>

SIM800C gsm(2, 3, 9);

// ====== AYARLAR ======
const char APN_STR[]   = "internet";

const char UDP_HOST[]  PROGMEM = "test.hibersoft.com.tr";
const char URL_TOKEN[] PROGMEM = "http://test.hibersoft.com.tr:2884/token";

static const int UDP_PORT = 2886;

char authToken[64] = "";
// =====================

// -------- HTTP ile token al (SAPBR bearer) --------
static bool httpGetToken() {
    Serial.println(F("[TOKEN] HTTP ile token aliniyor..."));

    {
        char cmd[52];
        gsm.sendAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
        snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,\"APN\",\"%s\"", APN_STR);
        gsm.sendAT(cmd);
        gsm.sendAT("AT+SAPBR=1,1", 15000);
        delay(2000);
    }

    gsm.sendAT("AT+HTTPTERM", 2000); delay(300);
    if (!gsm.sendATExpect("AT+HTTPINIT", "OK", 3000)) return false;
    if (!gsm.sendATExpect("AT+HTTPPARA=\"CID\",1", "OK")) return false;

    {
        char url[60]; strcpy_P(url, URL_TOKEN);
        char cmd[92]; snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
        if (!gsm.sendATExpect(cmd, "OK")) { gsm.sendAT("AT+HTTPTERM"); return false; }
    }
    gsm.sendATExpect("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK");

    String resp = gsm.sendAT("AT+HTTPDATA=2,5000", 5000);
    if (resp.indexOf(F("DOWNLOAD")) >= 0) gsm.sendAT("{}", 2000);

    gsm.sendAT("AT+HTTPACTION=1", 3000);
    int code = gsm.waitHttpActionCode(28000);
    Serial.print(F("  HTTP ")); Serial.println(code);

    if (code == 200) {
        resp = gsm.sendAT("AT+HTTPREAD", 8000);
        int s = resp.indexOf("\"token\":\"");
        if (s >= 0) {
            s += 9; int e = resp.indexOf('"', s);
            if (e > s) resp.substring(s, e).toCharArray(authToken, sizeof(authToken));
        }
        Serial.print(F("  Token: ")); Serial.println(authToken);
    }

    gsm.sendAT("AT+HTTPTERM");
    gsm.sendAT("AT+SAPBR=0,1", 5000);
    delay(500);

    if (authToken[0] != '\0') { Serial.println(F("[PASS] Token alindi!")); return true; }
    Serial.println(F("[WARN] Token alinamadi, devam...")); return false;
}

// -------- UDP JSON gonder --------
static bool udpSendJson(const char* json) {
    Serial.print(F("  Veri: ")); Serial.println(json);
    if (gsm.udpSend(json)) { Serial.println(F("[PASS] UDP gonderildi")); return true; }
    Serial.println(F("[FAIL] UDP gonderilemedi")); return false;
}

// ==================== SETUP ====================
void setup() {
    Serial.begin(9600);
    while (!Serial);

    Serial.println(F("========================================"));
    Serial.println(F("  Arduino UNO + SIM800C UDP Test"));
    Serial.println(F("  HiberSoft Test Sunucusu"));
    Serial.println(F("========================================"));

    // [1] Modem
    Serial.println(F("[1/5] Modem baslatiliyor..."));
    if (!gsm.begin()) { Serial.println(F("[FAIL] Modem hatasi!")); while (1) delay(1000); }
    Serial.println(F("[PASS] Modem hazir"));
    gsm.setDebug(false);

    // Takili kalanları temizle
    Serial.println(F("[INIT] Onceki oturum temizleniyor..."));
    gsm.sendAT("AT+HTTPTERM",  2000);
    gsm.sendAT("AT+CIPSHUT",   5000);
    gsm.sendAT("AT+SAPBR=0,1", 5000);
    delay(1000);
    Serial.println(F("[INIT] Temizlendi"));

    // [2] Network
    Serial.println(F("[2/5] Network bekleniyor..."));
    for (int i = 0; i < 30 && !gsm.isRegistered(); i++) { delay(1000); Serial.print('.'); }
    Serial.println();
    if (!gsm.isRegistered()) { Serial.println(F("[FAIL] Network yok!")); while (1) delay(1000); }
    Serial.print(F("[PASS] Network OK | CSQ: ")); Serial.println(gsm.getSignalQuality());

    // [3] CIP GPRS (UDP icin) — once ac
    Serial.println(F("[3/5] GPRS/CIP baslatiliyor..."));
    if (!gsm.initGPRS(APN_STR, "", "")) {
        Serial.println(F("[FAIL] GPRS hatasi!")); while (1) delay(1000);
    }
    Serial.print(F("[PASS] IP: ")); Serial.println(gsm.getLocalIP());

    // [4] Token (SAPBR — CIP'e dokunmaz)
    Serial.println(F("[4/5] Token aliniyor..."));
    httpGetToken();

    // [5] UDP soket ac ve gonder
    Serial.println(F("[5/5] UDP testi basliyor..."));
    {
        char host[36]; strcpy_P(host, UDP_HOST);
        Serial.print(F("  Sunucu: ")); Serial.print(host);
        Serial.print(':'); Serial.println(UDP_PORT);

        if (!gsm.udpStart(host, UDP_PORT)) {
            Serial.println(F("[FAIL] UDP soket acilamadi!"));
            gsm.closeGPRS(); while (1) delay(1000);
        }
    }
    Serial.println(F("[PASS] UDP soket hazir!"));

    // Mesaj 1: sensor verisi
    Serial.println(F("--- UDP Mesaj 1: Sensor ---"));
    {
        char tempStr[8], humStr[8];
        dtostrf(22.5f, 4, 1, tempStr);
        dtostrf(55.0f, 4, 1, humStr);
        char body[140];
        snprintf(body, sizeof(body),
                 "{\"token\":\"%s\",\"deviceId\":\"arduino-uno-udp\","
                 "\"data\":{\"temp\":%s,\"humi\":%s}}",
                 authToken, tempStr, humStr);
        udpSendJson(body);
    }
    delay(1500);

    // Mesaj 2: durum raporu
    Serial.println(F("--- UDP Mesaj 2: Durum ---"));
    {
        char body[120];
        snprintf(body, sizeof(body),
                 "{\"token\":\"%s\",\"deviceId\":\"arduino-uno-udp\","
                 "\"data\":{\"status\":\"online\",\"uptime\":%lu}}",
                 authToken, millis() / 1000UL);
        udpSendJson(body);
    }
    delay(1500);

    // Mesaj 3: konum
    Serial.println(F("--- UDP Mesaj 3: Konum ---"));
    {
        char latStr[12], lonStr[12];
        dtostrf(41.0082f, 7, 4, latStr);
        dtostrf(28.9784f, 7, 4, lonStr);
        char body[140];
        snprintf(body, sizeof(body),
                 "{\"token\":\"%s\",\"deviceId\":\"arduino-uno-udp\","
                 "\"data\":{\"lat\":%s,\"lon\":%s,\"label\":\"Istanbul\"}}",
                 authToken, latStr, lonStr);
        udpSendJson(body);
    }

    // Temizlik
    gsm.udpClose();
    gsm.closeGPRS();

    Serial.println(F("========================================"));
    Serial.println(F("  UDP testleri tamamlandi!"));
    Serial.println(F("========================================"));
}

void loop() { delay(10000); }
