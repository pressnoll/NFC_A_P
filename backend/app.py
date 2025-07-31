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

# Global db variable declaration
db = None

# Firebase initialization with better error handling
try:
    cred = credentials.Certificate("firebase_config.json")
    firebase_admin.initialize_app(cred)
    db = firestore.client()
    print("Firebase connection established successfully")
except Exception as e:
    print(f"CRITICAL ERROR initializing Firebase: {e}")
    # Don't exit - we'll check for db before using it

# Validate Firebase connection
if db is None:
    print("WARNING: Firebase database not initialized. Check your credentials.")

# Database helper functions
def get_user_by_uid(nfc_uid):
    """Find a user with NFC UID from registration collection"""
    if db is None:
        print("ERROR: Database not initialized")
        return None
        
    try:
        registration_ref = db.collection('registration')
        query = registration_ref.where('nfc_uid', '==', nfc_uid).limit(1)
        results = query.get()
        
        for user in results:
            return {**user.to_dict(), 'id': user.id}
        return None
    except Exception as e:
        print(f"Error querying user: {e}")
        return None

def record_attendance(user_id, nfc_uid, name, department, device_id="unknown"):
    """Record an attendance event using date-based subcollections"""
    if db is None:
        print("ERROR: Database not initialized")
        return None, "Database connection error"
        
    try:
        now = datetime.now(pytz.UTC)
        today = now.strftime("%Y-%m-%d")
        
        # Reference to today's attendance document
        date_doc_ref = db.collection('attendance_by_date').document(today)
        
        # Create the date document if it doesn't exist
        date_doc = date_doc_ref.get()
        if not date_doc.exists:
            date_doc_ref.set({
                'date': today,
                'count': 0,
                'departments': {}
            })
        
        # Check if user already has attendance record for today
        records_ref = date_doc_ref.collection('records')
        query = records_ref.where('user_id', '==', user_id).get()
        
        if len(query) > 0:
            return None, "Attendance already recorded for today"
        
        # Create new attendance record
        attendance_data = {
            'user_id': user_id,
            'nfc_uid': nfc_uid,
            'name': name,
            'department': department,
            'timestamp': now,
            'date': today,
            'action': 'check_in',
            'device_id': device_id
        }
        
        # Add to the subcollection
        record_ref = records_ref.document()
        record_ref.set(attendance_data)
        
        # Update the date document summary data
        date_doc_ref.update({
            'count': firestore.Increment(1),
            f'departments.{department}': firestore.Increment(1)
        })
        
        # Update user's status in registration
        db.collection('registration').document(user_id).update({
            'status': 'present',
            'timestamp': now
        })
        
        return {**attendance_data, 'id': record_ref.id}, None
    except Exception as e:
        print(f"Error in record_attendance: {e}")
        return None, f"Database error: {str(e)}"

# Routes
@app.route("/")
def index():
    return jsonify({
        "status": "online",
        "message": "NFC Attendance API is operational",
        "firebase": "connected" if db else "disconnected",
        "timestamp": datetime.now(pytz.UTC).strftime("%Y-%m-%d %H:%M:%S")
    })

@app.route("/api/attendance", methods=["POST"])
def process_attendance():
    """Process attendance from ESP32"""
    try:
        # Check database connection first
        if db is None:
            return jsonify({"error": "Database not connected"}), 500
            
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
        
        # Record attendance
        attendance, error = record_attendance(
            user_id=user['id'],
            nfc_uid=nfc_uid,
            name=user['name'],
            department=user.get('department', 'Unknown'),
            device_id=device_id
        )
        
        if error:
            return jsonify({"error": error}), 400
            
        return jsonify({
            "status": "success",
            "message": "Attendance recorded successfully",
            "user": user['name'],
            "timestamp": datetime.now(pytz.UTC).strftime("%Y-%m-%d %H:%M:%S")
        }), 201
        
    except Exception as e:
        print(f"Error recording attendance: {e}")
        return jsonify({"error": "Attendance recording failed"}), 500

@app.route("/api/users", methods=["GET"])
def list_users():
    """Get all registered users"""
    try:
        if db is None:
            return jsonify({"error": "Database not connected"}), 500
            
        users_ref = db.collection('registration')
        users = users_ref.get()
        
        users_list = []
        for user in users:
            data = user.to_dict()
            data['id'] = user.id
            users_list.append(data)
            
        return jsonify({"users": users_list}), 200
        
    except Exception as e:
        print(f"Error retrieving users: {e}")
        return jsonify({"error": "Failed to retrieve users"}), 500

@app.route("/api/attendance/daily", methods=["GET"])
def daily_attendance():
    """Get attendance records for a specific day using new subcollection structure"""
    try:
        if db is None:
            return jsonify({"error": "Database not connected"}), 500
            
        today = request.args.get('date', datetime.now(pytz.UTC).strftime("%Y-%m-%d"))
        
        # Get the date document first
        date_doc_ref = db.collection('attendance_by_date').document(today)
        date_doc = date_doc_ref.get()
        
        if not date_doc.exists:
            return jsonify({"date": today, "records": [], "count": 0}), 200
            
        # Get the summary data
        summary = date_doc.to_dict()
        
        # Get all records from the subcollection
        records_ref = date_doc_ref.collection('records')
        query = records_ref.stream()
        
        records = []
        for record in query:
            data = record.to_dict()
            data['id'] = record.id
            records.append(data)
            
        # Sort records by timestamp
        records.sort(key=lambda x: x.get('timestamp'))
            
        return jsonify({
            "date": today, 
            "count": summary.get('count', 0),
            "departments": summary.get('departments', {}),
            "records": records
        }), 200
        
    except Exception as e:
        print(f"Error retrieving attendance: {e}")
        return jsonify({"error": "Failed to retrieve attendance records"}), 500

@app.route("/api/attendance/migrate", methods=["POST"])
def migrate_attendance_data():
    """Migrate old attendance data to the new structure (admin only)"""
    try:
        # Get all attendance records from old collection
        old_attendance_ref = db.collection('attendance').stream()
        
        migrated = 0
        failed = 0
        
        for old_record in old_attendance_ref:
            try:
                data = old_record.to_dict()
                
                # Skip records without date
                if 'date' not in data:
                    continue
                    
                date = data.get('date')
                user_id = data.get('user_id')
                department = data.get('department', 'Unknown')
                
                # Reference to date document
                date_doc_ref = db.collection('attendance_by_date').document(date)
                
                # Create date document if it doesn't exist
                if not date_doc_ref.get().exists:
                    date_doc_ref.set({
                        'date': date,
                        'count': 0,
                        'departments': {}
                    })
                
                # Add to subcollection
                records_ref = date_doc_ref.collection('records')
                record_ref = records_ref.document(old_record.id)  # Keep same ID for traceability
                record_ref.set(data)
                
                # Update summary counts
                date_doc_ref.update({
                    'count': firestore.Increment(1),
                    f'departments.{department}': firestore.Increment(1)
                })
                
                migrated += 1
                
            except Exception as e:
                print(f"Error migrating record {old_record.id}: {e}")
                failed += 1
        
        return jsonify({
            "status": "success",
            "migrated": migrated,
            "failed": failed
        }), 200
        
    except Exception as e:
        print(f"Migration error: {e}")
        return jsonify({"error": "Migration failed"}), 500

@app.route("/dashboard/attendance", methods=["GET"])
def attendance_dashboard():
    """Get attendance data for a date range"""
    try:
        from datetime import timedelta
        
        # Get date range from query parameters or use default (last 7 days)
        end_date = request.args.get('end', datetime.now(pytz.UTC).strftime("%Y-%m-%d"))
        start_date = request.args.get('start', 
                                     (datetime.strptime(end_date, "%Y-%m-%d") - 
                                      timedelta(days=7)).strftime("%Y-%m-%d"))
        
        # Query for date documents in range
        date_docs = db.collection('attendance_by_date')\
                      .where('date', '>=', start_date)\
                      .where('date', '<=', end_date)\
                      .stream()
        
        results = []
        for date_doc in date_docs:
            summary = date_doc.to_dict()
            results.append({
                'date': summary.get('date'),
                'count': summary.get('count', 0),
                'departments': summary.get('departments', {})
            })
            
        # Sort by date
        results.sort(key=lambda x: x.get('date'))
            
        return jsonify({
            "start_date": start_date,
            "end_date": end_date,
            "days": results
        }), 200
        
    except Exception as e:
        print(f"Dashboard error: {e}")
        return jsonify({"error": "Failed to load dashboard data"}), 500

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    app.run(debug=True, host="0.0.0.0", port=port)