#include <Wire.h>
#include <Servo.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>

int servoPin = 9;
int buzzerPin = 8;
int ultrasonicEcho = 7;
int ultrasonicTrig = 6;
LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial Fingerprint(4, 5);  // RX, TX
SoftwareSerial ESP01(2, 3);        // RX, TX

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Fingerprint);
Servo myServo;
int unauthorizedAttempts = 0;
unsigned long timer = 0;
int maxInvalidAttemps = 2;
int freezeTime = 3000;
int allowedOpenTime = 5000;

void setup() {
  pinMode(servoPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ultrasonicTrig, OUTPUT);
  pinMode(ultrasonicEcho, INPUT);
  Serial.begin(9600);
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }

  ESP01.begin(57600);

  lcd.init();
  lcd.clear();
  lcd.backlight();

  initializeSystem();
}

void loop() {
  while (true) {
    int status = checkFingerprint();

    if (status == 1) {
      onFingerprintAuthorized();
      break;
    } else if (status == 2) {
      onFingerprintUnauthorized();
    } else if (status == 0) {
      Serial.println("No finger detected");
      delay(1000);  // Delay to avoid rapid looping
    } else {
      Serial.println("An error occurred");
      delay(1000);  // Delay to avoid rapid looping
    }
  }

  checkDoorStatus();
}

void initializeSystem() {
  unauthorizedAttempts = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Init.");
  delay(1000);
  lcd.print(".");
  delay(1000);
  lcd.print(".");
  delay(1000);
  clearMessage();
}

void onFingerprintAuthorized() {
  unauthorizedAttempts = 0;
  displayMessage("Access Granted");
  sendDataToCloud("1", "1");
  triggerSuccessSound();
  unlockDoor();
  timer = millis();  // Start timer
}

void onFingerprintUnauthorized() {
  unauthorizedAttempts++;
  sendDataToCloud("1", "0");
  if (unauthorizedAttempts >= maxInvalidAttemps) {
    displayMessage("Try Again After");
    displayMessage2("1 Min");
    triggerAlertSound();
    sendDataToCloud("3", "1");
    unauthorizedAttempts = 0;
    delay(freezeTime);
    clearMessage();
  } else {
    displayMessage("Access Denied");
    triggerAlertSound();
  }
}

void lockDoor() {
  setServoAngle(0);
}

void unlockDoor() {
  setServoAngle(90);
}

void checkDoorStatus() {
  long distance = measureDistance();

  while (distance >= 10) {  // Door is open
    if (millis() - timer > allowedOpenTime) {
      sendDataToCloud("2", "1");
      displayMessage("Access Timeout");
      displayMessage2("Pls Close Door");
      triggerAlertSound();

      distance = measureDistance();  // Re-measure distance
      if (distance < 10) {           // Door is close
        lockDoor();
        clearMessage();
        return;  // Exit the function
      }
    } else {
      delay(5000);  // Wait for 5 seconds before checking again
    }
  }
  // If the door is found to be closed
  lockDoor();
}

long measureDistance() {
  digitalWrite(ultrasonicTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(ultrasonicTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(ultrasonicTrig, LOW);

  // Read the duration of the pulse from the echo pin
  long duration = pulseIn(ultrasonicEcho, HIGH);

  // Calculate the distance in centimeters
  long distance = duration * 0.034 / 2;

  return distance;
}

void setServoAngle(int angle) {
  myServo.attach(servoPin);
  myServo.write(angle);
  delay(500);  // Wait for the servo to reach the position
  myServo.detach();
}

void activateBuzzer(int frequency, int duration) {
  tone(buzzerPin, frequency, duration);
  delay(duration);    // Wait for the tone to finish
  noTone(buzzerPin);  // Stop the tone
}

void triggerSuccessSound() {
  activateBuzzer(1000, 500);
}

void triggerAlertSound() {
  activateBuzzer(2000, 2000);
}

void displayMessage(String message) {
  Serial.println(message);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message);
}

void displayMessage2(String message) {
  Serial.println(message);
  lcd.setCursor(0, 1);
  lcd.print(message);
}

void clearMessage() {
  lcd.clear();
}

void sendDataToCloud(String field, String data) {
  // ESP01.println(data);  // Send data to ESP-01 for cloud communication
}

int checkFingerprint() {
  finger.begin(57600);
  uint8_t result = getFingerprintID();
  switch (result) {
    case 0:
      return 0;  // No finger detected
    case 1:
      return 1;  // Authorized finger detected
    case 2:
      return 2;  // Unauthorized finger detected
    default:
      return 3;  // An error occurred
  }
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER) {
    return 0;  // No finger detected
  } else if (p != FINGERPRINT_OK) {
    return 3;  // Other error
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    return 3;  // Error converting image
  }

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print("Found ID #");
    Serial.print(finger.fingerID);
    Serial.print(" with confidence of ");
    Serial.println(finger.confidence);
    return 1;  // Fingerprint found and authorized
  } else if (p == FINGERPRINT_NOTFOUND) {
    return 2;  // Fingerprint not found
  } else {
    return 3;  // Other error
  }
}