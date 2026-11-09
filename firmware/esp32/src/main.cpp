#define BLINKER_WIFI

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <stdio.h>
#include <string.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <Blinker.h>

#define STM_TX 17
#define STM_RX 16
#define EEPROM_SIZE 3000
#define SSID_ADDR  2500
#define PSWD_ADDR  2532

HardwareSerial STM_Serial(2); //初始化硬件串口
WebServer server(80); //初始化服务器

//Blinker控件
BlinkerButton Button1("btn_switch");
BlinkerNumber Number1("num_spd");
BlinkerNumber Number2("num_req");
BlinkerSlider Slider1("sp_adj");
BlinkerButton Button2("btn_alarm");
BlinkerButton Button3("btn_ref");

bool STM_Status = false;
String receiveBuffer;
String ssid = "";
String pswd = "";
String key = "YOUR_BLINKER_KEY"; // 请填入你自己的 Blinker 设备密钥
char stm_infusionState;
int stm_speed;
int stm_reqSpeed;
char stm_alarmstate = 0;
char stmSerialBuffer[64];


void STM_Init();
void LoadWifiConfig(void); //加载数据
void saveWifiConfig(String newssid, String newpswd); //存储数据
bool ConnectWifi(); //网卡连接操作
void handleRoot(); //web配置界面
void handleSave(); //web服务器保存
void Wifi_init_process(); //Wifi启动流程
void STM_ReadData();
void processJSONData(const char* JSONData); //JSON解析
bool STM_Setspeed(int speed);
bool STM_Switch();
void STM_ClearSerialBuffer();
void Blinker_AttachCallBackFunc(void);
void readBlinkerData(const String &data); //处理数据流
void button1_callback(const String &state);
void button2_callback(const String &state);
void button3_callback(const String &state);
void slider1_callback(int32_t data);
void sendAllStates(void); //发送读取到的数据到手机
void heartbeat_callback();

void setup()
{
  Serial.begin(115200);
  STM_Serial.begin(115200, SERIAL_8N1, 16, 17);
  Serial.println("ESP32 Module Start!");
  Serial.println("Powered by Snowmiku~");
  STM_Init();
  Wifi_init_process(); //加载Wifi
  STM_ClearSerialBuffer();
  Serial.println("正在启动Blinker服务...");
  BLINKER_DEBUG.stream(Serial);
  Blinker.begin(key.c_str(), ssid.c_str(), pswd.c_str());
  Blinker_AttachCallBackFunc();
}

void loop()
{
  Blinker.run();
  if(STM_Serial.available())
  {
    int l = STM_Serial.readBytesUntil('\n', stmSerialBuffer, sizeof(stmSerialBuffer) - 1);
    stmSerialBuffer[l] = 0;
    if(String(stmSerialBuffer) == "EMPTY" && stm_alarmstate == 0)
    {
      //发送通知
      stm_alarmstate = 1;
      Serial.println("液体空！");
      Blinker.push("输液已结束"); 
    }
  }
}

void Blinker_AttachCallBackFunc(void)
{
  Blinker.attachData(readBlinkerData);
  Blinker.attachHeartbeat(heartbeat_callback);
  Button1.attach(button1_callback);
  Slider1.attach(slider1_callback);
  Button2.attach(button2_callback);
  Button3.attach(button3_callback);
}

//Blinker回调函数

void readBlinkerData(const String &data) //处理数据流
{
  BLINKER_LOG("Blinker readString: ", data);
  STM_ReadData();
  sendAllStates(); //刷新手机端的数据
}

void heartbeat_callback()
{
  STM_ReadData();
  sendAllStates(); //刷新手机端的数据
}

void button1_callback(const String &state)
{
  BLINKER_LOG("get button state: ", state);
  //切换STM的输液状态
  if(STM_Switch() == true)
  {
    if(stm_infusionState == 0)
    {
      Button1.print("off");
    }
    else
    {
      Button1.print("on");
    }
    Serial.printf("STM32状态改变到:%d\n", stm_infusionState);
  }  
}

void button2_callback(const String &state)
{
  if(state == "on")
  {
    Serial.println("无效输入");
  }
  if(stm_alarmstate == 1 && state == "off")
  {
    stm_alarmstate = 0;
    Button2.print("off");
    Serial.println("警报已解除");
  }
}

void button3_callback(const String &state)
{
  if(state == "tap"){
    Serial.println("数据刷新");
    STM_ReadData();
    sendAllStates();
  }
}

void slider1_callback(int32_t data)
{
  BLINKER_LOG("get slider data: ", data);
  STM_Setspeed(data);
}

void sendAllStates(void) //发送读取到的数据到手机
{
  Serial.println("数据回传中...");
  if(stm_infusionState == 1)
  {
    Button1.print("on");
  }
  else Button1.print("off");
  if(stm_alarmstate)
  {
    Button2.print("on");
  }
  else 
  {
    Button2.print("off");
  }
  Number1.print(stm_speed);
  Number2.print(stm_reqSpeed);
  Slider1.print(stm_reqSpeed);
}

//STM控制函数

void STM_ClearSerialBuffer()
{
  STM_Serial.readBytesUntil('\n', stmSerialBuffer, sizeof(stmSerialBuffer) - 1);
}

void STM_Init()
{
  Serial.print("Connecting to STM32");
  while(1){
    STM_Serial.println("@ESP"); //ESP Connect to STM32
    if(STM_Serial.available())
    {
      receiveBuffer = STM_Serial.readStringUntil('\n');
      if(receiveBuffer == "STM") 
      {
        Serial.printf("\nConnected to STM32!\n");
        STM_Status = true;
        delay(10);
        break;
      }     
    }
    Serial.print(".");
    delay(5);
  }
}

void LoadWifiConfig(void) //加载数据
{
  //初始化EEPROM
  char pswd_buf[32] = {0};
  char ssid_buf[32] = {0};
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(SSID_ADDR, ssid_buf);
  EEPROM.get(PSWD_ADDR, pswd_buf);
  EEPROM.end();
  ssid = String(ssid_buf);
  pswd = String(pswd_buf);
  Serial.printf("SSID:%s, Password:%s\n", ssid, pswd);
}

void saveWifiConfig(String newssid, String newpswd) //存储数据
{
    EEPROM.begin(EEPROM_SIZE);
    int i = 0;
    //Write SSID
    while(newssid[i]){
      EEPROM.write(SSID_ADDR+i, newssid[i]);
      delay(1);
      i++;
    }
    EEPROM.write(SSID_ADDR+i, 0);
    //Write PSWD
    i = 0;
    while(newpswd[i]){
      EEPROM.write(PSWD_ADDR+i, newpswd[i]);
      delay(1);
      i++;
    }
    EEPROM.write(PSWD_ADDR+i, 0);
    delay(2);
    EEPROM.end();
    delay(2);
}

bool ConnectWifi()
{
    WiFi.begin(ssid.c_str(), pswd.c_str());
    Serial.print("Connecting to Wi-Fi");
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 10) {
        delay(1000);
        Serial.print(".");
        attempt++;
    }
    return WiFi.status() == WL_CONNECTED; //返回WiFi连接状态
}

// AP 配置页面
void handleRoot() {
    server.send(200, "text/html",
        "<html><body>"
        "<h1>ESP32 Wi-Fi Configure</h1>"
        "<form action='/save' method='POST'>"
        "SSID: <input type='text' name='ssid'><br>"
        "Password: <input type='password' name='pass'><br>"
        "<input type='submit' value='Save'>"
        "</form></body></html>"
    );
}

// 处理 Wi-Fi 配置提交
void handleSave() {
    if (server.hasArg("ssid") && server.hasArg("pass")) {
        String newSSID = server.arg("ssid");
        String newPass = server.arg("pass");
        saveWifiConfig(newSSID, newPass); //保存从网页获取的配置信息
        server.send(200, "text/plain", "Wi-Fi config saved, rebooting...");
        Serial.printf("\n收到SSID:%s, 密码:%s\n", newSSID, newPass);
        delay(2000);
        ESP.restart(); //重启设备
    } else {
        server.send(400, "text/plain", "Error!");
    }
}

// 网络连接程序
void Wifi_init_process()
{
  // 读取存储的 Wi-Fi 信息
    LoadWifiConfig();

    // 连接 Wi-Fi
    if (ssid.length() > 0 && ConnectWifi()) {
        Serial.println("\nWi-Fi 连接成功！");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWi-Fi 连接失败，进入 AP 模式！");
        WiFi.softAP("ESP32_Config", "12345678");
        IPAddress IP = WiFi.softAPIP();
        Serial.print("AP 模式启动，IP 地址：");
        Serial.println(IP);

        server.on("/", handleRoot); //给定服务器回调函数
        server.on("/save", handleSave);
        server.begin();
        while(1)
        {
          server.handleClient();
        }
    }
}

void STM_ReadData()
{
  char jsonBuffer[256];

  STM_ClearSerialBuffer();
  STM_Serial.println("@acquire");
  while(1){
    if (STM_Serial.available()) {
            Serial.println("Pull data successfully!");
            int len = STM_Serial.readBytesUntil('\n', jsonBuffer, sizeof(jsonBuffer) - 1);
            jsonBuffer[len] = '\0'; // 添加字符串终止符
            Serial.printf("JSON:%s\n", jsonBuffer);
            processJSONData(jsonBuffer); // 解析 JSON 数据
            break;
      }
  }
}

void processJSONData(const char* JSONData)
{
  // 创建 JSON 解析缓冲区
    StaticJsonDocument<200> doc;

    // 解析 JSON 字符串
    DeserializationError error = deserializeJson(doc, JSONData);
    if (error) {
        Serial.print("JSON 解析失败: ");
        Serial.println(error.f_str());
        return;
    }

    // 提取 JSON 数据
    int state = doc["state"];               // 读取 "state" 字段
    int speed = doc["speed"];               // 读取 "speed" 字段
    int requiredSpeed = doc["requiredspeed"]; // 读取 "requiredspeed" 字段

    //写入全局变量
    stm_infusionState = (char)state;
    stm_reqSpeed = requiredSpeed;
    stm_speed = speed;

    // 打印解析结果
    Serial.println("解析成功!");
    Serial.print("State: ");
    Serial.println(state);
    Serial.print("Speed: ");
    Serial.println(speed);
    Serial.print("Required Speed: ");
    Serial.println(requiredSpeed);
    Serial.println();
}

bool STM_Setspeed(int speed)
{
  if(speed > 120 || speed < 10)
  {
    Serial.println("调速函数的输入错误！");
    return false;
  }
  else 
  {
    Serial.printf("Set speed to %d\n", speed);
    STM_ClearSerialBuffer();
    STM_Serial.println("@set_speed");
    while(1)
    {
      if(STM_Serial.available())
      {
        int l = STM_Serial.readBytesUntil('\n', stmSerialBuffer, sizeof(stmSerialBuffer) - 1);
        stmSerialBuffer[l] = 0;
        if(String(stmSerialBuffer) == "OK")
        {
          Serial.println("STM32已响应速度调节指令,正在写入...");
          STM_ClearSerialBuffer();
          STM_Serial.print("@");
          STM_Serial.println(speed);
        }
        Serial.println("正在校验...");

        while(1){
          if(STM_Serial.available()){
            l = STM_Serial.readBytesUntil('\n', stmSerialBuffer, sizeof(stmSerialBuffer) - 1);
            stmSerialBuffer[l] = 0;
            processJSONData(stmSerialBuffer);
            if(stm_reqSpeed == speed) return true;
            else return false;
          }
        }
        
      }
    }
  }
}

bool STM_Switch()
{
  char ori_infusionstate = stm_infusionState;
  STM_ClearSerialBuffer();
  STM_Serial.println("@switch");
  while(1)
  {
    if(STM_Serial.available())
      {
        STM_Serial.readBytesUntil('\n', stmSerialBuffer, sizeof(stmSerialBuffer) - 1);
        STM_ReadData();
        if(stm_infusionState == abs(ori_infusionstate - 1)) return true;
        else return false;
      }
  }
}
