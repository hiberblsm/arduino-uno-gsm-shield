/*
 * 05_MQTTTest.ino
 * Arduino UNO + SIM800C — MQTT Publish / Subscribe Testi
 * 
 * Bu sketch SIM800C modülü ile MQTT protokolünü test eder:
 *   - GPRS bağlantısı
 *   - MQTT broker'a bağlanma (test.hibersoft.com.tr:2887)
 *   - Topic'e mesaj gönderme (publish)
 *   - Topic'e abone olma (subscribe)
 *   - Gelen mesajları okuma
 * 
 * MQTT Auth: Username/Password zorunlu
 * Topic: "test/" veya "devices/" ile baslamali
 * Payload: JSON
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

// HiberSoft MQTT Broker
const char* MQTT_BROKER  = "test.hibersoft.com.tr";
const int   MQTT_PORT    = 2887;
const char* MQTT_USER    = "testuser";
const char* MQTT_PASS    = "PUBLIC_MQTT_2026_PASS";
const char* MQTT_CLIENT  = "arduino-uno-sim800c";

// Topic ayarlari (test/ veya devices/ ile baslamali)
const char* PUB_TOPIC = "test/arduino-uno/sensor";
const char* SUB_TOPIC = "test/arduino-uno/cmd";
// =====================

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
    Serial.println(F("  HiberSoft MQTT Broker"));
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
    int csq = gsm.getSignalQuality();
    Serial.print(F("[PASS] Network OK | CSQ: "));
    Serial.print(csq);
    Serial.print(F(" | RSSI: "));
    Serial.print(-113 + csq * 2);
    Serial.println(F(" dBm"));
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

    // [4] MQTT baglan
    Serial.println(F("[4/5] MQTT broker'a baglaniliyor..."));
    Serial.print(F("  Broker: "));
    Serial.print(MQTT_BROKER);
    Serial.print(F(":"));
    Serial.println(MQTT_PORT);
    Serial.print(F("  Client: "));
    Serial.println(MQTT_CLIENT);
    Serial.print(F("  User: "));
    Serial.println(MQTT_USER);

    if (!gsm.mqttConnect(MQTT_BROKER, MQTT_PORT, MQTT_CLIENT, MQTT_USER, MQTT_PASS)) {
        Serial.println(F("[FAIL] MQTT baglanti hatasi!"));
        gsm.closeGPRS();
        while (1) delay(1000);
    }
    Serial.println(F("[PASS] MQTT baglanildi!"));
    Serial.println();

    // [5] Subscribe
    Serial.println(F("[5/5] Topic'e abone olunuyor..."));
    Serial.print(F("  Subscribe: "));
    Serial.println(SUB_TOPIC);

    if (gsm.mqttSubscribe(SUB_TOPIC)) {
        Serial.println(F("[PASS] Subscribe basarili!"));
    } else {
        Serial.println(F("[WARN] Subscribe hatasi - devam ediliyor"));
    }
    Serial.println();

    // Ilk publish — cihaz online bildirimi
    Serial.println(F("--- Baglanti bildirimi gonderiliyor ---"));
    String payload = "{\"deviceId\":\"arduino-uno-mqtt\","
                     "\"data\":{\"status\":\"online\","
                     "\"csq\":" + String(csq) + ","
                     "\"ip\":\"" + gsm.getLocalIP() + "\","
                     "\"imei\":\"" + gsm.getIMEI() + "\"}}";

    Serial.print(F("  Topic: "));
    Serial.println(PUB_TOPIC);
    Serial.print(F("  Payload: "));
    Serial.println(payload);

    if (gsm.mqttPublish(PUB_TOPIC, payload)) {
        Serial.println(F("[PASS] Online bildirimi gonderildi!"));
    } else {
        Serial.println(F("[FAIL] Publish hatasi!"));
    }
    Serial.println();

    printSeparator();
    Serial.println(F("  MQTT dongusu basliyor..."));
    Serial.println(F("  Her 30sn'de bir sensor verisi gonderilecek"));
    Serial.println(F("  Gelen komutlar gosterilecek"));
    Serial.println(F("  'quit' yazarak cikabilirsiniz"));
    Serial.println();
    Serial.println(F("  Test icin baska terminalden:"));
    Serial.print(F("  mosquitto_pub -h "));
    Serial.print(MQTT_BROKER);
    Serial.print(F(" -p "));
    Serial.println(MQTT_PORT);
    Serial.print(F("    -u \""));
    Serial.print(MQTT_USER);
    Serial.print(F("\" -P \""));
    Serial.print(MQTT_PASS);
    Serial.println(F("\""));
    Serial.print(F("    -t \""));
    Serial.print(SUB_TOPIC);
    Serial.println(F("\""));
    Serial.println(F("    -m '{\"cmd\":\"led\",\"val\":1}'"));
    printSeparator();
    Serial.println();

    lastPublish = millis();
    lastPing = millis();
}

void loop() {
    // Gelen MQTT mesajlarini kontrol et
    String incoming = gsm.mqttLoop(2000);
    if (incoming.length() > 0) {
        Serial.println(F("┌──────────────────────────────────┐"));
        Serial.print(F("│ GELEN MESAJ: "));
        Serial.println(incoming);
        Serial.println(F("└──────────────────────────────────┘"));
    }

    // Her 30 saniyede bir sensor verisi gonder
    if (millis() - lastPublish > 30000) {
        messageCount++;

        int csq = gsm.getSignalQuality();
        int rssi = -113 + csq * 2;

        String payload = "{\"deviceId\":\"arduino-uno-mqtt\","
                         "\"data\":{"
                         "\"msg\":" + String(messageCount) + ","
                         "\"csq\":" + String(csq) + ","
                         "\"rssi\":" + String(rssi) + ","
                         "\"uptime\":" + String(millis() / 1000) + ","
                         "\"freeRam\":" + String(freeRam()) + "}}";

        Serial.print(F("[PUB #"));
        Serial.print(messageCount);
        Serial.print(F("] "));
        Serial.print(PUB_TOPIC);
        Serial.print(F(" -> "));
        Serial.println(payload);

        if (gsm.mqttPublish(PUB_TOPIC, payload)) {
            Serial.println(F("  OK"));
        } else {
            Serial.println(F("  HATA! Yeniden baglanmaya calisiyor..."));
            // Yeniden baglanti denemesi
            gsm.mqttDisconnect();
            delay(2000);
            if (gsm.mqttConnect(MQTT_BROKER, MQTT_PORT, MQTT_CLIENT, MQTT_USER, MQTT_PASS)) {
                Serial.println(F("  Yeniden baglanildi!"));
                gsm.mqttSubscribe(SUB_TOPIC);
            }
        }
        lastPublish = millis();
    }

    // Her 50 saniyede bir ping (keepalive)
    if (millis() - lastPing > 50000) {
        if (gsm.mqttPing()) {
            Serial.println(F("[PING] OK"));
        } else {
            Serial.println(F("[PING] FAIL"));
        }
        lastPing = millis();
    }

    // Serial Monitor'dan komut
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "quit" || cmd == "exit") {
            Serial.println(F("MQTT kapatiliyor..."));
            // Offline bildirimi
            gsm.mqttPublish(PUB_TOPIC,
                "{\"deviceId\":\"arduino-uno-mqtt\",\"data\":{\"status\":\"offline\"}}");
            delay(500);
            gsm.mqttDisconnect();
            gsm.closeGPRS();
            Serial.println(F("Bitti! Reset'e basin."));
            while (1) delay(1000);
        }
        else if (cmd == "status") {
            Serial.println(F("--- DURUM ---"));
            Serial.print(F("  CSQ: "));
            Serial.println(gsm.getSignalQuality());
            Serial.print(F("  Mesaj sayisi: "));
            Serial.println(messageCount);
            Serial.print(F("  Uptime: "));
            Serial.print(millis() / 1000);
            Serial.println(F("s"));
            Serial.print(F("  Free RAM: "));
            Serial.print(freeRam());
            Serial.println(F(" bytes"));
        }
        else if (cmd.length() > 0) {
            // Girilen metni JSON olarak publish et
            String payload = "{\"deviceId\":\"arduino-uno-mqtt\","
                             "\"data\":{\"userMsg\":\"" + cmd + "\"}}";
            if (gsm.mqttPublish(PUB_TOPIC, payload)) {
                Serial.println(F("  Gonderildi!"));
            }
        }
    }
}

// Arduino UNO bos RAM olcumu
int freeRam() {
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
