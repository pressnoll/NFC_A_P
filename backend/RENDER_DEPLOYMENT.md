# Render Deployment Guide

## Files Created for Render Deployment

1. **Procfile** - Tells Render how to start your app
2. **runtime.txt** - Specifies Python version
3. **gunicorn.conf.py** - Gunicorn server configuration
4. **requirements.txt** - Updated with gunicorn dependency

## Deployment Steps

### 1. Prepare your Firebase credentials

✅ **Using Environment Variables (Secure)**

Your `firebase_config.json` file contains:
- `project_id`: nfc-attendance-8a156
- `private_key_id`: 2d69e8ee01246c79b2d01c6a22ce2a37d5ccef73
- `client_email`: firebase-adminsdk-fbsvc@nfc-attendance-8a156.iam.gserviceaccount.com
- `client_id`: 105328466030418549630
- `client_x509_cert_url`: https://www.googleapis.com/robot/v1/metadata/x509/firebase-adminsdk-fbsvc%40nfc-attendance-8a156.iam.gserviceaccount.com
- `private_key`: (the long private key - you'll copy this from your local file)

You'll need to set these as environment variables in Render.

### 2. Deploy to Render

1. **Create a new Web Service on Render**
   - Go to https://render.com and sign up/login
   - Click "New +" and select "Web Service"

2. **Connect your repository**
   - Connect your GitHub repository
   - Select the repository containing your backend

3. **Configure the service**
   - **Name**: Choose a name for your service
   - **Region**: Select your preferred region
   - **Branch**: main (or your default branch)
   - **Root Directory**: `backend` (important!)
   - **Runtime**: Python
   - **Build Command**: `pip install -r requirements.txt`
   - **Start Command**: `gunicorn app:app` (or leave empty, Procfile will handle it)

4. **Environment Variables** (Required)
   Set these environment variables in Render:
   - `FIREBASE_PROJECT_ID` = `nfc-attendance-8a156`
   - `FIREBASE_PRIVATE_KEY_ID` = `2d69e8ee01246c79b2d01c6a22ce2a37d5ccef73`
   - `FIREBASE_CLIENT_EMAIL` = `firebase-adminsdk-fbsvc@nfc-attendance-8a156.iam.gserviceaccount.com`
   - `FIREBASE_CLIENT_ID` = `105328466030418549630`
   - `FIREBASE_CLIENT_X509_CERT_URL` = `https://www.googleapis.com/robot/v1/metadata/x509/firebase-adminsdk-fbsvc%40nfc-attendance-8a156.iam.gserviceaccount.com`
   - `FIREBASE_PRIVATE_KEY` = (copy the entire private key from your local firebase_config.json file)
   - `FLASK_ENV` = `production`

5. **Deploy**
   - Click "Create Web Service"
   - Render will automatically deploy your app

### 3. Your API Endpoints

Once deployed, your API will be available at:
- `https://your-service-name.onrender.com/` - Health check
- `https://your-service-name.onrender.com/api/attendance` - POST attendance
- `https://your-service-name.onrender.com/api/users` - GET users
- `https://your-service-name.onrender.com/api/attendance/daily` - GET daily attendance

### 4. Update ESP32 Code

Update your ESP32 code to use the new Render URL instead of localhost:

```cpp
// Replace this
String serverURL = "http://localhost:5000/api/attendance";

// With this (replace your-service-name with actual name)
String serverURL = "https://your-service-name.onrender.com/api/attendance";
```

## Important Notes

- **Free Tier Limitations**: Render's free tier spins down after 15 minutes of inactivity
- **Cold Start**: First request after spin-down may take 30-60 seconds
- **Persistent Storage**: Use Firebase for data storage (which you're already doing)
- **Environment**: The app automatically detects production environment
- **CORS**: Already configured to accept requests from any origin

## Troubleshooting

1. **Build Failures**: Check the build logs in Render dashboard
2. **Runtime Errors**: Check the service logs in Render dashboard  
3. **Firebase Connection**: Verify your credentials and environment variables
4. **Port Issues**: Render automatically assigns port via PORT environment variable

## Testing Your Deployment

Test your endpoints using curl or Postman:

```bash
# Health check
curl https://your-service-name.onrender.com/

# List users
curl https://your-service-name.onrender.com/api/users

# Test attendance (replace with actual UID)
curl -X POST https://your-service-name.onrender.com/api/attendance \
  -H "Content-Type: application/json" \
  -d '{"uid": "your-nfc-uid", "device_id": "test"}'
```
