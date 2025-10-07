#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <RTClib.h>
#include <FS.h>
#include <Wire.h>

#define EEPROM_SIZE 128
#define RING_PIN 0  // GPIO5 = D1

ESP8266WebServer server(80);
RTC_DS1307 rtc;

String ssid = "";
String password = "";
IPAddress staticIP;
bool useStaticIP = false;

// ملفات التخزين
const char* scheduleFile = "/schedule.json";
const char* ringDaysFile = "/ringdays.json";
int lastCheckedMinute = -1;
const char* http_username = "YWRtaW4=";
const char* http_password = "MTIzNA==";

// أيام دق الجرس (7 خانات لكل يوم من الأحد=0 إلى السبت=6)
bool ringDays[7] = {true,true,true,true,true,false,false};  // افتراضي: الأحد-الخميس مفعلة

// --- EEPROM ---
void saveCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);

  int index = 0;
  for (int i = 0; i < ssid.length(); i++) EEPROM.write(index++, ssid[i]);
  EEPROM.write(index++, '|');
  for (int i = 0; i < password.length(); i++) EEPROM.write(index++, password[i]);

  EEPROM.write(index++, '|');
  EEPROM.write(index++, useStaticIP ? 1 : 0);
  if (useStaticIP) {
    for (int i = 0; i < 4; i++) EEPROM.write(index++, staticIP[i]);
  }
  EEPROM.commit();
}

void loadCredentials() {
  char data[EEPROM_SIZE];
  for (int i = 0; i < EEPROM_SIZE; i++) data[i] = EEPROM.read(i);
  String content = String(data);

  int firstSep = content.indexOf('|');
  if (firstSep < 0) return;

  ssid = content.substring(0, firstSep);

  int secondSep = content.indexOf('|', firstSep + 1);
  if (secondSep < 0) return;
  password = content.substring(firstSep + 1, secondSep);

  int thirdSep = content.indexOf('|', secondSep + 1);
  if (thirdSep < 0) return;

  useStaticIP = (EEPROM.read(secondSep + 1) == 1);
  if (useStaticIP) {
    for (int i = 0; i < 4; i++) {
      staticIP[i] = EEPROM.read(secondSep + 2 + i);
    }
  }
}

// --- WiFi ---
bool connectToWiFi() {
  if (ssid.length() == 0 || password.length() == 0) return false;

  WiFi.mode(WIFI_STA);
  if (useStaticIP) {
    Serial.print("Setting static IP to: ");
    Serial.println(staticIP);
    IPAddress gateway = WiFi.gatewayIP();
    IPAddress subnet = WiFi.subnetMask();
    WiFi.config(staticIP, gateway, subnet);
  }
  WiFi.begin(ssid.c_str(), password.c_str());

  Serial.print("Connecting to WiFi");
  for (int i = 0; i < 20; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected.");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      if (MDNS.begin("timer")) {
        Serial.println("mDNS responder started: http://timer.local");
      } else {
        Serial.println("Error setting up MDNS responder!");
      }

      return true;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connection failed.");
  return false;
}

void startAccessPoint() {
  Serial.println("Starting Access Point Mode...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP_Config", "12345678");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

// --- SPIFFS ---
bool saveStringToFS(const char* filename, const String& json) {
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount FS");
    return false;
  }
  File f = SPIFFS.open(filename, "w");
  if (!f) {
    Serial.println("Failed to open file for writing");
    return false;
  }
  f.print(json);
  f.close();
  Serial.println(String(filename) + " saved.");
  return true;
}

String loadStringFromFS(const char* filename) {
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount FS");
    return "{}";
  }
  if (!SPIFFS.exists(filename)) {
    Serial.println(String(filename) + " not found");
    return "{}";
  }
  File f = SPIFFS.open(filename, "r");
  if (!f) {
    Serial.println("Failed to open file");
    return "{}";
  }
  String content = f.readString();
  f.close();
  return content;
}

// --- Handlers ---

void handleRoot() {
  
  if (!server.authenticate(http_username, http_password)) {
    return server.requestAuthentication(); // تظهر نافذة تسجيل دخول
  }

  server.send(200, "text/html", R"rawliteral(
    <h1>ESP Bell Controller</h1>

    <h2>1️⃣ WiFi Configuration</h2>
    <form action="/configure" method="post">
      SSID: <input name="ssid"><br>
      Password: <input name="password" type="password"><br>
      Use Static IP: <input type="checkbox" name="staticIP" id="staticIP" onchange="toggleIPFields()"><br>
      <div id="ipFields" style="display:none;">
        Static IP: <input name="ip0" size="3"> . <input name="ip1" size="3"> . <input name="ip2" size="3"> . <input name="ip3" size="3"><br>
      </div>
      <input type="submit" value="Save WiFi">
    </form>

    <h2>2️⃣ RTC Time</h2>
    <input type="datetime-local" id="datetime">
    <button onclick="setTime()">Set Time</button>
    <p id="timeResponse"></p>
    <button onclick="getTime()">Get Current Time</button>
    <p id="currentTime"></p>

    <h2>3️⃣ Ring the Bell</h2>
    <button onclick="ringBell(500)">Ring 0.5s</button>
    <button onclick="ringBell(1000)">Ring 1s</button>
    <input type="text" id="customDuration" placeholder="Milliseconds">
    <button onclick="ringCustom()">Ring Custom</button>

    <script>
      function toggleIPFields() {
        var ipFields = document.getElementById('ipFields');
        ipFields.style.display = document.getElementById('staticIP').checked ? 'block' : 'none';
      }

      function setTime() {
        var dt = document.getElementById('datetime').value;
        if (!dt) { alert('Select a date/time'); return; }
        fetch('/settime', {
          method: 'POST',
          headers: {'Content-Type':'application/x-www-form-urlencoded'},
          body: 'datetime=' + encodeURIComponent(dt)
        }).then(resp => resp.text()).then(t => document.getElementById('timeResponse').innerText = t);
      }

      function getTime() {
        fetch('/getTime').then(resp => resp.json()).then(data => {
          document.getElementById('currentTime').innerText = data.datetime;
        });
      }

      function ringBell(ms) {
        fetch('/ring', {
          method:'POST',
          headers:{'Content-Type':'application/json'},
          body: JSON.stringify({duration: ms})
        }).then(resp=>resp.json()).then(d=>alert(JSON.stringify(d)));
      }

      function ringCustom() {
        var ms = parseInt(document.getElementById('customDuration').value);
        if (isNaN(ms) || ms <= 0) { alert('Enter valid ms'); return; }
        ringBell(ms);
      }
    </script>
  )rawliteral");
}

void handleConfigure() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  if (server.hasArg("ssid") && server.hasArg("password")) {
    ssid = server.arg("ssid");
    password = server.arg("password");
    useStaticIP = server.hasArg("staticIP");

    if (useStaticIP) {
      int ip0 = server.arg("ip0").toInt();
      int ip1 = server.arg("ip1").toInt();
      int ip2 = server.arg("ip2").toInt();
      int ip3 = server.arg("ip3").toInt();
      staticIP = IPAddress(ip0, ip1, ip2, ip3);
    }

    saveCredentials();

    server.send(200, "text/html", "<h1>Saved. Restarting...</h1>");
    delay(2000);
    ESP.restart();
    return;
  }

  server.send(400, "text/plain", "Bad Request");
}

void handleSetTime() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  if (!server.hasArg("datetime")) {
    server.send(400, "text/plain", "Missing 'datetime' field");
    return;
  }

  String datetimeStr = server.arg("datetime");
  Serial.println("Received datetime: " + datetimeStr);

  if (datetimeStr.length() < 19) {
    server.send(400, "text/plain", "Invalid datetime format");
    return;
  }

  int year = datetimeStr.substring(0, 4).toInt();
  int month = datetimeStr.substring(5, 7).toInt();
  int day = datetimeStr.substring(8, 10).toInt();
  int hour = datetimeStr.substring(11, 13).toInt();
  int minute = datetimeStr.substring(14, 16).toInt();
  int second = datetimeStr.substring(17, 19).toInt();

  DateTime dt(year, month, day, hour, minute, second);
  rtc.adjust(dt);

  server.send(200, "text/plain", "RTC time set to " + datetimeStr);
}

void handleSetSchedule() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Body required");
    return;
  }

  String body = server.arg("plain");
  Serial.println("Received schedule JSON:");
  Serial.println(body);

  StaticJsonDocument<2048> doc;
  auto error = deserializeJson(doc, body);
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  if (saveStringToFS(scheduleFile, body)) {
    server.send(200, "application/json", "{\"status\":\"Schedule saved\"}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to save schedule\"}");
  }
}

void handleGetSchedule() {
  String scheduleJson = loadStringFromFS(scheduleFile);
  server.send(200, "application/json", scheduleJson);
}

void handleSetRingDays() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Body required");
    return;
  }
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, body)) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  JsonArray arr = doc["activeDays"].as<JsonArray>();
  if (arr.isNull()) {
    server.send(400, "application/json", "{\"error\":\"Missing activeDays array\"}");
    return;
  }

  memset(ringDays, 0, sizeof(ringDays));
  for (int i=0; i < arr.size(); i++) {
    int d = arr[i];
    if (d >=0 && d <7) ringDays[d] = true;
  }

  saveStringToFS(ringDaysFile, body);
  server.send(200, "application/json", "{\"status\":\"Ring days saved\"}");
}

void handleGetRingDays() {
  String s = loadStringFromFS(ringDaysFile);
  server.send(200, "application/json", s);
}

void ringForDuration(unsigned long durationMs) {
  Serial.println("the bell is ringing");
  digitalWrite(RING_PIN, HIGH);
  delay(durationMs);
  digitalWrite(RING_PIN, LOW);
}

void ringWithPattern(JsonArray pattern) {
  Serial.println("🔔 Ring bell with pattern (ms)");
  for (size_t i = 0; i < pattern.size(); i++) {
    unsigned long segment = pattern[i].as<unsigned long>(); // الآن هي بالميلي ثانية مباشرة
    if (i % 2 == 0) {
      // تشغيل الجرس
      digitalWrite(RING_PIN, HIGH);
    } else {
      // إيقاف الجرس
      digitalWrite(RING_PIN, LOW);
    }
    delay(segment);
  }
  // تأكد أن الجرس مطفأ بعد الانتهاء
  digitalWrite(RING_PIN, LOW);
}

void handleRing() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Body required");
    return;
  }

  String body = server.arg("plain");
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  if (doc.containsKey("ringPattern")) {
    JsonArray pattern = doc["ringPattern"].as<JsonArray>();
    ringWithPattern(pattern);
    server.send(200, "application/json", "{\"status\":\"Rang bell with pattern\"}");
  } else {
    int durationMs = doc["duration"] | 500; // default 500 ms
    ringForDuration(durationMs);

    server.send(200, "application/json",
                "{\"status\":\"Rang bell for " + String(durationMs) + " ms\"}");
  }
}

void handleGetTime() {
  DateTime now = rtc.now();

  char buf[25];
  sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d", 
          now.year(), now.month(), now.day(),
          now.hour(), now.minute(), now.second());

  server.send(200, "application/json", "{\"datetime\":\"" + String(buf) + "\"}");
}

void setupServer() {
  server.on("/", handleRoot);
  server.on("/configure", HTTP_POST, handleConfigure);
  server.on("/settime", HTTP_POST, handleSetTime);
  server.on("/getTime", HTTP_GET, handleGetTime);
  server.on("/setSchedule", HTTP_POST, handleSetSchedule);
  server.on("/getSchedule", HTTP_GET, handleGetSchedule);
  server.on("/setRingDays", HTTP_POST, handleSetRingDays);
  server.on("/getRingDays", HTTP_GET, handleGetRingDays);
  server.on("/ring", HTTP_POST, handleRing);
  server.begin();
}

void parseRingPatternAndRing(String pattern) {
  Serial.println("ringing the scheduled bell");
  int segments[10]; // دعم حتى 10 أجزاء
  int count = 0;
  char *token = strtok((char *)pattern.c_str(), "-");

  while (token && count < 10) {
    segments[count++] = atoi(token);
    token = strtok(NULL, "-");
  }

  for (int i = 0; i < count; i++) {
    if (i % 2 == 0) {
      digitalWrite(RING_PIN, HIGH);
    } else {
      digitalWrite(RING_PIN, LOW);
    }
    delay(segments[i] * 1000);
  }

  digitalWrite(RING_PIN, LOW);
}

void checkAndRing(DateTime now) {
  Serial.println("🔍 checkAndRing called: " + String(now.hour()) + ":" + String(now.minute()));
  int currentWeekDay = now.dayOfTheWeek();

  bool ringDays[7] = {false, false, false, false, false, false, false};

  String ringDaysJson = loadStringFromFS(ringDaysFile);
  StaticJsonDocument<128> ringDoc;

  if (!deserializeJson(ringDoc, ringDaysJson)) {
    JsonArray arr = ringDoc["activeDays"].as<JsonArray>();
    for (int i = 0; i < arr.size(); i++) {
      int dayIndex = arr[i];
      if (dayIndex >= 0 && dayIndex < 7) {
        ringDays[dayIndex] = true;
      }
    }
  } else {
    Serial.println("Failed to parse ringdays.json");
    return;
  }

  if (!ringDays[currentWeekDay]) {
    return;
  }

  String json = loadStringFromFS(scheduleFile);

  // إصلاح السلسلة: إزالة علامات الاقتباس الخارجية إن وجدت
  if (json.startsWith("\"") && json.endsWith("\"")) {
    json = json.substring(1, json.length() - 1);
    json.replace("\\\"", "\"");  // إزالة علامات الهروب
  }

  StaticJsonDocument<2048> doc;

  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print("❌ Failed to parse JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Serial.println("📄 Schedule content: " + json);

  JsonArray schedule = doc["schedule"].as<JsonArray>();

  if (!schedule || schedule.size() == 0) {
    Serial.println("⚠️ 'schedule' is empty or not found in JSON");
    return;
  }
  char currentTimeStr[6];
  sprintf(currentTimeStr, "%02d:%02d", now.hour(), now.minute());

  for (JsonObject entry : schedule) {
    const char* startStr = entry["start"];
    const char* endStr = entry["end"];
    const char* ringStr = entry["ring"];
    const char* daysStr = entry["days"];

    if (strlen(daysStr) >= 7 && daysStr[currentWeekDay] == '1') {
        if (strcmp(currentTimeStr, startStr) == 0) {
          Serial.println("🔔 Matching schedule (start) found, ringing: " + String(ringStr));
          parseRingPatternAndRing(ringStr);
          delay(60000); // ✅ تأخير فقط عند التطابق
          break;
        }

        if (strcmp(currentTimeStr, endStr) == 0) {
          Serial.println("🔕 Matching schedule (end) found, ringing: " + String(ringStr));
          parseRingPatternAndRing(ringStr);
          delay(60000); // ✅ تأخير فقط عند التطابق
          break;
        }
      }
  }
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  pinMode(RING_PIN, OUTPUT);
  digitalWrite(RING_PIN, LOW);
  // استخدم SDA = GPIO2 (D4), و SCL = GPIO0 (D3)
  Wire.begin(2, 3);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  loadCredentials();

  if (connectToWiFi()) {
    Serial.println("Connected to WiFi in STA Mode.");
    Serial.println(WiFi.localIP());
  } else {
    startAccessPoint();
  }

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount SPIFFS");
  }

  setupServer();
}

void loop() {
  server.handleClient();
  MDNS.update();
  DateTime now = rtc.now();

  if (now.minute() != lastCheckedMinute) {
    lastCheckedMinute = now.minute();
    checkAndRing(now);
  }

}
