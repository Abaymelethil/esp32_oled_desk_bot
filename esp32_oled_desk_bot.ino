/*
  ESP32 OLED Desk Bot 🤖✨
  ------------------------------------------------
  An expressive ESP32-powered desktop companion
  with animated emotions, touch interaction,
  clock & weather display.

  Default demo location: New York City (NYC)

  ------------------------------------------------
  Created by: Abay Melethil
  GitHub: https://github.com/Abaymelethil
  License: MIT
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// ================= USER CONFIG =================
// CHANGE THESE BEFORE USE
const char* ssid = "YOUR_WIFI_SSID";         // <--- ENTER YOUR WIFI NAME
const char* password = "YOUR_WIFI_PASSWORD"; // <--- ENTER YOUR WIFI PASSWORD
const char* PLACE_NAME = "New York City";    // Your City Name

// Location for Weather
#define LATITUDE  40.7128
#define LONGITUDE -74.0060

// ================= PINS & SCREEN =================
#define TOUCH_PIN 27 
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= INPUT STATE MACHINE =================
const int DOUBLE_TAP_DELAY = 350; // Max time between taps for double-click
const int LONG_PRESS_TIME = 600;  // Time to hold before triggering Long Press
unsigned long touchStartTime = 0;
unsigned long lastTapTime = 0;
bool isTouching = false;
bool isLongPressing = false;
int tapCount = 0;

// ================= MODE VARIABLES =================
// 0=Alive, 1=Love, 2=Angry, 3=Sad, 4=Dizzy, 5=Clock, 6=Weather
int currentMode = 0; 
unsigned long lastInteractionTime = 0;
bool isSleeping = false;
const unsigned long SLEEP_TIMEOUT = 60000; // 1 Minute to sleep

// ================= INTERACTION FLAGS =================
// Mode 0 (Alive)
bool isPuppySquint = false; 
unsigned long squintEndTime = 0; 
bool isBeingPetted = false;

// Mode 2 (Angry)
bool isRejected = false; 
unsigned long rejectEndTime = 0; 
bool isFurious = false;

// Mode 3 (Sad)
bool isComforted = false; 

// ================= ANIMATION VARS =================
float outdoorTemp = NAN; 
bool weatherReady = false; 
bool timeReady = false; 
unsigned long lastWeatherUpdate = 0; 
unsigned long lastWeatherAttempt = 0; 

// Alive Logic
int lookDirection = 0; 
unsigned long nextLookTime = 0;
bool isYawning = false; 
unsigned long yawnEndTime = 0;
bool hasMidYawned = false; 
bool hasFinalYawned = false; 
bool isDriftingOff = false; 
unsigned long randomMidYawnTime = 0;

// Physics
float tearY = 0; 
float spiralAngle = 0.0;
unsigned long lastBlinkTime = 0; 
bool isBlinking = false; 
int blinkInterval = 2000;
float heartScale = 1.0;

// Face Geometry
const int BASE_EYE_W = 30; 
const int EYE_H = 44; 
const int EYE_Y = 5;
const int EYE_X_L = 16;     
const int EYE_X_R = 82;     
const int EYE_RADIUS = 8;   
const int MOUTH_Y = 42;     

// Transitions
float currentEyeW_L = BASE_EYE_W; float currentEyeW_R = BASE_EYE_W; float currentMouthX = 0;
float currentYawnFactor = 0.0; float currentEyeOpenFactor = 1.0; 
float targetEyeW_L = BASE_EYE_W; float targetEyeW_R = BASE_EYE_W; float targetMouthX = 0;
float targetYawnFactor = 0.0; float targetEyeOpenFactor = 1.0;

const float PAN_SPEED = 12.0; 
const float YAWN_SPEED = 0.08; 
const float SLEEP_SPEED = 0.05; 

// Clouds
unsigned long cloudAnimTimer = 0;
float mainCloudX = -50; float smallCloud1X = 20; float smallCloud2X = 90;

// ================= PROTOTYPES =================
void handleInput(); void triggerModeChange();
void triggerSingleTapAction(); void triggerLongPressAction(); void releaseLongPress();
void drawEyes(); void drawMouth(); void showClock(); void showWeather();
void fetchWeather(); void syncTime(); void animateClouds();
void drawMainCloud(int x, int y); void drawSmallCloud(int x, int y);
void drawAngryFire(int x, int y);
void updateAliveAnimations(); void updateHeartbeat(); void triggerYawn(int duration);
void centerText(const char* txt, int y, int size); void showMessage(const char* msg);

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);

  Wire.begin(21, 22);
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 failed"));
    for(;;);
  }
  display.setTextColor(SSD1306_WHITE);

  // UPDATED: "Waking up" message
  showMessage("Waking up...");
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    syncTime();
    fetchWeather();
  }
  
  randomSeed(analogRead(0));
  lastInteractionTime = millis();
  randomMidYawnTime = random(20000, 40000); 
}

// ================= LOOP =================
void loop() {
  unsigned long currentMillis = millis();

  // 1. INPUT HANDLING
  handleInput();

  // 2. TIMELINE LOGIC (Sleep Sequence)
  if (!isSleeping && currentMode == 0 && !isBeingPetted) {
      unsigned long elapsed = currentMillis - lastInteractionTime;

      if (!hasMidYawned && elapsed > randomMidYawnTime) {
          triggerYawn(2500); hasMidYawned = true;
      }
      if (!hasFinalYawned && elapsed > (SLEEP_TIMEOUT - 6000)) {
          triggerYawn(3500); hasFinalYawned = true;
      }
      if (!isDriftingOff && elapsed > (SLEEP_TIMEOUT - 2000)) {
          isDriftingOff = true; 
          targetEyeOpenFactor = 0.0; targetYawnFactor = 0.0; targetMouthX = 0;          
      }
      if (elapsed > SLEEP_TIMEOUT) {
          isSleeping = true;
      }
  }

  // 3. BACKGROUND TASKS (Non-Blocking)
  if (WiFi.status() != WL_CONNECTED && (currentMillis - lastWeatherAttempt > 10000)) {
     WiFi.reconnect(); lastWeatherAttempt = currentMillis;
  }

  bool needUpdate = (currentMillis - lastWeatherUpdate > 600000); // 10 mins
  bool missingData = (currentMode == 6 && !weatherReady && (currentMillis - lastWeatherAttempt > 5000));
  
  if (WiFi.status() == WL_CONNECTED && (needUpdate || missingData)) {
    fetchWeather(); lastWeatherAttempt = currentMillis; 
  }
  
  // Update Animations
  if (!isSleeping) {
      if (currentMode == 0) updateAliveAnimations();
      if (currentMode == 1) updateHeartbeat(); 
  }

  // 4. RENDER
  display.clearDisplay();

  if (isSleeping) {
      showClock(); 
  } 
  else if (currentMode <= 4) {
      drawEyes();
      drawMouth();
  } 
  else if (currentMode == 5) {
      showClock();
  } 
  else if (currentMode == 6) {
      showWeather();
  }

  display.display();
  delay(20); 
}

// ================= INPUT SYSTEM =================
void handleInput() {
  bool touch = digitalRead(TOUCH_PIN);
  unsigned long now = millis();

  // Rise
  if (touch && !isTouching) {
      isTouching = true;
      touchStartTime = now;
      lastInteractionTime = now; 
      if(isSleeping) {
        isSleeping = false; isDriftingOff = false; targetEyeOpenFactor = 1.0;
        currentMode = 0;
      }
  }

  // Hold
  if (touch && isTouching) {
      if (!isLongPressing && (now - touchStartTime > LONG_PRESS_TIME)) {
          isLongPressing = true;
          triggerLongPressAction();
          tapCount = 0; 
      }
  }

  // Fall
  if (!touch && isTouching) {
      isTouching = false;
      if (isLongPressing) {
          isLongPressing = false;
          releaseLongPress();
      } else {
          tapCount++;
          lastTapTime = now;
      }
  }

  // Timeout Check
  if (!touch && !isLongPressing && tapCount > 0) {
      if (now - lastTapTime > DOUBLE_TAP_DELAY) {
          if (tapCount == 1) triggerSingleTapAction();
          else if (tapCount >= 2) triggerModeChange();
          tapCount = 0; 
      }
  }
}

void triggerModeChange() {
    currentMode++;
    if (currentMode > 6) currentMode = 0;
    
    // Reset Flags
    isPuppySquint = false; isBeingPetted = false;
    isRejected = false; isFurious = false;
    isComforted = false;
    isSleeping = false; isDriftingOff = false;
    hasMidYawned = false; hasFinalYawned = false;
    currentEyeOpenFactor = targetEyeOpenFactor = 1.0;
}

void triggerSingleTapAction() {
    if (currentMode == 0) { isPuppySquint = true; squintEndTime = millis() + 1500; }
    else if (currentMode == 2) { isRejected = true; rejectEndTime = millis() + 1000; }
}

void triggerLongPressAction() {
    if (currentMode == 0) isBeingPetted = true;
    if (currentMode == 2) isFurious = true;
    if (currentMode == 3) isComforted = true; 
}

void releaseLongPress() {
    isBeingPetted = false;
    isFurious = false;
    isComforted = false; 
}

// ================= ANIMATION LOGIC =================
void triggerYawn(int duration) {
    if(isBeingPetted) return; 
    isYawning = true;
    yawnEndTime = millis() + duration;
    targetYawnFactor = 1.0;
}

void updateHeartbeat() {
    float beat = sin(millis() * 0.015);
    if (beat > 0.8) heartScale = 1.2;
    else if (beat > 0.0) heartScale = 1.0 + (beat * 0.2);
    else heartScale = 1.0;
}

void updateAliveAnimations() {
  unsigned long now = millis();

  // Reset temp flags
  if (isPuppySquint && now > squintEndTime) isPuppySquint = false;
  if (isRejected && now > rejectEndTime) isRejected = false;

  // Yawn Logic
  if (isYawning) {
      if (now > yawnEndTime) { isYawning = false; targetYawnFactor = 0.0; } 
      else targetYawnFactor = 1.0;
  } 

  // Look Logic
  bool canLook = !isDriftingOff && currentYawnFactor < 0.1 && !isYawning && !isBeingPetted && !isPuppySquint;
  
  if (canLook) {
      if (now > nextLookTime) { 
          int rand = random(0, 10);
          if (rand < 6) { lookDirection = 0; nextLookTime = now + random(2000, 5000); }
          else if (rand < 8) { lookDirection = -1; nextLookTime = now + 600; }
          else { lookDirection = 1; nextLookTime = now + 600; }
      }
      
      if (lookDirection == 0) {
          targetEyeW_L = BASE_EYE_W; targetEyeW_R = BASE_EYE_W; targetMouthX = 0;
      } else if (lookDirection == -1) {
          targetEyeW_L = BASE_EYE_W - 14; targetEyeW_R = BASE_EYE_W + 14; targetMouthX = -15;              
      } else if (lookDirection == 1) {
          targetEyeW_L = BASE_EYE_W + 14; targetEyeW_R = BASE_EYE_W - 14; targetMouthX = 15;               
      }
  } else {
      targetEyeW_L = BASE_EYE_W; targetEyeW_R = BASE_EYE_W; targetMouthX = 0;
  }

  // Tweening
  auto moveTowards = [](float current, float target, float speed) {
      if (abs(current - target) < speed) return target;
      if (current < target) return current + speed;
      return current - speed;
  };

  currentEyeW_L = moveTowards(currentEyeW_L, targetEyeW_L, PAN_SPEED);
  currentEyeW_R = moveTowards(currentEyeW_R, targetEyeW_R, PAN_SPEED);
  currentMouthX = moveTowards(currentMouthX, targetMouthX, PAN_SPEED);
  currentYawnFactor = moveTowards(currentYawnFactor, targetYawnFactor, YAWN_SPEED);
  currentEyeOpenFactor = moveTowards(currentEyeOpenFactor, targetEyeOpenFactor, SLEEP_SPEED);

  // Blinking
  if (!isDriftingOff && !isBeingPetted && currentYawnFactor < 0.1) { 
    if (now - lastBlinkTime > blinkInterval) {
        isBlinking = true;
        if (now - lastBlinkTime > blinkInterval + 150) { 
            isBlinking = false;
            lastBlinkTime = now;
            blinkInterval = random(800, 3500); 
        }
    }
  }
}

// ================= DRAWING FUNCTIONS =================

void drawHeart(int x, int y, float scale) {
  int r = 8 * scale; int offX = 8 * scale; int offY = 5 * scale; int triH = 16 * scale;
  display.fillCircle(x - offX, y - offY, r, WHITE);
  display.fillCircle(x + offX, y - offY, r, WHITE);
  display.fillTriangle(x - (r*2), y - offY, x + (r*2), y - offY, x, y + triH, WHITE);
}

void drawSingleTear(int x, int y) {
  display.fillCircle(x, y, 2, WHITE);
  display.fillTriangle(x - 1, y, x + 1, y, x, y - 5, WHITE);
}

void drawTears() {
  if (isComforted) return; 
  int startY = 38; 
  for (int i = 0; i < 8; i++) {
    int dropOffset = (int)(tearY + i * 8) % 28; 
    int currentY = startY + dropOffset;
    int wobble = (int)(sin((tearY + i) * 0.4) * 2);
    if(dropOffset < 26) {
        drawSingleTear(31 + wobble, currentY); 
        drawSingleTear(97 - wobble, currentY); 
    }
  }
  tearY += 0.6; 
}

void drawSpiral(int centerX, int centerY, int direction, float rotationOffset) {
  float angle = 0; float radius = 0;
  while (radius < 14) { 
    float effectiveAngle = (angle + rotationOffset) * direction;
    int x = centerX + (int)(cos(effectiveAngle) * radius);
    int y = centerY + (int)(sin(effectiveAngle) * radius);
    display.drawPixel(x, y, WHITE); display.drawPixel(x+1, y, WHITE); 
    angle += 0.4; radius += 0.25; 
  }
}

// Anime Fire Animation
void drawAngryFire(int centerX, int bottomY) {
    int frame = (millis() / 100) % 2; 
    if (frame == 0) {
        display.fillTriangle(centerX - 6, bottomY, centerX + 6, bottomY, centerX, bottomY - 14, WHITE);
        display.fillTriangle(centerX - 9, bottomY - 2, centerX - 5, bottomY - 2, centerX - 7, bottomY - 8, WHITE);
        display.fillTriangle(centerX + 5, bottomY - 2, centerX + 9, bottomY - 2, centerX + 7, bottomY - 8, WHITE);
    } else {
        display.fillTriangle(centerX - 7, bottomY, centerX + 7, bottomY, centerX, bottomY - 16, WHITE);
        display.fillTriangle(centerX - 11, bottomY - 4, centerX - 7, bottomY - 4, centerX - 9, bottomY - 10, WHITE);
        display.fillTriangle(centerX + 7, bottomY - 4, centerX + 11, bottomY - 4, centerX + 9, bottomY - 10, WHITE);
    }
}

void drawEyes() {
  // --- MODE 0: ALIVE ---
  if (currentMode == 0) {
      int wL = (int)(currentEyeW_L + 0.5);
      int wR = (int)(currentEyeW_R + 0.5);
      float h = EYE_H;
      
      if (currentYawnFactor > 0) h = map((int)(currentYawnFactor * 100), 0, 100, EYE_H, 6);
      h = h * currentEyeOpenFactor;
      
      if (isPuppySquint) h = 10; 
      if (isBeingPetted) h = 4; 
      if (isBlinking && !isDriftingOff && !isBeingPetted) h = 4;
      if (h < 2 && currentEyeOpenFactor > 0.1) h = 2;
      
      int finalH = (int)h;
      int l_x = (EYE_X_L + BASE_EYE_W/2) - wL/2; int r_x = (EYE_X_R + BASE_EYE_W/2) - wR/2;
      int l_y = EYE_Y + (EYE_H - finalH)/2; int r_y = EYE_Y + (EYE_H - finalH)/2;

      if (finalH <= 6) {
          display.fillRect(l_x, l_y, wL, finalH, WHITE);
          display.fillRect(r_x, r_y, wR, finalH, WHITE);
      } else {
          display.fillRoundRect(l_x, l_y, wL, finalH, EYE_RADIUS, WHITE);
          display.fillRoundRect(r_x, r_y, wR, finalH, EYE_RADIUS, WHITE);
      }
      return; 
  }

  // --- MODE 2: ANGRY ---
  if (currentMode == 2) {
      int shakeX = 0; int shakeY = 0; int angryOffset = 0;
      int eyebrowHeight = 4; 

      if (isFurious) {
          shakeX = random(-2, 3); shakeY = random(-2, 3);
          eyebrowHeight = 12; // Eyes become slits
          drawAngryFire(64 + shakeX, EYE_Y + 13 + shakeY);
      }
      if (isRejected) angryOffset = -15;

      display.fillRoundRect(EYE_X_L + shakeX + angryOffset, EYE_Y + 18 + shakeY, BASE_EYE_W, EYE_H - 18, 6, WHITE);
      display.fillRect(EYE_X_L + shakeX + angryOffset, EYE_Y + 18 + shakeY, BASE_EYE_W, eyebrowHeight, BLACK); 
      
      display.fillRoundRect(EYE_X_R + shakeX + angryOffset, EYE_Y + 18 + shakeY, BASE_EYE_W, EYE_H - 18, 6, WHITE);
      display.fillRect(EYE_X_R + shakeX + angryOffset, EYE_Y + 18 + shakeY, BASE_EYE_W, eyebrowHeight, BLACK); 
      return;
  }

  switch(currentMode) {
    case 1: drawHeart(EYE_X_L + BASE_EYE_W/2, EYE_Y + EYE_H/2, heartScale); drawHeart(EYE_X_R + BASE_EYE_W/2, EYE_Y + EYE_H/2, heartScale); break;
    case 3: display.fillRect(EYE_X_L, EYE_Y + 28, BASE_EYE_W, 5, WHITE); display.fillRect(EYE_X_R, EYE_Y + 28, BASE_EYE_W, 5, WHITE); drawTears(); break;
    case 4: drawSpiral(EYE_X_L + BASE_EYE_W/2, EYE_Y + EYE_H/2, -1, spiralAngle); drawSpiral(EYE_X_R + BASE_EYE_W/2, EYE_Y + EYE_H/2, 1, spiralAngle); spiralAngle += 0.3; break;
  }
}

void drawMouth() {
  int centerX = 64;
  
  if (currentMode == 0) {
      int x = centerX + (int)(currentMouthX + 0.5); 
      if (isBeingPetted) {
           display.fillCircle(x, MOUTH_Y + 2, 12, WHITE);       
           display.fillCircle(x, MOUTH_Y - 2, 12, BLACK); 
           return;
      }
      if (currentYawnFactor > 0.05) {
          int yW = 16 + (currentYawnFactor * 4); int yH = currentYawnFactor * 18;
          display.fillRoundRect(x - yW/2, MOUTH_Y + 5 - yH/2, yW, yH + 5, 5, WHITE);
          display.fillRoundRect(x - yW/2 + 2, MOUTH_Y + 5 - yH/2 + 2, yW - 4, yH + 5 - 4, 3, BLACK);
      } else {
          display.fillCircle(x, MOUTH_Y + 5, 9, WHITE); display.fillCircle(x, MOUTH_Y + 1, 9, BLACK);  
      }
      return;
  }

  if (currentMode == 2) {
      int shakeX = 0; 
      if (isFurious) shakeX = random(-2, 3);
      int angryOffset = 0;
      if (isRejected) angryOffset = -15;
      display.fillRoundRect(centerX - 12 + shakeX + angryOffset, MOUTH_Y + 10, 24, 4, 1, WHITE);
      return;
  }

  switch(currentMode) {
    case 1: display.fillCircle(centerX, MOUTH_Y + 5, 9, WHITE); display.fillCircle(centerX, MOUTH_Y + 1, 9, BLACK); break;
    case 3: if (isComforted) display.fillRect(centerX - 8, MOUTH_Y + 14, 16, 3, WHITE); else { display.fillCircle(centerX, MOUTH_Y + 12, 9, WHITE); display.fillCircle(centerX, MOUTH_Y + 16, 9, BLACK); } break;
    case 4: for (int x = -14; x < 14; x++) { int yOffset = (int)(sin(x * 0.6) * 3); display.fillCircle(centerX + x, MOUTH_Y + 10 + yOffset, 1, WHITE); } break;
  }
}

// ================= CLOCK & WEATHER =================
void syncTime() { configTime(19800, 0, "pool.ntp.org"); struct tm t; for (int i = 0; i < 10; i++) { if (getLocalTime(&t)) { timeReady = true; return; } delay(100); } }

// UPDATED: "Asking servers for time" message
void showClock() { 
    struct tm t; 
    if (!getLocalTime(&t)) { 
        centerText("Asking servers", 25, 1);
        centerText("for the time...", 35, 1);
        return; 
    } 
    char dateStr[20]; strftime(dateStr, sizeof(dateStr), "%a, %d %b", &t); centerText(dateStr, 0, 1); 
    int hr = t.tm_hour % 12; if (hr == 0) hr = 12; bool pm = t.tm_hour >= 12; 
    char timeStr[6]; sprintf(timeStr, "%02d:%02d", hr, t.tm_min); 
    display.setTextSize(3); int w = strlen(timeStr) * 6 * 3; display.setCursor((128 - w) / 2, 18); display.print(timeStr); 
    centerText(pm ? "PM" : "AM", 52, 1); 
}

void fetchWeather() { HTTPClient http; String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE) + "&longitude=" + String(LONGITUDE) + "&current_weather=true"; http.begin(url); int httpCode = http.GET(); if (httpCode == 200) { DynamicJsonDocument doc(1024); deserializeJson(doc, http.getString()); outdoorTemp = doc["current_weather"]["temperature"]; weatherReady = true; lastWeatherUpdate = millis(); } http.end(); }
void showWeather() { if (!weatherReady) { centerText("Waiting for WiFi...", 30, 1); return; } centerText(PLACE_NAME, 0, 1); animateClouds(); display.setTextSize(2); display.setCursor(26, 46); display.print(outdoorTemp, 1); display.print((char)247); display.print("C"); }
void drawMainCloud(int x, int y) { display.fillCircle(x + 12, y + 10, 8, SSD1306_WHITE); display.fillCircle(x + 24, y + 7, 10, SSD1306_WHITE); display.fillCircle(x + 36, y + 10, 8, SSD1306_WHITE); display.fillCircle(x + 18, y + 14, 9, SSD1306_WHITE); display.fillCircle(x + 30, y + 14, 9, SSD1306_WHITE); }
void drawSmallCloud(int x, int y) { display.fillCircle(x + 6, y + 6, 4, SSD1306_WHITE); display.fillCircle(x + 12, y + 4, 5, SSD1306_WHITE); display.fillCircle(x + 18, y + 6, 4, SSD1306_WHITE); }
void animateClouds() { if (millis() - cloudAnimTimer > 30) { mainCloudX += 0.5; smallCloud1X += 0.5; smallCloud2X += 0.5; if (mainCloudX > 140) mainCloudX = -60; if (smallCloud1X > 140) smallCloud1X = -40; if (smallCloud2X > 140) smallCloud2X = -50; cloudAnimTimer = millis(); } drawMainCloud((int)mainCloudX, 20); drawSmallCloud((int)smallCloud1X, 16); drawSmallCloud((int)smallCloud2X, 30); }
void centerText(const char* txt, int y, int size) { display.setTextSize(size); int w = strlen(txt) * 6 * size; display.setCursor((128 - w) / 2, y); display.print(txt); }
void showMessage(const char* msg) { display.clearDisplay(); centerText(msg, 30, 1); display.display(); }