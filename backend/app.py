from flask import Flask, request, jsonify
from flask_cors import CORS
import firebase_admin
from firebase_admin import credentials, firestore
from datetime import datetime
import pytz
import os

# Initialize Flask app
app = Flask(__name__)
CORS(app)  # Enable CORS for all routes

# firebase configuration
config_path = ""

# Initialize Firebase
try:
    cred = credentials.Certificate("firebase_config.json")
    firebase_admin.initialize_app(cred)
    db = firestore.client()
    print("Firebase connection established successfully")
except Exception as e:
    print(f"Error initializing Firebase: {e}")

# Database helper functions
def get_user_by_uid(nfc_uid):
    """Find a user with UID"""
    users_ref = db.collection('users')
    query = users_ref.where('uid', '==', nfc_uid).limit(1)
    results = query.get()
    
    for user in results:
        return {**user.to_dict(), 'id': user.id}
    return None

def record_attendance(user_id, uid, name, department, device_id="unknown"):
    """Record an attendance """
    now = datetime.now(pytz.UTC)
    today = now.strftime("%Y-%m-%d")
    
    # Check if user already has attendance record for today
    attendance_ref = db.collection('attendance')
    query = attendance_ref.where('user_id', '==', user_id).where('date', '==', today).get()
    
    if len(query) > 0:
        return None, "Attendance already recorded for today"
    
    # Create new attendance record
    attendance_data = {
        'user_id': user_id,
        'uid': uid,
        'name': name,
        'department': department,
        'timestamp': now,
        'date': today,
        'type': 'check_in',  
        'device_id': device_id
    }
    
    attendance_ref = db.collection('attendance').document()
    attendance_ref.set(attendance_data)
    
    # Update user's last_attendance field
    db.collection('users').document(user_id).update({
        'last_attendance': now
    })
    
    return {**attendance_data, 'id': attendance_ref.id}, None

# Basic route to verify the API is running
@app.route("/")
def index():
    return jsonify({
        "status": "online",
        "message": "NFC Attendance API is operational",
        "timestamp": datetime.now(pytz.UTC).strftime("%Y-%m-%d %H:%M:%S")
    })

@app.route("/api/attendance", methods=["POST"])
def process_attendance():
    """Process attendance from ESP32"""
    try:
        data = request.get_json()
        
        # Validate required fields
        if 'uid' not in data:
            return jsonify({"error": "Missing NFC UID"}), 400
            
        nfc_uid = data['uid']
        device_id = data.get('device_id', 'unknown')
        
        # Find user by UID
        user = get_user_by_uid(nfc_uid)
        if not user:
            return jsonify({
                "error": "User not found", 
                "uid": nfc_uid,
                "timestamp": datetime.now(pytz.UTC).strftime("%Y-%m-%d %H:%M:%S")
            }), 404
            
        # Check if user is active
        if user.get('status') != 'active':
            return jsonify({"error": "User is not active"}), 403
            
        # Record attendance
        attendance, error = record_attendance(
            user_id=user['id'],
            uid=nfc_uid,
            name=user['name'],
            department=user['department'],
            device_id=device_id
        )
        
        if error:
            return jsonify({"error": error}), 400
            
        return jsonify({
            "message": "Attendance recorded successfully",
            "user": user['name'],
            "timestamp": datetime.now(pytz.UTC).strftime("%Y-%m-%d %H:%M:%S")
        }), 201
        
    except Exception as e:
        print(f"Error recording attendance: {e}")
        return jsonify({"error": "Attendance recording failed"}), 500

@app.route("/api/users", methods=["GET"])
def list_users():
    """List all registered users"""
    try:
        users_ref = db.collection('users').stream()
        users = []
        for user in users_ref:
            user_data = user.to_dict()
            user_data['id'] = user.id
            users.append(user_data)
            
        return jsonify({"users": users}), 200
    except Exception as e:
        print(f"Error listing users: {e}")
        return jsonify({"error": "Failed to retrieve users"}), 500

@app.route("/api/attendance/daily", methods=["GET"])
def daily_attendance():
    """Get today's attendance records"""
    try:
        today = datetime.now(pytz.UTC).strftime("%Y-%m-%d")
        
        attendance_ref = db.collection('attendance')
        query = attendance_ref.where('date', '==', today).stream()
        
        records = []
        for record in query:
            data = record.to_dict()
            data['id'] = record.id
            records.append(data)
            
        return jsonify({"date": today, "records": records}), 200
    except Exception as e:
        print(f"Error retrieving attendance: {e}")
        return jsonify({"error": "Failed to retrieve attendance records"}), 500

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    app.run(debug=True, host="0.0.0.0", port=port)