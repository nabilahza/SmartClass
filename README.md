# SmartClass (Smart Classroom Monitoring and Attendance System)
CPC357 IOT ARCHITECTURE AND SMART APPLICATIONS (PROJECT)

---

## Project Description
**SmartClass** is a functional IoT system for smart classrooms that integrates RFID-based attendance and door access, PIR motion detection, and real-time data visualization through a web dashboard. The system mainly uses Maker Feather AIoT S3, HiveMQ and Google Cloud Platform to demonstrate IoT architecture, cloud integration, and data-driven decision-making, which contributing to which contributes to the United Nations Sustainable Development Goal (SDG) 11 – Sustainable Cities and Communities.

---
## Project Objectives
- To develop a functional Smart Classroom system using IoT technology to enhance safety, energy efficiency, and automation in urban learning environments.
- To integrate real-time sensor data with cloud services for centralized data processing, storage, and analytics.
- To develop a web-based dashboard that visualizes real-time classroom status, attendance statistics and access log to support monitoring and analysis of classroom usage.
  
---

## Key Functionalities of the System
- **RFID-Based Controlled Access**  
  Students enter the classroom using RFID student cards during allowed hours to prevent unauthorized access.

- **RFID-Based Attendance**  
  Student attendance is automatically recorded when RFID cards are scanned during class sessions, with duplicate prevention for accuracy.

- **Smart Energy Management**  
  A PIR motion sensor controls lights and fans based on classroom occupancy to reduce unnecessary energy usage.
  
---
## System Requirements

### Hardware Requirements
- Maker Feather AIoT S3 Microcontroller
- PIR Sensor
- RFID-RC522 Module
- Push Button
- Solenoid Door Lock 
- White LEDs (x2)
- Shaft motor with propeller 
- 1602 LCD Display (Visual Output)
- 5V USB 
- 3.7V 1100mAh Li-Ion Battery (x3)
- Jumper Wires
- Qwiic Cables
- Optocoupler Relay (x2)
- DS3231 Real Time Clock (RTC) Module
- Resistor (x3)

---

### Software Requirements

#### Arduino
- Arduino IDE  
- Required libraries:
  - PubSubClient (Nick O’Leary)  
  - LiquidCrystal_I2C  
  - RTCLib (Adafruit)  
  - MFRC522  
- ESP32 board package (Espressif Systems)

---

### Cloud Services

#### HiveMQ Cloud
- Create a HiveMQ Cloud MQTT broker  
- Create credentials with **Publish & Subscribe** permission  

#### Google Cloud Platform (GCP)
- Create a new project  
- Enable:
  - Firestore  
  - Cloud Run  
  - Cloud Build  
- Create Firestore collections:
  - `students`
  - `attendance`
  - `access_logs`
  - `classroom_usage`
- Create a Service Account:
  - Role: **Cloud Datastore User**
  - Download JSON key  
---
## Setup Guide
1. **Hardware Setup**  
   Connect the RFID reader, PIR sensor, relay, LEDs, fan, RTC, and LCD to the Maker Feather AIoT S3 and ensure proper power supply for Maker Feather AIoT S3 and     solenoid.
2. **Firmware**  
   Upload `firmware/smartclass.ino` to the Maker Feather AIoT S3 using Arduino IDE.

3. **Cloud Backend**  
   Deploy the Flask backend to Google Cloud Run by uploading the following files:
   - `cloud/app.py`
   - `cloud/requirements.txt`
   - `cloud/templates/index.html`

   Place your Google Cloud service account key at:
   cloud/service-account.json  
   (This file is not included in the repository for security reasons)

4. **Dashboard**  
   Access the web dashboard via the Cloud Run URL.


---
## Repository Structure
```
SmartClass/
├── firmware/
│   └── smartclass.ino          # Maker Feather AIoT S3 firmware
│
├── cloud/
│   ├── app.py                  # Flask backend (MQTT, Firestore, REST API)
│   ├── requirements.txt        # Python dependencies
│   ├── templates/
│   │   └── index.html          # Web dashboard UI
│   └── service-account.json    # NOT in GitHub for security purpose (Google Cloud credentials)
│
├── screenshot/                 # System screenshots for documentation
│   └── attendance.png/
│   └── access_logs.png/
│
└── README.md
```
---
## Sustainable Development Goals (SDG) Alignment
This project supports SDG 11: Sustainable Cities and Communities specifically on targets 11.1 and 11.6 through efficient use of resources, enhancing safety and security within urban learning environments. 




  
