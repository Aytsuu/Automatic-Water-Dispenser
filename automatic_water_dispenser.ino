#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

#define trigPin A3
#define echoPin A2
#define relayPin A1

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String correctUsername = "10";
String correctPassword = "11";
String inputUsername = "";
String inputPassword = "";
bool enteringUsername = true;
bool authenticated = false;
int cursorPos = 0;

// Initialize pins
int waterSensorPin = A0;
int ledPins[3] = {13, 12, 11};
bool activateUltrasonic = false;

// Ultrasonic sensor improvements
const int MAX_DISTANCE = 7; // 7 cm maximum detection range
const int SAMPLE_SIZE = 5;   // Number of samples for averaging
unsigned long lastMeasurement = 0;
const unsigned long MEASUREMENT_INTERVAL = 100; // ms between measurements

// Timing variables for detection cycle
unsigned long detectionStartTime = 0;
const unsigned long DETECTION_DURATION = 7000; // 7 seconds detection
const unsigned long COOLDOWN_DURATION = 3000;   // 5 seconds cooldown
bool inDetectionPhase = false;
bool inCooldownPhase = false;
bool objectCurrentlyDetected = false;

void setup() {
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);
  delay(100);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  for(int i = 0; i < 3; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  };
  lcd.init();
  lcd.backlight();
  lcd.print("Enter Username:");
  Serial.begin(9600);
}

void loop() {
  char key = keypad.getKey();

  if(key) {
    handleKeyInput(key);
  }

  if (authenticated) {
    runWaterLevelSensor();

    if (activateUltrasonic) {
      runUltrasonic();
    }
  }
}

void runWaterLevelSensor(){
  int level = calculateWaterLevel();
  Serial.print("Water Level: "); Serial.println(level);

  if(level > 525) {
    // Water rising above threshold
    digitalWrite(ledPins[2], HIGH);
    digitalWrite(ledPins[0], LOW);
    activateUltrasonic = true;
  } 
  else {
    // Water falling below lower threshold
    digitalWrite(ledPins[0], HIGH);
    digitalWrite(ledPins[1], LOW);
    digitalWrite(ledPins[2], LOW);
    activateUltrasonic = false; 
    digitalWrite(relayPin, HIGH);
    // Reset detection phases when water level is low
    inDetectionPhase = false;
    inCooldownPhase = false;
    objectCurrentlyDetected = false;
  }
}

void runUltrasonic() {  
  if (millis() - lastMeasurement >= MEASUREMENT_INTERVAL) {
    lastMeasurement = millis();
    unsigned long distance = getFilteredDistance();
    bool objectDetectedNow = (distance <= MAX_DISTANCE && distance >= 0);
    
    // Check if we're in cooldown phase
    if (inCooldownPhase) {
      if (millis() - detectionStartTime >= COOLDOWN_DURATION) {
        // Cooldown finished, ready for new detection
        inCooldownPhase = false;
        Serial.println("Cooldown finished, ready for new detection");
      }
      return;
    }
    
    // Handle object detection changes
    if (objectDetectedNow && !objectCurrentlyDetected) {
      // New object detected
      objectCurrentlyDetected = true;
      inDetectionPhase = true;
      detectionStartTime = millis();
      digitalWrite(ledPins[1], HIGH);
      delay(300);
      digitalWrite(relayPin, LOW);  // Pump ON
      Serial.println("Pump ON - Object detected");
      delay(500);
    }
    else if (!objectDetectedNow && objectCurrentlyDetected) {
      // Object removed
      objectCurrentlyDetected = false;
      inDetectionPhase = false;
      inCooldownPhase = false;
      detectionStartTime = millis();
      digitalWrite(ledPins[1], LOW);
      digitalWrite(relayPin, HIGH); // Pump OFF
      Serial.println("Pump OFF - Object removed");
    }
    
    // If in detection phase, check if time is up
    if (inDetectionPhase) {
      if (millis() - detectionStartTime >= DETECTION_DURATION) {
        // Maximum detection time reached
        inDetectionPhase = false;
        inCooldownPhase = true;
        detectionStartTime = millis();
        objectCurrentlyDetected = false;
        digitalWrite(ledPins[1], LOW);
        digitalWrite(relayPin, HIGH); // Pump OFF
        Serial.println("Pump OFF - Maximum detection time reached");
      }
    }
  }
}

unsigned long getFilteredDistance() {
  // Take multiple samples and average them
  unsigned long sum = 0;
  int validSamples = 0;
  
  for (int i = 0; i < SAMPLE_SIZE; i++) {
    unsigned long dist = calculateDistance();
    if (dist > 0 && dist <= 400) { // Filter out invalid readings
      sum += dist;
      validSamples++;
    }
    delay(10); // Small delay between samples
  }
  
  return validSamples > 0 ? sum / validSamples : 0;
}

void handleKeyInput(char key) {
  // If authenticated, only * key resets the system
  if (authenticated && key == '*') {
    resetSystem();
    return;
  }   

  // Normal login process when not authenticated
  if (!authenticated) {
    if (key == '#') {
      if (enteringUsername) {
        enteringUsername = false;
        lcd.clear();
        lcd.print("Enter Password:");
        cursorPos = 0;
      } else {
        verifyLogin();
      }
    } else if (key == '*') {
      lcd.clear();
      resetInput();
      lcd.print("Enter Username:");
    } else {
      processCharacterInput(key);
    }
  }
}

void processCharacterInput(char key) {
  if (enteringUsername) {
    inputUsername += key;
    lcd.setCursor(cursorPos, 1);
    lcd.print(key);
  } else {
    inputPassword += key;
    lcd.setCursor(cursorPos, 1);
    lcd.print("*");
  }
  cursorPos++;
}

void verifyLogin() {
  lcd.clear();
  if (inputUsername.equals(correctUsername) && inputPassword.equals(correctPassword)) {
    lcd.print("Access Granted!");
    authenticated = true;
    delay(1000);
    lcd.clear();
    lcd.print("Sensor Activated!");
    lcd.setCursor(0, 1);
    lcd.print("* to reset");
  } else {
    lcd.print("Access Denied!");
    delay(1000);
    lcd.clear();
    lcd.print("Enter Username:");
    resetInput();
  }
}

void resetSystem() {
  authenticated = false;
  activateUltrasonic = false;
  digitalWrite(relayPin, HIGH);
  turnOffLights();
  resetInput();
  lcd.clear();
  lcd.print("System Reset");
  delay(1000);
  lcd.clear();
  lcd.print("Enter Username:");
}

void resetInput() {
  inputUsername = "";
  inputPassword = "";
  enteringUsername = true;
  cursorPos = 0;
}

void turnOffLights() {
  for(int i = 0; i < 3; i++) {
    digitalWrite(ledPins[i], LOW);
  }
  return;
}

unsigned long calculateDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  unsigned long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout (~5m max)
  if (duration == 0) return -1; // No echo detected
  return duration * 0.034 / 2;  // Distance in cm
}

int calculateWaterLevel() {
  const int numReadings = 10;  // Number of samples to average
  int sum = 0;
  
  for(int i = 0; i < numReadings; i++) {
    sum += analogRead(waterSensorPin);
    delay(10);  // Small delay between readings
  }
  return sum / numReadings;
}
