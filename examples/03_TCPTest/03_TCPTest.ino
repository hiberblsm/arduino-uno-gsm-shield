/*
 * 03_TCPTest.ino
 * Arduino UNO + SIM800C — TCP Soket Testi
 *
 * Test akisi:
 *   1) HTTP bearer (SAPBR) ile token al (port 2884)
 *   2) HTTP bearer kapat
 *   3) CIP stack (initGPRS) ile TCP baglan (port 2885)
 *   4) JSON satirlari gonder, yanit al
 *
 * TCP Format: Her mesaj bir JSON satiri + '\n'
 * Auth: JSON icinde "token" alani
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
const char APN_STR[]  = "internet";

// Flash'ta host/URL sakla
const char TCP_HOST[] PROGMEM = "test.hibersoft.com.tr";
const char URL_TOKEN[] PROGMEM = "http://test.hibersoft.com.tr:2884/token";

static const int  TCP_PORT = 2885;

// Token (runtime'da HTTP ile alinacak)
char authToken[64] = "";
// =====================

// -------- HTTP ile token al (SAPBR bearer) --------
static bool httpGetToken() {
    Serial.println(F("[TOKEN] HTTP ile token aliniyor..."));

    // SAPBR bearer
    {
        char cmd[52];
        gsm.sendAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
        snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,\"APN\",\"%s\"", APN_STR);
        gsm.sendAT(cmd);
        gsm.sendAT("AT+SAPBR=1,1", 15000);
        delay(2000);
    }

    // HTTPINIT
    gsm.sendAT("AT+HTTPTERM", 2000); delay(300);
    if (!gsm.sendATExpect("AT+HTTPINIT", "OK", 3000)) return false;
    if (!gsm.sendATExpect("AT+HTTPPARA=\"CID\",1", "OK"))  return false;

    // URL (PROGMEM'den oku)
    {
        char url[60]; strcpy_P(url, URL_TOKEN);
        char cmd[92]; snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
        if (!gsm.sendATExpect(cmd, "OK")) { gsm.sendAT("AT+HTTPTERM"); return false; }
    }

    gsm.sendATExpect("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK");

    // POST body: {}
    String resp = gsm.sendAT("AT+HTTPDATA=2,5000", 5000);
    if (resp.indexOf(F("DOWNLOAD")) >= 0) gsm.sendAT("{}", 2000);

    // Action + heap-free URC parser
    gsm.sendAT("AT+HTTPACTION=1", 3000);
    int code = gsm.waitHttpActionCode(28000);
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
    gsm.sendAT("AT+SAPBR=0,1", 5000);   // bearer kapat — TCP CIP ile catismaz
    delay(500);

    if (authToken[0] != '\0') { Serial.println(F("[PASS] Token alindi!")); return true; }
    Serial.println(F("[WARN] Token alinamadi, devam...")); return false;
}

// -------- TCP ile JSON gonder --------
static bool tcpSendJson(const char* json) {
    Serial.print(F("  Veri: ")); Serial.println(json);
    if (gsm.tcpSend(json)) {
        Serial.println(F("[PASS] TCP gonderildi"));
        return true;
    }
    Serial.println(F("[FAIL] TCP gonderilemedi"));
    return false;
}

// ==================== SETUP ====================
void setup() {
    Serial.begin(9600);
    while (!Serial);

    Serial.println(F("========================================"));
    Serial.println(F("  Arduino UNO + SIM800C TCP Test"));
    Serial.println(F("  HiberSoft Test Sunucusu"));
    Serial.println(F("========================================"));

    // [1] Modem
    Serial.println(F("[1/6] Modem baslatiliyor..."));
    if (!gsm.begin()) { Serial.println(F("[FAIL] Modem hatasi!")); while (1) delay(1000); }
    Serial.println(F("[PASS] Modem hazir"));
    gsm.setDebug(false);

    // Takili kalabilecek her seyi temizle
    Serial.println(F("[INIT] Onceki oturum temizleniyor..."));
    gsm.sendAT("AT+HTTPTERM",  2000);
    gsm.sendAT("AT+CIPSHUT",   5000);
    gsm.sendAT("AT+SAPBR=0,1", 5000);
    delay(1000);
    Serial.println(F("[INIT] Temizlendi"));

    // [2] Network
    Serial.println(F("[2/6] Network bekleniyor..."));
    for (int i = 0; i < 30 && !gsm.isRegistered(); i++) { delay(1000); Serial.print('.'); }
    Serial.println();
    if (!gsm.isRegistered()) { Serial.println(F("[FAIL] Network yok!")); while (1) delay(1000); }
    Serial.print(F("[PASS] Network OK | CSQ: ")); Serial.println(gsm.getSignalQuality());

    // [3] CIP GPRS — TCP icin ONCE ac, token HTTP'den sonra da acik kalacak
    Serial.println(F("[3/6] GPRS/CIP baslatiliyor..."));
    if (!gsm.initGPRS(APN_STR, "", "")) {
        Serial.println(F("[FAIL] GPRS hatasi!")); while (1) delay(1000);
    }
    Serial.print(F("[PASS] IP: ")); Serial.println(gsm.getLocalIP());

    // [4] Token (SAPBR bearer — CIP'den bagimsiz, ustune acilabilir)
    Serial.println(F("[4/6] Token aliniyor (HTTP)..."));
    httpGetToken();   // icinde SAPBR acar/kapatir, CIP'e dokunmaz

    // [5] TCP baglan (CIP zaten acik)
    Serial.println(F("[5/6] TCP baglanti kuruluyor..."));
    {
        char host[36]; strcpy_P(host, TCP_HOST);
        Serial.print(F("  Sunucu: ")); Serial.print(host);
        Serial.print(':'); Serial.println(TCP_PORT);
        if (!gsm.tcpConnect(host, TCP_PORT)) {
            Serial.println(F("[FAIL] TCP baglantisi kurulamadi!"));
            gsm.closeGPRS(); while (1) delay(1000);
        }
    }
    Serial.println(F("[PASS] TCP baglanildi!"));

    // [6] Veri gonder
    Serial.println(F("[6/6] JSON verileri gonderiliyor..."));
    {
        char tempStr[8], humStr[8];
        dtostrf(24.5f, 4, 1, tempStr);
        dtostrf(60.2f, 4, 1, humStr);

        // 1. mesaj: sensor verisi
        char body1[140];
        snprintf(body1, sizeof(body1),
                 "{\"token\":\"%s\",\"deviceId\":\"arduino-uno-tcp\","
                 "\"data\":{\"temp\":%s,\"humi\":%s}}\n",
                 authToken, tempStr, humStr);
        tcpSendJson(body1);

        // Yanit
        delay(2000);
        String resp = gsm.tcpReceive(5000);
        if (resp.length() > 0) {
            Serial.println(F("  --- YANIT ---"));
            Serial.println(resp);
            Serial.println(F("  --- BITIS ---"));
        } else {
            Serial.println(F("  [INFO] Sunucu yanit yok (normal)"));
        }

        delay(1000);

        // 2. mesaj: metin
        char body2[120];
        snprintf(body2, sizeof(body2),
                 "{\"token\":\"%s\",\"deviceId\":\"arduino-uno-tcp\","
                 "\"data\":{\"msg\":\"Merhaba HiberSoft!\"}}\n",
                 authToken);
        tcpSendJson(body2);

        delay(2000);
        resp = gsm.tcpReceive(4000);
        if (resp.length() > 0) Serial.println(resp);
    }

    // Temizlik
    gsm.tcpClose();
    gsm.closeGPRS();

    Serial.println(F("========================================"));
    Serial.println(F("  TCP testleri tamamlandi!"));
    Serial.println(F("========================================"));
}

void loop() { delay(10000); }
