#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// 1. Konfigurasi Wi-Fi dan MQTT
const char* ssid          = "Crescent";      // Pastikan Hotspot aktif dan mode 2.4 GHz
const char* password      = "dkbso2829";     // Pastikan huruf besar dan kecil sesuai

const char* mqtt_server   = "broker.hivemq.com";
const int   mqtt_port     = 1883;

// Topik MQTT sesuai dengan index.html
const char* topic_data    = "smartplant/tubes2026/data";
const char* topic_control = "smartplant/tubes2026/control";

// 2. Pemetaan Pin Hardware
#define PIN_SOIL A0   // Pin A0 Soil Moisture Sensor ke A0 NodeMCU
#define PIN_LDR  D5   // Pin DO Modul Sensor LDR ke D5 NodeMCU
#define PIN_PUMP D1   // Pin IN1 Modul Relay Pompa ke D1 NodeMCU
#define PIN_LAMP D2   // Pin IN2 Modul Relay Lampu ke D2 NodeMCU

// Modul Relay 2-Channel bersifat Active LOW (LOW menyala, HIGH mati)
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// Nilai Kalibrasi Soil Moisture Sensor
#define SOIL_DRY  790 // Nilai analog saat sensor kering
#define SOIL_WET  300 // Nilai analog saat sensor basah

// Penyiraman Mode Otomatis: pompa menyala sebentar, lalu menunggu air meresap
#define PUMP_PULSE_MS 500    // Lama pompa menyala tiap penyiraman (0,5 detik)
#define PUMP_REST_MS  10000  // Jeda sebelum boleh menyiram lagi (10 detik)

// 3. Variabel Sistem
int soilThreshold = 30;       // Batas kelembapan tanah di bawah 30 persen
int soilMoisturePercent = 0;
int soilRaw = 0;
bool isDark = false;          // Status sensor cahaya LDR

bool autoMode = true;         // Status mode kerja sistem (Otomatis atau Manual)
bool pumpState = false;       // Status aktif Pompa Air
bool lampState = false;       // Status aktif Grow Light

bool pumpPulseActive = false;       // Sedang dalam penyiraman otomatis 0,5 detik
bool pumpHasRun = false;            // Sudah pernah menyiram sejak menyala
unsigned long pumpPulseStart = 0;   // Waktu pompa mulai menyala
unsigned long pumpPulseEnd = 0;     // Waktu pompa terakhir dimatikan

unsigned long lastMsgTime = 0;
unsigned long lastWiFiCheck = 0;
WiFiClient espClient;
PubSubClient client(espClient);

void publishData();

// 4. Baca Sensor dan Eksekusi Logika
void readSensors() {
  // Baca nilai kelembapan tanah berdasarkan kalibrasi kering 790 dan basah 300
  soilRaw = analogRead(PIN_SOIL);
  soilMoisturePercent = map(soilRaw, SOIL_DRY, SOIL_WET, 0, 100);
  soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);

  // Baca sensor LDR (HIGH berarti gelap atau kurang cahaya, LOW berarti terang)
  isDark = (digitalRead(PIN_LDR) == HIGH);

  // Tentukan status aktuator jika dalam Mode Otomatis
  if (autoMode) {
    lampState = isDark;

    if (!pumpPulseActive) {
      // Di luar penyiraman, pompa selalu mati pada Mode Otomatis
      pumpState = false;

      // Tanah kering dan jeda sudah lewat: mulai penyiraman 0,5 detik
      bool rested = !pumpHasRun || (millis() - pumpPulseEnd >= PUMP_REST_MS);
      if (soilMoisturePercent < soilThreshold && rested) {
        pumpState = true;
        pumpPulseActive = true;
        pumpPulseStart = millis();
      }
    }
  }
}

void updateRelays() {
  // Jeda kecil antar-relay untuk mencegah lonjakan arus bersamaan
  digitalWrite(PIN_PUMP, pumpState ? RELAY_ON : RELAY_OFF);
  delay(30);
  digitalWrite(PIN_LAMP, lampState ? RELAY_ON : RELAY_OFF);
}

// Matikan pompa tepat setelah 0,5 detik. Dipanggil sesering mungkin, termasuk
// saat Wi-Fi atau MQTT terputus, supaya pompa tidak pernah tertinggal menyala.
// Mengembalikan true bila pompa baru saja dimatikan.
bool servicePump() {
  if (pumpPulseActive && millis() - pumpPulseStart >= PUMP_PULSE_MS) {
    pumpPulseActive = false;
    pumpHasRun = true;
    pumpPulseEnd = millis();
    pumpState = false;
    digitalWrite(PIN_PUMP, RELAY_OFF);
    return true;
  }
  return false;
}

// 5. Kirim Data JSON ke Cloud
void publishData() {
  readSensors();

  String json = "{";
  json += "\"soil\":" + String(soilMoisturePercent) + ",";
  json += "\"dark\":" + String(isDark ? "true" : "false") + ",";
  json += "\"autoMode\":" + String(autoMode ? "true" : "false") + ",";
  json += "\"pump\":" + String(pumpState ? "true" : "false") + ",";
  json += "\"lamp\":" + String(lampState ? "true" : "false");
  json += "}";

  // Kirim data ke Cloud terlebih dahulu sebelum menggerakkan relay
  bool sent = client.publish(topic_data, json.c_str(), true);

  Serial.print(sent ? "[BERHASIL] " : "[GAGAL] ");
  Serial.print("Data Terkirim (Raw Soil: ");
  Serial.print(soilRaw);
  Serial.print(") -> ");
  Serial.println(json);

  // Eksekusi perubahan relay fisik setelah data terkirim
  updateRelays();
}

// 6. Terima Perintah dari Web
void callback(char* topic, byte* payload, unsigned int length) {
  String cmd = "";
  for (unsigned int i = 0; i < length; i++) {
    cmd += (char)payload[i];
  }
  cmd.trim();
  Serial.println("Perintah dari Web: " + cmd);

  if (cmd == "MODE_AUTO") {
    autoMode = true;
  } else if (cmd == "MODE_MANUAL") {
    autoMode = false;
    pumpPulseActive = false;  // Timer 0,5 detik hanya berlaku di Mode Otomatis
  } else if (!autoMode) {
    if (cmd == "PUMP_ON")  pumpState = true;
    if (cmd == "PUMP_OFF") pumpState = false;
    if (cmd == "LAMP_ON")  lampState = true;
    if (cmd == "LAMP_OFF") lampState = false;
  }

  publishData();
}

// 7. Koneksi Ulang Otomatis ke MQTT
void reconnectMQTT() {
  while (!client.connected() && WiFi.status() == WL_CONNECTED) {
    servicePump();
    Serial.print("Menghubungkan ke HiveMQ Cloud...");
    String clientId = "NodeMCU_SmartPlant_" + String(ESP.getChipId(), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println(" Berhasil Terhubung!");
      digitalWrite(LED_BUILTIN, LOW); // LED Biru NodeMCU menyala tanda Online
      client.subscribe(topic_control);
      publishData();
    } else {
      Serial.print(" Gagal (rc: ");
      Serial.print(client.state());
      Serial.println("), mencoba lagi dalam 3 detik...");
      // Tunggu 3 detik sambil tetap menjaga timer pompa
      for (int i = 0; i < 60; i++) {
        servicePump();
        delay(50);
      }
    }
  }
}

// 8. Setup dan Loop Utama
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_LAMP, OUTPUT);

  // Matikan kedua relay saat awal menyala
  digitalWrite(PIN_PUMP, RELAY_OFF);
  digitalWrite(PIN_LAMP, RELAY_OFF);

  Serial.println("\nMemulai Sistem Smart Plant");
  Serial.print("Menghubungkan ke Wi-Fi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(ssid, password);

  // LED Biru berkedip cepat selama mencari Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(150);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(150);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi Terhubung!");
  Serial.print("IP Lokal ESP8266: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(256);
}

void loop() {
  unsigned long now = millis();

  // Matikan pompa bila 0,5 detik sudah lewat, apa pun kondisi jaringannya
  bool pumpJustStopped = servicePump();

  // Cek koneksi Wi-Fi setiap 5 detik apabila terputus
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWiFiCheck >= 5000) {
      lastWiFiCheck = now;
      Serial.println("Koneksi Wi-Fi terputus! Menyambung ulang...");
      WiFi.reconnect();
    }
    return;
  }

  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  // Kabari web segera setelah pompa mati, tanpa menunggu jadwal 2 detik
  if (pumpJustStopped) {
    publishData();
  }

  // Kirim data sensor terbaru setiap 2 detik (2000 ms)
  if (now - lastMsgTime >= 2000) {
    lastMsgTime = now;
    publishData();
  }
}
