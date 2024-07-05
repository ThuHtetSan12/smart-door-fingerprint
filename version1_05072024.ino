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
// SoftwareSerial Fingerprint(4, 5);  // RX, TX
SoftwareSerial mySerial(4, 5);
SoftwareSerial ESP01(2, 3);        // RX, TX

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial,1337); 
Servo myServo;
int unauthorizedAttempts = 0;
unsigned long timer = 0;

void setup() {
  pinMode(servoPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ultrasonicTrig, OUTPUT);
  pinMode(ultrasonicEcho, INPUT);
  Serial.begin(9600);
  finger.begin(57600);
  ESP01.begin(9600);

  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }

  lcd.init();
  lcd.clear();
  lcd.backlight();

  initializeSystem();
}

void loop() {
  // if (checkFingerprint()) {
  // onFingerprintAuthorized();
  // } else {
  //   onFingerprintUnauthorized();
  // }

  // checkDoorStatus();


  if (true) {
    onFingerprintAuthorized();
  } else {
    onFingerprintUnauthorized();
  }

  checkDoorStatus();
  displayMessage("END");
  delay(999999);
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
}

void onFingerprintAuthorized() {
  unauthorizedAttempts = 0;
  displayMessage("Access Granted");
  sendDataToCloud("Access Granted");
  triggerSuccessSound();
  unlockDoor();
  timer = millis();  // Start timer
}

void onFingerprintUnauthorized() {
  unauthorizedAttempts++;
  if (unauthorizedAttempts > 5) {
    displayMessage("Try Again After 1 Min");
    triggerAlertSound();
    sendDataToCloud("Too Many Attempts");
    unauthorizedAttempts = 0;
    delay(60 * 1000);  // Freeze for 1 minute
  } else {
    displayMessage("Access Denied");
    triggerAlertSound();
    sendDataToCloud("Unauthorized Access");
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

  while (distance >= 10) {              // Door is open
    if (millis() - timer > 6 * 1000) {  // Check if 1 minute has passed
      sendDataToCloud("Access Timeout");
      displayMessage("Access Timeout");
      displayMessage2("Pls Close Door");
      triggerAlertSound();

      distance = measureDistance();  // Re-measure distance
      if (distance < 10) {           // Door is close
        lockDoor();
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


void sendDataToCloud(String data) {
  // ESP01.println(data);  // Send data to ESP-01 for cloud communication
}
