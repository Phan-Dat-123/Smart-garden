#include <WiFiManager.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "time.h"
  // ================ Cấu hình thời gian  ================ //
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;     // GMT+7
const int daylightOffset_sec = 0;

String getTimeHHMMSS() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00:00:00";
  
  char buffer[10];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

String getDateDDMMYYYY() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00/00/0000";

  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y", &timeinfo);
  return String(buffer);
}
  // ================ Firebase Realtime Database URL  ================ //
const char* FIREBASE_URL = "https://n28hdd-default-rtdb.asia-southeast1.firebasedatabase.app/n28hdd.json";
#define API_KEY "AIzaSyDDckjEcF8HyPyKb-ZAWGZqz6S1bj-PpNE"  // ← Thay bằng API key của bạn

void sendDataToFirebase(float temperature,float humidity, float lux, float soilPercent,  float rainPercent) 
{
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Ghép API key vào URL
    String firebaseURL = String(FIREBASE_URL) + "?auth=" + API_KEY;
    http.begin(firebaseURL);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"temperature\": " + String(temperature, 2) + ",";
    payload += "\"humidity\": " + String(humidity, 2) + ",";
    payload += "\"lux\": " + String(lux, 2) + ",";
    payload += "\"soilPercent\": " + String(soilPercent, 2) + ",";
    payload += "\"rainPercent\": " + String(rainPercent, 2);
    payload += "}";

    int httpCode = http.PUT(payload);
    if (httpCode > 0) {
      Serial.println("[+] Gửi thành công: " + http.getString());
    } else {
      Serial.println("[-] Lỗi gửi Firebase: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("[-] Không có kết nối WiFi. Không thể gửi dữ liệu.");
  }
}

void handleControlFromFirebase(int lux, int soilPercent, int rainPercent) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Lấy mode
    String modeURL = "https://n28hdd-default-rtdb.asia-southeast1.firebasedatabase.app/mode.json?auth=" + String(API_KEY);
    http.begin(modeURL);
    int httpCode = http.GET();
    String mode = "";
    
    if (httpCode > 0) {
      mode = http.getString();
      mode.replace("\"", ""); // Xóa dấu ngoặc kép
      Serial2.println("[+] Chế độ: " + mode);
    } else {
      Serial.println("[-] Không đọc được mode từ Firebase");
      http.end();
      return;
    }
    http.end();

    // Lấy control (led, motor)
    String controlURL = "https://n28hdd-default-rtdb.asia-southeast1.firebasedatabase.app/control.json?auth=" + String(API_KEY);
    http.begin(controlURL);
    httpCode = http.GET();
    
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("[+] Nhận control: " + payload);

      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.println("[-] Lỗi parse control JSON: " + String(error.c_str()));
        http.end();
        return;
      }

      bool ledManual = doc["led"];
      bool motorManual = doc["motor"];
      bool motorManual2 = doc["motor2"];

      if (mode == "auto") {
        bool autoLed = (lux < 3);
        bool autoMotor = (soilPercent < 60);
        bool autoMotor2 = (rainPercent > 60);
        
        Serial2.println("[AUTO] Den: " + String(autoLed ? "BAT" : "TAT"));
        Serial2.println("[AUTO] Dong co: " + String(autoMotor ? "BAT" : "TAT"));
        Serial2.println("[AUTO] Dong co 2: " + String(autoMotor2 ? "BAT" : "TAT"));

      } else if (mode == "manual") {
        Serial2.println("[MANUAL] Den: " + String(ledManual ? "BAT" : "TAT"));
        Serial2.println("[MANUAL] Dong co: " + String(motorManual ? "BAT" : "TAT"));
        Serial2.println("[MANUAL] Dong co 2: " + String(motorManual2 ? "BAT" : "TAT"));
      }
    } else {
      Serial.println("[-] Lỗi đọc control từ Firebase: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("[-] Không có kết nối WiFi.");
  }
}

//
void handleSettingFromFirebase() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String settingsURL = "https://n28hdd-default-rtdb.asia-southeast1.firebasedatabase.app/settings.json?auth=" + String(API_KEY);
    http.begin(settingsURL);
    
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        // Đảm bảo gửi đúng định dạng STM32 cần
        String tempSet = doc["settingtemp"] | "NULL";
        String humSet = doc["settinghum"] | "NULL";
        String timeSet = doc["settingtime"] | "NULL";
        Serial.println("Nhiet do cai dat:" + tempSet);
        Serial.println("Do am dat cai dat:" + humSet); 
        Serial.println("Thoi gian kich hoat:" + timeSet);

        Serial2.println("Nhiet do cai dat:" + tempSet);
        Serial2.println("Do am dat cai dat:" + humSet); 
        Serial2.println("Thoi gian kich hoat:" + timeSet);
      }
    }
    http.end();
  }
}

#define DHTPIN 15         // DHT11 Data
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define LDR_PIN 34        // ADC input
#define SOIL_PIN 35
#define RAIN_PIN 32

void setup() {
  Serial.begin(115200);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);    // Khởi tạo đồng hồ thời gian thực từ NTP
  dht.begin();
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // RX=16, TX=17 (ESP32 nhận từ STM32)
  // --- Thử kết nối WiFi trước 5-7 giây ---
  WiFi.begin();
  int waitTime = 0;
  while (WiFi.status() != WL_CONNECTED && waitTime < 5000) {
    delay(500);
    Serial.print(".");
    waitTime += 500;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n-> Khong tim thay WiFi, mo AP");

    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    bool res = wm.autoConnect("ESP32-WIFI");

    if (!res) {
      Serial.println("Khong ket noi WiFi duoc");
      delay(3000);
      // ESP.restart();  // chỉ dùng nếu thật sự cần
    }
  }
}

void loop() {
  String timeNow = getTimeHHMMSS();     // VD: "14:05:23"
  String dateNow = getDateDDMMYYYY();   // VD: "18/04/2025"
  // Đọc DHT11
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Đọc ADC thô
  int ldrRaw = analogRead(LDR_PIN);
  int soilRaw = analogRead(SOIL_PIN);
  int rainRaw = analogRead(RAIN_PIN);

  // Chuyển đổi đơn vị
  // Độ sáng (lux ước lượng)
  float lux = 10000.0 / (ldrRaw + 1);  // +1 để tránh chia cho 0
  // Độ ẩm đất (%)
  int soilPercent = map(soilRaw, 4095, 0, 0, 100);  // 4095 là khô nhất
  // Lượng mưa (% ướt)
  int rainPercent = map(rainRaw, 4095, 0, 0, 100);  // 4095 là khô, 0 là mưa nhiều

  // In ra Serial
  if (isnan(temperature) || isnan(humidity))
  {
    Serial.println("DHT11 lỗi đọc dữ liệu!");
  } 
  else 
  {
    Serial.print("Nhiệt độ: ");
    Serial.print(temperature);
    Serial.print(" °C | Độ ẩm không khí: ");
    Serial.print(humidity);
    Serial.println(" %");
  }

  // Serial.print("Cường độ ánh sáng: ");
  // Serial.print(lux, 1);
  // Serial.println(" lux");

  // Serial.print("Độ ẩm đất: ");
  // Serial.print(soilPercent);
  // Serial.println(" %");

  // Serial.print("Lượng mưa: ");
  // Serial.print(rainPercent);
  // Serial.println(" %");

  // Serial.println("--------------------------");

  // ================ Gửi dữ liệu qua cho STM32 ================ //

  // String data = timeNow + "," + dateNow + "," +
  //               String(temperature, 1) + "," +
  //               String(humidity, 1) + "," +
  //               String(lux, 1) + "," +
  //               String(soilPercent) + "," +
  //               String(rainPercent);
  // // Gửi dữ liệu qua USART (Serial2)
  // Serial2.println(data);
  Serial2.println("TG thuc:" + timeNow);
  Serial2.println("Ngay thuc:" + dateNow);
  Serial2.println("CB T:" + String(temperature));
  Serial2.println("CB D:" + String(soilPercent));

  // Debug trên Serial Monitor của ESP32
  // Serial.print("Data sent: ");
  // Serial.println(data);
  
  // ================ Gửi dữ liệu lên Firebase ================ //
  sendDataToFirebase(temperature,humidity,lux,soilPercent,rainPercent);
  handleControlFromFirebase(lux, soilPercent, rainPercent);
  handleSettingFromFirebase();
  delay(1500);
}
