# Arduino UNO + SIM800C GSM Shield Kütüphanesi

[![Arduino](https://img.shields.io/badge/Arduino-UNO-blue?logo=arduino)](https://www.arduino.cc/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![GSM](https://img.shields.io/badge/Module-SIM800C-green)](https://www.simcom.com/)

Arduino UNO üzerinde SIM800C GSM modülü ile **Serial, HTTP, TCP, UDP ve MQTT** protokollerini test etmek için hazır kütüphane ve örnek sketch'ler.

## 📋 Özellikler

- ✅ AT komut yönetimi ve otomatik yanıt ayrıştırma
- ✅ Modem bilgileri (IMEI, IMSI, CSQ, operatör)
- ✅ GPRS bağlantı yönetimi
- ✅ HTTP GET / POST istekleri
- ✅ TCP soket bağlantı
- ✅ UDP veri gönderim/alım
- ✅ MQTT publish / subscribe (raw TCP üzerinden)
- ✅ Debug modu (Serial Monitor)

## 🔌 Pin Bağlantıları

| Arduino UNO | SIM800C | Açıklama |
|:-----------:|:-------:|:--------:|
| Pin 2       | TX      | Modem TX → Arduino RX |
| Pin 3       | RX      | Arduino TX → Modem RX |
| Pin 9       | Boot    | Power Key / Boot |
| 5V          | VCC     | Güç (2A adaptör önerilir) |
| GND         | GND     | Toprak |

> ⚠️ **Önemli**: SIM800C modülü yoğun akım çeker (2A pik). Arduino USB gücü yeterli olmayabilir. Harici 5V/2A güç kaynağı kullanın.

## 📦 Kurulum

### Arduino IDE ile:
1. Bu repository'yi ZIP olarak indirin
2. Arduino IDE → **Sketch** → **Include Library** → **Add .ZIP Library...**
3. İndirdiğiniz ZIP dosyasını seçin

### Manuel:
```bash
cd ~/Arduino/libraries/
git clone https://github.com/hiberblsm/arduino-uno-gsm-shield.git
```

## 🌐 Test Sunucusu

Tüm sketch'ler **HiberSoft Test Sunucusu** ile çalışır:

| Protokol | Sunucu | Port |
|:--------:|:------:|:----:|
| HTTP     | `test.hibersoft.com.tr` | `2884` |
| TCP      | `test.hibersoft.com.tr` | `2885` |
| UDP      | `test.hibersoft.com.tr` | `2886` |
| MQTT     | `test.hibersoft.com.tr` | `2887` |

### Token Alma
HTTP, TCP ve UDP testleri için önce token almak gerekir:
```bash
curl -s -X POST http://test.hibersoft.com.tr:2884/token \
  -H "x-api-key: PUBLIC_TEST_2026_ESP_ARDUINO"
```

### MQTT Kimlik Bilgileri
- Username: `testuser`
- Password: `PUBLIC_MQTT_2026_PASS`
- Topic prefix: `test/` veya `devices/`

## 🚀 Örnek Sketch'ler

### 01 - Serial Test
Modem temel fonksiyonlarını test eder: AT yanıtı, IMEI, IMSI, sinyal gücü, SIM durumu, operatör. Ayrıca interaktif AT komut terminali.

```cpp
#include <SIM800C.h>
SIM800C gsm(2, 3, 9);

void setup() {
    Serial.begin(9600);
    gsm.begin();
    
    Serial.println("IMEI: " + gsm.getIMEI());
    Serial.println("CSQ: " + String(gsm.getSignalQuality()));
    Serial.println("Operator: " + gsm.getOperator());
}
```

### 02 - HTTP Test
Token alma → POST /ingest ile veri gönderme → GET /messages ile mesajları görüntüleme.

### 03 - TCP Test
Token alma → TCP soket bağlantı → JSON veri gönderme (`\n` ile biten satırlar).

### 04 - UDP Test
Token alma → UDP soket → 3 farklı JSON paket gönderimi (sensör, durum, konum).

### 05 - MQTT Test
Broker'a bağlanma → Subscribe → Periyodik publish (30sn) → Gelen komut okuma → Auto-reconnect.

## ⚙️ APN Ayarları

Her sketch'in başında APN ayarları bulunur:

```cpp
const char* APN  = "internet";       // Turkcell
// const char* APN = "internet";     // Vodafone
// const char* APN = "tt";           // Turk Telekom
```

## 📊 Sinyal Gücü Tablosu

| CSQ | RSSI (dBm) | Kalite |
|:---:|:----------:|:------:|
| 0   | -113       | Yok    |
| 1-9 | -111...-95 | Zayıf  |
| 10-14 | -93...-85 | Orta   |
| 15-19 | -83...-75 | İyi    |
| 20-31 | -73...-51 | Mükemmel |
| 99  | —          | Bilinmiyor |

## 🏗️ Proje Yapısı

```
arduino-uno-gsm-shield/
├── src/
│   ├── SIM800C.h          # Kütüphane header
│   └── SIM800C.cpp        # Kütüphane implementasyon
├── examples/
│   ├── 01_SerialTest/     # AT komut testi
│   ├── 02_HTTPTest/       # HTTP GET/POST testi
│   ├── 03_TCPTest/        # TCP soket testi
│   ├── 04_UDPTest/        # UDP testi
│   └── 05_MQTTTest/       # MQTT publish/subscribe
├── library.properties     # Library Manager metadata
├── keywords.txt           # Syntax highlighting
├── README.md
└── LICENSE
```

## 📄 Lisans

MIT License — bkz. [LICENSE](LICENSE)

## 👨‍💻 Geliştirici

**HiberSoft** — [github.com/hiberblsm](https://github.com/hiberblsm)
