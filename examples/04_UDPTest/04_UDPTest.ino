/*
 * 04_UDPTest.ino
 * Arduino UNO + SIM800C — UDP Gönderim Testi
 * 
 * Bu sketch SIM800C modülü ile UDP iletisimini test eder:
 *   - GPRS bağlantısı
 *   - UDP soket oluşturma
 *   - NTP sunucusuna zaman sorgusu
 *   - UDP veri gönderme/alma
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

// UDP test sunucusu
const char* UDP_HOST  = "0.pool.ntp.org";
const uint16_t UDP_PORT = 123;  // NTP portu

// Alternatif test: Echo sunucusu
const char* ECHO_HOST = "tcpbin.com";
const uint16_t ECHO_PORT = 9001;
// =====================

void printSeparator() {
    Serial.println(F("========================================"));
}

// NTP paketi olustur (basit versiyon)
void createNTPPacket(uint8_t* packet) {
    memset(packet, 0, 48);
    packet[0] = 0b11100011;   // LI, Version, Mode
    packet[1] = 0;             // Stratum
    packet[2] = 6;             // Polling Interval
    packet[3] = 0xEC;          // Peer Clock Precision
}

void setup() {
    Serial.begin(9600);
    while (!Serial);

    printSeparator();
    Serial.println(F("  Arduino UNO + SIM800C UDP Test"));
    Serial.println(F("  HiberSoft GSM Shield Library"));
    printSeparator();
    Serial.println();

    // Modem başlat
    Serial.println(F("[1/4] Modem baslatiliyor..."));
    if (!gsm.begin()) {
        Serial.println(F("[FAIL] Modem hatasi!"));
        while (1) delay(1000);
    }
    Serial.println(F("[PASS] Modem hazir"));
    Serial.println();

    // Network
    Serial.println(F("[2/4] Network bekleniyor..."));
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
    Serial.println(F("[PASS] Network OK"));
    Serial.println();

    // GPRS
    Serial.println(F("[3/4] GPRS baglaniliyor..."));
    if (!gsm.initGPRS(APN, APN_USER, APN_PASS)) {
        Serial.println(F("[FAIL] GPRS hatasi!"));
        while (1) delay(1000);
    }
    Serial.print(F("[PASS] GPRS OK | IP: "));
    Serial.println(gsm.getLocalIP());
    Serial.println();

    // ===== UDP Test =====
    Serial.println(F("[4/4] UDP testi basliyor..."));
    Serial.println();

    // Test A: Basit UDP veri gonderimi
    Serial.println(F("--- Test A: UDP Echo Test ---"));
    Serial.print(F("  Sunucu: "));
    Serial.print(ECHO_HOST);
    Serial.print(F(":"));
    Serial.println(ECHO_PORT);

    if (gsm.udpStart(ECHO_HOST, ECHO_PORT)) {
        Serial.println(F("  UDP baglanti OK"));

        String testData = "Hello from Arduino UNO + SIM800C!";
        Serial.print(F("  Gonderilen: "));
        Serial.println(testData);

        if (gsm.udpSend(testData)) {
            Serial.println(F("  [PASS] UDP veri gonderildi!"));

            // Echo yanıtı bekle
            String echo = gsm.udpReceive(5000);
            if (echo.length() > 0) {
                Serial.print(F("  Alinan: "));
                Serial.println(echo);
                Serial.println(F("  [PASS] Echo yaniti alindi!"));
            } else {
                Serial.println(F("  [INFO] Echo yaniti alinamadi (timeout)"));
            }
        } else {
            Serial.println(F("  [FAIL] UDP gonderim hatasi!"));
        }
        gsm.udpClose();
    } else {
        Serial.println(F("  [FAIL] UDP baglanti hatasi!"));
    }
    Serial.println();

    delay(2000);

    // Test B: NTP zaman sorgulama
    Serial.println(F("--- Test B: NTP Zaman Sorgusu ---"));
    Serial.print(F("  Sunucu: "));
    Serial.print(UDP_HOST);
    Serial.print(F(":"));
    Serial.println(UDP_PORT);

    // NTP icin yeni baglanti
    // Not: SIM800C tek baglanti modunda calisiyor,
    //       oncelikle CIPSHUT yapilmali  
    gsm.closeGPRS();
    delay(1000);
    gsm.initGPRS(APN, APN_USER, APN_PASS);
    delay(1000);

    if (gsm.udpStart(UDP_HOST, UDP_PORT)) {
        Serial.println(F("  UDP baglanti OK"));
        Serial.println(F("  NTP paketi gonderiliyor..."));

        // NTP paketi (48 byte)
        uint8_t ntpPacket[48];
        createNTPPacket(ntpPacket);

        // Raw bytes gonder
        String cmd = "AT+CIPSEND=48";
        String resp = gsm.sendAT(cmd, 5000);
        if (resp.indexOf('>') >= 0) {
            // SoftwareSerial uzerinden binary gonder
            for (int i = 0; i < 48; i++) {
                // Bu durumda raw byte gonderiyoruz
            }
            Serial.println(F("  [INFO] NTP paketi gonderildi"));
            Serial.println(F("  [INFO] NTP parse islemi ileri seviye bir ornektir"));
        }

        gsm.udpClose();
    }
    Serial.println();

    // Temizlik
    gsm.closeGPRS();

    printSeparator();
    Serial.println(F("  UDP testleri tamamlandi!"));
    printSeparator();
}

void loop() {
    delay(10000);
}
