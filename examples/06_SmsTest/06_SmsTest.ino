/*
 * 06_SmsTest.ino - SMS Gönderme / Alma / Okuma / Silme Testi
 *
 * Bu örnek SIM800C kütüphanesi ile:
 *   1. SMS gönderme  (AT+CMGS)
 *   2. SMS listeleme (AT+CMGL)
 *   3. SMS okuma     (AT+CMGR)
 *   4. SMS alma      (AT+CNMI  +CMT: URC)
 *   5. SMS silme     (AT+CMGD)
 * işlemlerini test eder. GPRS gerektirmez.
 *
 * Kurulum:
 *   TEST_PHONE_NUMBER  ->  kendi numaranı yaz (+905XXXXXXXXX gibi)
 *
 * UYARI: Hat üzerinde SMS gönderme kredisi / yetkisi gerekebilir.
 *
 * Pin Bağlantıları:
 *   Arduino Pin 2  ->  SIM800C TX
 *   Arduino Pin 3  ->  SIM800C RX
 *   Arduino Pin 9  ->  SIM800C Boot/Power Key
 *
 * GitHub: https://github.com/hiberblsm/arduino-uno-gsm-shield
 */

#include "SIM800C.h"

// ============================================================
//  YAPILANDIRMA
// ============================================================
#define TEST_PHONE_NUMBER  "+905468422222"
// ============================================================

SIM800C gsm;

// ============================================================
void setup() {
    Serial.begin(9600);
    delay(1000);

    Serial.println(F("\n=============================="));
    Serial.println(F("  06 - SMS Test"));
    Serial.println(F("=============================="));

    // ----------------------------------------------------------
    // [1/6] Modem başlat
    // ----------------------------------------------------------
    Serial.println(F("\n[1/6] Modem baslatiliyor..."));

    // hardReset() → powerOff → powerOn → begin()
    // begin() icinde 'SMS Ready' URC bekleniyor (15sn)
    // Ayri begin() cagirma — iki kez cagirmak 'SMS Ready' URC'yi yer
    gsm.hardReset();

    // CPIN kontrolu — SIM hazir mi?
    String simStatus = "UNKNOWN";
    for (int i = 0; i < 10; i++) {
        simStatus = gsm.getSIMStatus();
        if (simStatus == "READY") break;
        Serial.print(F("  SIM bekleniyor: "));
        Serial.println(simStatus);
        delay(1000);
    }
    if (simStatus != "READY") {
        Serial.print(F("[FAIL] SIM kart hatasi: "));
        Serial.println(simStatus);
        while (1);
    }

    // Sinyal kalitesi
    int rssi = gsm.getSignalQuality();
    Serial.print(F("  RSSI: "));
    Serial.print(rssi);
    Serial.println(rssi == 99 ? F("  (sinyal yok!)") : F("  OK"));

    // Şebeke kaydı
    Serial.print(F("  Sebeke: "));
    bool reg = false;
    for (int i = 0; i < 30; i++) {
        if (gsm.isRegistered()) { reg = true; break; }
        delay(1000);
    }
    if (!reg) {
        Serial.println(F("[FAIL] Sebekeye kayit yok! SIM kart veya anten kontrol edin."));
        while (1);
    }

    Serial.print(F("  Operator: "));
    Serial.println(gsm.getOperator());
    Serial.println(F("[PASS] Modem hazir, SIM OK, sebeke kayitli"));

    delay(1000);

    // ----------------------------------------------------------
    // [2/6] SMS GÖNDER
    // ----------------------------------------------------------
    Serial.println(F("\n[2/6] SMS gonderme testi..."));
    Serial.print(F("  Hedef: "));
    Serial.println(F(TEST_PHONE_NUMBER));

    // PROGMEM'den mesaj yükle
    static const char PROGMEM smsText[] =
        "Merhaba! Arduino UNO + SIM800C test mesaji. 06_SmsTest OK.";
    char msgBuf[70];
    strncpy_P(msgBuf, smsText, sizeof(msgBuf) - 1);
    msgBuf[sizeof(msgBuf) - 1] = '\0';

    if (gsm.smsSend(TEST_PHONE_NUMBER, msgBuf)) {
        Serial.println(F("[PASS] SMS gonderildi!"));
    } else {
        Serial.println(F("[FAIL] SMS gonderilemedi (kredi/yetki kontrol edin)"));
    }

    delay(3000);

    // ----------------------------------------------------------
    // [3/6] TUM SMS LISTELE
    // ----------------------------------------------------------
    Serial.println(F("\n[3/6] SMS listeleme testi (AT+CMGL)..."));
    String list = gsm.smsList("ALL");
    if (list.length() > 5 && list.indexOf("+CMGL:") >= 0) {
        Serial.println(F("[PASS] Listede mesajlar mevcut:"));
        Serial.println(list);
    } else {
        Serial.println(F("  Kuyrukta mesaj yok (ya da hepsi silindi)"));
        Serial.println(list);
    }

    delay(1000);

    // ----------------------------------------------------------
    // [4/6] SMS OKU (index 1)
    // ----------------------------------------------------------
    Serial.println(F("\n[4/6] SMS okuma testi (AT+CMGR=1)..."));
    String readResult = gsm.smsRead(1);
    if (readResult.indexOf("+CMGR:") >= 0) {
        Serial.println(F("[PASS] Mesaj okundu:"));
        Serial.println(readResult);
    } else {
        Serial.println(F("[WARN] Index 1'de mesaj bulunamadi"));
    }

    delay(1000);

    // ----------------------------------------------------------
    // [5/6] GELEN SMS BEKLE (+CMT: URC)
    // ----------------------------------------------------------
    Serial.println(F("\n[5/6] Gelen SMS bekleniyor (30 sn)..."));
    Serial.println(F("  Lutfen simdi telefona bir SMS gonderin!"));

    gsm.smsSetIncoming(true);   // +CMT: URC etkinlestir

    String incoming = gsm.smsWaitIncoming(30000);

    if (incoming.length() > 0) {
        Serial.println(F("[PASS] Gelen SMS yakalanc:"));
        Serial.println(incoming);
    } else {
        Serial.println(F("[WARN] 30 sn icinde SMS gelmedi (test atlandi)"));
    }

    gsm.smsSetIncoming(false);  // URC kapat

    delay(1000);

    // ----------------------------------------------------------
    // [6/6] TUM SMS SIL
    // ----------------------------------------------------------
    Serial.println(F("\n[6/6] Tum SMS silme testi (AT+CMGD=1,4)..."));
    if (gsm.smsDeleteAll()) {
        Serial.println(F("[PASS] Tum SMS'ler silindi!"));
    } else {
        Serial.println(F("[FAIL] SMS silme hatasi"));
    }

    // ----------------------------------------------------------
    // OZET
    // ----------------------------------------------------------
    Serial.println(F("\n=============================="));
    Serial.println(F("  SMS testleri tamamlandi!"));
    Serial.println(F("=============================="));
    Serial.println(F("  1. SMS Gonderme  - AT+CMGS"));
    Serial.println(F("  2. SMS Listeleme - AT+CMGL"));
    Serial.println(F("  3. SMS Okuma     - AT+CMGR"));
    Serial.println(F("  4. SMS Alma      - AT+CNMI (+CMT:)"));
    Serial.println(F("  5. SMS Silme     - AT+CMGD"));
}

void loop() {
    // Tek seferlik test tamamlandı
}
