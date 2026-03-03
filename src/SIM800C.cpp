/*
 * SIM800C.cpp - Arduino UNO GSM Shield Kütüphanesi
 * SIM800C modülü ile temel iletişim fonksiyonları
 *
 * GitHub: https://github.com/hiberblsm/arduino-uno-gsm-shield
 */

#include "SIM800C.h"

// ==================== KURUCU & BAŞLATMA ====================

SIM800C::SIM800C(uint8_t txPin, uint8_t rxPin, uint8_t bootPin)
    : _bootPin(bootPin), _debugEnabled(true),
      _mqttConnected(false), _mqttPort(1883)
{
    _serial = new SoftwareSerial(txPin, rxPin);
}

bool SIM800C::begin(long baud) {
    _serial->begin(baud);
    pinMode(_bootPin, OUTPUT);
    digitalWrite(_bootPin, HIGH);

    delay(1000);
    debugPrint(F("SIM800C baslatiliyor..."));

    // Modem cevap veriyor mu?
    for (int i = 0; i < 5; i++) {
        if (sendATExpect("AT", "OK", 2000)) {
            debugPrint(F("Modem hazir!"));
            // Echo kapat
            sendAT("ATE0");
            // Metin modu SMS
            sendAT("AT+CMGF=1");
            return true;
        }
        delay(1000);
    }
    debugPrint(F("HATA: Modem cevap vermiyor!"));
    return false;
}

void SIM800C::powerOn() {
    digitalWrite(_bootPin, LOW);
    delay(1500);
    digitalWrite(_bootPin, HIGH);
    delay(3000);
    debugPrint(F("Power ON"));
}

void SIM800C::powerOff() {
    digitalWrite(_bootPin, LOW);
    delay(1500);
    digitalWrite(_bootPin, HIGH);
    debugPrint(F("Power OFF"));
}

void SIM800C::hardReset() {
    debugPrint(F("Hard reset..."));
    powerOff();
    delay(2000);
    powerOn();
    delay(5000);
    begin();
}

// ==================== TEMEL AT KOMUTLARI ====================

String SIM800C::sendAT(const String &cmd, uint32_t timeoutMs) {
    clearBuffer();
    _serial->println(cmd);

    String response = "";
    unsigned long start = millis();

    while (millis() - start < timeoutMs) {
        while (_serial->available()) {
            char c = _serial->read();
            response += c;
        }
        // OK veya ERROR geldi mi?
        if (response.indexOf("OK") >= 0 || response.indexOf("ERROR") >= 0) {
            break;
        }
    }

    if (_debugEnabled) {
        Serial.print(F("[TX] "));
        Serial.println(cmd);
        Serial.print(F("[RX] "));
        Serial.println(response);
    }

    return response;
}

bool SIM800C::sendATExpect(const String &cmd, const String &expected, uint32_t timeoutMs) {
    String resp = sendAT(cmd, timeoutMs);
    return resp.indexOf(expected) >= 0;
}

bool SIM800C::waitForResponse(const String &expected, uint32_t timeoutMs) {
    String response = "";
    unsigned long start = millis();

    while (millis() - start < timeoutMs) {
        while (_serial->available()) {
            char c = _serial->read();
            response += c;
        }
        if (response.indexOf(expected) >= 0) {
            return true;
        }
    }
    return false;
}

void SIM800C::clearBuffer() {
    while (_serial->available()) {
        _serial->read();
    }
}

// Heap kullanmadan +HTTPACTION: URC'yi bekle ve HTTP kodu doandur
int SIM800C::waitHttpActionCode(uint32_t timeoutMs) {
    char buf[64] = {0};
    uint8_t pos = 0;
    unsigned long start = millis();

    while (millis() - start < timeoutMs) {
        while (_serial->available()) {
            char c = _serial->read();
            // Sliding window: tam oldugunda bas1 kapat
            if (pos >= 63) {
                memmove(buf, buf + 1, 62);
                pos = 62;
            }
            buf[pos++] = c;
            buf[pos]   = '\0';

            if (c == '\n' && pos > 15) {
                char *ha = strstr(buf, "+HTTPACTION:");
                if (ha) {
                    char *c1 = strchr(ha, ',');
                    if (c1) {
                        char *c2 = strchr(c1 + 1, ',');
                        if (c2) {
                            if (_debugEnabled) {
                                Serial.print(F("[HTTP] code="));
                                Serial.println(atoi(c1 + 1));
                            }
                            return atoi(c1 + 1);
                        }
                    }
                }
                // Yeni satir: window'u "+HTTPACTION:" oncesine temizle
                if (!strstr(buf, "+HTTPACT")) { pos = 0; buf[0] = '\0'; }
            }
        }
    }
    return 0;  // timeout
}

// Trigger stringi gelene kadar (veya timeout dolana kadar) seri portu oku
String SIM800C::waitForData(const String &trigger, uint32_t timeoutMs) {
    String response = "";
    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        while (_serial->available()) {
            char c = _serial->read();
            response += c;
        }
        if (response.indexOf(trigger) >= 0) break;
    }
    if (_debugEnabled) {
        Serial.print(F("[WAIT] "));
        Serial.println(response);
    }
    return response;
}

// ==================== MODEM BİLGİLERİ ====================

String SIM800C::getIMEI() {
    String resp = sendAT("AT+GSN");
    // Yanıt: \r\n864...\r\n\r\nOK
    int start = resp.indexOf('\n');
    int end = resp.indexOf('\r', start + 1);
    if (start >= 0 && end > start) {
        return resp.substring(start + 1, end);
    }
    return "";
}

String SIM800C::getIMSI() {
    String resp = sendAT("AT+CIMI");
    int start = resp.indexOf('\n');
    int end = resp.indexOf('\r', start + 1);
    if (start >= 0 && end > start) {
        return resp.substring(start + 1, end);
    }
    return "";
}

String SIM800C::getOperator() {
    String resp = sendAT("AT+COPS?");
    // +COPS: 0,0,"TURKCELL"
    int start = resp.indexOf('"');
    int end = resp.indexOf('"', start + 1);
    if (start >= 0 && end > start) {
        return resp.substring(start + 1, end);
    }
    return "";
}

int SIM800C::getSignalQuality() {
    String resp = sendAT("AT+CSQ");
    // +CSQ: 18,0
    int idx = resp.indexOf("+CSQ:");
    if (idx >= 0) {
        int commaIdx = resp.indexOf(',', idx);
        String csqStr = resp.substring(idx + 6, commaIdx);
        csqStr.trim();
        return csqStr.toInt();
    }
    return 99; // bilinmiyor
}

String SIM800C::getSIMStatus() {
    String resp = sendAT("AT+CPIN?");
    if (resp.indexOf("READY") >= 0) return "READY";
    if (resp.indexOf("SIM PIN") >= 0) return "PIN_REQUIRED";
    if (resp.indexOf("SIM PUK") >= 0) return "PUK_REQUIRED";
    if (resp.indexOf("NOT INSERTED") >= 0) return "NOT_INSERTED";
    return "UNKNOWN";
}

bool SIM800C::isRegistered() {
    String resp = sendAT("AT+CREG?");
    // +CREG: 0,1 (home) veya +CREG: 0,5 (roaming)
    return (resp.indexOf(",1") >= 0 || resp.indexOf(",5") >= 0);
}

// ==================== GPRS BAĞLANTI ====================

bool SIM800C::initGPRS(const String &apn, const String &user, const String &pass) {
    debugPrint(F("GPRS baslatiliyor..."));

    // Mevcut bağlantıyı kapat
    sendAT("AT+CIPSHUT", 5000);
    delay(1000);

    // Tek bağlantı modu
    if (!sendATExpect("AT+CIPMUX=0", "OK")) return false;

    // APN ayarla
    String cmd = "AT+CSTT=\"" + apn + "\"";
    if (user.length() > 0) {
        cmd += ",\"" + user + "\",\"" + pass + "\"";
    }
    if (!sendATExpect(cmd, "OK")) return false;

    // GPRS bağlantısını aç
    if (!sendATExpect("AT+CIICR", "OK", GPRS_TIMEOUT)) return false;

    delay(2000);

    // IP adresini al
    String ip = getLocalIP();
    if (ip.length() > 0 && ip != "0.0.0.0") {
        debugPrint("GPRS OK! IP: " + ip);
        return true;
    }

    debugPrint(F("HATA: GPRS baglanti basarisiz!"));
    return false;
}

bool SIM800C::closeGPRS() {
    return sendATExpect("AT+CIPSHUT", "SHUT OK", 5000);
}

bool SIM800C::isGPRSConnected() {
    String resp = sendAT("AT+CGATT?");
    return resp.indexOf("+CGATT: 1") >= 0;
}

String SIM800C::getLocalIP() {
    String resp = sendAT("AT+CIFSR", 3000);
    resp.trim();
    // Yanit birden fazla satir iceriyorsa son satirı al
    int lastNewline = resp.lastIndexOf('\n');
    String ip = (lastNewline >= 0) ? resp.substring(lastNewline + 1) : resp;
    ip.trim();
    if (ip.indexOf('.') > 0 && ip.indexOf("ERROR") < 0) {
        return ip;
    }
    return "";
}

// ==================== HTTP FONKSİYONLAR ====================

bool SIM800C::httpGET(const String &url, String &response, int &httpCode) {
    debugPrint("HTTP GET: " + url);

    // HTTP servisini başlat
    sendAT("AT+HTTPTERM", 2000);
    delay(500);

    if (!sendATExpect("AT+HTTPINIT", "OK")) return false;
    if (!sendATExpect("AT+HTTPPARA=\"CID\",1", "OK")) return false;
    if (!sendATExpect("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK")) return false;

    // GET isteği
    String resp = sendAT("AT+HTTPACTION=0", HTTP_TIMEOUT);
    delay(3000);

    // +HTTPACTION: 0,200,xxx yanıtını bekle
    resp += sendAT("", 5000);
    int idx = resp.indexOf("+HTTPACTION:");
    if (idx >= 0) {
        int firstComma = resp.indexOf(',', idx);
        int secondComma = resp.indexOf(',', firstComma + 1);
        httpCode = resp.substring(firstComma + 1, secondComma).toInt();
    } else {
        httpCode = 0;
    }

    // Yanıtı oku
    resp = sendAT("AT+HTTPREAD", HTTP_TIMEOUT);
    int dataStart = resp.indexOf('\n', resp.indexOf("+HTTPREAD:"));
    int dataEnd = resp.indexOf("\r\nOK");
    if (dataStart >= 0 && dataEnd > dataStart) {
        response = resp.substring(dataStart + 1, dataEnd);
        response.trim();
    }

    sendAT("AT+HTTPTERM");
    debugPrint("HTTP Code: " + String(httpCode));
    return httpCode == 200;
}

bool SIM800C::httpPOST(const String &url, const String &contentType,
                        const String &data, String &response, int &httpCode) {
    debugPrint("HTTP POST: " + url);

    sendAT("AT+HTTPTERM", 2000);
    delay(500);

    if (!sendATExpect("AT+HTTPINIT", "OK")) return false;
    if (!sendATExpect("AT+HTTPPARA=\"CID\",1", "OK")) return false;
    if (!sendATExpect("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK")) return false;
    if (!sendATExpect("AT+HTTPPARA=\"CONTENT\",\"" + contentType + "\"", "OK")) return false;

    // Veri uzunluğu & timeout (30sn)
    String dataCmd = "AT+HTTPDATA=" + String(data.length()) + ",30000";
    String resp = sendAT(dataCmd, 5000);
    if (resp.indexOf("DOWNLOAD") < 0) {
        sendAT("AT+HTTPTERM");
        return false;
    }

    // Veriyi gönder
    _serial->print(data);
    delay(1000);
    if (!waitForResponse("OK", 5000)) {
        sendAT("AT+HTTPTERM");
        return false;
    }

    // POST isteği
    resp = sendAT("AT+HTTPACTION=1", HTTP_TIMEOUT);
    delay(3000);

    resp += sendAT("", 5000);
    int idx = resp.indexOf("+HTTPACTION:");
    if (idx >= 0) {
        int firstComma = resp.indexOf(',', idx);
        int secondComma = resp.indexOf(',', firstComma + 1);
        httpCode = resp.substring(firstComma + 1, secondComma).toInt();
    } else {
        httpCode = 0;
    }

    // Yanıtı oku
    resp = sendAT("AT+HTTPREAD", HTTP_TIMEOUT);
    int dataStart = resp.indexOf('\n', resp.indexOf("+HTTPREAD:"));
    int dataEnd = resp.indexOf("\r\nOK");
    if (dataStart >= 0 && dataEnd > dataStart) {
        response = resp.substring(dataStart + 1, dataEnd);
        response.trim();
    }

    sendAT("AT+HTTPTERM");
    debugPrint("HTTP Code: " + String(httpCode));
    return httpCode >= 200 && httpCode < 300;
}

// ==================== TCP FONKSİYONLAR ====================

bool SIM800C::tcpConnect(const String &host, uint16_t port) {
    debugPrint("TCP baglanti: " + host + ":" + String(port));

    String cmd = "AT+CIPSTART=\"TCP\",\"" + host + "\"," + String(port);
    String resp = sendAT(cmd, GPRS_TIMEOUT);

    // CONNECT OK beklenir
    if (resp.indexOf("CONNECT OK") >= 0 || resp.indexOf("ALREADY CONNECT") >= 0) {
        debugPrint(F("TCP baglanildi!"));
        return true;
    }

    // Bazen gecikmeli gelir
    if (waitForResponse("CONNECT OK", 10000)) {
        debugPrint(F("TCP baglanildi!"));
        return true;
    }

    debugPrint(F("HATA: TCP baglanti basarisiz!"));
    return false;
}

bool SIM800C::tcpSend(const String &data) {
    String cmd = "AT+CIPSEND=" + String(data.length());
    String resp = sendAT(cmd, 5000);

    if (resp.indexOf('>') >= 0) {
        _serial->print(data);
        delay(100);
        if (waitForResponse("SEND OK", 10000)) {
            debugPrint(F("TCP veri gonderildi"));
            return true;
        }
    }
    debugPrint(F("HATA: TCP gonderim basarisiz!"));
    return false;
}

String SIM800C::tcpReceive(uint32_t timeoutMs) {
    String data = "";
    unsigned long start = millis();

    while (millis() - start < timeoutMs) {
        while (_serial->available()) {
            char c = _serial->read();
            data += c;
        }
        if (data.length() > 0 && !_serial->available()) {
            delay(100);
            if (!_serial->available()) break;
        }
    }
    return data;
}

bool SIM800C::tcpClose() {
    return sendATExpect("AT+CIPCLOSE", "CLOSE OK", 5000);
}

bool SIM800C::isTCPConnected() {
    String resp = sendAT("AT+CIPSTATUS");
    return resp.indexOf("CONNECT OK") >= 0;
}

// ==================== UDP FONKSİYONLAR ====================

bool SIM800C::udpStart(const String &host, uint16_t port) {
    debugPrint("UDP baslat: " + host + ":" + String(port));

    String cmd = "AT+CIPSTART=\"UDP\",\"" + host + "\"," + String(port);
    String resp = sendAT(cmd, GPRS_TIMEOUT);

    if (resp.indexOf("CONNECT OK") >= 0 || resp.indexOf("ALREADY CONNECT") >= 0) {
        debugPrint(F("UDP hazir!"));
        return true;
    }

    if (waitForResponse("CONNECT OK", 10000)) {
        debugPrint(F("UDP hazir!"));
        return true;
    }

    debugPrint(F("HATA: UDP baglanti basarisiz!"));
    return false;
}

bool SIM800C::udpSend(const String &data) {
    String cmd = "AT+CIPSEND=" + String(data.length());
    String resp = sendAT(cmd, 5000);

    if (resp.indexOf('>') >= 0) {
        _serial->print(data);
        delay(100);
        if (waitForResponse("SEND OK", 10000)) {
            debugPrint(F("UDP veri gonderildi"));
            return true;
        }
    }
    debugPrint(F("HATA: UDP gonderim basarisiz!"));
    return false;
}

String SIM800C::udpReceive(uint32_t timeoutMs) {
    return tcpReceive(timeoutMs); // aynı mantık
}

bool SIM800C::udpClose() {
    return sendATExpect("AT+CIPCLOSE", "CLOSE OK", 5000);
}

// ==================== MQTT FONKSİYONLAR ====================

bool SIM800C::mqttConnect(const String &broker, uint16_t port,
                           const String &clientId,
                           const String &user, const String &pass) {
    debugPrint("MQTT baglanti: " + broker + ":" + String(port));

    _mqttBroker = broker;
    _mqttPort = port;

    // Önce TCP bağlantısı
    if (!tcpConnect(broker, port)) {
        return false;
    }
    delay(1000);

    // MQTT CONNECT paketi oluştur
    // Değişken header: Protocol name + level + flags + keepalive
    uint8_t packet[128];
    size_t idx = 0;

    // Fixed header
    packet[idx++] = MQTT_CONNECT;  // CONNECT paketi

    // Remaining length placeholder
    size_t lenIdx = idx;
    idx++; // remaining length (sonra hesaplanacak)

    // Variable header
    // Protocol Name "MQTT"
    packet[idx++] = 0x00; packet[idx++] = 0x04;
    packet[idx++] = 'M'; packet[idx++] = 'Q';
    packet[idx++] = 'T'; packet[idx++] = 'T';

    // Protocol Level (4 = MQTT 3.1.1)
    packet[idx++] = 0x04;

    // Connect Flags
    uint8_t flags = 0x02; // Clean Session
    if (user.length() > 0) flags |= 0x80; // Username
    if (pass.length() > 0) flags |= 0x40; // Password
    packet[idx++] = flags;

    // Keep Alive (60 saniye)
    packet[idx++] = 0x00;
    packet[idx++] = 0x3C;

    // Payload: Client ID
    packet[idx++] = (clientId.length() >> 8) & 0xFF;
    packet[idx++] = clientId.length() & 0xFF;
    memcpy(&packet[idx], clientId.c_str(), clientId.length());
    idx += clientId.length();

    // Username
    if (user.length() > 0) {
        packet[idx++] = (user.length() >> 8) & 0xFF;
        packet[idx++] = user.length() & 0xFF;
        memcpy(&packet[idx], user.c_str(), user.length());
        idx += user.length();
    }

    // Password
    if (pass.length() > 0) {
        packet[idx++] = (pass.length() >> 8) & 0xFF;
        packet[idx++] = pass.length() & 0xFF;
        memcpy(&packet[idx], pass.c_str(), pass.length());
        idx += pass.length();
    }

    // Remaining length hesapla
    packet[lenIdx] = idx - 2;

    // Gönder (AT+CIPSEND kullan)
    String cmd = "AT+CIPSEND=" + String(idx);
    String resp = sendAT(cmd, 5000);
    if (resp.indexOf('>') >= 0) {
        for (size_t i = 0; i < idx; i++) {
            _serial->write(packet[i]);
        }
        delay(500);

        // CONNACK bekle
        if (waitForResponse("SEND OK", 5000)) {
            delay(1000);
            String connResp = tcpReceive(5000);
            // CONNACK paketinin ilk byte'ı 0x20 olmalı
            if (connResp.length() >= 2) {
                _mqttConnected = true;
                debugPrint(F("MQTT baglanildi!"));
                return true;
            }
        }
    }

    debugPrint(F("HATA: MQTT baglanti basarisiz!"));
    return false;
}

bool SIM800C::mqttPublish(const String &topic, const String &payload) {
    if (!_mqttConnected) return false;

    debugPrint("MQTT Publish: " + topic + " = " + payload);

    uint8_t packet[256];
    size_t idx = 0;

    // Fixed header
    packet[idx++] = MQTT_PUBLISH; // PUBLISH, QoS 0

    // Remaining Length (topic length field + topic + payload)
    uint16_t remainLen = 2 + topic.length() + payload.length();
    packet[idx++] = remainLen & 0x7F;

    // Topic
    packet[idx++] = (topic.length() >> 8) & 0xFF;
    packet[idx++] = topic.length() & 0xFF;
    memcpy(&packet[idx], topic.c_str(), topic.length());
    idx += topic.length();

    // Payload
    memcpy(&packet[idx], payload.c_str(), payload.length());
    idx += payload.length();

    // AT+CIPSEND ile gönder
    String cmd = "AT+CIPSEND=" + String(idx);
    String resp = sendAT(cmd, 5000);
    if (resp.indexOf('>') >= 0) {
        for (size_t i = 0; i < idx; i++) {
            _serial->write(packet[i]);
        }
        if (waitForResponse("SEND OK", 5000)) {
            debugPrint(F("MQTT publish OK"));
            return true;
        }
    }

    debugPrint(F("HATA: MQTT publish basarisiz!"));
    return false;
}

bool SIM800C::mqttSubscribe(const String &topic) {
    if (!_mqttConnected) return false;

    debugPrint("MQTT Subscribe: " + topic);

    uint8_t packet[128];
    size_t idx = 0;

    // Fixed header
    packet[idx++] = MQTT_SUBSCRIBE;

    // Remaining length
    uint16_t remainLen = 2 + 2 + topic.length() + 1; // packetId + topic len + topic + QoS
    packet[idx++] = remainLen & 0x7F;

    // Packet identifier
    packet[idx++] = 0x00;
    packet[idx++] = 0x01;

    // Topic
    packet[idx++] = (topic.length() >> 8) & 0xFF;
    packet[idx++] = topic.length() & 0xFF;
    memcpy(&packet[idx], topic.c_str(), topic.length());
    idx += topic.length();

    // Requested QoS
    packet[idx++] = 0x00; // QoS 0

    // Gönder
    String cmd = "AT+CIPSEND=" + String(idx);
    String resp = sendAT(cmd, 5000);
    if (resp.indexOf('>') >= 0) {
        for (size_t i = 0; i < idx; i++) {
            _serial->write(packet[i]);
        }
        if (waitForResponse("SEND OK", 5000)) {
            debugPrint(F("MQTT subscribe OK"));
            return true;
        }
    }

    debugPrint(F("HATA: MQTT subscribe basarisiz!"));
    return false;
}

String SIM800C::mqttLoop(uint32_t timeoutMs) {
    // Gelen MQTT mesajlarını oku
    String raw = tcpReceive(timeoutMs);
    if (raw.length() < 4) return "";

    // PUBLISH paketi var mı kontrol et (0x30)
    for (size_t i = 0; i < raw.length(); i++) {
        if ((raw[i] & 0xF0) == MQTT_PUBLISH) {
            // Topic ve payload ayrıştır
            size_t pos = i + 2; // remaining length atla
            if (pos + 2 > raw.length()) break;

            uint16_t topicLen = ((uint8_t)raw[pos] << 8) | (uint8_t)raw[pos + 1];
            pos += 2;

            if (pos + topicLen > raw.length()) break;
            String topic = raw.substring(pos, pos + topicLen);
            pos += topicLen;

            String payload = raw.substring(pos);
            return topic + ": " + payload;
        }
    }
    return "";
}

bool SIM800C::mqttPing() {
    if (!_mqttConnected) return false;

    uint8_t packet[2] = {MQTT_PINGREQ, 0x00};
    String cmd = "AT+CIPSEND=2";
    String resp = sendAT(cmd, 5000);
    if (resp.indexOf('>') >= 0) {
        _serial->write(packet, 2);
        return waitForResponse("SEND OK", 5000);
    }
    return false;
}

bool SIM800C::mqttDisconnect() {
    if (!_mqttConnected) return true;

    uint8_t packet[2] = {MQTT_DISCONNECT, 0x00};
    String cmd = "AT+CIPSEND=2";
    String resp = sendAT(cmd, 5000);
    if (resp.indexOf('>') >= 0) {
        _serial->write(packet, 2);
        waitForResponse("SEND OK", 3000);
    }
    _mqttConnected = false;
    tcpClose();
    debugPrint(F("MQTT baglanti kapatildi"));
    return true;
}

// ==================== YARDIMCI ====================

void SIM800C::debugPrint(const String &msg) {
    if (_debugEnabled) {
        Serial.print(F("[SIM800C] "));
        Serial.println(msg);
    }
}

void SIM800C::setDebug(bool enabled) {
    _debugEnabled = enabled;
}
