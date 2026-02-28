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

## 🚀 Örnek Sketch'ler

### 01 - Serial Test
Modem temel fonksiyonlarını test eder: AT yanıtı, IMEI, IMSI, sinyal gücü, SIM durumu, operatör.

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
GPRS üzerinden HTTP GET ve POST istekleri.

### 03 - TCP Test
TCP soket bağlantı, veri gönderme/alma.

### 04 - UDP Test
UDP veri gönderim, NTP zaman sorgusu.

### 05 - MQTT Test
MQTT broker'a bağlanma, publish/subscribe, gelen mesaj okuma.

## ⚙️ Ayarlar

Her sketch'in başında APN ve test sunucusu ayarları bulunur:

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
