#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Deklarasi PIN yang digunakan
#define TRIG_PIN 19
#define ECHO_PIN 18
#define BUZZER_PIN 13
#define LED_PIN 2
#define PUMP_PIN 32

// Konfigurasi WiFi credentials
const char* ssid = "Iphone";
const char* password = "bayarduaribu";

// URL server backend
const char* serverUrl = "http://172.20.10.4:5000/api/esp32/data";

// Interval pengiriman datanya
const unsigned long SEND_INTERVAL = 2000; // 2 detik
unsigned long lastSendTime = 0;
unsigned long lastBuzzerTime = 0;

// Batas untuk ketinggian air
const float DANGER_LEVEL = 10.0;  // cm
const float WARNING_LEVEL = 15.0;
const float STOP_LEVEL = 25.0;
const float START_LEVEL = 8.0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

  connectToWifi();
}

void loop() {
  unsigned long currentMillis = millis();

  if (WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }

  float distance = measureDistance();
  if (distance > 0) {
    Serial.print("Jarak: ");
    Serial.print(distance);
    Serial.println(" cm");

    controlPump(distance);
    controlBuzzer(distance);

    if (currentMillis - lastSendTime >= SEND_INTERVAL) {
      lastSendTime = currentMillis;
      sendData(distance);
    }
  } else {
    Serial.println("Sensor tidak valid.");
  }

  delay(100); // Delay minimal untuk stabilitas
}

float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration == 0) return -1;
  
  float distance = duration * 0.0343 / 2;
  if (distance < 2 || distance > 50) return -1; // misal tinggi botol 50 cm
  return distance;
}

void connectToWifi() {
  Serial.println("Menghubungkan ke WiFi...");
  WiFi.begin(ssid, password);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt++ < 10) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi terhubung.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nGagal terhubung ke WiFi.");
  }
}

void sendData(float distance) {
  HTTPClient http;
  DynamicJsonDocument jsonDoc(128);
  jsonDoc["distance"] = distance;

  String payload;
  serializeJson(jsonDoc, payload);

  http.begin(serverUrl);
  http.setTimeout(3000); // timeout dikonfig lebih pendek
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(payload);
  if (code > 0) {
    Serial.print("Data terkirim (");
    Serial.print(code);
    Serial.println(")");
  } else {
    Serial.print("Gagal kirim data: ");
    Serial.println(code);
  }

  http.end();
}

void controlPump(float distance) {
  if (distance <= START_LEVEL) {
    Serial.println("Pompa ON");
    digitalWrite(PUMP_PIN, LOW); // Aktif (tergantung wiring)
  } else if (distance >= STOP_LEVEL) {
    Serial.println("Pompa OFF");
    digitalWrite(PUMP_PIN, HIGH); // Mati
  }
}

void controlBuzzer(float distance) {
  unsigned long now = millis();
  if (distance <= DANGER_LEVEL && now - lastBuzzerTime > 1000) {
    Serial.println("BAHAYA! Air hampir penuh!");
    tone(BUZZER_PIN, 3000, 100);
    lastBuzzerTime = now;
  } else if (distance <= WARNING_LEVEL && now - lastBuzzerTime > 2000) {
    Serial.println("PERINGATAN! Air tinggi.");
    tone(BUZZER_PIN, 2000, 100);
    lastBuzzerTime = now;
  }
}