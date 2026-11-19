#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"

#include <ESP32Servo.h>

Servo servoPhongNgu;  // Khai báo đối tượng Servo cho phòng ngủ
Servo servoKeyPad;    // Khai báo đối tượng Servo cho keypad
const int servoPinPhongNgu = 13;  // Chân kết nối servo cho phòng ngủ
const int servoPinKeyPad = 12;    // Chân kết nối servo cho keypad
const int fanPhongNgu = 14;       // Chân kết nối quạt phòng ngủ
const int fanPhongKhach = 27;     // Chân kết nối quạt phòng khách
const int ledPhongNgu = 26;       // Chân kết nối LED phòng ngủ
const int ledPhongKhach = 15;       // Chân kết nối LED phòng khách
const int ledGarage = 33;       // Chân kết nối LED garage
const int airPhongNgu = 4; // Chân kết nối điều hòa phòng ngủ
const int airPhongKhach = 18; // Chân kết nối điều hòa phòng khách
const int buttonFanPhongNgu = 17; // Nút nhấn điều khiển quạt phòng ngủ
const int buttonFanPhongKhach = 16; // Nút nhấn điều khiển quạt phòng khách

const int openPosition = 90;     // Góc mở cửa (90 độ)
const int closePosition = 0;     // Góc đóng cửa (0 độ)
const int relayPin = 5;  // Chân điều khiển relay máy bơm

bool relayState = false;  // Trạng thái relay máy bơm
bool doorIsOpenPhongNgu = false;    // Trạng thái cửa từ phòng ngủ (mở hoặc đóng)
bool doorIsOpenKeyPad = false;      // Trạng thái cửa từ keypad (mở hoặc đóng)
bool inMotionPhongNgu = false;      // Trạng thái để kiểm tra xem cửa từ phòng ngủ đang chuyển động không
bool inMotionKeyPad = false;        // Trạng thái để kiểm tra xem cửa từ keypad đang chuyển động không
bool ledStatePhongNgu = false;      // Trạng thái của đèn LED phòng ngủ
bool ledStatePhongKhach = false;    // Trạng thái của đèn LED phòng khách
bool ledStateGarage = false;        // Trạng thái của đèn LED garage
bool fanStatePhongNgu = false;       // Trạng thái của quạt phòng ngủ
bool fanStatePhongKhach = false;     // Trạng thái của quạt phòng khách
bool lastButtonFanStatePhongNgu = HIGH; // Trạng thái trước đó của nút nhấn quạt phòng ngủ
bool lastButtonFanStatePhongKhach = HIGH; // Trạng thái trước đó của nút nhấn quạt phòng khách
bool alertSent = false;
boolean FLAME_SENSOR_STATE_NEW     = LOW;  // current state
boolean FLAME_SENSOR_STATE_OLD     = LOW;  // previous state



// ESP RainMaker định nghĩa thiết bị
static Switch *fanSwitchPhongNgu = NULL;
static Switch *fanSwitchPhongKhach = NULL;
static Switch *ledPhongNguDevice = NULL;
static Switch *ledPhongKhachDevice = NULL;
static Switch *ledGarageDevice = NULL;
static TemperatureSensor temperatureSensorPhongNgu("Temp Phong Ngu");
static TemperatureSensor humiditySensorPhongNgu("Humidity Phong Ngu");
static TemperatureSensor temperatureSensorPhongKhach("Temp Phong Khach");
static TemperatureSensor humiditySensorPhongKhach("Humidity Phong Khach");

char device1[] = "SecuritySwitch";
static Switch SecuritySwitch(device1, NULL);
bool SECURITY_STATE = false;
uint32_t chipId = 0;

const char *service_name = "Khong Co Nha";
const char *pop = "7777@9999";

void sysProvEvent(arduino_event_t *sys_event)
{
    switch (sys_event->event_id) {
        case ARDUINO_EVENT_PROV_START:
#if CONFIG_IDF_TARGET_ESP32S2
            Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n", service_name, pop);
            printQR(service_name, pop, "softap");
#else
            Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
            printQR(service_name, pop, "ble");
#endif
            break;
        case ARDUINO_EVENT_PROV_INIT:
            wifi_prov_mgr_disable_auto_stop(10000);
            break;
        case ARDUINO_EVENT_PROV_CRED_SUCCESS:
            wifi_prov_mgr_stop_provisioning();
            break;
        default:;
    }
}

// Callback khi thiết bị RainMaker thay đổi trạng thái
void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx) {
  const char *device_name = device->getDeviceName();
  const char *param_name = param->getParamName();

  if (strcmp(device_name, "Fan Phong Ngu") == 0 && strcmp(param_name, "Power") == 0) {
    fanStatePhongNgu = val.val.b;
    digitalWrite(fanPhongNgu, fanStatePhongNgu ? HIGH : LOW);
    param->updateAndReport(val);  // Cập nhật trạng thái trên cloud
    Serial.printf("Fan Phong Ngu: %s\n", fanStatePhongNgu ? "ON" : "OFF");
  } else if (strcmp(device_name, "Fan Phong Khach") == 0 && strcmp(param_name, "Power") == 0) {
    fanStatePhongKhach = val.val.b;
    digitalWrite(fanPhongKhach, fanStatePhongKhach ? HIGH : LOW);
    param->updateAndReport(val);
    Serial.printf("Fan Phong Khach: %s\n", fanStatePhongKhach ? "ON" : "OFF");
  } else if (strcmp(device_name, "LED Phong Ngu") == 0 && strcmp(param_name, "Power") == 0) {
    ledStatePhongNgu = val.val.b > 0;
    digitalWrite(ledPhongNgu, ledStatePhongNgu ? HIGH : LOW);
    param->updateAndReport(val);
    Serial.printf("LED Phong Ngu: %s\n", ledStatePhongNgu ? "ON" : "OFF");
  } else if (strcmp(device_name, "LED Phong Khach") == 0 && strcmp(param_name, "Power") == 0) {
    ledStatePhongKhach = val.val.b > 0;
    digitalWrite(ledPhongKhach, ledStatePhongKhach ? HIGH : LOW);
    param->updateAndReport(val);
    Serial.printf("LED Phong Khach: %s\n", ledStatePhongKhach ? "ON" : "OFF");
  } else if (strcmp(device_name, "LED Garage") == 0 && strcmp(param_name, "Power") == 0) {
    ledStateGarage = val.val.b > 0;
    digitalWrite(ledGarage, ledStateGarage ? HIGH : LOW);
    param->updateAndReport(val);
    Serial.printf("LED Garage: %s\n", ledStateGarage ? "ON" : "OFF");
  } 
  if(strcmp(device_name, device1) == 0) {
    
    Serial.printf("SecuritySwitch = %s\n", val.val.b? "true" : "false");
    
    if(strcmp(param_name, "Power") == 0) {
      Serial.printf("Received value = %s for %s - %s\n", 
                    val.val.b? "true" : "false", device_name, param_name);
      SECURITY_STATE = val.val.b;
      param->updateAndReport(val);
      if(SECURITY_STATE == true){
        esp_rmaker_raise_alert("Security is ON");
        }
        else{
          esp_rmaker_raise_alert("Security is OFF");
        }        
    }
  }
}



void setup() {
  Serial.begin(9600);  // Giao tiếp UART với tốc độ baud rate 9600
  servoPhongNgu.attach(servoPinPhongNgu);  // Kết nối servo phòng ngủ
  servoKeyPad.attach(servoPinKeyPad);      // Kết nối servo keypad
  servoPhongNgu.write(closePosition);       // Ban đầu servo phòng ngủ ở vị trí đóng
  servoKeyPad.write(closePosition);         // Ban đầu servo keypad ở vị trí đóng
  pinMode(fanPhongNgu, OUTPUT);             // Cài đặt chân quạt phòng ngủ
  pinMode(buttonFanPhongNgu, INPUT_PULLUP);
  digitalWrite(fanPhongNgu, LOW);           // Đảm bảo quạt phòng ngủ tắt ban đầu
  pinMode(airPhongNgu, OUTPUT);             // Cài đặt chân điều hòa phòng ngủ
  digitalWrite(airPhongNgu, LOW);
  pinMode(fanPhongKhach, OUTPUT);           // Cài đặt chân quạt phòng khách
  pinMode(buttonFanPhongKhach, INPUT_PULLUP);
  digitalWrite(fanPhongKhach, LOW);         // Đảm bảo quạt phòng khách tắt ban đầu
  pinMode(airPhongKhach, OUTPUT);             // Cài đặt chân điều hòa phòng Khách
  digitalWrite(airPhongKhach, LOW);
  pinMode(ledPhongNgu, OUTPUT);             // Cài đặt chân LED là OUTPUT
  pinMode(ledPhongKhach, OUTPUT);             // Cài đặt chân LED là OUTPUT
  pinMode(ledGarage, OUTPUT);             // Cài đặt chân LED là OUTPUT
  pinMode(relayPin, OUTPUT);  // Đặt relay là đầu ra
  digitalWrite(relayPin, LOW);  // Đảm bảo relay tắt ban đầu
  Serial.println("Pump relay initialized.");

  Serial.println("System initialized, waiting for command...");  // Thông báo trạng thái ban đầu

  // ESP RainMaker Node
  Node my_node = RMaker.initNode("ESP RainMaker Node");

  // Fan Phong Ngu
  fanSwitchPhongNgu = new Switch("Fan Phong Ngu", &fanStatePhongNgu);
  fanSwitchPhongNgu->addCb(write_callback);
  my_node.addDevice(*fanSwitchPhongNgu);

  // Fan Phong Khach
  fanSwitchPhongKhach = new Switch("Fan Phong Khach", &fanStatePhongKhach);
  fanSwitchPhongKhach->addCb(write_callback);
  my_node.addDevice(*fanSwitchPhongKhach);

  // LED Phong Ngu
  ledPhongNguDevice = new Switch("LED Phong Ngu", &ledStatePhongNgu);
  ledPhongNguDevice->addCb(write_callback);
  my_node.addDevice(*ledPhongNguDevice);

  // LED Phong Khach
  ledPhongKhachDevice = new Switch("LED Phong Khach", &ledStatePhongKhach);
  ledPhongKhachDevice->addCb(write_callback);
  my_node.addDevice(*ledPhongKhachDevice);

  // LED Garage
  ledGarageDevice = new Switch("LED Garage", &ledStateGarage);
  ledGarageDevice->addCb(write_callback);
  my_node.addDevice(*ledGarageDevice);

  my_node.addDevice(temperatureSensorPhongNgu);
  my_node.addDevice(humiditySensorPhongNgu);
  my_node.addDevice(temperatureSensorPhongKhach);
  my_node.addDevice(humiditySensorPhongKhach);

  //Cảnh báo lửa
  SecuritySwitch.addCb(write_callback);
  my_node.addDevice(SecuritySwitch);

  RMaker.enableOTA(OTA_USING_TOPICS);
  RMaker.enableTZService();
  RMaker.enableSchedule();
  RMaker.enableScenes();
  for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  Serial.printf("\nChip ID:  %d Service Name: %s\n", chipId, service_name);
  Serial.printf("\nStarting ESP-RainMaker\n");
  RMaker.start();

  
  WiFi.onEvent(sysProvEvent);
#if CONFIG_IDF_TARGET_ESP32S2
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, pop, service_name);
#else
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, pop, service_name);
#endif

SecuritySwitch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, false);
}

void loop() {

  // Đọc trạng thái nút nhấn quạt phòng ngủ
  bool buttonFanStatePhongNgu = digitalRead(buttonFanPhongNgu);
  if (buttonFanStatePhongNgu != lastButtonFanStatePhongNgu && buttonFanStatePhongNgu == LOW) {
    fanStatePhongNgu = !fanStatePhongNgu;  // Đổi trạng thái quạt phòng ngủ
    digitalWrite(fanPhongNgu, fanStatePhongNgu ? HIGH : LOW);
    Serial.print("Fan Phong Ngu: ");
    Serial.println(fanStatePhongNgu ? "ON" : "OFF");
  }
  lastButtonFanStatePhongNgu = buttonFanStatePhongNgu;

  // Đọc trạng thái nút nhấn quạt phòng khách
  bool buttonFanStatePhongKhach = digitalRead(buttonFanPhongKhach);
  if (buttonFanStatePhongKhach != lastButtonFanStatePhongKhach && buttonFanStatePhongKhach == LOW) {
    fanStatePhongKhach = !fanStatePhongKhach;  // Đổi trạng thái quạt phòng khách
    digitalWrite(fanPhongKhach, fanStatePhongKhach ? HIGH : LOW);
    Serial.print("Fan Phong Khach: ");
    Serial.println(fanStatePhongKhach ? "ON" : "OFF");
  }
  lastButtonFanStatePhongKhach = buttonFanStatePhongKhach;

  if (Serial.available() > 0) {  // Nếu có dữ liệu nhận qua UART
    String command = Serial.readStringUntil('\n');  // Đọc lệnh nhận được qua UART
    command.trim();  // Xóa bỏ các ký tự xuống dòng hoặc khoảng trắng

    Serial.print("Received command: ");  // Hiển thị lệnh nhận được
    Serial.println(command);

    // Kiểm tra nếu dữ liệu là lệnh OPEN từ phòng ngủ
    if (command == "OPEN_BED" && !doorIsOpenPhongNgu && !inMotionPhongNgu) {
      inMotionPhongNgu = true;  // Đặt cờ báo hiệu cửa từ phòng ngủ đang chuyển động
      Serial.println("Opening door from bedroom...");
      servoPhongNgu.write(openPosition);  // Mở cửa (góc 90 độ)
      doorIsOpenPhongNgu = true;  // Cập nhật trạng thái cửa thành mở

      // Delay 5 giây trước khi đóng cửa
      delay(5000);  // Chờ 5 giây

      Serial.println("Closing door from bedroom...");
      servoPhongNgu.write(closePosition);  // Đóng cửa lại (góc 0 độ)
      doorIsOpenPhongNgu = false;  // Cập nhật trạng thái cửa thành đóng
      inMotionPhongNgu = false;  // Cửa đã hoàn thành đóng, có thể nhận lệnh tiếp
    }

    // Kiểm tra nếu dữ liệu là thông tin nhiệt độ và độ ẩm từ phòng ngủ
    if (command.startsWith("DATA_BED")) {
      // Parse dữ liệu nhiệt độ và độ ẩm từ phòng ngủ
      String data = command.substring(9);  // Lấy phần sau của "DATA_BED "
      int commaIndex = data.indexOf(',');  // Tìm vị trí dấu phẩy
      String temperature = data.substring(0, commaIndex);  // Nhiệt độ
      String humidity = data.substring(commaIndex + 1);  // Độ ẩm

      // In ra nhiệt độ và độ ẩm nhận được từ phòng ngủ
      Serial.print("Bedroom Temperature: ");
      Serial.println(temperature);
      Serial.print("Bedroom Humidity: ");
      Serial.println(humidity);

      float tempera = temperature.toFloat();
      float humi = humidity.toFloat();
      Serial.println(tempera);
      Serial.println(humi);
      temperatureSensorPhongNgu.updateAndReportParam("Temperature", tempera);
      humiditySensorPhongNgu.updateAndReportParam("Temperature", humi);
      

      // Điều khiển quạt dựa trên nhiệt độ phòng ngủ
      if (temperature.toFloat() > 32.0) {
        digitalWrite(airPhongNgu, HIGH);  // Bật quạt phòng ngủ
        Serial.println("Bedroom Fan ON");
      } else {
        digitalWrite(airPhongNgu, LOW);   // Tắt quạt phòng ngủ
        Serial.println("Bedroom Fan OFF");
      }
    }

    // Kiểm tra nếu dữ liệu là thông tin nhiệt độ và độ ẩm từ phòng khách
    if (command.startsWith("DATA_LIVING")) {
      // Parse dữ liệu nhiệt độ và độ ẩm từ phòng khách
      String data = command.substring(12);  // Lấy phần sau của "DATA_LIVING "
      int commaIndex = data.indexOf(',');  // Tìm vị trí dấu phẩy
      String temperature = data.substring(0, commaIndex);  // Nhiệt độ
      String humidity = data.substring(commaIndex + 1);  // Độ ẩm

      // In ra nhiệt độ và độ ẩm nhận được từ phòng khách
      Serial.print("Living Room Temperature: ");
      Serial.println(temperature);
      Serial.print("Living Room Humidity: ");
      Serial.println(humidity);

      float tempera = temperature.toFloat();
      float humi = humidity.toFloat();
      Serial.println(tempera);
      Serial.println(humi);
      temperatureSensorPhongKhach.updateAndReportParam("Temperature", tempera);
      humiditySensorPhongKhach.updateAndReportParam("Temperature", humi);


      // Điều khiển quạt dựa trên nhiệt độ phòng khách
      if (temperature.toFloat() > 32.0) {
        digitalWrite(airPhongKhach, HIGH);  // Bật quạt phòng khách
        Serial.println("Living Room Fan ON");
      } else {
        digitalWrite(airPhongKhach, LOW);   // Tắt quạt phòng khách
        Serial.println("Living Room Fan OFF");
      }
    }

    // Kiểm tra nếu có lệnh mở cửa từ keypad
    if (command == "OPEN_KEYPAD" && !doorIsOpenKeyPad && !inMotionKeyPad) {
      inMotionKeyPad = true;  // Đặt cờ báo hiệu cửa từ keypad đang chuyển động
      Serial.println("Opening door from keypad...");
      servoKeyPad.write(openPosition);  // Mở cửa từ keypad

      // Delay 5 giây trước khi đóng cửa
      delay(5000);  // Chờ 5 giây

      Serial.println("Closing door from keypad...");
      servoKeyPad.write(closePosition);  // Đóng cửa lại
      doorIsOpenKeyPad = false;  // Cập nhật trạng thái cửa thành đóng
      inMotionKeyPad = false;  // Cửa đã hoàn thành đóng, có thể nhận lệnh tiếp
    }
    
    if (command == "FIRE_DETECTED") {  // Nếu phát hiện lửa
      relayState = true;
      digitalWrite(relayPin, HIGH);  // Bật relay
      Serial.println("Pump ON");
      esp_rmaker_raise_alert("Security Alert!Fire is detected.");
    } else if (command == "NO_FIRE") {  // Nếu không còn lửa
      relayState = false;
      digitalWrite(relayPin, LOW);  // Tắt relay
      Serial.println("Pump OFF");
      esp_rmaker_raise_alert("Safe. No Fire");
    }
  }
}
