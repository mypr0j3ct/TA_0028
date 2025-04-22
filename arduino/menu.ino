#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>
#include <DS3231.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoEigen.h>
#include <LiquidCrystal_I2C.h>
#include "heartRate.h"
#include "MAX30105.h"
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

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
const String menu[] = {"Mulai Ukur", "Upload Data", "Setting WiFi"};
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
uint8_t GLU, CHOL;
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
byte rates[RATE_SIZE];
byte irValues[RATE_SIZE];
byte rateSpot = 0;

Vector3f xp1;
Vector3f x_offset;
Vector3f gain;
VectorXf a1;
VectorXf b1;
MatrixXf IW1_1;
RowVectorXf LW2_1;
float a2;
float y1_output; 
float b2;
float y_gain;
float y_xoffset;

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
void handleIdmicroInput(bool button1Pressed, bool button3Pressed);
float myNeuralNetworkFunction(const Eigen::Vector3f& input, int network_id);

void setup() {
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
  rateSpot = 0;
  lastBeat = 0;
  fingerDetected = false;
  beatsPerMinute = 0;
  beatAvg = 0;
  irAvg = 0;
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
    umur = Age_MIN;
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
    idmicroDigits[0] = 0;
    idmicroDigits[1] = 0;
    idmicroDigits[2] = 0;
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
  HR = 0;
  IR = 0;
  long irValue = particleSensor.getIR();

  if (irValue > 50000 && !fingerDetected) {
    delay(500);
    particleSensor.setPulseAmplitudeRed(64);
    fingerDetected = true;
    beatsPerMinute = 0;
    beatAvg = 0;
    irAvg = 0;
    interval = millis();
  } else if (irValue <= 50000 && fingerDetected) {
    delay(500);
    particleSensor.setPulseAmplitudeRed(0x06);
    fingerDetected = false;
  }

  if (fingerDetected) {
    if (millis() - interval <= 30000) {
      tampilkan_hasil = false;

      irValues[rateSpot] = irValue;

      if (checkForBeat(irValue) == true) {
        long delta = millis() - lastBeat;
        lastBeat = millis();

        beatsPerMinute = 60 / (delta / 1000.0);

        if (beatsPerMinute < 180 && beatsPerMinute > 50) {
          rates[rateSpot] = (byte)beatsPerMinute;
          rateSpot = (rateSpot + 1) % RATE_SIZE;

          beatAvg = 0;
          for (byte x = 0; x < RATE_SIZE; x++) {
            beatAvg += rates[x];
          }
          beatAvg /= RATE_SIZE;

          irAvg = 0;
          for (byte x = 0; x < RATE_SIZE; x++) {
            irAvg += irValue;
          }
          irAvg /= RATE_SIZE;
        }
      }
      int progress = map(millis() - interval, 0, 30000, 0, 100);
      lcd.setCursor(0, 0);
      lcd.print("Mengukur : ");
      lcd.setCursor(0, 1);
      lcd.print(progress);
      lcd.print("%");
    } else {
      avgbpm = beatAvg;
      avgir = irAvg;
      copyValuee(avgbpm, HR);
      copyValue(avgir, IR);
      copyValuu(umur, umurr);
      delay(100);
      tampilkan_hasil = true;
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
    inputData << IR, umurr, HR;
    GLU = roundUpToUint8(myNeuralNetworkFunction(inputData, 1));
    CHOL = roundUpToUint8(myNeuralNetworkFunction(inputData, 2));
    ACD = roundToOneDecimal(myNeuralNetworkFunction(inputData, 3));
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Glu: ");
    lcd.print(GLU);
    lcd.print(" mg/dL");
    lcd.setCursor(0, 1);
    lcd.print("Acid: ");
    lcd.print(ACD, 1);
    lcd.print(" mg/dL");
    delay(4000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Chol: ");
    lcd.print(CHOL);
    lcd.print(" mg/dL");
    saveDataToJson();
    delay(3000);
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
  sensorData["glucose"] = GLU;
  sensorData["cholestrol"] = CHOL;
  sensorData["urid"] = ACD;
  sensorData["heart"] = HR;
  sensorData["alter"] = umur;
  sensorData["timestamp"] = timestamp;

  file = LittleFS.open(var2Path, FILE_WRITE);
  if (!file) {
    return;
  }

  serializeJson(doc, file);
  file.close();
}

String getTimestamp() {
  char dateBuffer[25];
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
  delay(1000);
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
    return;
  }

  File file = LittleFS.open(var2Path, FILE_READ);
  if (!file) {
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    return;
  }

  JsonObject sensorData = doc["sensor"];
  bool allDataSent = true;

  for (JsonPair kv : sensorData) {
    String timestamp = kv.key().c_str();
    JsonObject data = kv.value();
    FirebaseJson json;
    json.set(idPath.c_str(), data["idmicro"].as<String>());
    json.set(gluPath.c_str(), data["glucose"].as<String>());
    json.set(cholPath.c_str(), data["cholestrol"].as<String>());
    json.set(uridPath.c_str(), data["urid"].as<String>());
    json.set(heartPath.c_str(), data["heart"].as<String>());
    json.set(alterPath.c_str(), data["alter"].as<String>());
    json.set(timePath, timestamp);

    String databasePath = "/sensor";
    String parentPath = databasePath + "/" + timestamp;

    if (Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json)) {
    } else {
      allDataSent = false;
    }
  }

  if (allDataSent) {
    deleteFile(LittleFS, var2Path);
  }
}

void startWiFiSetup() {
  WiFi.softAP("ESP-WIFI-MANAGER", NULL);
  IPAddress IP = WiFi.softAPIP();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/wifimanager.html", "text/html");
  });

  server.serveStatic("/", LittleFS, "/");
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
        "IP: " +
            ip);
    delay(3000);
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

uint8_t roundUpToUint8(float value) {
  return static_cast<uint8_t>(ceil(value));
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

  switch (network_id) {
    case 1: {
      Eigen::Vector3f x_offset(51735, 9, 26);
      Eigen::Vector3f gain(3.64896916621055e-05, 0.0294117647058824,
                           0.0266666666666667);
      xp1 = (x - x_offset).array() * gain.array() + (-1.0f);

      float b1 = -16.63759766878351698f;
      Eigen::RowVector3f IW1_1(2.1493558295058914354f,
                               14.726331644390356246f,
                               16.513591564909340548f);
      float sum = IW1_1 * xp1 + b1;
      a1.resize(1);
      a1[0] = 2.0f / (1.0f + exp(-2.0f * sum)) - 1.0f;

      float b2 = -0.39083029729430657229f;
      float LW2_1 = 0.24426653523419675218f;
      a2 = LW2_1 * a1[0] + b2;

      float y_gain = 0.0117647058823529f;
      float y_xoffset = 94.0f;
      y1_output = (a2 - (-1.0f)) / y_gain + y_xoffset;
      break;
    }
    case 2: {
      Eigen::Vector3f x_offset(58124, 9, 55);
      Eigen::Vector3f gain(4.34839326868722e-05, 0.0266666666666667,
                           0.0416666666666667);
      xp1 = (x - x_offset).array() * gain.array() + (-1.0f);

      Eigen::VectorXf b1(5);
      b1 << -4.1572857480307350286f, -0.38029472457922613993f,
          0.70156562858405513428f, 1.071440477708899941f,
          2.1105006102053316397f;
      Eigen::MatrixXf IW1_1(5, 3);
      IW1_1 << 2.5350106320688046146f, -3.2503410452606162906f,
          1.6572979890684240711f, 0.90462799086409484417f,
          2.4814449873300388205f, -0.0057440872056210220964f,
          -1.0605734542426203948f, -0.19615437391647000398f,
          1.9414825351178413015f, 1.7987390581184692362f,
          -0.77200529967437936385f, -0.99063946624861731749f,
          0.63577746361881404269f, 2.1635580643615264229f,
          -1.3125931768397518518f;
      Eigen::VectorXf layer1 = IW1_1 * xp1 + b1;
      a1.resize(5);
      for (int i = 0; i < 5; i++) {
        a1[i] = 2.0f / (1.0f + exp(-2.0f * layer1[i])) - 1.0f;
      }

      float b2 = 0.57574514652368069534f;
      Eigen::RowVectorXf LW2_1(5);
      LW2_1 << 2.0816195454248540564f, 0.40146089545829760636f,
          0.24168468981366877935f, -0.0050136968048260094344f,
          1.0409722567092083434f;
      a2 = LW2_1 * a1 + b2;

      float y_gain = 0.3125f;
      float y_xoffset = 2.4f;
      y1_output = (a2 - (-1.0f)) / y_gain + y_xoffset;
      break;
    }
    case 3: {
      Eigen::Vector3f x_offset(52788, 17, 26);
      Eigen::Vector3f gain(3.98374631503466e-05, 0.0298507462686567,
                           0.025974025974026);
      xp1 = (x - x_offset).array() * gain.array() + (-1.0f);

      Eigen::VectorXf b1(3);
      b1 << 2.8987119892188055736f, 9.9275370296609057874f,
          7.3558450309877070339f;
      Eigen::MatrixXf IW1_1(3, 3);
      IW1_1 << 7.8709740143197421958f, 13.886396469436782297f,
          12.231008407705287411f, -13.894948315389491711f,
          -33.600792028840380965f, -15.022515547619278209f,
          0.57951782866092504953f, 5.9329585451750430636f,
          -5.2409751499447558842f;
      Eigen::VectorXf layer1 = IW1_1 * xp1 + b1;
      a1.resize(3);
      for (int i = 0; i < 3; i++) {
        a1[i] = 2.0f / (1.0f + exp(-2.0f * layer1[i])) - 1.0f;
      }

      float b2 = 0.50460391480801192188f;
      Eigen::RowVectorXf LW2_1(3);
      LW2_1 << 0.18972498791585371003f, -0.36183087980935402239f,
          -0.69365338494794059887f;
      a2 = LW2_1 * a1 + b2;

      float y_gain = 0.0105263157894737f;
      float y_xoffset = 100.0f;
      y1_output = (a2 - (-1.0f)) / y_gain + y_xoffset;
      break;
    }
    default:
      return -1.0f;
  }
  return y1_output;
}
