# SmartClass (IoT Based Smart Classroom)

## Project Description
The **SmartClass** is functional IoT system for smart classrooms that integrates RFID-based attendance, automated door access, PIR motion detection, and real-time data visualization through a web dashboard. The system mainly uses Maker Feather AIoT S3, HiveMQ and Google Cloud Platform to demonstrates IoT architecture, cloud integration, and data-driven decision-making, which contributing to UN Sustainable Development Goal (SDG) 11 – Sustainable Cities and Communities.

---
## Key Features 
- **Automated Occupancy-Based Environment Control**: Automatic fan and lighting control based on classroom occupancy.
- **RFID-Based Smart Attendance and Access Control**: Automatically records student attendance during scheduled class hours while restricting classroom access outside     permitted operating times.
- **Real-Time and Historical Classroom Monitoring Dashboard**: Displays live classroom status along with historical attendance and access logs for monitoring classroom    usage.
  
---
## Prerequisites
Ensure the following libraries and tools are installed:

## Software Requirements
- Arduino IDE
  - Go to the Sketch → Include Library → Manage Libraries
  - Install these libraries:
    - PubSubClient by Nick O'Leary
    - LiquidCrystal_I2C by Martin
    - RTCLib by Adafruit
    - MFRC522 by GithubCommunity
  - Go to the Tools → Boards → Boards Manager
    - Install esp32 by Espressif Systems
   
- HiveMQ Cloud
  - Sign in to HiveMQ Cloud and create a new MQTT broker instance
  - Go to Access Management → Credentials and configure the following parameters:
    - Username: smartclassroom
    - Password: 12345Smartclassroom
    - Permission: Publish and Subscribe 
  Note: Use HiveMQ’s web client can be used to test message flow

- Google Cloud Platfrom (GCP)
  - Sign in to Google Cloud Console and create a new project, e.g., SmartClassroomIoT
  - Enable the following APIs: Cloud Run, Cloud Build, and Firestore
  - Set up Firestore Database by creating collections: students, attendance, access_logs
  - Create a Service Account for the project:
    - Go to IAM & Admin → Service Accounts → Create Service Account
    - Assign the role Cloud Datastore User
    - Create a JSON key and download it as service-account.json
  - In Cloud Run, create a new service with the following configuration:
    - Service name: smart-dashboard
    - Region: asia-southeast1 (Singapore)
    - Runtime: Python 3.14
    - Authentication: Allow public access
    - Billing request: Request-based
    - Service scaling: Auto-scaling
    - Ingress: All
  - Upload the source code to the service, including:
    - app.py
    - requirements.txt
    - service-account.json
    - templates / index.html
  - Save and redeploy the service
  - Open the Cloud Run URL to access the dashboard
  - Check Firestore collections to confirm that data from HiveMQ is being received and stored correctly
    
## Software Requirements
- Maker Feather AIoT S3 Microcontroller
- PIR Sensor
- RFID-RC522 Sensor
- Push Button
- Solenoid Door Lock 
- White LEDs
- Shaft motor with propeller 
- 1602 LCD Display (Visual Output)
- 5V USB 
- 3.7V 1100mAh Li-Ion Battery (x3)
- Jumper Wires
- Optocoupler Relay (x2)
- DS3231 Real Time Clock (RTC) Module
- Resistor 
- 3x18650 Battery Holder

---
## Setup Guide
**1. Hardware Setup**
[insert diagram]

**2. Software Setup**
Download this repository into zip file or copy the coding into the Arduino IDE (tak complete lagi)

---
## Sustainable Development Goals (SDG) Alignment
This project supports SDG 11: Sustainable Cities and Communities specifically on targets 11.1 and 11.6 through efficient use of resources, enhancing safety and security within urban learning environments. 




  
