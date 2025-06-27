#include <Arduino.h>
#include <cmath>
#include <cstdint>
#include <ArduinoEigen.h>

#include <Wire.h>
#include <DS3231.h>
#include <MAX30105.h>
#include <LiquidCrystal_I2C.h>
#include "heartRate.h"

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <LittleFS.h>
#include <ArduinoJson.h>

#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP Wi-Fi Manager</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html {
      font-family: Arial, Helvetica, sans-serif; 
      display: inline-block; 
      text-align: center;
    }
    h1 {
      font-size: 1.8rem; 
      color: white;
    }
    p { 
      font-size: 1.4rem;
    }
    .topnav { 
      overflow: hidden; 
      background-color: #0A1128;
    }
    body {  
      margin: 0;
    }
    .content { 
      padding: 5%;
    }
    .card-grid { 
      max-width: 800px; 
      margin: 0 auto; 
      display: grid; 
      grid-gap: 2rem; 
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    }
    .card { 
      background-color: white; 
      box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
    }
    .card-title { 
      font-size: 1.2rem;
      font-weight: bold;
      color: #034078;
    }
    input[type=submit] {
      border: none;
      color: #FEFCFB;
      background-color: #034078;
      padding: 15px 15px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 16px;
      width: 100px;
      margin-right: 10px;
      border-radius: 4px;
      transition-duration: 0.4s;
    }
    input[type=submit]:hover {
      background-color: #1282A2;
    }
    input[type=text], input[type=number], select {
      width: 50%;
      padding: 12px 20px;
      margin: 18px;
      display: inline-block;
      border: 1px solid #ccc;
      border-radius: 4px;
      box-sizing: border-box;
    }
    label {
      font-size: 1.2rem; 
    }
    .value {
      font-size: 1.2rem;
      color: #1282A2;  
    }
    .state {
      font-size: 1.2rem;
      color: #1282A2;
    }
    button {
      border: none;
      color: #FEFCFB;
      padding: 15px 32px;
      text-align: center;
      font-size: 16px;
      width: 100px;
      border-radius: 4px;
      transition-duration: 0.4s;
    }
    .button-on {
      background-color: #034078;
    }
    .button-on:hover {
      background-color: #1282A2;
    }
    .button-off {
      background-color: #858585;
    }
    .button-off:hover {
      background-color: #252524;
    }
  </style>
</head>
<body>
  <div class="topnav">
    <h1>ESP Wi-Fi Manager</h1>
  </div>
  <div class="content">
    <div class="card-grid">
      <div class="card">
        <form action="/" method="POST">
            <p>
              <label for="ssid">SSID</label>
              <input type="text" id="ssid" name="ssid"><br>
              <label for="pass">Password</label>
              <input type="text" id="pass" name="pass"><br>
              <input type="hidden" id="ip" name="ip" value="192.168.1.200">
              <input type="hidden" id="gateway" name="gateway" value="192.168.1.1">
              <input type="submit" value="Submit">
            </p>
        </form>
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";

#define API_KEY "AIzaSyDX9FQQai2rWeUubCX903z-yPMro_TGTIQ"
#define DATABASE_URL "https://projectv1-a9190-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define PUSH_BUTTON_1 18
#define PUSH_BUTTON_3 5
#define DEBOUNCE_DELAY 50
#define FORMAT_LITTLEFS_IF_FAILED true

using namespace Eigen;

struct Button {
  int pin;
  int state;
  int lastState;
  unsigned long lastDebounceTime;
  bool isPressed;
};

const uint8_t Age_MIN = 20;
const uint8_t Age_MAX = 60;
const byte RATE_SIZE = 4;
const unsigned long BUTTON_DELAY = 200;
const long intervalWifi = 10000;
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt";
const char* var2Path = "/data/value.txt";
const String menu[] = {"Mulai Ukur", "Setting WiFi", "Upload Data"};
const int menuSize = sizeof(menu) / sizeof(menu[0]);

DS3231 myRTC;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
MAX30105 particleSensor;
AsyncWebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2);
IPAddress localIP;
IPAddress localGateway;
IPAddress subnet(255, 255, 0, 0);

Button button1 = {PUSH_BUTTON_1, HIGH, HIGH, 0, false};
Button button3 = {PUSH_BUTTON_3, HIGH, HIGH, 0, false};

uint8_t menuIndex = 0;
uint8_t cursorRow = 1;
uint8_t umurr = 0;
uint8_t umur = Age_MIN;
uint8_t inputStage = 0;
uint8_t idmicroInputStage = 0;
uint8_t idmicroDigits[3] = {0, 0, 0};
uint8_t GLU, CHOL, healthStatus;
bool inSubMenu = false;
bool initialized = false;
bool isSettingAge = false;
bool isSettingIdmicro = false;
bool shouldRunBacaSensor = false;
bool selesai_baca = false;
bool fingerDetected = false;
bool tampilkan_hasil;
String idmicro = "";
String ssid;
String pass;
String ip;
String gateway;
String idPath = "/idmicro";
String gluPath = "/glucose";
String cholPath = "/cholestrol";
String uridPath = "/urid";
String heartPath = "/heart";
String alterPath = "/alter";
String timePath = "/timestamp";
String statPath = "/status";
String IRPath = "/ifra";

unsigned long startTime;
unsigned long lastButtonPressTime = 0;
unsigned long previousMillis = 0;
unsigned long interval;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;
uint32_t irAvg;
int HR = 0;
uint64_t IR = 0;
int avgir, avgbpm;
float ACD;
uint64_t irSum = 0;
uint32_t irCount = 0;
long beatSum = 0;
int beatCount = 0;

void startWiFiSetup();
void initWiFi();
void initLittleFS();
void saveDataToJson();
void setupFirebase();
void kirimDataKeFirebase();
void syncTimeFromNTP();
void showMainMenu();
void returnToMainMenu();
void handleSubMenu();
void handleMainMenu();
void updateAgeDisplay();
void updateIdmicroDisplay();
void bacasensorStep();
String getTimestamp();
void displaySSID();
void displayStoredData();
uint8_t roundUpToUint8(float value);
bool debounceButton(Button& button);
float roundToOneDecimal(float value);
void createDir(fs::FS &fs, const char *path);
String readFile(fs::FS &fs, const char *path);
String readFile2(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const char *message);
void appendToFile(fs::FS &fs, const char *path, const char *message);
void deleteFile(fs::FS &fs, const char *path);
void copyValuu(uint8_t source, uint8_t& destination);
void copyValue(uint64_t source, uint64_t& destination);
void copyValuee(int source, int& destination);
void handleAgeInput(bool button1Pressed, bool button3Pressed);
uint8_t cekStatusKesehatan(uint8_t GLU, uint8_t CHOL, float ACD);
void handleIdmicroInput(bool button1Pressed, bool button3Pressed);
float myNeuralNetworkFunction(const Eigen::Vector3f& input, int network_id);

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  initLittleFS();
  createDir(LittleFS, "/data");
  String storedData = readFile2(LittleFS, var2Path);
  if (!particleSensor.begin(Wire)) {
    while (1)
      ;
  }
  pinMode(PUSH_BUTTON_1, INPUT_PULLUP);
  pinMode(PUSH_BUTTON_3, INPUT_PULLUP);
  byte ledBrightness = 45;
  particleSensor.setup(ledBrightness);
  particleSensor.setPulseAmplitudeRed(0x96);
  particleSensor.setPulseAmplitudeGreen(0xff);
  showMainMenu();
  startTime = millis();
  //displayStoredData();
  displaySSID();
}

void loop() {
  bool button1Pressed = debounceButton(button1);
  bool button3Pressed = debounceButton(button3);
  if (!inSubMenu && button1Pressed) {
    handleMainMenu();
   }
   if (isSettingAge) {
     handleAgeInput(button1Pressed, button3Pressed);
   } else if (isSettingIdmicro) {
     handleIdmicroInput(button1Pressed, button3Pressed);
   }
   if (button3Pressed) {
     if (!inSubMenu && cursorRow == 1) {
       handleSubMenu();
     } else if (inSubMenu && !isSettingAge && !isSettingIdmicro &&
                !shouldRunBacaSensor) {
       returnToMainMenu();
     }
   }
   if (shouldRunBacaSensor) {
     bacasensorStep();
   }
}

void initLittleFS() {
  LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED);
}

void createDir(fs::FS &fs, const char *path) {
  fs.mkdir(path);
}

String readFile(fs::FS &fs, const char *path) {
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    return String();
  }
  String fileContent;
  while (file.available()) {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    return;
  }
  file.print(message);
}

String readFile2(fs::FS &fs, const char *path) {
  File file = fs.open(path, FILE_READ);
  if (!file) {
    return String();
  }
  String fileContent;
  while (file.available()) {
    fileContent += file.readString();
  }
  file.close();
  return fileContent;
}

void appendToFile(fs::FS &fs, const char *path, const char *message) {
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    return;
  }
  file.println(message);
  file.close();
}

void deleteFile(fs::FS &fs, const char *path) {
  fs.remove(path);
}

void copyValuu(uint8_t source, uint8_t& destination) {
  destination = source;
}

void copyValue(uint64_t source, uint64_t& destination) {
  destination = source;
}

void copyValuee(int source, int& destination) {
  destination = source;
}

void showMainMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("#==== MENU ====#");
  lcd.setCursor(0, 1);
  lcd.print(menu[menuIndex]);
}

void returnToMainMenu() {
  inSubMenu = false;
  isSettingAge = false;
  isSettingIdmicro = false;
  shouldRunBacaSensor = false;
  selesai_baca = false;
  showMainMenu();
  lastBeat = 0;
  fingerDetected = false;
  beatsPerMinute = 0;
  beatAvg = 0;
  irAvg = 0;
  irSum = 0;
  irCount = 0;
  beatSum = 0;
  beatCount = 0;
  WiFi.softAPdisconnect(true);
}

void handleMainMenu() {
  if (millis() - lastButtonPressTime >= BUTTON_DELAY) {
    menuIndex = (menuIndex + 1) % menuSize;
    lcd.setCursor(0, 1);
    lcd.print(menu[menuIndex]);
    lcd.print("                ");
    lastButtonPressTime = millis();
  }
}

void handleSubMenu() {
  lcd.clear();
  if (menu[menuIndex] == "Mulai Ukur") {
    umur = Age_MIN;
    idmicroDigits[0] = 0;
    idmicroDigits[1] = 0;
    idmicroDigits[2] = 0;
    isSettingAge = true;
    initialized = false;
    handleAgeInput(false, false);
  } else if (menu[menuIndex] == "Upload Data") {
    lcd.print("Mengirim Data...");
    initWiFi();
  } else if (menu[menuIndex] == "Setting WiFi") {
    lcd.print("Mengatur WiFi...");
    startWiFiSetup();
  }
  inSubMenu = true;
}

bool debounceButton(Button& button) {
  int reading = digitalRead(button.pin);
  bool buttonPressed = false;
  if (reading != button.lastState) {
    button.lastDebounceTime = millis();
  }
  if ((millis() - button.lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != button.state) {
      button.state = reading;
      if (button.state == LOW && !button.isPressed) {
        button.isPressed = true;
        buttonPressed = true;
      } else if (button.state == HIGH) {
        button.isPressed = false;
      }
    }
  }
  button.lastState = reading;
  return buttonPressed;
}

void updateAgeDisplay() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("Umur: ");
  lcd.print(umur);
}

void handleAgeInput(bool button1Pressed, bool button3Pressed) {
  if (!initialized) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pilih Puluhan");
    updateAgeDisplay();
    initialized = true;
    inputStage = 0;
    return;
  }
  if (shouldRunBacaSensor) {
    return;
  }
  if (button1Pressed && millis() - lastButtonPressTime >= BUTTON_DELAY) {
    if (inputStage == 0) {
      umur += 10;
      if (umur > Age_MAX) {
        umur = Age_MIN;
      }
      updateAgeDisplay();
    } else if (inputStage == 1) {
      uint8_t puluhan = (umur / 10) * 10;
      uint8_t satuan = umur % 10;
      satuan++;
      if (satuan > 9) {
        satuan = 0;
      }
      umur = puluhan + satuan;
      if (umur > Age_MAX) {
        umur = puluhan;
      }
      updateAgeDisplay();
    }
    lastButtonPressTime = millis();
  }
  if (button3Pressed) {
    if (inputStage == 0) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Pilih Satuan");
      updateAgeDisplay();
      inputStage = 1;
    } else if (inputStage == 1) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Umur tersimpan:");
      lcd.setCursor(0, 1);
      lcd.print(umur);
      lcd.print(" tahun");
      delay(1000);
      inputStage = 2;
      isSettingAge = false;
      isSettingIdmicro = true;
      initialized = false;
      handleIdmicroInput(false, false);
    }
  }
}

void updateIdmicroDisplay() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("ID: ");
  lcd.print(idmicroDigits[0]);
  lcd.print(idmicroDigits[1]);
  lcd.print(idmicroDigits[2]);
}

void handleIdmicroInput(bool button1Pressed, bool button3Pressed) {
  if (!initialized) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pilih Ratusan");
    updateIdmicroDisplay();
    initialized = true;
    idmicroInputStage = 0;
    return;
  }
  if (shouldRunBacaSensor) {
    return;
  }
  if (button1Pressed && millis() - lastButtonPressTime >= BUTTON_DELAY) {
    if (idmicroInputStage == 0) {
      idmicroDigits[0]++;
      if (idmicroDigits[0] > 9) {
        idmicroDigits[0] = 0;
      }
    } else if (idmicroInputStage == 1) {
      idmicroDigits[1]++;
      if (idmicroDigits[1] > 9) {
        idmicroDigits[1] = 0;
      }
    } else if (idmicroInputStage == 2) {
      idmicroDigits[2]++;
      if (idmicroDigits[2] > 9) {
        idmicroDigits[2] = 0;
      }
    }
    updateIdmicroDisplay();
    lastButtonPressTime = millis();
  }
  if (button3Pressed) {
    if (idmicroInputStage == 0) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Pilih Puluhan");
      updateIdmicroDisplay();
      idmicroInputStage = 1;
    } else if (idmicroInputStage == 1) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Pilih Satuan");
      updateIdmicroDisplay();
      idmicroInputStage = 2;
    } else if (idmicroInputStage == 2) {
      idmicro = String(idmicroDigits[0]) + String(idmicroDigits[1]) +
                String(idmicroDigits[2]);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ID tersimpan:");
      lcd.setCursor(0, 1);
      lcd.print(idmicro);
      delay(1000);
      idmicroInputStage = 3;
      isSettingIdmicro = false;
      shouldRunBacaSensor = true;
      lcd.clear();
      button3.isPressed = false;
    }
  }
}

void bacasensorStep() {
  long irValue = particleSensor.getIR();
  if (irValue > 50000 && !fingerDetected) {
    delay(500);
    particleSensor.setPulseAmplitudeRed(64);
    fingerDetected = true;
    beatsPerMinute = 0;
    beatAvg = 0;
    irAvg = 0;
    irSum = 0;
    irCount = 0;
    beatSum = 0;
    beatCount = 0;
    lastBeat = 0;
    interval = millis();
  } else if (irValue <= 50000 && fingerDetected) {
    delay(500);
    particleSensor.setPulseAmplitudeRed(0x96);
    fingerDetected = false;
  }
  if (fingerDetected) {
    if (millis() - interval <= 30000) {
      tampilkan_hasil = false;
      irSum += irValue;
      irCount++;
      if (checkForBeat(irValue) == true) {
        long delta = millis() - lastBeat;
        lastBeat = millis();
        beatsPerMinute = 60 / (delta / 1000.0);
        if (beatsPerMinute < 180 && beatsPerMinute > 40) {
          beatSum += beatsPerMinute;
          beatCount++;
        }
      }
      int progress = map(millis() - interval, 0, 30000, 0, 100);
      lcd.setCursor(0, 0);
      lcd.print("Mengukur : ");
      lcd.setCursor(0, 1);
      lcd.print(progress);
      lcd.print("%");
    } else {
      if (beatCount > 0) {
        avgbpm = beatSum / beatCount;
      } else {
        avgbpm = 0;
      }
      if (irCount > 0) {
        avgir = irSum / irCount;
      } else {
        avgir = 0;
      }
      if (avgbpm > 40 && avgir > 50000) {
        copyValuee(avgbpm, HR);
        copyValue(avgir, IR);
        copyValuu(umur, umurr);
        delay(100);
        tampilkan_hasil = true;
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Gagal Membaca");
        lcd.setCursor(0, 1);
        lcd.print("Coba Lagi");
        delay(2000);
        returnToMainMenu();
      }
    }
  }
  if (tampilkan_hasil) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IR: ");
    lcd.print(IR);
    lcd.setCursor(0, 1);
    lcd.print("HR: ");
    lcd.print(HR);
    lcd.print(" bpm");
    delay(2000);
    selesai_baca = true;
  }
  if (selesai_baca) {
    Vector3f inputData;
    //inputData << static_cast<float>(IR), static_cast<float>(umurr), static_cast<float>(HR);
    inputData << static_cast<float>(98084), static_cast<float>(52), static_cast<float>(75);
    GLU = roundUpToUint8(myNeuralNetworkFunction(inputData, 1));
    CHOL = roundUpToUint8(myNeuralNetworkFunction(inputData, 3));
    ACD = roundToOneDecimal(myNeuralNetworkFunction(inputData, 2));
    healthStatus = cekStatusKesehatan(GLU, CHOL, ACD);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Glu: ");
    lcd.print(GLU);
    lcd.print(" mg/dL");
    lcd.setCursor(0, 1);
    lcd.print("Acid: ");
    lcd.print(ACD, 1);
    lcd.print(" mg/dL");
    delay(2000);
    if (healthStatus == 0) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Chol: ");
      lcd.print(CHOL);
      lcd.print(" mg/dL");
      lcd.setCursor(0, 1);
      lcd.print("Status: Sakit");
    } else if (healthStatus == 1) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Chol: ");
      lcd.print(CHOL);
      lcd.print(" mg/dL");
      lcd.setCursor(0, 1);
      lcd.print("Status: Sehat");
    }
    saveDataToJson();
    delay(2000);
    selesai_baca = false;
    shouldRunBacaSensor = false;
    String storedData = readFile2(LittleFS, var2Path);
    returnToMainMenu();
  }
}

void saveDataToJson() {
  File file = LittleFS.open(var2Path, FILE_READ);
  DynamicJsonDocument doc(1024);
  if (file) {
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
      doc["sensor"] = JsonObject();
    }
    file.close();
  } else {
    doc["sensor"] = JsonObject();
  }
  String timestamp = getTimestamp();
  JsonObject sensorData = doc["sensor"].createNestedObject(timestamp);
  sensorData["idmicro"] = idmicro;
  sensorData["infrared"] = IR;
  sensorData["glucose"] = GLU;
  sensorData["cholestrol"] = CHOL;
  sensorData["urid"] = ACD;
  sensorData["heart"] = HR;
  sensorData["alter"] = umur;
  sensorData["stat"] = healthStatus;
  sensorData["timestamp"] = timestamp;
  file = LittleFS.open(var2Path, FILE_WRITE);
  if (!file) {
    return;
  }
  serializeJson(doc, file);
  file.close();
  Serial.println("Data berhasil disimpan ke file JSON di LittleFS");
}

String getTimestamp() {
  char dateBuffer[26];
  bool isCentury = false;
  bool is12HourFormat = false;
  bool isPM = false;
  snprintf(dateBuffer, sizeof(dateBuffer), "%04u-%02u-%02uT%02u:%02u:%02u+07:00",
           myRTC.getYear() + 2000, myRTC.getMonth(isCentury), myRTC.getDate(),
           myRTC.getHour(is12HourFormat, isPM), myRTC.getMinute(),
           myRTC.getSecond());
  return String(dateBuffer);
}

void initWiFi() {
  ssid = readFile(LittleFS, ssidPath);
  pass = readFile(LittleFS, passPath);
  if (ssid == "") {
    return;
  }
  WiFi.mode(WIFI_STA);
  if (ip != "") {
    localIP.fromString(ip.c_str());
    localGateway.fromString(gateway.c_str());
    if (!WiFi.config(localIP, localGateway, subnet)) {
    }
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long currentMillis = millis();
  previousMillis = currentMillis;
  while (WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= intervalWifi) {
      ESP.restart();
    }
  }
  syncTimeFromNTP();
  setupFirebase();
  kirimDataKeFirebase();
  ESP.restart();
}

void setupFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    Firebase.reconnectWiFi(true);
  } else {
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void kirimDataKeFirebase() {
  if (!Firebase.ready()) {
    Serial.println("Firebase tidak siap.");
    return;
  }
  File file = LittleFS.open(var2Path, FILE_READ);
  if (!file) {
    Serial.println("Gagal membuka file data atau file tidak ada.");
    lcd.clear();
    lcd.print("Tidak Ada Data");
    delay(2000);
    return;
  }
  DynamicJsonDocument doc(2048); // Kapasitas disesuaikan jika perlu
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.print("deserializeJson() gagal: ");
    Serial.println(error.c_str());
    return;
  }
  
  JsonObject sensorData = doc["sensor"];
  if (sensorData.isNull() || sensorData.size() == 0) {
      Serial.println("Tidak ada data sensor untuk dikirim.");
      lcd.clear();
      lcd.print("Tidak Ada Data");
      delay(2000);
      return;
  }
  
  bool allDataSent = true;
  
  // ==================== MODIFIKASI DIMULAI ====================
  unsigned long startTime = millis(); // Catat waktu mulai
  int dataCount = sensorData.size();  // Hitung jumlah data yang akan dikirim
  // ==========================================================

  for (JsonPair kv : sensorData) {
    String timestamp = kv.key().c_str();
    JsonObject data = kv.value();
    FirebaseJson json;
    json.set(idPath.c_str(), data["idmicro"].as<String>());
    json.set(IRPath.c_str(), data["infrared"].as<String>());
    json.set(gluPath.c_str(), data["glucose"].as<String>());
    json.set(cholPath.c_str(), data["cholestrol"].as<String>());
    json.set(uridPath.c_str(), data["urid"].as<String>());
    json.set(heartPath.c_str(), data["heart"].as<String>());
    json.set(alterPath.c_str(), data["alter"].as<String>());
    json.set(statPath.c_str(), data["stat"].as<String>());
    json.set(timePath, timestamp);
    String databasePath = "/sensor";
    String parentPath = databasePath + "/" + timestamp;
    if (Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json)) {
      Serial.print("Data dengan timestamp ");
      Serial.print(timestamp);
      Serial.println(" berhasil dikirim ke Firebase.");
    } else {
      Serial.print("Gagal mengirim data dengan timestamp ");
      Serial.print(timestamp);
      Serial.println(". Error: " + fbdo.errorReason());
      allDataSent = false;
    }
  }

  // ==================== MODIFIKASI DIMULAI ====================
  unsigned long endTime = millis(); // Catat waktu selesai
  unsigned long duration = endTime - startTime; // Hitung durasi

  Serial.println("\n=============================================");
  Serial.println("          HASIL PENGIRIMAN DATA          ");
  Serial.println("---------------------------------------------");
  Serial.print("Jumlah data yang dikirim : ");
  Serial.println(dataCount);
  Serial.print("Waktu total pengiriman   : ");
  Serial.print(duration);
  Serial.println(" ms");
  if (dataCount > 0) {
    Serial.print("Waktu rata-rata per data : ");
    Serial.print((float)duration / dataCount);
    Serial.println(" ms");
  }
  Serial.println("=============================================\n");
  // ==================== MODIFIKASI SELESAI ====================

  if (allDataSent) {
    deleteFile(LittleFS, var2Path);
    Serial.println("Semua data terkirim, file JSON telah dihapus dari memori flash.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Data Terkirim");
    lcd.setCursor(0, 1);
    lcd.print("Ke Firebase!");
    delay(2000);
  } else {
    Serial.println("Beberapa data gagal dikirim. File tidak akan dihapus.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Gagal Kirim");
    lcd.setCursor(0, 1);
    lcd.print("Cek Serial");
    delay(3000);
  }
}

void startWiFiSetup() {
  WiFi.softAP("ESP-WIFI-MANAGER", NULL);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ketik Alamat");
  lcd.setCursor(0, 1);
  lcd.print(IP);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    int params = request->params();
    for (int i = 0; i < params; i++) {
      const AsyncWebParameter* p = request->getParam(i);
      if (p->isPost()) {
        if (p->name() == PARAM_INPUT_1) {
          ssid = p->value().c_str();
          writeFile(LittleFS, ssidPath, ssid.c_str());
        }
        if (p->name() == PARAM_INPUT_2) {
          pass = p->value().c_str();
          writeFile(LittleFS, passPath, pass.c_str());
        }
        if (p->name() == PARAM_INPUT_3) {
          ip = p->value().c_str();
          writeFile(LittleFS, ipPath, ip.c_str());
        }
        if (p->name() == PARAM_INPUT_4) {
          gateway = p->value().c_str();
          writeFile(LittleFS, gatewayPath, gateway.c_str());
        }
      }
    }
    request->send(
        200, "text/plain",
        "Selesai. ESP akan restart, hubungkan ke router Anda dan buka alamat "
        "IP: " + ip);
    ESP.restart();
  });
  server.begin();
}

void syncTimeFromNTP() {
  configTime(25200, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;
  }
  myRTC.setYear(timeinfo.tm_year + 1900 - 2000);
  myRTC.setMonth(timeinfo.tm_mon + 1);
  myRTC.setDate(timeinfo.tm_mday);
  myRTC.setHour(timeinfo.tm_hour);
  myRTC.setMinute(timeinfo.tm_min);
  myRTC.setSecond(timeinfo.tm_sec);
}

void displayStoredData() {
  String storedData = readFile2(LittleFS, var2Path);
  if (storedData == "") {
    Serial.println("Tidak ada data tersimpan.");
    delay(1000);
  } else {
    Serial.println("Data tersimpan:");
    Serial.println(storedData);
    delay(1000);
  }
}

uint8_t cekStatusKesehatan(uint8_t GLU, uint8_t CHOL, float ACD) {
  bool asamUratNormal = (ACD >= 3.5f && ACD <= 7.0f);
  bool kolesterolNormal = (CHOL < 200);
  bool gulaDarahNormal = (GLU >= 70 && GLU <= 140);
  return (asamUratNormal && kolesterolNormal && gulaDarahNormal) ? 1 : 0;
}

uint8_t roundUpToUint8(float value) {
  if (value < 0.0f) return 0;
  float rounded_up = ceil(value);
  if (rounded_up > 255.0f) return 255;
  return static_cast<uint8_t>(rounded_up);
}

float roundToOneDecimal(float value) {
  return round(value * 10.0f) / 10.0f;
}

float myNeuralNetworkFunction(const Eigen::Vector3f& input, int network_id) {
  Eigen::Vector3f x = input;
  Eigen::Vector3f xp1;
  Eigen::VectorXf a1;
  float a2;
  float y1_output;
  const float input_ymin = 0.0f;
  const float output_ymin = 0.0f;
  switch (network_id) {
    case 1: {
      Eigen::Vector3f x_offset(56543.0f, 24.0f, 60.0f);
      Eigen::Vector3f gain(1.99992000319987e-05f, 0.0188679245283019f,
                             0.024390243902439f);
      Eigen::VectorXf b1(5);
      b1 << 0.043478314201888204615f, 0.045332591642713096491f,
        -0.22422832665006922626f, -0.10298572226547086927f,
        -0.020543463362604305611f;
      Eigen::MatrixXf IW1_1(5, 3);
      IW1_1 << -0.15040868318387079494f, -0.19201539462629302335f,
        -0.15899545524421829223f, -0.15466743692436682456f,
        -0.19760650735482382379f, -0.16369386148328080033f,
        0.21563835158211197562f, 0.32556422845982946335f,
        0.29314867821981638318f, 0.18160375245888715767f,
        0.24912212264468475142f, 0.21000944829333154096f,
        0.11972911628732171851f, 0.14768162356584002559f,
        0.12178649633391974705f;
      float b2 = -0.004746110129271247785f;
      Eigen::RowVectorXf LW2_1(5);
      LW2_1 << -0.3090060562877842143f, -0.31274375113945251936f,
        0.5983171408725674878f, 0.43022673601241495644f,
        0.2376365869375564599f;
      float y_gain = 0.0111111111111111f;
      float y_xoffset = 95.0f;
      xp1 = (x - x_offset).array() * gain.array() + input_ymin;
      Eigen::VectorXf n1 = IW1_1 * xp1 + b1;
      a1.resize(n1.size());
      for (int i = 0; i < n1.size(); ++i) {
        a1[i] = 2.0f / (1.0f + exp(-2.0f * n1[i])) - 1.0f;
      }
      a2 = LW2_1 * a1 + b2;
      y1_output = (a2 - output_ymin) / y_gain + y_xoffset;
      break;
    }

    case 2: {
      Eigen::Vector3f x_offset(58124.0f, 31.0f, 60.0f);
      Eigen::Vector3f gain(2.01987557566454e-05f, 0.0208333333333333f,
                             0.0232558139534884f);
      Eigen::VectorXf b1(5);
      b1 << -0.040023234152423231569f, 0.040023244160910798062f,
        -0.040023244902930021905f, 0.040023254155707996271f,
        -0.040023248486739465557f;
      Eigen::MatrixXf IW1_1(5, 3);
      IW1_1 << 0.20554751351572525531f, 0.12053672989609462429f,
        0.21052588565821947486f, -0.20554752218654617768f,
        -0.12053673177065263311f, -0.21052588755705048396f,
        0.2055475303154167821f, 0.12053673697757459615f,
        0.21052589717051167773f, -0.20554754097468952434f,
        -0.12053674049997620266f, -0.21052590227057590977f,
        0.2055475345499658546f, 0.12053673841362209929f,
        0.2105258992799771689f;
      float b2 = 0.08601257368366782563f;
      Eigen::RowVectorXf LW2_1(5);
      LW2_1 << 0.34970358559638287099f, -0.34970360773312308966f,
        0.34970361041289860227f, -0.34970363124485964734f,
        0.34970361849628894824f;
      float y_gain = 0.222222222222222f;
      float y_xoffset = 2.4f;
      xp1 = (x - x_offset).array() * gain.array() + input_ymin;
      Eigen::VectorXf n1 = IW1_1 * xp1 + b1;
      a1.resize(n1.size());
      for (int i = 0; i < n1.size(); ++i) {
        a1[i] = 2.0f / (1.0f + exp(-2.0f * n1[i])) - 1.0f;
      }
      a2 = LW2_1 * a1 + b2;
      y1_output = (a2 - output_ymin) / y_gain + y_xoffset;
      break;
    }

    case 3: {
      Eigen::Vector3f x_offset(58124.0f, 25.0f, 60.0f);
      Eigen::Vector3f gain(2.0652196361083e-05f, 0.0185185185185185f,
                             0.0232558139534884f);
      Eigen::VectorXf b1(5);
      b1 << 0.054253637728892738223f, 0.054253716535899769446f,
        -0.054253724693496700737f, 0.054253738543225793478f,
        -0.054253724411656552296f;
      Eigen::MatrixXf IW1_1(5, 3);
      IW1_1 << -0.19201222264032605236f, -0.2161493748276628879f,
        -0.17137429011668622869f, -0.19201232566285278414f,
        -0.21614950858101003583f, -0.17137438662408638335f,
        0.19201230866404364606f, 0.21614950008515446123f,
        0.17137437494174911912f, -0.19201227274336196693f,
        -0.21614947995887770493f, -0.17137434957630559573f,
        0.19201230559940885012f, 0.21614949742931777177f,
        0.17137437248431902637f;
      float b2 = 0.036625506519114281456f;
      Eigen::RowVectorXf LW2_1(5);
      LW2_1 << -0.36182559629564642334f, -0.3618257745877578313f,
        0.36182579099826850388f, -0.36182581833756233269f,
        0.36182579016130633764f;
      float y_gain = 0.00892857142857143f;
      float y_xoffset = 123.0f;
      xp1 = (x - x_offset).array() * gain.array() + input_ymin;
      Eigen::VectorXf n1 = IW1_1 * xp1 + b1;
      a1.resize(n1.size());
      for (int i = 0; i < n1.size(); ++i) {
        a1[i] = 2.0f / (1.0f + exp(-2.0f * n1[i])) - 1.0f;
      }
      a2 = LW2_1 * a1 + b2;
      y1_output = (a2 - output_ymin) / y_gain + y_xoffset;
      break;
    }

    default:
      Serial.print("Error: Invalid network_id provided to "
                     "myNeuralNetworkFunction: ");
      Serial.println(network_id);
      return -1.0f;
  }
  return y1_output;
}

void displaySSID() {
  Serial.println("\n==================================");
  Serial.println("Membaca Kredensial WiFi Tersimpan");
  
  String savedSSID = readFile(LittleFS, ssidPath);
  String savedPass = readFile(LittleFS, passPath);

  if (savedSSID != "") {
    Serial.print("SSID: ");
    Serial.println(savedSSID);
  } else {
    Serial.println("SSID belum di-setting.");
  }

  if (savedPass != "") {
    Serial.print("Password: ");
    Serial.println(savedPass);
  } else {
    Serial.println("Password belum di-setting.");
  }
  Serial.println("==================================");
}
