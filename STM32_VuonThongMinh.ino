#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Khai báo LCD (địa chỉ I2C thường là 0x27 hoặc 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Khai báo chân điều khiển
#define LED_PIN     PA0
#define MOTOR1_PIN  PA1
#define MOTOR2_PIN  PA2

// Biến lưu trữ dữ liệu
String timeNow = "00:00:00";
String dateNow = "00/00/0000";
String tempStr = "0";
String soilStr = "0";
String tempSetting = "--";
String humSetting = "--";
String timeSetting = "--";

bool autoMode = false;
unsigned long lastDisplayChange = 0;
int displayState = 0;

void setup() {
  // Cấu hình chân OUTPUT
  pinMode(LED_PIN, OUTPUT);
  pinMode(MOTOR1_PIN, OUTPUT);
  pinMode(MOTOR2_PIN, OUTPUT);
  
  // Khởi tạo Serial (USART1) - PA9(TX), PA10(RX)
  Serial.begin(115200);
  
  // Khởi tạo LCD
  lcd.init();
  lcd.backlight();
  lcd.print("Dang ket noi...");
  delay(1000);
  lcd.clear();
}

void loop() {
  // Xử lý dữ liệu nhận được từ ESP32
  if (Serial.available()) {
    String receivedData = Serial.readStringUntil('\n');
    receivedData.trim(); // Loại bỏ ký tự thừa
    
    if (receivedData.startsWith("TG thuc:")) {
      timeNow = receivedData.substring(8);
    } 
    else if (receivedData.startsWith("Ngay thuc:")) {
      dateNow = receivedData.substring(10);
    } 
    else if (receivedData.startsWith("CB T:")) {
      tempStr = receivedData.substring(5);
    } 
    else if (receivedData.startsWith("CB D:")) {
      soilStr = receivedData.substring(5);
    }
    else {
      processControlData(receivedData);
    }
  }

  // Hiển thị luân phiên trên LCD mỗi 2 giây
  if (millis() - lastDisplayChange > 2000) {
    lastDisplayChange = millis();
    displayState = (displayState + 1) % 4;
    updateDisplay();
  }
}

void processControlData(String data) {
  // Xử lý chế độ (Auto/Manual)
  if (data.startsWith("[+] Che do: ")) {
    autoMode = (data.substring(12) == "auto");
  }
  
  // Xử lý điều khiển thiết bị
  if (data.startsWith("[AUTO] ") || data.startsWith("[MANUAL] ")) {
    bool state = data.endsWith("BAT");
    String device = data.substring(data.indexOf("]") + 2, data.indexOf(":"));
    
    if (device == "Den") {
      digitalWrite(LED_PIN, state);
    } 
    else if (device == "Dong co") {
      digitalWrite(MOTOR1_PIN, state);
    } 
    else if (device == "Dong co 2") {
      digitalWrite(MOTOR2_PIN, state);
    }
  }
  
  // Xử lý cài đặt từ Firebase
  if (data.startsWith("Nhiet do cai dat:")) {
    tempSetting = data.substring(17); // Lấy phần sau dấu :
    tempSetting.trim(); // Loại bỏ khoảng trắng thừa
  } 
  else if (data.startsWith("Do am dat cai dat:")) {
    humSetting = data.substring(18);
    humSetting.trim();
  }
  else if (data.startsWith("Thoi gian kich hoat:")) {
    timeSetting = data.substring(20);
    timeSetting.trim();
  }
}

void updateDisplay() {
  lcd.clear();
  switch (displayState) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Time: " + timeNow);
      lcd.setCursor(0, 1);
      lcd.print("Date: " + dateNow);
      break;
    case 1:
      lcd.setCursor(0, 0);
      lcd.print("Nhiet do: " + tempStr + "C");
      lcd.setCursor(0, 1);
      lcd.print("Do am: " + soilStr + "%");
      break;
    case 2:
      lcd.setCursor(0, 0);
      lcd.print("Cai Dat Tren App");
      lcd.setCursor(0, 1);
      lcd.print("Time:" + timeSetting);
      break;
    case 3:
      lcd.setCursor(0, 0);
      lcd.print("T cai dat: " + tempSetting);
      lcd.setCursor(0, 1);
      lcd.print("DA cai dat:" + humSetting);
      break;
  }
}