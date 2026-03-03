/*
 * 05_MQTTTest.ino
 * Arduino UNO + SIM800C — MQTT Publish / Subscribe Testi
 *
 * Test akisi:
 *   1) CIP GPRS ac
 *   2) MQTT broker'a baglan (port 2887)
 *   3) Topic'e abone ol
 *   4) 3 mesaj publish et
 *   5) 5sn subscribe dinle
 *   6) Temiz kapat
 *
 * MQTT Auth: Username / Password
 * Topic: "test/" ile baslamali
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
const char APN_STR[] = "internet";

// Flash'ta sakla
const char MQTT_BROKER_H[] PROGMEM = "test.hibersoft.com.tr";

// Kisa sabit stringler SRAM'da (toplam ~75 byte)
static const char MQTT_USER[]   = "testuser";
static const char MQTT_PASS[]   = "PUBLIC_MQTT_2026_PASS";
static const char MQTT_CLIENT[] = "arduino-uno-sim800c";
static const char PUB_TOPIC[]   = "test/arduino-uno/sensor";
static const char SUB_TOPIC[]   = "test/arduino-uno/cmd";
static const int  MQTT_PORT     = 2887;
// =====================

static bool mqttPub(const char* topic, const char* payload) {
    Serial.print(F("  [PUB] ")); Serial.print(topic);
    Serial.print(F(" -> ")); Serial.println(payload);
    if (gsm.mqttPublish(topic, payload)) {
        Serial.println(F("  [PASS] Publish OK"));
        return true;
    }
    Serial.println(F("  [FAIL] Publish hatasi"));
    return false;
}

void setup() {
    Serial.begin(9600);
    while (!Serial);

    Serial.println(F("========================================"));
    Serial.println(F("  Arduino UNO + SIM800C MQTT Test"));
    Serial.println(F("  HiberSoft MQTT Broker"));
    Serial.println(F("========================================"));

    // [1] Modem
    Serial.println(F("[1/5] Modem baslatiliyor..."));
    if (!gsm.begin()) { Serial.println(F("[FAIL] Modem hatasi!")); while (1) delay(1000); }
    Serial.println(F("[PASS] Modem hazir"));
    gsm.setDebug(false);

    // Takili kalanlari temizle
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
    int csq = gsm.getSignalQuality();
    Serial.print(F("[PASS] Network OK | CSQ: ")); Serial.println(csq);

    // [3] GPRS/CIP (MQTT TCP icin)
    Serial.println(F("[3/5] GPRS/CIP baslatiliyor..."));
    if (!gsm.initGPRS(APN_STR, "", "")) {
        Serial.println(F("[FAIL] GPRS hatasi!")); while (1) delay(1000);
    }
    Serial.print(F("[PASS] IP: ")); Serial.println(gsm.getLocalIP());

    // [4] MQTT baglan
    Serial.println(F("[4/5] MQTT broker'a baglaniliyor..."));
    {
        char broker[36]; strcpy_P(broker, MQTT_BROKER_H);
        Serial.print(F("  Broker: ")); Serial.print(broker);
        Serial.print(':'); Serial.println(MQTT_PORT);
        Serial.print(F("  Client: ")); Serial.println(MQTT_CLIENT);
        Serial.print(F("  User:   ")); Serial.println(MQTT_USER);

        if (!gsm.mqttConnect(broker, MQTT_PORT, MQTT_CLIENT, MQTT_USER, MQTT_PASS)) {
            Serial.println(F("[FAIL] MQTT baglanti hatasi!"));
            gsm.closeGPRS(); while (1) delay(1000);
        }
    }
    Serial.println(F("[PASS] MQTT baglanildi!"));

    // [5] Subscribe
    Serial.println(F("[5/5] Topic'e abone olunuyor..."));
    Serial.print(F("  Subscribe: ")); Serial.println(SUB_TOPIC);
    if (gsm.mqttSubscribe(SUB_TOPIC)) {
        Serial.println(F("[PASS] Subscribe OK"));
    } else {
        Serial.println(F("[WARN] Subscribe hatasi, devam..."));
    }

    // ---- PUBLISH 1: Online bildirimi ----
    Serial.println(F("--- Publish 1: Online ---"));
    {
        char body[80];
        snprintf(body, sizeof(body),
                 "{\"deviceId\":\"arduino-uno-mqtt\","
                 "\"data\":{\"status\":\"online\",\"csq\":%d}}",
                 csq);
        mqttPub(PUB_TOPIC, body);
    }
    delay(1000);

    // ---- PUBLISH 2: Sensor verisi ----
    Serial.println(F("--- Publish 2: Sensor ---"));
    {
        char tempStr[8], humStr[8];
        dtostrf(24.5f, 4, 1, tempStr);
        dtostrf(60.2f, 4, 1, humStr);
        char body[120];
        snprintf(body, sizeof(body),
                 "{\"deviceId\":\"arduino-uno-mqtt\","
                 "\"data\":{\"temp\":%s,\"humi\":%s,\"uptime\":%lu}}",
                 tempStr, humStr, millis() / 1000UL);
        mqttPub(PUB_TOPIC, body);
    }
    delay(1000);

    // ---- PUBLISH 3: Veda ----
    Serial.println(F("--- Publish 3: Offline ---"));
    mqttPub(PUB_TOPIC,
            "{\"deviceId\":\"arduino-uno-mqtt\","
            "\"data\":{\"status\":\"offline\"}}");
    delay(1000);

    // ---- Subscribe mesajlari dinle (5sn) ----
    Serial.println(F("--- Subscribe dinleniyor (5sn)... ---"));
    {
        String incoming = gsm.mqttLoop(5000);
        if (incoming.length() > 0) {
            Serial.print(F("  GELEN: ")); Serial.println(incoming);
            Serial.println(F("[PASS] Subscribe mesaji alindi!"));
        } else {
            Serial.println(F("  [INFO] Gelen mesaj yok (normal)"));
        }
    }

    // Temizlik
    gsm.mqttDisconnect();
    gsm.closeGPRS();

    Serial.println(F("========================================"));
    Serial.println(F("  MQTT testleri tamamlandi!"));
    Serial.println(F("========================================"));
}

void loop() { delay(10000); }
