#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

#define trigPin 10
#define echoPin 11

LiquidCrystal_I2C lcd(0x27, 16, 2);

//Keypad setup
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

String correctUsername = "ABCD";
String correctPassword = "1234";
String inputUsername = "";
String inputPassword = "";
bool enteringUsername = true;
bool authenticated = false;
int cursorPos = 0;

// Initialize pins
int duration;
int distance;
int waterSensorPin = A0;
int ledPins[2] = {13, 12};
bool activateUltrasonic = false;

void setup() {
  Serial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  for(int i = 0; i < 2; i++) {
    pinMode(ledPins[i], OUTPUT);
  };
  lcd.init();
  lcd.backlight();
  lcd.print("Enter Username:");
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
  int value = calculateWaterLevel();

  if(value >= 310){
    digitalWrite(ledPins[0], HIGH);
    activateUltrasonic = true;
  } else {
    digitalWrite(ledPins[0], LOW);
    turnOffLights();
    activateUltrasonic = false; 
  }
}

void runUltrasonic() {  
  unsigned long distance = calculateDistance();
  Serial.print("Distance: ");
  Serial.println(distance);
  
  if(distance > 0 && distance <= 30) {  // Valid range: 1cm-400cm
    digitalWrite(ledPins[1], HIGH);  // Object detected → LED ON
  } else {
    digitalWrite(ledPins[1], LOW);   // No object → LED OFF
  }
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
  for(int i = 0; i < 2; i++) {
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
  return analogRead(waterSensorPin);
 }