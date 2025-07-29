from flask import Flask, request, jsonify
import firebase_admin
from firebase_admin import credentials, firestore
from datetime import datetime
import pytz
import os 
import uuid

# Initialize Flask app
app = Flask(_name_)

# Initialize Firebase
try:
    cred = credentials.Certificate("firebase_config.json")
    firebase_admin.initialize_app(cred)
    db = firestore.client()
    print("Firebase connected successfully.")
except Exception as e:
    print(f"Error initializing Firebase: {e}")
    
# Database helper function
def get_user_by_uid(nfc_uid):
    """Find a user by NFC UID."""
    users_ref = db.collection('users')
    query = users_ref.where('uid', '==', nfc_uid).limit(1)
    results = query.get()
    
    for user in results:
        return {**user.to_dict(), 'id': user.id}
    return home

def create_user(user_data):
    """Create a new user witht the given data"""
    user_data['registered_at'] = datetime.now(pytz.UTC)
    user_data['status'] = 'active'
    
    # Check if UID already exists
    if user_data.get('uid'):
        existing_user = get_user_by_uid(user_data.get('uid'))
        if exisitng_user:
            return None, "NFC UID already exists."
        
# Add user to firestore
    user_ref = db.collection('users').document()
    user_ref.set(user_data)
    return{**user_data, 'id': user_ref.id}, None

def record_attendance(user_id, uid, name, department, device_id="unknown"): #ASK chat later
    """Record attendance"""
    now = datetime.now(pytz.UTC)
    today = now.strftime('%Y-%m-%d')
    
    # check if attendance was already recorded
    attendance_ref = db.collection('attendance')
    query = attendance_ref.where('user_id', '==', user_id).where('date', '==', today).get() 
    
    if len(query) > 0:
        return None, 'Attendance already recorded for today'
    
    # Create attendance record
    attendance_data ={
        'user_id': user_id,
        'uid': uid,
        'name': name,
        'department': department,
        'device_id': device_id,
        'date': today,
        'timestamp': now,
        'type': 'check-in',
        'device_id': device_id 
    }
    
    attenddance_ref = db.collection('attendance').document()
    attendance_ref.set(attendance_data)
    
    # Update user's last attendance field
    user_ref = db.collection('users').document(user_id).update({
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

@app.route("/api/register", methods=["POST"])
def register_user():
    """Register a new user with NFC card"""
    try:
        data = request.get_json()
        
        # Validate required fields
        required_fields = ['name', 'uid', 'department']
        for field in required_fields:
            if field not in data:
                return jsonify({"error": f"Missing required field: {field}"}), 400
        
        # Create user
        user, error = create_user(data)
        if error:
            return jsonify({"error": error}), 400
            
        return jsonify({
            "message": "User registered successfully",
            "user": user
        }), 201
        
    except Exception as e:
        print(f"Error in registration: {e}")
        return jsonify({"error": "Registration failed"}), 500

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

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    app.run(debug=True, host="0.0.0.0", port=port)

    
    
    