users/
├── [user_id]/
│   ├── uid: String (NFC card UID, e.g., "04:A2:B6:C8:D1")
│   ├── name: String
│   ├── email: String
│   ├── department: String
│   ├── role: String (e.g., "student", "staff", "admin")
│   ├── registered_at: Timestamp
│   ├── last_attendance: Timestamp (optional, for quick lookup)
│   ├── status: String (e.g., "active", "inactive")
│   └── balance: Number (if payment system added later)



attendance/
├── [attendance_id]/
│   ├── user_id: String (reference to users collection)
│   ├── uid: String (NFC card UID for faster queries)
│   ├── name: String (denormalized for reporting efficiency)
│   ├── department: String (denormalized)
│   ├── timestamp: Timestamp (e.g., "2025-07-28T22:29:19Z")
│   ├── date: String (YYYY-MM-DD format, for easier daily queries)
│   ├── type: String (e.g., "check_in", "check_out")
│   └── device_id: String (which ESP32 recorded this entry)


Next Steps (Implementation Order)
Flask Backend - Setup Database Connection

Configure Firestore connection
Create helper functions for user and attendance operations
Registration API Endpoint

Endpoint for web form to register users
Handle the NFC UID collection during registration
Attendance API Endpoint for ESP32

Receive UID from ESP32
Look up user by UID
Record attendance with timestamp
Admin Dashboard APIs

List users
View attendance records
Generate reports