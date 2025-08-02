📘 ESP32 SchoolBell – API Documentation
🌐 Base URL

    STA Mode:
    http://schoolbell.local/
    (أو http://<ESP32_IP_ADDRESS>/)

    AP Mode:
    http://192.168.4.1/

🔔 GET /ring
📄 Description

Triggers the bell (activates the relay) for a default duration.
✅ Response

{
  "status": "ringing",
  "duration_ms": 2000
}

🔔 GET /ring?duration=3000
📄 Description

Triggers the bell for a custom duration in milliseconds.
🔸 Query Parameters

    duration (optional): Duration in milliseconds (e.g., 2000)

✅ Response

{
  "status": "ringing",
  "duration_ms": 3000
}

🕐 GET /time
📄 Description

Returns the current date and time from the RTC.
✅ Response

{
  "datetime": "2025-08-02T13:45:00",
  "epoch": 1690974300
}

🧭 POST /set-time
📄 Description

Sets the RTC time.
🔸 Body (JSON)

{
  "datetime": "2025-08-02T13:45:00"
}

✅ Response

{
  "status": "RTC updated",
  "datetime": "2025-08-02T13:45:00"
}

📶 GET /status
📄 Description

Returns WiFi and system status.
✅ Response

{
  "mode": "STA",
  "ip": "192.168.1.55",
  "ssid": "YourWiFi",
  "mdns": "http://schoolbell.local"
}

📥 POST /wifi-config
📄 Description

Updates the stored WiFi credentials and restarts the ESP32.
🔸 Body (JSON)

{
  "ssid": "YourNetwork",
  "password": "YourPassword"
}

✅ Response

{
  "status": "WiFi config saved. Rebooting..."
}

📁 GET /files
📄 Description

Lists available files in SPIFFS (e.g., schedules, logs).
✅ Response

{
  "files": [
    "schedule.json",
    "logs.txt"
  ]
}

🗓️ GET /schedule
📄 Description

Returns the current bell schedule (if stored in SPIFFS).
✅ Response

{
  "schedule": [
    {"time": "08:00", "days": ["Mon", "Tue", "Wed"], "duration": 2000},
    {"time": "13:00", "days": ["Thu"], "duration": 3000}
  ]
}

🗓️ POST /schedule
📄 Description

Updates the bell schedule.
🔸 Body (JSON)

{
  "schedule": [
    {"time": "08:00", "days": ["Mon", "Tue"], "duration": 2000}
  ]
}

✅ Response

{
  "status": "Schedule updated"
}

🧪 GET /test
📄 Description

Basic test endpoint to check server availability.
✅ Response

{
  "status": "online",
  "version": "1.0.0"
}

🛑 GET /reboot
📄 Description

Triggers a device restart.
✅ Response

{
  "status": "restarting..."
}

🗑️ GET /reset
📄 Description

Resets all settings (WiFi, schedule) and restarts ESP32.
✅ Response

{
  "status": "factory reset. Restarting..."
}
