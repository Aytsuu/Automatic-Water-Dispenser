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

String correctUsername = "1010";
String correctPassword = "1111";
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
const int MAX_DISTANCE = 5; // 5 cm maximum detection range
const int SAMPLE_SIZE = 5;   // Number of samples for averaging
unsigned long lastMeasurement = 0;
const unsigned long MEASUREMENT_INTERVAL = 100; // ms between measurements

void setup() {
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
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
  static unsigned long lastChangeTime = 0;
  const unsigned long debounceTime = 500; // 500ms minimum state duration
  
  int level = calculateWaterLevel();
  Serial.print("Water Level: "); Serial.println(level);

  if(level >= 500) {
    if(millis() - lastChangeTime > debounceTime) {
      // Water rising above threshold
      digitalWrite(ledPins[2], HIGH);
      digitalWrite(ledPins[0], LOW);
      activateUltrasonic = true;
    }
  } 
  else {
    if(millis() - lastChangeTime > debounceTime) {
      // Water falling below lower threshold
      digitalWrite(ledPins[0], HIGH);
      digitalWrite(ledPins[1], LOW);
      digitalWrite(ledPins[2], LOW);
      activateUltrasonic = false; 
      digitalWrite(relayPin, LOW);
    }
  }
}

void runUltrasonic() {  
  static bool objectDetected = false;
  
  // Only take measurements at regular intervals
  if (millis() - lastMeasurement >= MEASUREMENT_INTERVAL) {
    lastMeasurement = millis();
    
    unsigned long distance = getFilteredDistance();
    Serial.print("Object Distance: "); Serial.println(distance);
     
    if(distance >= 0 && distance <= MAX_DISTANCE) {
      if (!objectDetected) {
        objectDetected = true;
        digitalWrite(ledPins[1], HIGH);
        digitalWrite(relayPin, HIGH);
        Serial.println("Object detected - Turning pump ON");
        delay(50);
      }
    } else {
      if (objectDetected) {
        objectDetected = false;
        digitalWrite(ledPins[1], LOW);
        digitalWrite(relayPin, LOW);
        Serial.println("No object detected");
        delay(50);
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
    lcd.print("Water Sensor Active");
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
  digitalWrite(relayPin, LOW);
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
  authenticated = false;
  activateUltrasonic = false;
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
