/*
 * 05_MQTTTest.ino
 * Arduino UNO + SIM800C — MQTT Publish / Subscribe Testi
 * 
 * Bu sketch SIM800C modülü ile MQTT protokolünü test eder:
 *   - GPRS bağlantısı
 *   - MQTT broker'a bağlanma (TCP üzerinden)
 *   - Topic'e mesaj gönderme (publish)
 *   - Topic'e abone olma (subscribe)
 *   - Gelen mesajları okuma
 * 
 * Bağlantılar:
 *   Arduino Pin 2 -> SIM800C TX
 *   Arduino Pin 3 -> SIM800C RX
 *   Arduino Pin 9 -> SIM800C Boot/Power Key
 * 
 * Serial Monitor: 9600 baud
 * 
 * NOT: MQTT broker ayarlarini kendi sunucunuza gore degistirin!
 */

#include <SIM800C.h>

SIM800C gsm(2, 3, 9);

// ====== AYARLAR — BURADAN DEĞİŞTİRİN ======
const char* APN        = "internet";
const char* APN_USER   = "";
const char* APN_PASS   = "";

// MQTT Broker ayarlari
const char* MQTT_BROKER   = "broker.hivemq.com";  // Ucretsiz public broker
const uint16_t MQTT_PORT  = 1883;
const char* MQTT_CLIENT   = "arduino-uno-sim800c"; // Benzersiz client ID
const char* MQTT_USER     = "";                     // Opsiyonel
const char* MQTT_PASS     = "";                     // Opsiyonel

// Topic ayarlari
const char* PUB_TOPIC  = "hibersoft/arduino/test";
const char* SUB_TOPIC  = "hibersoft/arduino/cmd";
// =============================================

unsigned long lastPublish = 0;
unsigned long lastPing = 0;
int messageCount = 0;

void printSeparator() {
    Serial.println(F("========================================"));
}

void setup() {
    Serial.begin(9600);
    while (!Serial);

    printSeparator();
    Serial.println(F("  Arduino UNO + SIM800C MQTT Test"));
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

    // Network
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

    // GPRS
    Serial.println(F("[3/5] GPRS baglaniliyor..."));
    if (!gsm.initGPRS(APN, APN_USER, APN_PASS)) {
        Serial.println(F("[FAIL] GPRS hatasi!"));
        while (1) delay(1000);
    }
    Serial.print(F("[PASS] GPRS OK | IP: "));
    Serial.println(gsm.getLocalIP());
    Serial.println();

    // MQTT bağlan
    Serial.println(F("[4/5] MQTT broker'a baglaniliyor..."));
    Serial.print(F("  Broker: "));
    Serial.print(MQTT_BROKER);
    Serial.print(F(":"));
    Serial.println(MQTT_PORT);
    Serial.print(F("  Client: "));
    Serial.println(MQTT_CLIENT);

    if (!gsm.mqttConnect(MQTT_BROKER, MQTT_PORT, MQTT_CLIENT, MQTT_USER, MQTT_PASS)) {
        Serial.println(F("[FAIL] MQTT baglanti hatasi!"));
        gsm.closeGPRS();
        while (1) delay(1000);
    }
    Serial.println(F("[PASS] MQTT baglanildi!"));
    Serial.println();

    // Subscribe
    Serial.println(F("[5/5] Topic'e abone olunuyor..."));
    Serial.print(F("  Subscribe: "));
    Serial.println(SUB_TOPIC);

    if (gsm.mqttSubscribe(SUB_TOPIC)) {
        Serial.println(F("[PASS] Subscribe basarili!"));
    } else {
        Serial.println(F("[FAIL] Subscribe hatasi!"));
    }
    Serial.println();

    // İlk publish
    Serial.println(F("--- Ilk mesaj gonderiliyor ---"));
    String payload = "{\"device\":\"arduino-uno\",\"status\":\"online\",\"msg\":0}";
    Serial.print(F("  Topic: "));
    Serial.println(PUB_TOPIC);
    Serial.print(F("  Payload: "));
    Serial.println(payload);

    if (gsm.mqttPublish(PUB_TOPIC, payload)) {
        Serial.println(F("[PASS] Publish basarili!"));
    } else {
        Serial.println(F("[FAIL] Publish hatasi!"));
    }
    Serial.println();

    printSeparator();
    Serial.println(F("  MQTT dongusu basliyor..."));
    Serial.println(F("  Her 30sn'de bir mesaj gonderilecek"));
    Serial.println(F("  Gelen mesajlar gosterilecek"));
    printSeparator();
    Serial.println();

    lastPublish = millis();
    lastPing = millis();
}

void loop() {
    // Gelen MQTT mesajlarını kontrol et
    String incoming = gsm.mqttLoop(2000);
    if (incoming.length() > 0) {
        Serial.print(F("[MESAJ] "));
        Serial.println(incoming);
    }

    // Her 30 saniyede bir publish
    if (millis() - lastPublish > 30000) {
        messageCount++;
        String payload = "{\"device\":\"arduino-uno\",\"msg\":" + String(messageCount) + 
                          ",\"csq\":" + String(gsm.getSignalQuality()) + "}";

        Serial.print(F("[PUB] "));
        Serial.print(PUB_TOPIC);
        Serial.print(F(" -> "));
        Serial.println(payload);

        if (gsm.mqttPublish(PUB_TOPIC, payload)) {
            Serial.println(F("  OK"));
        } else {
            Serial.println(F("  HATA!"));
        }
        lastPublish = millis();
    }

    // Her 50 saniyede bir ping (keepalive)
    if (millis() - lastPing > 50000) {
        gsm.mqttPing();
        lastPing = millis();
    }

    // Serial Monitor'dan komut gönderme
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "quit" || cmd == "exit") {
            Serial.println(F("MQTT kapatiliyor..."));
            gsm.mqttDisconnect();
            gsm.closeGPRS();
            Serial.println(F("Bitti!"));
            while (1) delay(1000);
        } else if (cmd.length() > 0) {
            // Girilen metni publish et
            if (gsm.mqttPublish(PUB_TOPIC, cmd)) {
                Serial.println(F("  Gonderildi!"));
            }
        }
    }
}
