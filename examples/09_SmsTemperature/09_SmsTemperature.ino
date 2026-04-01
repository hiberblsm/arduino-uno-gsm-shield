/*
 * 09_SmsTemperature.ino - SMS ile Sıcaklık Ölçümü
 *
 * DS18B20 sıcaklık sensörüne SMS gönderilince anlık sıcaklık
 * gönderenin numarasına SMS olarak geri iletilir.
 *
 * Desteklenen SMS Komutları (büyük/küçük harf fark etmez):
 *   SICAKLIK   → Anlık sıcaklık değerini SMS ile bildir
 *   DURUM      → Sıcaklık + sensör durumunu SMS ile bildir
 *   (diğer)    → Yine sıcaklık gönderilir
 *
 * Donanım - Pin Bağlantıları:
 *   Arduino Pin 2   →  SIM800C TX
 *   Arduino Pin 3   →  SIM800C RX
 *   Arduino Pin 9   →  SIM800C Boot/Power Key
 *   Arduino Pin 10  →  DS18B20 Data (sarı/beyaz tel)
 *   DS18B20 VCC     →  3.3V veya 5V
 *   DS18B20 GND     →  GND
 *   DS18B20 Data    →  4.7kΩ dirençle VCC'ye pull-up
 *
 * Gerekli Kütüphaneler:
 *   - OneWire        (arduino-cli lib install "OneWire")
 *   - DallasTemperature (arduino-cli lib install "DallasTemperature")
 *
 * GitHub: https://github.com/hiberblsm/arduino-uno-gsm-shield
 * Geliştirici: Hiber Bilişim - https://www.hiber.com.tr
 */

#include "SIM800C.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// ============================================================
//  YAPILANDIRMA
// ============================================================

#define DS18B20_PIN   10       // DS18B20 data pini

// Yetkili telefon numarası (boş = herkese cevap ver)
#define YETKILI       ""       // örn: "+905XXXXXXXXX"

// ============================================================

OneWire           oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);
SIM800C           gsm;

// ============================================================
//  SICAKLIK OKU
// ============================================================

// Sıcaklığı °C olarak oku, hata durumunda -127 döndür
float sicaklikOku() {
    sensors.requestTemperatures();
    float t = sensors.getTempCByIndex(0);
    return t;  // DEVICE_DISCONNECTED_C = -127.0
}

// ============================================================
//  SMS YANITI OLUŞTUR VE GÖNDER
// ============================================================

void sicaklikSmsGonder(const char *numara) {
    float t = sicaklikOku();
    char cevap[60];

    if (t <= -127.0f) {
        strncpy(cevap, "HATA: DS18B20 sensoru bulunamadi!", sizeof(cevap) - 1);
        cevap[sizeof(cevap) - 1] = '\0';
        Serial.println(F("  [WARN] Sensor bulunamadi!"));
    } else {
        char tmpStr[8];
        dtostrf(t, 4, 1, tmpStr);   // float → "24.5"
        snprintf(cevap, sizeof(cevap),
            "Sicaklik: %s C\nSensor: DS18B20 (Pin %d)\nhiber.com.tr",
            tmpStr, DS18B20_PIN);
        Serial.print(F("  [SENSOR] "));
        Serial.print(tmpStr);
        Serial.println(F(" C"));
    }

    gsm.smsSend(numara, cevap);
    Serial.print(F("  [SMS] Yanit gonderildi -> "));
    Serial.println(numara);
}

void durumSmsGonder(const char *numara) {
    float t = sicaklikOku();
    char cevap[80];
    char tmpStr[8];

    if (t <= -127.0f) {
        strncpy(cevap, "Durum: HATA\nDS18B20 sensoru bulunamadi!\nBaglanti kontrol edin.",
                sizeof(cevap) - 1);
    } else {
        dtostrf(t, 4, 1, tmpStr);
        int csq = gsm.getSignalQuality();
        char csqStr[4];
        itoa(csq, csqStr, 10);
        snprintf(cevap, sizeof(cevap),
            "Durum: OK\nSicaklik: %s C\nSinyal: %s\nDS18B20 Pin%d",
            tmpStr, csqStr, DS18B20_PIN);
    }

    cevap[sizeof(cevap) - 1] = '\0';
    gsm.smsSend(numara, cevap);
    Serial.println(F("  [SMS] Durum gonderildi"));
}

// ============================================================
//  KOMUT İŞLE
// ============================================================

void komutIsle(const char *gonderen, const char *komut) {
    Serial.print(F("[SMS] "));
    Serial.print(gonderen);
    Serial.print(F(" -> \""));
    Serial.print(komut);
    Serial.println('"');

    // Yetki kontrolü
    if (strlen(YETKILI) > 3 && strcmp(gonderen, YETKILI) != 0) {
        Serial.println(F("  [RED] Yetkisiz numara"));
        return;
    }

    if (!strcmp(komut, "DURUM")) {
        durumSmsGonder(gonderen);
    } else {
        // SICAKLIK, herhangi bir komut veya boş → sıcaklık gönder
        sicaklikSmsGonder(gonderen);
    }
}

// ============================================================
//  SETUP
// ============================================================

void setup() {
    Serial.begin(9600);
    delay(500);

    Serial.println(F("\n=============================="));
    Serial.println(F("  09 - SMS Sicaklik Olcumu"));
    Serial.println(F("  Hiber Bilisim - hiber.com.tr"));
    Serial.println(F("=============================="));

    // DS18B20 başlat
    sensors.begin();
    uint8_t adet = sensors.getDeviceCount();
    Serial.print(F("  DS18B20: "));
    Serial.print(adet);
    Serial.println(F(" sensor bulundu"));

    if (adet == 0) {
        Serial.println(F("  [WARN] Sensor bulunamadi! Pull-up direnci ve kablo kontrol edin."));
    } else {
        float t = sicaklikOku();
        char tmpStr[8];
        dtostrf(t, 4, 1, tmpStr);
        Serial.print(F("  Baslangic okumasi: "));
        Serial.print(tmpStr);
        Serial.println(F(" C"));
    }

    // Modem başlat
    Serial.println(F("\nModem baslatiliyor..."));
    gsm.hardReset();

    // SIM kart hazır mı?
    Serial.print(F("  SIM  : "));
    {
        uint8_t i;
        for (i = 0; i < 20; i++) {
            if (gsm.getSIMStatus() == "READY") break;
            delay(1000);
        }
        Serial.println(i < 20 ? F("READY") : F("HATA!"));
        if (i >= 20) { while (1); }
    }

    // Şebeke
    Serial.print(F("  Sebeke: "));
    for (uint8_t i = 0; i < 30; i++) {
        if (gsm.isRegistered()) break;
        delay(1000);
    }
    Serial.println(gsm.getOperator());

    // Sinyal
    Serial.print(F("  RSSI : "));
    Serial.println(gsm.getSignalQuality());

    // Eski SMS'leri temizle
    gsm.smsDeleteAll();

    // Gelen SMS bildirimi aç
    gsm.smsSetIncoming(true);

    Serial.println(F("\n[HAZIR] SMS bekleniyor..."));
    Serial.println(F("  Komutlar: SICAKLIK | DURUM"));
    if (strlen(YETKILI) > 3) {
        Serial.print(F("  Yetkili : "));
        Serial.println(F(YETKILI));
    } else {
        Serial.println(F("  (Herkese acik)"));
    }
}

// ============================================================
//  LOOP
// ============================================================

void loop() {
    char gonderen[20];
    char komut[32];

    if (gsm.smsPoll(gonderen, sizeof(gonderen), komut, sizeof(komut))) {
        komutIsle(gonderen, komut);
        delay(500);
        gsm.smsDeleteAll();
    }
}
