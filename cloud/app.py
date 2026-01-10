"""
SmartClass Cloud Backend
---------------------------------------
This Flask application:
- Receives real-time data from ESP32 via MQTT
- Stores attendance, access logs, and classroom usage in Firestore
- Serves a live web dashboard for monitoring
"""

from flask import Flask, jsonify, render_template
import json, threading, time, os
import paho.mqtt.client as mqtt
from google.cloud import firestore
from datetime import datetime

# ================== FLASK APP ==================
app = Flask(__name__)

# ================== FIRESTORE ==================
# Initialize Firestore
os.environ["GOOGLE_APPLICATION_CREDENTIALS"] = "service-account.json"
db = firestore.Client()

# ================== MQTT CONFIG ==================
# HiveMQ Cloud broker for SmartClass
MQTT_BROKER = "c6e3d5ced8d447c4b85b036030ad870c.s1.eu.hivemq.cloud"
MQTT_PORT = 8883
MQTT_USER = "smartclassroom"
MQTT_PASS = "12345Smartclassroom"
MQTT_EVENT_TOPIC = "smartclassroom/classA/event"
MQTT_TELEMETRY_TOPIC = "smartclassroom/classA/telemetry"


# ================== GLOBAL STATE ==================
# These variables store the latest system state and recent logs
latest_system_status = {}
attendance_data = []
access_log_data = []

# Variables for tracking classroom usage over time
last_usage_state = None
last_usage_time = None

# Thread locks to prevent race conditions between MQTT & Flask
usage_lock = threading.Lock()
data_lock = threading.Lock()
attendance_lock = threading.Lock()

# Office hours (in minutes since midnight)
office_start = 8 * 60    # 08:00
office_end = 18 * 60    # 18:00


# ================== HELPER FUNCTIONS ==================
def get_rtc_datetime(payload):
    """
    Extract RTC date and time sent by ESP32.
    Falls back to system time if not available.
    """
    time_data = payload.get("time", {})
   
    if isinstance(time_data, dict):
        rtc_time = time_data.get("time", datetime.now().strftime("%H:%M:%S"))
        rtc_date = time_data.get("date", datetime.now().strftime("%Y-%m-%d"))
    else:
        rtc_time = datetime.now().strftime("%H:%M:%S")
        rtc_date = datetime.now().strftime("%Y-%m-%d")
   
    return rtc_time, rtc_date

def today_date():
    """Returns the current date based on ESP32 RTC"""
    return latest_system_status.get("time", {}).get("date")

def rtc_to_minutes(rtc_time):
    """Convert HH:MM or HH:MM:SS to minutes since midnight"""
    parts = rtc_time.split(":")
    h = int(parts[0])
    m = int(parts[1])
    return h * 60 + m

def get_student_name(uid):
    """
    Look up student name from Firestore using RFID UID (matric ID).
    Used for displaying meaningful names in dashboard and logs.
    """
    try:
        uid_clean = uid.replace(" ", "").upper()
        print(f"Looking up UID: '{uid_clean}'")
       
        matching_students = list(db.collection("students")
            .where("matricid", "==", uid_clean)
            .limit(1)
            .stream())
       
        if matching_students:
            student_data = matching_students[0].to_dict()
            student_name = student_data.get("name", "Unknown")
            print(f"Match found: {student_name}")
            return student_name
       
        print(f"No student found for matricid: {uid_clean}")
    except Exception as e:
        print(f"Error looking up student: {e}")
   
    return "Unknown"

# Update cache from Firestore
def update_cache():
    """
    Load latest attendance and access logs from Firestore into memory.
    This reduces database reads when dashboard requests data.
    """
    global attendance_data, access_log_data
    try:
        with data_lock:
            # Get latest 50 attendance records
            attendance_data = [
                d.to_dict() for d in
                db.collection("attendance")
                  .order_by("createdAt", direction=firestore.Query.DESCENDING)
                  .limit(50).stream()
            ]
            
            # Get latest 50 access log records
            access_log_data = [
                d.to_dict() for d in
                db.collection("access_logs")
                  .order_by("createdAt", direction=firestore.Query.DESCENDING)
                  .limit(50).stream()
            ]
       
        print(f"Cache updated: {len(attendance_data)} attendance, {len(access_log_data)} access")
    except Exception as e:
        print(f"Cache update error: {e}")

# ================== MQTT CALLBACKS ==================
def on_connect(client, userdata, flags, rc):
    """Subscribe to both telemetry and RFID event topics"""
    print(f"MQTT Connected with code: {rc}")
    client.subscribe(MQTT_EVENT_TOPIC)
    client.subscribe(MQTT_TELEMETRY_TOPIC)

def on_message(client, userdata, msg):
    """
    Handles all incoming MQTT messages from ESP32.
    - Telemetry → updates system status & classroom usage
    - Events → logs attendance and access
    """
    global latest_system_status
   
    try:
        payload = json.loads(msg.payload.decode())
       
        # Extract RTC time and date from ESP32
        rtc_time, rtc_date = get_rtc_datetime(payload)

        # ---------- Classroom usage tracking ----------
        system_active = payload.get("system", {}).get("systemActive", 0)

        with usage_lock:
            global last_usage_state, last_usage_time

            current_minutes = rtc_to_minutes(rtc_time)

            # Reset when new day starts
            if last_usage_time is not None and current_minutes < last_usage_time:
                last_usage_time = None
                last_usage_state = None

            # Only count usage during office hours
            if office_start <= current_minutes < office_end:
                if last_usage_state is not None and last_usage_time is not None:
                    duration = max(0, current_minutes - last_usage_time)

                    doc_ref = db.collection("classroom_usage").document(rtc_date)
                    doc = doc_ref.get()

                    if doc.exists:
                        data = doc.to_dict()
                        occupied = data.get("occupiedMinutes", 0)
                        idle = data.get("idleMinutes", 0)
                    else:
                        occupied = 0
                        idle = 0

                    if last_usage_state:
                        occupied += duration
                    else:
                        idle += duration

                    doc_ref.set({
                        "date": rtc_date,
                        "occupiedMinutes": occupied,
                        "idleMinutes": idle,
                        "updatedAt": firestore.SERVER_TIMESTAMP
                    })

            last_usage_state = system_active
            last_usage_time = current_minutes

       
        # Update latest system status for dashboard
        with data_lock:
            latest_system_status = payload
       
        # ---------- Process RFID events ----------
        if msg.topic == MQTT_EVENT_TOPIC:
            rfid = payload.get("rfid", {})
            uid = rfid.get("uid", "").strip()
            scanned = rfid.get("scanned", False)
            attendance = rfid.get("attendance", False)
            classID = payload.get("classID", "").strip()
           
            print(f"Event: matricid='{uid}', scanned={scanned}, attendance={attendance}, class='{classID}'")
           
            # Only process if card was actually scanned
            if not scanned or not uid:
                return
           
            # Clean UID once at the beginning
            uid_clean = uid.replace(" ", "").upper()
           
            # Look up student name (using cleaned UID)
            student_name = get_student_name(uid_clean)

            # Attendance logging
            if attendance and classID:
                # ✅ FIX: Use lock to prevent race condition
                with attendance_lock:
                    try:
                        # Check for duplicate: same UID, same class, same date
                        existing_records = list(db.collection("attendance")
                            .where("matricid", "==", uid_clean)
                            .where("classID", "==", classID)
                            .where("date", "==", rtc_date)
                            .stream())
                       
                        duplicate_count = len(existing_records)
                        print(f"Duplicate check: {duplicate_count} existing records for {classID} on {rtc_date}")
                       
                        if duplicate_count == 0:
                            # No duplicate log attendance
                            db.collection("attendance").add({
                                "matricid": uid_clean,
                                "studentName": student_name,
                                "classID": classID,
                                "timestamp": rtc_time,
                                "date": rtc_date,
                                "createdAt": firestore.SERVER_TIMESTAMP
                            })
                            print(f"Attendance logged: {student_name} - {classID} at {rtc_time}")
                            update_cache()
                        else:
                            print(f"Duplicate ignored: {student_name} already marked for {classID} today")
                           
                    except Exception as e:
                        print(f"Attendance error: {e}")
           
            # Access logging
            elif uid and not attendance:
                try:
                    current_minutes = rtc_to_minutes(rtc_time)

                    if office_start <= current_minutes < office_end:
                        access_result = "Allowed"
                    else:
                        access_result = "Not Allowed"

                    doc_id = f"{uid_clean}_{rtc_date}_{rtc_time}"
                    db.collection("access_logs").document(doc_id).set({
                        "matricid": uid_clean,
                        "studentName": student_name,
                        "classID": classID if classID else "-",
                        "result": access_result,
                        "timestamp": rtc_time,
                        "date": rtc_date,
                        "createdAt": firestore.SERVER_TIMESTAMP
                    })

                    print(f"Access {access_result}: {student_name} at {rtc_time}")
                    update_cache()

                except Exception as e:
                    print(f"Access log error: {e}")


   
    except json.JSONDecodeError as e:
        print(f"JSON error: {e}")
    except Exception as e:
        print(f"MQTT error: {e}")


# ================== MQTT THREAD ==================
def start_mqtt():
    """Initialize and run MQTT client"""
    client = mqtt.Client()
    client.username_pw_set(MQTT_USER, MQTT_PASS)
    client.tls_set()
    client.on_connect = on_connect
    client.on_message = on_message
   
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        print("MQTT client starting...")
        client.loop_forever()
    except Exception as e:
        print(f"MQTT connection failed: {e}")


threading.Thread(target=start_mqtt, daemon=True).start()


# Load initial cache after startup
def load_initial():
    time.sleep(2)  # Wait for MQTT to connect
    update_cache()
    print("Initial cache loaded")


threading.Thread(target=load_initial, daemon=True).start()


# ================== FLASK ROUTES ==================
@app.route("/")
def index():
    """Serve the web dashboard"""
    return render_template("index.html")


@app.route("/data")
def data():
    """Return today's classroom data for dashboard"""
    today = today_date()

    # filter today attendance only (for table + chart)
    today_attendance = [a for a in attendance_data if a.get("date") == today]
    
    # filter today access log (for table)
    today_access = [a for a in access_log_data if a.get("date") == today]

    # attendance summary per class (count + percentage placeholder)
    class_summary = {}
    for a in today_attendance:
        cls = a.get("classID", "Unknown")
        class_summary.setdefault(cls, 0)
        class_summary[cls] += 1

    usage_doc = db.collection("classroom_usage").document(today).get()
    usage_data = usage_doc.to_dict() if usage_doc.exists else {
        "occupiedMinutes": 0,
        "idleMinutes": 0
    }

    return jsonify({
        "system": latest_system_status or {},
        "attendance": today_attendance or [],   
        "attendanceSummary": class_summary,     
        "access": today_access or [], 
        "currentClass": latest_system_status.get("classID", ""),
        "usage": usage_data
    })



@app.route("/health")
def health():
    """Health check endpoint"""
    return jsonify({"status": "ok"}), 200


if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8080))
    app.run(host="0.0.0.0", port=port)
