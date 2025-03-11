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
#include "heartRate.h"
#include "MAX30105.h"
#include <LiquidCrystal_I2C.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define API_KEY "AIzaSyDX9FQQai2rWeUubCX903z-yPMro_TGTIQ"
#define DATABASE_URL "https://projectv1-a9190-default-rtdb.asia-southeast1.firebasedatabase.app/"

#define PUSH_BUTTON_1 18
#define PUSH_BUTTON_3 5
#define DEBOUNCE_DELAY 50
#define FORMAT_LITTLEFS_IF_FAILED true

DS3231 myRTC;
FirebaseData fbdo;
FirebaseAuth auth;
IPAddress localIP;
FirebaseConfig config;
IPAddress localGateway;
MAX30105 particleSensor;
AsyncWebServer server(80);
IPAddress subnet(255, 255, 0, 0);
LiquidCrystal_I2C lcd(0x27, 16, 2);

struct Button {
  int pin;
  int state;
  int lastState;
  unsigned long lastDebounceTime;
  bool isPressed;
};

const uint8_t Age_MIN = 20;
const uint8_t Age_MAX = 99;

uint8_t menuIndex = 0;
uint8_t cursorRow = 1;
uint8_t umurr = 0;
bool inSubMenu = false;
bool initialized = false;
unsigned long lastButtonPressTime = 0;
const unsigned long BUTTON_DELAY = 200;
const String menu[] = {"Mulai Ukur", "Upload Data", "Setting WiFi"};
const int menuSize = sizeof(menu) / sizeof(menu[0]);
const char* var2Path = "/data/value.txt";

unsigned long startTime;
bool isSettingAge = false;
bool isSettingIdmicro = false;
bool shouldRunBacaSensor = false;
bool selesai_baca = false;
uint8_t umur = Age_MIN;
uint8_t inputStage = 0;
uint8_t idmicroInputStage = 0;
uint8_t idmicroDigits[3] = {0, 0, 0};
String idmicro = "";

Button button1 = {PUSH_BUTTON_1, HIGH, HIGH, 0, false};
Button button3 = {PUSH_BUTTON_3, HIGH, HIGH, 0, false};

double scaledHR_1, scaledIR_1, scaledUmur_1;
double scaledHR_2, scaledIR_2, scaledUmur_2;
double scaledHR_3, scaledIR_3, scaledUmur_3;
double findValue_1, findValue_2, findValue_3;
double sigmoid_1, sigmoid_2, sigmoid_3;

uint8_t GLU, CHOL;
float ACD;
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte irValues[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;
uint32_t irAvg;
int HR = 0;
uint64_t IR = 0;
bool fingerDetected = false;
unsigned long interval;
int avgir, avgbpm;
bool tampilkan_hasil;

const uint8_t GLU_MIN = 82;
const uint16_t GLU_MAX = 396;
const uint8_t CHOL_MIN = 139;
const uint8_t CHOL_MAX = 223;
const float ACD_MIN = 3.8;
const float ACD_MAX = 7;

const uint8_t HR_MIN_1 = 66;
const uint8_t HR_MAX_1 = 112;
const uint8_t HR_MIN_2 = 66;
const uint8_t HR_MAX_2 = 112;
const uint8_t HR_MIN_3 = 66;
const uint8_t HR_MAX_3 = 112;

const uint32_t IR_MIN_1 = 86765;
const uint32_t IR_MAX_1 = 107756;
const uint32_t IR_MIN_2 = 86765;
const uint32_t IR_MAX_2 = 107756;
const uint32_t IR_MIN_3 = 86765;
const uint32_t IR_MAX_3 = 107756;

const float Age_Omega_1 = -0.04673855434301688;
const float IR_Omega_1 = -0.4197309962951855;
const float HR_Omega_1 = -0.2835012140361256;
const float Bias_1 = -0.8315069733269372;

const float Age_Omega_2 = 0.00850885812967044;
const float IR_Omega_2 = -0.09736722986940606;
const float HR_Omega_2 = -0.012682509022442733;
const float Bias_2 = -0.176179293760974;

const float Age_Omega_3 = 0.31296509375281173;
const float IR_Omega_3 = -0.34460762103122644;
const float HR_Omega_3 = -0.18902828792276205;
const float Bias_3 = -0.42445516595368277;

const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";

String ssid;
String pass;
String ip;
String gateway;

const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt";
String idPath = "/idmicro";
String gluPath = "/glucose";
String cholPath = "/cholestrol";
String uridPath = "/urid";
String heartPath = "/heart";
String alterPath = "/alter";
String timePath = "/timestamp";

unsigned long previousMillis = 0;
const long intervalWifi = 10000;

void startWiFiSetup();
void initWiFi();
void initLittleFS();
void saveDataToJson();
void setupFirebase();
void kirimDataKeFirebase();
void syncTimeFromNTP();
void createDir(fs::FS &fs, const char *path);
String readFile(fs::FS &fs, const char * path);
void appendToFile(fs::FS &fs, const char *path, const char *message);
void writeFile(fs::FS &fs, const char * path, const char * message);
void deleteFile(fs::FS &fs, const char *path);
String readFile2(fs::FS &fs, const char *path);
void copyValuu(uint8_t source, uint8_t& destination);
void copyValue(uint64_t source, uint64_t& destination);
void copyValuee(int source, int& destination);
double minMaxScaling(double value, double min, double max);
uint8_t RE_minMaxScaling(double value, double min, double max);
float REE_minMaxScaling(double value, double min, double max);
double findValue(double value1, double value2, double value3, double omega1, double omega2, double omega3, double omega4);
double sigmoid(double value);
void showMainMenu();
void returnToMainMenu();
void melihat();
void handleSubMenu();
void handleMainMenu();
bool debounceButton(Button& button);
void handleAgeInput(bool button1Pressed, bool button3Pressed);
String getTimestamp();
void updateAgeDisplay();
void updateIdmicroDisplay();
void handleIdmicroInput(bool button1Pressed, bool button3Pressed);
void bacasensorStep();

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");
  initLittleFS();
  createDir(LittleFS, "/data");
  String storedData = readFile2(LittleFS, var2Path);
  if (!particleSensor.begin(Wire)) {
    Serial.println("MAX30105 tidak terdeteksi. Mohon periksa wiring/power.");
    while (1);
  }
  Serial.println("Letakkan jari di sensor dengan tekanan stabil.");
  lcd.init();
  lcd.backlight();
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
    } else if (inSubMenu && !isSettingAge && !isSettingIdmicro && !shouldRunBacaSensor) { 
      returnToMainMenu();
    }
  }

  if (shouldRunBacaSensor) {
    bacasensorStep();
  }
}

void initLittleFS() {
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println("Gagal memasang LittleFS");
    return;
  }
  Serial.println("LittleFS berhasil dipasang");
}

void createDir(fs::FS &fs, const char *path) {
  Serial.printf("Membuat Direktori: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Direktori dibuat");
  } else {
    Serial.println("Gagal membuat direktori");
  }
}

String readFile(fs::FS &fs, const char * path) {
  Serial.printf("Membaca file: %s\r\n", path);
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- Gagal membuka file untuk dibaca");
    return String();
  }
  String fileContent;
  while (file.available()) {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Menulis file: %s\r\n", path);
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("- Gagal membuka file untuk ditulis");
    return;
  }
  if (file.print(message)) {
    Serial.println("- File berhasil ditulis");
  } else {
    Serial.println("- Gagal menulis file");
  }
}

String readFile2(fs::FS &fs, const char *path) {
  Serial.printf("Membaca file: %s\n", path);
  File file = fs.open(path, FILE_READ);
  if (!file) {
    Serial.println("Gagal membuka file untuk dibaca");
    return String();
  }
  String fileContent;
  while (file.available()) {
    fileContent += file.readString();
  }
  file.close();
  return fileContent;
}

void appendToFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Menambahkan ke file: %s\n", path);
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Gagal membuka file untuk ditambahkan");
    return;
  }
  if (file.println(message)) {
    Serial.println("File berhasil ditambahkan");
  } else {
    Serial.println("Gagal menambahkan");
  }
  file.close();
}

void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Menghapus file: %s\r\n", path);
  if (fs.remove(path)) {
    Serial.println("- File dihapus");
  } else {
    Serial.println("- Gagal menghapus");
  }
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

double minMaxScaling(double value, double min, double max) {
  return (value - min) / (max - min);
}

uint8_t RE_minMaxScaling(double value, double min, double max) {
  return static_cast<uint8_t>((value * (max - min)) + min);
}

float REE_minMaxScaling(double value, double min, double max) {
  float scaledValue = (value * (max - min)) + min;
  return round(scaledValue * 10) / 10.0;
}

double findValue(double value1, double value2, double value3, double omega1, double omega2, double omega3, double omega4) {
  return ((value1 * omega1) + (value2 * omega2) + (value3 * omega3) + omega4);
}

double sigmoid(double value) {
  return 1.0 / (1.0 + exp(-value));
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
  Serial.println("Pengukuran direset.");
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
      idmicro = String(idmicroDigits[0]) + String(idmicroDigits[1]) + String(idmicroDigits[2]);
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
    Serial.print("IR: ");
    Serial.println(IR);
    Serial.print("BPM: ");
    Serial.println(HR);
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
  } else {
    Serial.print("IR=");
    Serial.print(irValue);
    Serial.print(", BPM=");
    Serial.print(beatsPerMinute);
    Serial.print(", Avg BPM=");
    Serial.print(beatAvg);
    Serial.print(", Avg IR=");
    Serial.print(irAvg);

    if (!fingerDetected) Serial.print(" (Tidak ada jari terdeteksi)");
    Serial.println();
  }

  if (selesai_baca) {
    scaledHR_1 = minMaxScaling(HR, HR_MIN_1, HR_MAX_1);
    scaledIR_1 = minMaxScaling(IR, IR_MIN_1, IR_MAX_1);
    scaledUmur_1 = minMaxScaling(umurr, Age_MIN, Age_MAX);
    scaledHR_2 = minMaxScaling(HR, HR_MIN_2, HR_MAX_2);
    scaledIR_2 = minMaxScaling(IR, IR_MIN_2, IR_MAX_2);
    scaledUmur_2 = minMaxScaling(umurr, Age_MIN, Age_MAX);
    scaledHR_3 = minMaxScaling(HR, HR_MIN_3, HR_MAX_3);
    scaledIR_3 = minMaxScaling(IR, IR_MIN_3, IR_MAX_3);
    scaledUmur_3 = minMaxScaling(umurr, Age_MIN, Age_MAX);

    findValue_1 = findValue(scaledHR_1, scaledIR_1, scaledUmur_1, HR_Omega_1, IR_Omega_1, Age_Omega_1, Bias_1);
    findValue_2 = findValue(scaledHR_2, scaledIR_2, scaledUmur_2, HR_Omega_2, IR_Omega_2, Age_Omega_2, Bias_2);
    findValue_3 = findValue(scaledHR_3, scaledIR_3, scaledUmur_3, HR_Omega_3, IR_Omega_3, Age_Omega_3, Bias_3);

    sigmoid_1 = sigmoid(findValue_1);
    sigmoid_2 = sigmoid(findValue_2);
    sigmoid_3 = sigmoid(findValue_3);

    GLU = RE_minMaxScaling(sigmoid_1, GLU_MIN, GLU_MAX);
    CHOL = RE_minMaxScaling(sigmoid_2, CHOL_MIN, CHOL_MAX);
    ACD = REE_minMaxScaling(sigmoid_3, ACD_MIN, ACD_MAX);

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
    melihat();
    returnToMainMenu();
  }
}

void saveDataToJson() {
  File file = LittleFS.open(var2Path, FILE_READ);
  DynamicJsonDocument doc(1024);

  if (file) {
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
      Serial.println("Gagal membaca JSON, membuat yang baru.");
      doc["sensor"] = JsonObject();
    }
    file.close();
  } else {
    Serial.println("File tidak ditemukan, membuat yang baru.");
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
    Serial.println("Gagal membuka file JSON untuk menulis.");
    return;
  }

  serializeJson(doc, file);
  file.close();
  Serial.println("Data berhasil disimpan dalam JSON.");
}

String getTimestamp() {
  char dateBuffer[25];
  bool isCentury = false;
  bool is12HourFormat = false;
  bool isPM = false;

  snprintf(dateBuffer, sizeof(dateBuffer), "%04u-%02u-%02uT%02u:%02u:%02u+07:00",
           myRTC.getYear() + 2000,
           myRTC.getMonth(isCentury),
           myRTC.getDate(),
           myRTC.getHour(is12HourFormat, isPM),
           myRTC.getMinute(),
           myRTC.getSecond());
  return String(dateBuffer);
}

void initWiFi() {
  ssid = readFile(LittleFS, ssidPath);
  pass = readFile(LittleFS, passPath);
  if (ssid == "") {
    Serial.println("SSID tidak terdefinisi.");
    return;
  }
  WiFi.mode(WIFI_STA);
  if (ip != "") {
    localIP.fromString(ip.c_str());
    localGateway.fromString(gateway.c_str());
    if (!WiFi.config(localIP, localGateway, subnet)) {
      Serial.println("Gagal mengkonfigurasi STA");
    }
  }

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Menghubungkan ke WiFi...");
  unsigned long currentMillis = millis();
  previousMillis = currentMillis;
  while (WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= intervalWifi) {
      Serial.println("Gagal terhubung.");
      ESP.restart();
    }
  }
  Serial.println(WiFi.localIP());
  Serial.println("Berhasil terhubung!");
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
    Serial.println("Firebase Signup berhasil");
  } else {
    Serial.println("Firebase Signup gagal");
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void kirimDataKeFirebase() {
  if (!Firebase.ready()) {
    Serial.println("Firebase belum siap");
    return;
  }

  File file = LittleFS.open(var2Path, FILE_READ);
  if (!file) {
    Serial.println("Gagal membuka file JSON");
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("Gagal mem-parse JSON: ");
    Serial.println(error.c_str());
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
      Serial.println("Data berhasil disimpan ke " + parentPath);
    } else {
      Serial.print("Gagal mengirim data, alasan: ");
      Serial.println(fbdo.errorReason());
      allDataSent = false;
    }
  }

  if (allDataSent) {
    Serial.println("Data berhasil dikirim dan dihapus");
    deleteFile(LittleFS, var2Path);
  }
}

void startWiFiSetup() {
  Serial.println("Mengatur AP (Access Point)");
  WiFi.softAP("ESP-WIFI-MANAGER", NULL);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Alamat IP AP: ");
  Serial.println(IP);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
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
          Serial.print("SSID diatur ke: ");
          Serial.println(ssid);
          writeFile(LittleFS, ssidPath, ssid.c_str());
        }
        if (p->name() == PARAM_INPUT_2) {
          pass = p->value().c_str();
          Serial.print("Kata sandi diatur ke: ");
          Serial.println(pass);
          writeFile(LittleFS, passPath, pass.c_str());
        }
        if (p->name() == PARAM_INPUT_3) {
          ip = p->value().c_str();
          Serial.print("Alamat IP diatur ke: ");
          Serial.println(ip);
          writeFile(LittleFS, ipPath, ip.c_str());
        }
        if (p->name() == PARAM_INPUT_4) {
          gateway = p->value().c_str();
          Serial.print("Gateway diatur ke: ");
          Serial.println(gateway);
          writeFile(LittleFS, gatewayPath, gateway.c_str());
        }
      }
    }
    request->send(200, "text/plain", "Selesai. ESP akan restart, hubungkan ke router Anda dan buka alamat IP: " + ip);
    delay(3000);
    ESP.restart();
  });
  server.begin();
}

void syncTimeFromNTP() {
  Serial.println("Menyinkronkan waktu dengan NTP...");

  configTime(25200, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Gagal mendapatkan waktu dari NTP");
    return;
  }

  myRTC.setYear(timeinfo.tm_year + 1900 - 2000);
  myRTC.setMonth(timeinfo.tm_mon + 1);
  myRTC.setDate(timeinfo.tm_mday);
  myRTC.setHour(timeinfo.tm_hour);
  myRTC.setMinute(timeinfo.tm_min);
  myRTC.setSecond(timeinfo.tm_sec);

  Serial.println("Waktu berhasil diperbarui dari NTP");
}

void melihat() {
  Serial.println();
  Serial.println("Nilai Mentah");
  Serial.print("IR: ");
  Serial.println(IR);
  Serial.print("HR: ");
  Serial.println(HR);
  Serial.print("Umur: ");
  Serial.println(umur);
  Serial.println();
  Serial.println("Gula Darah");
  Serial.print("IR Min-Max Scaling: ");
  Serial.println(scaledHR_1);
  Serial.print("HR Min-Max Scaling: ");
  Serial.println(scaledIR_1);
  Serial.print("Umur Min-Max Scaling: ");
  Serial.println(scaledUmur_1);
  Serial.print("Nilai Y Gula Darah: ");
  Serial.println(sigmoid_1);
  Serial.print("Prediksi Gula Darah: ");
  Serial.println(GLU);
  Serial.println();
  Serial.println("Asam Urat");
  Serial.print("IR Min-Max Scaling: ");
  Serial.println(scaledHR_3);
  Serial.print("HR Min-Max Scaling: ");
  Serial.println(scaledIR_3);
  Serial.print("Umur Min-Max Scaling: ");
  Serial.println(scaledUmur_3);
  Serial.print("Nilai Y Asam urat: ");
  Serial.println(sigmoid_3);
  Serial.print("Prediksi Asam urat: ");
  Serial.println(ACD);
  Serial.println();
  Serial.println("Kolestrol");
  Serial.print("IR Min-Max Scaling: ");
  Serial.println(scaledHR_2);
  Serial.print("HR Min-Max Scaling: ");
  Serial.println(scaledIR_2);
  Serial.print("Umur Min-Max Scaling: ");
  Serial.println(scaledUmur_2);
  Serial.print("Nilai Y Kolestrol: ");
  Serial.println(sigmoid_2);
  Serial.print("Prediksi Kolestrol: ");
  Serial.println(CHOL);
  String storedData = readFile2(LittleFS, var2Path);
  Serial.println();
  Serial.println(storedData);
}
