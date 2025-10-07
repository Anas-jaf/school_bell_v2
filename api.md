
## 🔔 Bell Controller API Documentation

### Base URL

```
http://<device-ip>/
```

Example:

```
http://192.168.0.159/
```

---

## 1️⃣ GET `/`

**Description:**
Returns a basic HTML status or welcome page. (Password protected if authentication is enabled.)

**Response Example:**

```html
<h1>Bell Controller</h1>
<p>Status: Ready</p>
```

---

## 2️⃣ POST `/configure`

**Description:**
Sets configuration parameters such as WiFi credentials or other system settings.

**Request Body (JSON):**

```json
{
  "ssid": "MyWiFi",
  "password": "MyPass"
}
```

**Response:**

```json
{ "status": "Configuration saved" }
```

---

## 3️⃣ POST `/settime`

**Description:**
Sets the device’s internal clock.

**Request Body (JSON):**

```json
{
  "hour": 10,
  "minute": 30,
  "second": 0
}
```

**Response:**

```json
{ "status": "Time updated", "time": "10:30:00" }
```

---

## 4️⃣ GET `/getTime`

**Description:**
Returns the current time stored on the device.

**Response:**

```json
{
  "hour": 10,
  "minute": 31,
  "second": 15
}
```

---

## 5️⃣ POST `/setSchedule`

**Description:**
Saves a list of bell schedules (times during the day).

**Request Body (JSON):**

```json
{
  "schedule": [
    {"hour": 8, "minute": 0, "duration": 3},
    {"hour": 9, "minute": 0, "duration": 2}
  ]
}
```

**Response:**

```json
{ "status": "Schedule updated", "count": 2 }
```

---

## 6️⃣ GET `/getSchedule`

**Description:**
Returns all stored bell schedule entries.

**Response:**

```json
{
  "schedule": [
    {"hour": 8, "minute": 0, "duration": 3},
    {"hour": 9, "minute": 0, "duration": 2}
  ]
}
```

---

## 7️⃣ POST `/setRingDays`

**Description:**
Defines which days the bell should ring.

**Request Body (JSON):**

```json
{
  "days": [1, 2, 3, 4, 5]  // 0=Sunday, 6=Saturday
}
```

**Response:**

```json
{ "status": "Ring days updated" }
```

---

## 8️⃣ GET `/getRingDays`

**Description:**
Returns the current active ringing days.

**Response:**

```json
{
  "days": [1, 2, 3, 4, 5]
}
```

---

## 9️⃣ POST `/ring`

**Description:**
Triggers the bell manually.

### Option 1: Fixed duration

```bash
curl -X POST http://192.168.0.159/ring -H "Content-Type: application/json" -d '{"duration": 1500}'
```

**Request Body:**

```json
{ "duration": 1500 }   // milliseconds
```

**Response:**

```json
{ "status": "Rang bell for 1500 ms" }
```

---

### Option 2: Ring pattern (on/off sequence)

```bash
curl -X POST http://192.168.0.159/ring -H "Content-Type: application/json" -d '{"ringPattern": [200, 100, 200, 100, 400]}'
```

**Request Body:**

```json
{
  "ringPattern": [200, 100, 200, 100, 400]
}
```

👉 Each number represents milliseconds — alternating between ON and OFF durations.

**Response:**

```json
{ "status": "Rang bell with pattern" }
```
