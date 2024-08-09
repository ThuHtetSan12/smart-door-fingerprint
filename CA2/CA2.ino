#include <Wire.h>
#include <Servo.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>

#define DEBUG true

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
int maxInvalidAttemps = 5;
int freezeTime = 60 * 1000;  // 3 Seconds
int allowedOpenTime = 5 * 1000; // 5 Seconds

String apiKey1 = "82968VT3NE9NJMGY";  //Channel 1: Fingerprint Stauts
String apiKey2 = "7C0IPXV0SKCJLF1Y";  //Channel 2: Door Stauts

void setup() {
  initializeSystem();

  pinMode(servoPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ultrasonicTrig, OUTPUT);
  pinMode(ultrasonicEcho, INPUT);

  Serial.begin(9600);
  ESP01.begin(9600);
  Fingerprint.begin(57600);

  delay(1000);  // Wait a bit for Software Serial to initialize

  initializeESP01();

  lcd.clear();
}

void loop() {
  // Part 1: System Logic
  int status = checkFingerprint();

  if (status == 0) {
    displayMessageLine1("Hello, Welcome!");
    Serial.println("No finger detected");
    delay(1000);  // Delay to avoid rapid looping
  } else if (status == 1) {
    onFingerprintAuthorized();
    checkDoorStatus();
  } else if (status == 2) {
    onFingerprintUnauthorized();
  } else {
    Serial.println("An error occurred");
    delay(1000);  // Delay to avoid rapid looping
  }

  // Part 2: HTTP Server
  timer = millis();
  while (millis() - timer < 2000) {  // Allow 2 seconds for checking HTTP requests
    checkHTTPRequests();
  }
}

void initializeSystem() {
  lcd.init();
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Init System.");
  delay(1000);
  lcd.print(".");
  delay(1000);
  lcd.print(".");
  delay(1000);
}

void initializeESP01() {
  // Reset ESP-01 module
  sendData("AT+RST\r\n", 5000, DEBUG);

  // Set ESP-01 to station mode
  sendData("AT+CWMODE=1\r\n", 2000, DEBUG);

  // Connect to Wi-Fi network
  sendData("AT+CWJAP=\"Galaxy S24+ 5G\",\"jasper666\"\r\n", 10000, true);
  // sendData("AT+CWJAP=\"John@Tan\",\"07@5562402\"\r\n", 10000, true);
  // sendData("AT+CWJAP=\"eee-iot\",\"I0t@eee2024!\"\r\n", 10000, true);
  // sendData("AT+CWJAP=\"GoodBai\",\"bjtismylife\"\r\n", 10000, DEBUG);

  // Enable multiple connections
  sendData("AT+CIPMUX=1\r\n", 2000, true); //server
  // sendData("AT+CIPMUX=0\r\n", 2000, true); //thinkspeak

  // Start server on port 80
  sendData("AT+CIPSERVER=1,80\r\n", 2000, true);

  getIPAddress();
}

void onFingerprintAuthorized() {
  unauthorizedAttempts = 0;
  displayMessageLine1("Access Granted");
  unlockDoor();
  timer = millis();  // Start timer
  sendDataToCloud("channel1", "0");
}

void onFingerprintUnauthorized() {
  unauthorizedAttempts++;
  if (unauthorizedAttempts >= maxInvalidAttemps) {
    displayMessageLine1("Try Again After");
    displayMessageLine2("1 Min");
    triggerAlertSound();
    sendDataToCloud("channel1", "2");
    unauthorizedAttempts = 0;
    delay(freezeTime);
    lcd.clear();
  } else {
    displayMessageLine1("Access Denied");
    triggerAlertSound();
    sendDataToCloud("channel1", "1");
  }
}

void lockDoor() {
  setServoAngle(0);
}

void unlockDoor() {
  triggerSuccessSound();
  setServoAngle(90);
}

void checkDoorStatus() {
  long distance = measureDistance();
  while (distance >= 10) {  // Door is open
    if (millis() - timer > allowedOpenTime) {
      displayMessageLine1("Access Timeout");
      displayMessageLine2("Pls Close Door");
      triggerAlertSound();
      sendDataToCloud("channel2", "1");

      distance = measureDistance();  // Re-measure distance
      if (distance < 10) {           // Door is closed
        lockDoor();
        lcd.clear();
        sendDataToCloud("channel2", "0");
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

void triggerSuccessSound() {
  tone(buzzerPin, 1000, 500);  //pin, frequency, duration
  delay(500);                  // Wait for the tone to finish
  noTone(buzzerPin);           // Stop the tone
}

void triggerAlertSound() {
  tone(buzzerPin, 2000, 2000);  //pin, frequency, duration
  delay(2000);                  // Wait for the tone to finish
  noTone(buzzerPin);            // Stop the tone
}

void displayMessageLine1(String message) {
  Serial.println(message);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message);
}

void displayMessageLine2(String message) {
  Serial.println(message);
  lcd.setCursor(0, 1);
  lcd.print(message);
}

void sendDataToCloud(String channel, String data) {
  // String cmd = "AT+CIPSTART=\"TCP\",\"";
  // cmd += "184.106.153.149";  // Thingspeak.com's IP address
  // cmd += "\",80\r\n";
  // sendData(cmd, 5000, true);

  // String getStr = "";
  // getStr = "GET /update?api_key=";
  // Serial.println("channel:" + channel);
  // if (channel == "channel1") {

  //   getStr += apiKey1;
  //   Serial.println("channel1");
  // } else if (channel == "channel2") {
  //   Serial.println("channel2");
  //   getStr += apiKey2;
  // }
  // getStr += "&field1=";
  // getStr += data;
  // getStr += "\r\n";
  // ESP01.print("AT+CIPSEND=");
  // ESP01.println(getStr.length());
  // Serial.print("AT+CIPSEND=");
  // Serial.println(getStr.length());
  // delay(1000);
  // if (ESP01.find(">")) {
  //   Serial.print(">");
  //   sendData(getStr, 2000, true);
  // } else {
  //   Serial.println("Failed to receive '>'");
  // }
  // sendData("AT+CIPCLOSE\r\n", 2000, true);
}

String sendData(String command, const int timeout, boolean debug) {
  ESP01.listen();
  String response = "";
  ESP01.print(command);
  long int time = millis();
  while ((time + timeout) > millis()) {
    while (ESP01.available()) {
      char c = ESP01.read();
      response += c;
    }
  }
  if (debug) {
    Serial.print(response);
  }
  return response;
}

int checkFingerprint() {
  Fingerprint.listen();
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

void getIPAddress() {
  String response = sendData("AT+CIFSR\r\n", 2000, true);
  int ipStart = response.indexOf("STAIP,\"") + 7;
  int ipEnd = response.indexOf("\"", ipStart);

  String ipAddress = response.substring(ipStart, ipEnd);
  Serial.print("ESP-01 IP Address: ");
  Serial.println(ipAddress);
}

void checkHTTPRequests() {
  ESP01.listen();
  if (ESP01.available()) {
    String request = ESP01.readStringUntil('\n');

    // Check if the request contains "unlockDoor"
    // http://IP_ADDRESS/msg=unlockDoor
    if (request.indexOf("GET /msg=unlockDoor") != -1) {
      onFingerprintAuthorized();
      checkDoorStatus();
    }
  }
}