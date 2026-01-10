
#define MQTT_MAX_PACKET_SIZE 512      //Allow messages up to 512 bytes

/*
 SmartClass (Smart Classroom Monitoring and Attendance System)
 ---------------------------------
 - RFID for access control & attendance
 - PIR for occupancy detection & energy saving
 - RTC for time-based rules (office hours & class)
 - MQTT for sending real-time data to Google Cloud
 - LCD for local user feedback
*/


#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <MFRC522.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

/* ================= LCD ================= */
// LCD used to show time, RFID status, and access messages
LiquidCrystal_I2C lcd(0x27, 16, 2);

/* ================= RTC ================= */
// RTC provides real-world time for office hours and class schedule
RTC_DS3231 rtc;

/* ================= RFID ================= */
// SPI pins for RFID reader (RC522)
#define SDA_PIN 42
#define SCL_PIN 41
#define RST_PIN 15
#define SS_PIN 10
#define SCK_PIN 17
#define MOSI_PIN 8
#define MISO_PIN 18
MFRC522 mfrc522(SS_PIN, RST_PIN);

/* ================= SOLENOID / RELAY ================= */
// Controls the door lock (HIGH = unlocked, LOW = locked)
#define RELAY_PIN 39
unsigned long currentUnlockTime = 3000; // Door unlock duration (ms)
unsigned long relayStartTime = 0;
bool relayActive = false;

/* ================= PIR + LED + FAN ================= */
// PIR detects motion, LEDs and fan are controlled based on occupancy
const int pirPin = 4;
const int whiteLEDPin1 = 9;
const int whiteLEDPin2 = 6;
const int fanPin = 38;

/* ================= Inside door exit button ================= */
// Allows users to unlock door from inside without RFID
const int doorButtonPin = 5;
const unsigned long idleDelay = 10000; // Time before room is considered idle

/* ================= HEARTBEAT ================= */
// Periodic MQTT telemetry to keep dashboard updated
unsigned long lastHeartbeatTime = 0;
const unsigned long HEARTBEAT_INTERVAL = 5000;

/* ================= STATE VARIABLES ================= */
volatile bool motionFlag = false;   // Set by PIR interrupt
bool systemActive = false;          // True if room is occupied
unsigned long lastMotionTime = 0;
unsigned long messageStartTime = 0;
bool showMessage = false;

/* ================= WIFI + MQTT ================= */
// Network credentials and MQTT topics
const char* ssid = "cslab";
const char* password = "aksesg31";
const char* mqtt_server = "c6e3d5ced8d447c4b85b036030ad870c.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "smartclassroom";
const char* mqtt_pass = "12345Smartclassroom";
const char* mqtt_event_topic = "smartclassroom/classA/event";
const char* mqtt_telemetry_topic = "smartclassroom/classA/telemetry";

WiFiClientSecure espClient;
PubSubClient client(espClient);

/* ================= PIR INTERRUPT ================= */
// Triggered whenever motion is detected
// Only sets a flag – main loop handles logic
void IRAM_ATTR motionDetect() {
  motionFlag = true;
}

/* ================= CLASS LOGIC ================= */
// Determines which class is active based on time
// Used to decide whether RFID scan is attendance or just access
String getCurrentClass(int hour) {
  if (hour >= 9 && hour < 10 ) return "CS101";
  if (hour >= 11 && hour < 12) return "CS305";
  if (hour >= 14 && hour < 15) return "CS203";
  if (hour >= 16 && hour < 17) return "CS411";
  return "";
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  
  // Initialize I2C (LCD, RTC) and SPI (RFID)
  Wire.begin(SDA_PIN, SCL_PIN);
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  
  // Initialize LCD display
  lcd.init();
  lcd.backlight();
  
  // Initialize RTC and RFID reader
  rtc.begin();
  mfrc522.PCD_Init();
  
  // Configure I/O pins for actuators and sensors
  pinMode(RELAY_PIN, OUTPUT);                 //Door lock
  pinMode(pirPin, INPUT);                     //Motion sensor
  pinMode(whiteLEDPin1, OUTPUT);              //LED 1
  pinMode(whiteLEDPin2, OUTPUT);              //LED 2
  pinMode(fanPin, OUTPUT);                    //Fan
  pinMode(doorButtonPin, INPUT_PULLUP);       //Push button (inside exit button)

  // Ensure all devices start in OFF / locked state
  digitalWrite(RELAY_PIN, LOW);             
  digitalWrite(fanPin, LOW);
  digitalWrite(whiteLEDPin1, LOW);
  digitalWrite(whiteLEDPin2, LOW);
  
  // Attach interrupt for PIR motion detection
  attachInterrupt(digitalPinToInterrupt(pirPin), motionDetect, RISING);
  
  // Connect to WiFi and configure MQTT
  setupWiFi();
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(512);
  client.setKeepAlive(15);
  
  // Display startup message
  lcd.print("System Ready");
  delay(2000);
  lcd.clear();
}

/* ================= WIFI ================= */
// Connects microcontroller to WiFi network
void setupWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

/* ================= MQTT RECONNECT ================= */
// Reconnects to MQTT broker if connection is lost
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");
    if (client.connect("ESP32_SmartClassroom", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2s");
      delay(2000);
    }
  }
}

/* ================= MQTT SEND ================= */
// Builds and sends JSON data to cloud (telemetry or RFID events)
void sendMQTTStatus(String uid = "", bool attendance = false, String classID = "", bool isTelemetry = false) {
  
  // Ensure MQTT connection
  if (!client.connected()) reconnectMQTT();  
  client.loop();

  // Get current date and time
  DateTime now = rtc.now();

   // Build JSON payload
  String payload = "{";
  payload += "\"deviceID\":\"ESP32_CLASS_A\",";
  payload += "\"classID\":\"" + classID + "\",";

  // System status (fan, lights, door, room state)
  payload += "\"system\":{";
  payload += "\"fan\":" + String(digitalRead(fanPin)) + ",";
  payload += "\"led1\":" + String(digitalRead(whiteLEDPin1)) + ",";
  payload += "\"led2\":" + String(digitalRead(whiteLEDPin2)) + ",";
  payload += "\"doorLocked\":" + String(!relayActive) + ",";
  payload += "\"systemActive\":" + String(systemActive);
  payload += "},";

  // Environment status
  payload += "\"environment\":{";
  payload += "\"motion\":" + String(digitalRead(pirPin));
  payload += "},";

  // RFID data
  payload += "\"rfid\":{";
  payload += "\"uid\":\"" + uid + "\",";
  payload += "\"attendance\":" + String(attendance ? "true":"false") + ",";
  payload += "\"scanned\":" + String(uid != "" ? "true" : "false");
  payload += "},";

  // Time and date
  payload += "\"time\":{";
  String timeStr = "";
  if (now.hour() < 10) timeStr += "0";
  timeStr += String(now.hour());
  timeStr += ":";
  if (now.minute() < 10) timeStr += "0";
  timeStr += String(now.minute());
  payload += "\"time\":\"" + timeStr + "\",";
  payload += "\"date\":\"" + String(now.year()) + "-";
  if (now.month() < 10) payload += "0";
  payload += String(now.month()) + "-";
  if (now.day() < 10) payload += "0";
  payload += String(now.day()) + "\"";
  payload += "}";
  payload += "}";

  Serial.print("Client connected: ");
  Serial.println(client.connected());
  Serial.print("Payload size: ");
  Serial.println(payload.length());
  
  // Publish data to correct MQTT topic
  bool ok;
  if (isTelemetry) {
    ok = client.publish(mqtt_telemetry_topic, payload.c_str());
    Serial.print("Telemetry publish: ");
    Serial.println(ok ? "OK" : "FAIL");
  } else {
    ok = client.publish(mqtt_event_topic, payload.c_str());
    Serial.print("Event publish: ");
    Serial.println(ok ? "OK" : "FAIL");
  }
 
  if (!ok) {
    Serial.print("MQTT State: ");
    Serial.println(client.state());
  }
 
  Serial.println(payload);
}

/* ================= LOOP ================= */
void loop() {
  unsigned long nowMillis = millis();
  DateTime now = rtc.now();
  int hour = now.hour();

  /* ---------- MQTT HEARTBEAT ---------- */
  // Send periodic system data to cloud
  if (nowMillis - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
    lastHeartbeatTime = nowMillis;
    sendMQTTStatus("", false, getCurrentClass(hour), true);
  }

  /* ---------- MQTT CONNECTION ---------- */
  if (!client.connected()) reconnectMQTT();
  client.loop();

  /* ---------- PIR OCCUPANCY & ENERGY CONTROL ---------- */
  if (motionFlag) {         // Motion detected
    motionFlag = false;
    systemActive = true;
    lastMotionTime = nowMillis;
    digitalWrite(fanPin, HIGH);
    digitalWrite(whiteLEDPin1, HIGH);
    digitalWrite(whiteLEDPin2, HIGH);
  }

  if (systemActive && digitalRead(pirPin) == HIGH) {
    lastMotionTime = nowMillis;
  }
  
  // Turn off devices if room is idle
  if (systemActive && (nowMillis - lastMotionTime > idleDelay)) {
    systemActive = false;
    digitalWrite(fanPin, LOW);
    digitalWrite(whiteLEDPin1, LOW);
    digitalWrite(whiteLEDPin2, LOW);
  }

  // Update systemActive based on actual fan/LED status
  // If any device is ON, system is active
  bool anyDeviceOn = (digitalRead(fanPin) == HIGH) ||
                     (digitalRead(whiteLEDPin1) == HIGH) ||
                     (digitalRead(whiteLEDPin2) == HIGH);
  systemActive = anyDeviceOn;

  /* ---------- INSIDE DOOR BUTTON ---------- */
  static bool lastDoorButtonState = HIGH;
  bool currentDoorButtonState = digitalRead(doorButtonPin);
  if (lastDoorButtonState == HIGH && currentDoorButtonState == LOW) {
    relayStartTime = nowMillis;
    relayActive = true;
    digitalWrite(RELAY_PIN, HIGH);
  }
  lastDoorButtonState = currentDoorButtonState;

  /* ---------- RFID ACCESS & ATTENDANCE ---------- */
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {

    // Read card UID
    String uid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      if (mfrc522.uid.uidByte[i] < 0x10) {
        uid += "0";
      }
      uid += String(mfrc522.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();

    Serial.print("UID Scanned: ");
    Serial.println(uid);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan: " + uid);
    
    bool officeHours = (hour >= 8 && hour < 18);   //office hour logic
    String classID = getCurrentClass(hour);
    bool attendanceLogged = false;

    Serial.print("Office Hours: ");
    Serial.println(officeHours);
    Serial.print("Class ID: ");
    Serial.println(classID);

    if (officeHours) {
      relayStartTime = nowMillis;
      relayActive = true;
      digitalWrite(RELAY_PIN, HIGH);

      if (classID != "") {
        attendanceLogged = true;
        lcd.setCursor(0, 1);
        lcd.print("Attendance Recorded");
      } else {
        lcd.setCursor(0, 1);
        lcd.print("Access Logged");
      }
    } else {
      lcd.setCursor(0, 1);
      lcd.print("Door Locked");
    }

    Serial.print("Sending MQTT Event - Attendance: ");
    Serial.println(attendanceLogged);

    // Send RFID event to cloud
    sendMQTTStatus(uid, attendanceLogged, classID, false);

    showMessage = true;
    messageStartTime = nowMillis;

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }

  /* ---------- SOLENOID AUTO-LOCK ---------- */
  if (relayActive && nowMillis - relayStartTime >= currentUnlockTime) {
    digitalWrite(RELAY_PIN, LOW);
    relayActive = false;
  }

  /* ---------- LCD DEFAULT ---------- */
  if (showMessage && nowMillis - messageStartTime > 2000) {
    showMessage = false;
  }

  if (!showMessage) {
    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 0);
    if (now.hour() < 10) lcd.print("0");
    lcd.print(now.hour());
    lcd.print(":");
    if (now.minute() < 10) lcd.print("0");
    lcd.print(now.minute());
    lcd.setCursor(0, 1);
    lcd.print("Scan RFID here      ");
  }

  delay(50);
  yield();
}
