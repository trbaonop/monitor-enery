#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <HLW8032.h>

/* ================= WIFI ================= */
const char* ssid = "VJU Office";
const char* password = "VJuOffice@2023";

/* ================= GOOGLE SCRIPT ================= */
String google_url =
"https://script.google.com/macros/s/AKfycbwkmgvFBo__LTcMGpYcvYlYQdKMeFotokXGD9-RnOkYZPWrsGtZo6lMu_vowQrOLZRR/exec";

/* ================= HLW8032 (ESP32-C3) =================
   HLW8032 TX  -> qua opto -> ESP32-C3 IO4  (data vào, RX của UART1)
   HLW8032 RX  -> qua opto -> ESP32-C3 IO6  (chân điều khiển/SELECT của HLW8032)
   HLW8032 PF  -> qua opto -> ESP32-C3 "PF" (KHÔNG cần dùng trong code, thư viện
                                              tự lấy PF từ khung UART)

   Trên ESP32-C3: Flash SPI nội bộ dùng GPIO12-17, strapping pin là GPIO2/8/9.
   => GPIO4 và GPIO6 là GPIO bình thường, dùng được, không xung đột gì.
*/
#define HLW_RXD     4   // IO4
#define HLW_SEL_PIN 6   // IO6

HardwareSerial HLWSerial(1);   // dùng UART1 của ESP32 (chỉ cần chân RX)
HLW8032 HLW;

/* ================= RELAY (ESP32-C3) =================
   Relay nối qua opto tới IO0.
   Trên ESP32-C3, GPIO0 là GPIO bình thường (không phải strapping/boot pin
   như ESP32 thường) -> dùng cho relay hoàn toàn ổn, không xung đột boot.
*/
#define RELAY_PIN 0

/* ================= WEBSERVER ================= */
WebServer server(80);

/* ================= DATA ================= */
float voltage = 0;
float current = 0;
float power = 0;
float pf = 0;
const float frequency = 50.0;  // HLW8032 không trả tần số, lưới VN mặc định 50Hz

/* ================= FLAGS ================= */
bool isMeasuring = true;

/* ================= TIMER ================= */
unsigned long lastSend = 0;
const unsigned long sendInterval = 15000;

/* ================================================= */
void setup() {

  Serial.begin(115200);

  /* RELAY */
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);   // Relay OFF lúc khởi động (ACTIVE LOW RELAY)

  /* HLW8032 - chỉ cần chân nhận (RX), không cần chân phát (TX = -1) */
  HLWSerial.begin(4800, SERIAL_8E1, HLW_RXD, -1);
  HLW.begin(HLWSerial, HLW_SEL_PIN);

  Serial.println("=================================");
  Serial.println("ESP32 SYSTEM START");
  Serial.println("=================================");

  /* ================= WIFI ================= */
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  /* ================= RELAY CONTROL ================= */
  server.on("/relay/on", []() {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("==========");
    Serial.println("Relay ON");
    Serial.println("==========");
    server.send(200, "text/plain", "Relay ON");
  });

  server.on("/relay/off", []() {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("==========");
    Serial.println("Relay OFF");
    Serial.println("==========");
    server.send(200, "text/plain", "Relay OFF");
  });

  /* ================= MEASURE CONTROL ================= */
  server.on("/measure/on", []() {
    isMeasuring = true;
    Serial.println("Measuring ON");
    server.send(200, "text/plain", "Measuring ON");
  });

  server.on("/measure/off", []() {
    isMeasuring = false;
    Serial.println("Measuring OFF");
    server.send(200, "text/plain", "Measuring OFF");
  });

  /* ================= HOME PAGE ================= */
  server.on("/", []() {

    String html = "";

    html += "<html>";
    html += "<head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>ESP32 HLW8032</title>";
    html += "</head>";

    html += "<body style='font-family:Arial;text-align:center;'>";

    html += "<h1>ESP32 SMART CONTROL</h1>";

    html += "<h2>Relay Control</h2>";

    html += "<p>";
    html += "<a href='/relay/on'>";
    html += "<button style='width:220px;height:60px;font-size:22px;background:green;color:white;'>";
    html += "RELAY ON";
    html += "</button>";
    html += "</a>";
    html += "</p>";

    html += "<p>";
    html += "<a href='/relay/off'>";
    html += "<button style='width:220px;height:60px;font-size:22px;background:red;color:white;'>";
    html += "RELAY OFF";
    html += "</button>";
    html += "</a>";
    html += "</p>";

    html += "<hr>";

    html += "<h2>Measure Control</h2>";

    html += "<p>";
    html += "<a href='/measure/on'>";
    html += "<button style='width:220px;height:60px;font-size:22px;'>";
    html += "MEASURE ON";
    html += "</button>";
    html += "</a>";
    html += "</p>";

    html += "<p>";
    html += "<a href='/measure/off'>";
    html += "<button style='width:220px;height:60px;font-size:22px;'>";
    html += "MEASURE OFF";
    html += "</button>";
    html += "</a>";
    html += "</p>";

    html += "<hr>";

    html += "<h2>HLW8032 DATA</h2>";

    html += "<p>Voltage: " + String(voltage, 2) + " V</p>";
    html += "<p>Current: " + String(current, 2) + " A</p>";
    html += "<p>Power: " + String(power, 2) + " W</p>";
    html += "<p>Frequency: " + String(frequency, 1) + " Hz</p>";
    html += "<p>PF: " + String(pf, 2) + "</p>";

    html += "</body>";
    html += "</html>";

    server.send(200, "text/html", html);
  });

  /* ================= START SERVER ================= */
  server.begin();

  Serial.println("=================================");
  Serial.println("WebServer Started!");
  Serial.println("=================================");
}

/* ================================================= */
void send_to_google() {

  String url = google_url +
"?voltage="   + String(voltage, 2) +
               "&current="   + String(current, 2) +
               "&power="     + String(power, 2) +
               "&frequency=" + String(frequency, 1) +
               "&pf="        + String(pf, 2);

  Serial.println("Sending To Google...");
  Serial.println(url);

  if (WiFi.status() == WL_CONNECTED) {

    WiFiClientSecure client;

    client.setInsecure();

    HTTPClient http;

    http.begin(client, url);

    // FIX REDIRECT 302
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int httpCode = http.GET();

    if (httpCode > 0) {

      Serial.print("HTTP Response Code: ");
      Serial.println(httpCode);

      String payload = http.getString();

      Serial.println(payload);

    } else {

      Serial.print("HTTP Error: ");
      Serial.println(http.errorToString(httpCode));
    }

    http.end();

  } else {

    Serial.println("WiFi Disconnected");
  }
}

/* ================================================= */
void loop() {

  // LUÔN xử lý request app/web
  server.handleClient();

  // PHẢI gọi liên tục để bắt đúng khung dữ liệu UART từ HLW8032
  if (isMeasuring) {
    HLW.SerialReadLoop();
  }

  // Gửi dữ liệu mỗi 15 giây
  if (millis() - lastSend >= sendInterval) {

    lastSend = millis();

    if (isMeasuring) {

      voltage = HLW.GetVol();
      current = HLW.GetCurrent();
      power   = HLW.GetActivePower();
      pf      = HLW.GetPowerFactor();

      if (isnan(voltage) || isinf(voltage)) {

        Serial.println("HLW8032 NOT READY / NO DATA");

      } else {

        Serial.println("========== HLW8032 DATA ==========");
        Serial.printf("Voltage : %.2f V\n", voltage);
        Serial.printf("Current : %.2f A\n", current);
        Serial.printf("Power   : %.2f W\n", power);
        Serial.printf("PF      : %.2f\n", pf);
        Serial.println("===================================");

        send_to_google();
      }
    }
  }
}
