#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <DNSServer.h>

// Access Point credentials (matching FuelCI pattern)
const char* ap_ssid = "ChristmasLantern";
const char* ap_password = "12345678";  // 8+ characters required

// Fixed IP configuration (same as FuelCI)
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

DNSServer dnsServer;
const byte DNS_PORT = 53;

// LED Strip configuration - 6 CIRCLES
#define NUM_CIRCLES 6
#define LEDS_PER_CIRCLE 50
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define MAX_SEQUENCE_LENGTH 10

// Pin definitions for 6 circles
const int LED_PINS[NUM_CIRCLES] = {2, 4, 16, 17, 18, 19};

// LED arrays for each circle
CRGB leds[NUM_CIRCLES][LEDS_PER_CIRCLE];

// Circle configuration
struct CircleInfo {
  bool enabled;
  int currentPattern;
  int ledCount;
  int patternSequence[MAX_SEQUENCE_LENGTH];
  int sequenceLength;
  int currentSequenceIndex;
  bool isSequenceMode;
  unsigned long lastSequenceUpdate;
};

CircleInfo circles[NUM_CIRCLES];

WebServer server(80);

// Global variables
int globalCurrentPattern = 0; // Start with pattern 0
CRGB globalCurrentColor = CRGB::White;
bool isRunning = true;
bool isAutoPilot = true;
bool isShowcaseMode = true; // New showcase mode flag
bool hasAppConnected = false;
unsigned long lastUpdate = 0;
unsigned long lastAutoPilotUpdate = 0;
unsigned long lastShowcaseUpdate = 0; // For showcase pattern switching
unsigned long lastAppContact = 0;
int autoPilotHue = 0;
int showcasePatternIndex = 0; // Current pattern in showcase
int globalBrightness = 220;

const unsigned long AUTO_PILOT_TIMEOUT = 2 * 60 * 1000;
const unsigned long SEQUENCE_INTERVAL = 5000;
const unsigned long SHOWCASE_INTERVAL = 30000; // 30 seconds per pattern

// Pattern names for display
const char* patternNames[10] = {
  "Static Color",
  "Breathing",
  "Rainbow",
  "Chase",
  "Twinkle",
  "Wave",
  "Fire",
  "Christmas",
  "Circle Wave",
  "Alternating"
};

// WiFi status tracking
bool wifiAPStarted = false;
unsigned long lastStatusCheck = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n=== ESP32 Christmas Lantern Controller Starting ===");
  Serial.println("Firmware Version: v3.1 - Showcase Mode");
  Serial.println("Default Mode: 30-second pattern showcase with color cycling");
 
  // Initialize circles
  for (int i = 0; i < NUM_CIRCLES; i++) {
    circles[i].enabled = true;
    circles[i].currentPattern = 0;
    circles[i].ledCount = LEDS_PER_CIRCLE;
    circles[i].isSequenceMode = false;
    circles[i].currentSequenceIndex = 0;
    circles[i].sequenceLength = 0;
    circles[i].lastSequenceUpdate = 0;
   
    for (int j = 0; j < MAX_SEQUENCE_LENGTH; j++) {
      circles[i].patternSequence[j] = 0;
    }
  }
 
  // Initialize LED strips
  Serial.println("Initializing LED strips...");
  FastLED.addLeds<LED_TYPE, 2, COLOR_ORDER>(leds[0], LEDS_PER_CIRCLE);
  FastLED.addLeds<LED_TYPE, 4, COLOR_ORDER>(leds[1], LEDS_PER_CIRCLE);
  FastLED.addLeds<LED_TYPE, 16, COLOR_ORDER>(leds[2], LEDS_PER_CIRCLE);
  FastLED.addLeds<LED_TYPE, 17, COLOR_ORDER>(leds[3], LEDS_PER_CIRCLE);
  FastLED.addLeds<LED_TYPE, 18, COLOR_ORDER>(leds[4], LEDS_PER_CIRCLE);
  FastLED.addLeds<LED_TYPE, 19, COLOR_ORDER>(leds[5], LEDS_PER_CIRCLE);
 
  FastLED.setBrightness(100); // Brighter for showcase
  FastLED.clear();
  FastLED.show();
 
  setupIndicator();
  setupWiFiAccessPoint(); // Enhanced setup like FuelCI
 
  if (wifiAPStarted) {
    Serial.println("Starting DNS server...");
    dnsServer.start(DNS_PORT, "*", local_ip);
   
    setupWebServer(); // Enhanced web server setup
   
    Serial.println("=== Setup Complete - System Ready ===");
    Serial.println("WiFi: " + String(ap_ssid));
    Serial.println("IP: " + local_ip.toString());
    Serial.println("Mode: Showcase - cycling through all 10 patterns (30s each with color cycling)");
   
    lastAppContact = millis();
    lastShowcaseUpdate = millis();
    successIndicator();
  } else {
    Serial.println("=== Setup Complete - AP Failed but System Running ===");
    showConnectionFailed();
  }
  
  // Start showcase mode
  enableShowcaseMode();
}

void enableShowcaseMode() {
  isShowcaseMode = true;
  isAutoPilot = false; // Disable auto pilot when in showcase
  showcasePatternIndex = 0;
  globalCurrentPattern = 0;
  lastShowcaseUpdate = millis();
  
  // Set different colors for different patterns to make them more distinct
  setPatternColor(showcasePatternIndex);
  
  Serial.println("🎭 Showcase Mode Enabled - Pattern 0: " + String(patternNames[0]));
}

void disableShowcaseMode() {
  isShowcaseMode = false;
  Serial.println("🎭 Showcase Mode Disabled");
}

void setPatternColor(int patternIndex) {
  // Cycle through all colors for each pattern to showcase variety
  unsigned long colorCycleTime = (millis() / 2000) % 12; // Change color every 2 seconds
  
  switch(colorCycleTime) {
    case 0: globalCurrentColor = CRGB::Red; break;
    case 1: globalCurrentColor = CRGB::Green; break;
    case 2: globalCurrentColor = CRGB::Blue; break;
    case 3: globalCurrentColor = CRGB::Yellow; break;
    case 4: globalCurrentColor = CRGB::Purple; break;
    case 5: globalCurrentColor = CRGB::Cyan; break;
    case 6: globalCurrentColor = CRGB::Orange; break;
    case 7: globalCurrentColor = CRGB::Pink; break;
    case 8: globalCurrentColor = CRGB::White; break;
    case 9: globalCurrentColor = CRGB::Magenta; break;
    case 10: globalCurrentColor = CRGB::Lime; break;
    case 11: globalCurrentColor = CRGB::Aqua; break;
    default: globalCurrentColor = CRGB::White; break;
  }
}

void updateShowcaseMode() {
  unsigned long currentTime = millis();
  
  // Update colors every 2 seconds for current pattern
  setPatternColor(showcasePatternIndex);
  
  if (currentTime - lastShowcaseUpdate >= SHOWCASE_INTERVAL) {
    // Move to next pattern
    showcasePatternIndex = (showcasePatternIndex + 1) % 10; // Cycle through 0-9
    globalCurrentPattern = showcasePatternIndex;
    
    // Update all circles to the new pattern (unless in sequence mode)
    for (int i = 0; i < NUM_CIRCLES; i++) {
      if (!circles[i].isSequenceMode) {
        circles[i].currentPattern = globalCurrentPattern;
      }
    }
    
    lastShowcaseUpdate = currentTime;
    
    Serial.println("🎭 Showcase: Pattern " + String(showcasePatternIndex) + " - " + String(patternNames[showcasePatternIndex]) + " (30s with color cycling)");
  }
}

void setupWiFiAccessPoint() {
  Serial.println("\n=== WiFi Access Point Setup ===");
 
  // Enhanced WiFi setup based on FuelCI
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(1000);
  
  WiFi.mode(WIFI_AP);
  delay(1000);
  
  if (!WiFi.softAPConfig(local_ip, gateway, subnet)) {
    Serial.println("❌ Failed to configure Access Point IP");
    wifiAPStarted = false;
    return;
  }
  
  Serial.printf("Creating Access Point: '%s'\n", ap_ssid);
  Serial.printf("Password: '%s'\n", ap_password);
  Serial.printf("AP IP: %s\n", local_ip.toString().c_str());
  
  // Start AP with specific settings for better mobile compatibility
  if (WiFi.softAP(ap_ssid, ap_password, 1, 0, 4)) { // channel 1, not hidden, max 4 connections
    wifiAPStarted = true;
    
    delay(2000); // Wait for AP to stabilize
    
    Serial.println("✅ WiFi Access Point Started Successfully!");
    Serial.printf("AP SSID: %s\n", WiFi.softAPSSID().c_str());
    Serial.printf("AP IP Address: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("AP MAC Address: %s\n", WiFi.softAPmacAddress().c_str());
    Serial.printf("Channel: 1\n");
    Serial.printf("Max connections: 4\n");
    
  } else {
    Serial.println("❌ Failed to start WiFi Access Point!");
    wifiAPStarted = false;
    printAPDiagnostics();
  }
}

void checkAccessPointStatus() {
  if (wifiAPStarted) {
    int connectedClients = WiFi.softAPgetStationNum();
    Serial.printf("📊 AP Status: %d clients connected\n", connectedClients);
    
    if (connectedClients > 0) {
      Serial.println("📱 Devices connected to Access Point");
    }
  } else {
    Serial.println("⚠️ Access Point is not running - attempting restart");
    setupWiFiAccessPoint();
  }
}

void printAPDiagnostics() {
  Serial.println("\n=== Access Point Diagnostics ===");
  Serial.printf("AP Started: %s\n", wifiAPStarted ? "Yes" : "No");
  Serial.printf("SSID: '%s'\n", ap_ssid);
  Serial.printf("Password: '%s'\n", ap_password);
  Serial.printf("Target IP: %s\n", local_ip.toString().c_str());
  Serial.printf("ESP32 MAC: %s\n", WiFi.macAddress().c_str());
}

void setupIndicator() {
  // Quick blue flash during setup
  for (int i = 0; i < 3; i++) {
    for (int circle = 0; circle < NUM_CIRCLES; circle++) {
      fill_solid(leds[circle], LEDS_PER_CIRCLE, CRGB::Blue);
    }
    FastLED.show();
    delay(200);
    FastLED.clear();
    FastLED.show();
    delay(200);
  }
}

void successIndicator() {
  // Green wave when ready
  for (int circle = 0; circle < NUM_CIRCLES; circle++) {
    fill_solid(leds[circle], LEDS_PER_CIRCLE, CRGB::Green);
    FastLED.show();
    delay(150);
    fill_solid(leds[circle], LEDS_PER_CIRCLE, CRGB::Black);
  }
  FastLED.clear();
  FastLED.show();
}

void showConnectionFailed() {
  // Red flash pattern for connection failure
  for (int i = 0; i < 5; i++) {
    for (int circle = 0; circle < NUM_CIRCLES; circle++) {
      fill_solid(leds[circle], LEDS_PER_CIRCLE, CRGB::Red);
    }
    FastLED.show();
    delay(300);
    FastLED.clear();
    FastLED.show();
    delay(300);
  }
}

void setupWebServer() {
  // Enhanced CORS handling based on FuelCI
  server.enableCORS(true);
  
  // Main routes
  server.on("/", handleRoot);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/control", HTTP_POST, handleApiControl);
  server.on("/api/color", HTTP_POST, handleApiColor);
  server.on("/api/circles", HTTP_POST, handleApiCircles);
  server.on("/api/sequences", HTTP_POST, handleApiSequences);
  server.on("/test", HTTP_GET, handleTest); // Add test endpoint like FuelCI
  
  // Enhanced OPTIONS handling for mobile browsers
  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With, Accept");
      server.sendHeader("Access-Control-Max-Age", "86400");
      server.sendHeader("Cache-Control", "no-cache");
      server.send(200, "text/plain", "OK");
    } else {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(404, "application/json", "{\"error\":\"Not found\"}");
    }
  });
 
  server.begin();
  Serial.println("✅ HTTP server started on port 80");
  if (wifiAPStarted) {
    Serial.printf("🌐 Access via: http://%s\n", WiFi.softAPIP().toString().c_str());
  }
}

void handleRoot() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
 
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>Christmas Lantern Controller v3.1</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "</head>";
  html += "<body style='font-family: Arial; margin: 20px; background: #1a1a1a; color: white;'>";
  html += "<h1>🎄 ESP32 Christmas Lantern Controller v3.1</h1>";
  html += "<h2>Showcase Mode Enhanced</h2>";
  
  html += "<h2>System Status</h2>";
  html += "<p>Access Point: <span style='color: " + String(wifiAPStarted ? "#4CAF50" : "#F44336") + "'>";
  html += wifiAPStarted ? "✅ Running" : "❌ Failed";
  html += "</span></p>";
  
  if (wifiAPStarted) {
    html += "<p>AP IP: <strong>" + WiFi.softAPIP().toString() + "</strong></p>";
    html += "<p>SSID: <strong>" + String(ap_ssid) + "</strong></p>";
    html += "<p>Connected Devices: <strong>" + String(WiFi.softAPgetStationNum()) + "</strong></p>";
  }
  
  html += "<p>Uptime: <strong>" + String(millis() / 1000) + " seconds</strong></p>";
  html += "<p>Current Pattern: <strong>" + String(globalCurrentPattern) + " - " + String(patternNames[globalCurrentPattern]) + "</strong></p>";
  html += "<p>Showcase Mode: <strong>" + String(isShowcaseMode ? "ON (30s intervals with color cycling)" : "OFF") + "</strong></p>";
  html += "<p>Auto Pilot: <strong>" + String(isAutoPilot ? "ON" : "OFF") + "</strong></p>";
  html += "<p>Running: <strong>" + String(isRunning ? "ON" : "OFF") + "</strong></p>";
  
  html += "<h2>Pattern Showcase</h2>";
  html += "<p>Default mode cycles through all 10 patterns every 30 seconds with color cycling every 2 seconds:</p>";
  for (int i = 0; i < 10; i++) {
    html += "<p>" + String(i) + ": " + String(patternNames[i]);
    if (i == showcasePatternIndex && isShowcaseMode) {
      html += " <strong style='color: #4CAF50;'>← CURRENT</strong>";
    }
    html += "</p>";
  }
  
  html += "<h2>LED Configuration</h2>";
  for (int i = 0; i < NUM_CIRCLES; i++) {
    html += "<p>Circle " + String(i+1) + ": Pin " + String(LED_PINS[i]);
    html += ", Pattern " + String(circles[i].currentPattern) + " (" + String(patternNames[circles[i].currentPattern]) + ")";
    html += ", " + String(circles[i].enabled ? "Enabled" : "Disabled");
    if (circles[i].isSequenceMode) {
      html += " (SEQUENCE)";
    }
    html += "</p>";
  }

  html += "</body></html>";
 
  server.send(200, "text/html", html);
}

void handleTest() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  
  StaticJsonDocument<400> doc;
  doc["status"] = "success";
  doc["message"] = "ESP32 Christmas Lantern Controller is working!";
  doc["ap_started"] = wifiAPStarted;
  doc["ip"] = wifiAPStarted ? WiFi.softAPIP().toString() : "Not started";
  doc["clients"] = WiFi.softAPgetStationNum();
  doc["firmware"] = "v3.1-SHOWCASE-MODE";
  doc["uptime"] = millis();
  doc["pattern"] = globalCurrentPattern;
  doc["pattern_name"] = patternNames[globalCurrentPattern];
  doc["showcase_mode"] = isShowcaseMode;
  doc["auto_pilot"] = isAutoPilot;
  doc["running"] = isRunning;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  Serial.println("Test endpoint accessed from: " + server.client().remoteIP().toString());
}

void handleApiStatus() {
  // Enhanced CORS headers like FuelCI
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
 
  hasAppConnected = true;
  lastAppContact = millis();
 
  Serial.println("Status request from client IP: " + server.client().remoteIP().toString());
 
  StaticJsonDocument<1200> doc;
  doc["isOn"] = isRunning;
  doc["autoMode"] = isAutoPilot;
  doc["showcaseMode"] = isShowcaseMode;
  doc["pattern"] = globalCurrentPattern;
  doc["patternName"] = patternNames[globalCurrentPattern];
  doc["showcasePatternIndex"] = showcasePatternIndex;
  doc["color"] = rgbToHex(globalCurrentColor);
  doc["brightness"] = globalBrightness;
  doc["connected_clients"] = WiFi.softAPgetStationNum();
  doc["uptime"] = millis();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["esp_ip"] = WiFi.softAPIP().toString();
  doc["firmware"] = "v3.1-SHOWCASE";
 
  // Pattern names array
  JsonArray patternsArray = doc.createNestedArray("availablePatterns");
  for (int i = 0; i < 10; i++) {
    JsonObject patternObj = patternsArray.createNestedObject();
    patternObj["id"] = i;
    patternObj["name"] = patternNames[i];
    patternObj["active"] = (i == showcasePatternIndex && isShowcaseMode);
  }
 
  // Circle information
  JsonArray circleArray = doc.createNestedArray("circles");
  for (int i = 0; i < NUM_CIRCLES; i++) {
    JsonObject circleObj = circleArray.createNestedObject();
    circleObj["id"] = i;
    circleObj["enabled"] = circles[i].enabled;
    circleObj["ledCount"] = circles[i].ledCount;
    circleObj["currentPattern"] = circles[i].currentPattern;
    circleObj["patternName"] = patternNames[circles[i].currentPattern];
  }
 
  // Sequence information
  JsonObject sequences = doc.createNestedObject("sequences");
  for (int i = 0; i < NUM_CIRCLES; i++) {
    if (circles[i].isSequenceMode) {
      String circleKey = "circle_" + String(i);
      JsonObject seqObj = sequences.createNestedObject(circleKey);
     
      JsonArray patternsArray = seqObj.createNestedArray("patterns");
      for (int j = 0; j < circles[i].sequenceLength; j++) {
        patternsArray.add(circles[i].patternSequence[j]);
      }
     
      seqObj["currentIndex"] = circles[i].currentSequenceIndex;
    }
  }
 
  String response;
  serializeJson(doc, response);
 
  server.send(200, "application/json", response);
  Serial.println("Status sent to: " + server.client().remoteIP().toString());
}

void handleApiControl() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
 
  hasAppConnected = true;
  lastAppContact = millis();
 
  Serial.println("Control request from: " + server.client().remoteIP().toString());
 
  if (server.hasArg("plain")) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
   
    if (error) {
      Serial.println("JSON parse error: " + String(error.c_str()));
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
   
    String command = doc["command"];
    Serial.println("Command: " + command);
   
    if (command == "power_on") {
      isRunning = true;
      disableAutoPilot();
    }
    else if (command == "power_off") {
      isRunning = false;
      FastLED.clear();
      FastLED.show();
    }
    else if (command == "set_pattern") {
      globalCurrentPattern = doc["value"];
      disableShowcaseMode(); // User took control
      disableAutoPilot();
      for (int i = 0; i < NUM_CIRCLES; i++) {
        if (!circles[i].isSequenceMode) {
          circles[i].currentPattern = globalCurrentPattern;
        }
      }
    }
    else if (command == "set_brightness") {
      globalBrightness = doc["value"];
      FastLED.setBrightness(globalBrightness);
    }
    else if (command == "toggle_auto") {
      if (isAutoPilot) {
        disableAutoPilot();
      } else {
        enableAutoPilot();
        disableShowcaseMode(); // Auto pilot takes priority
      }
    }
    else if (command == "toggle_showcase") {
      if (isShowcaseMode) {
        disableShowcaseMode();
      } else {
        enableShowcaseMode();
        disableAutoPilot(); // Showcase takes priority
      }
    }
   
    server.send(200, "application/json", "{\"status\":\"success\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"No data\"}");
  }
}

void handleApiColor() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
 
  hasAppConnected = true;
  lastAppContact = millis();
 
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
   
    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
   
    if (doc.containsKey("r") && doc.containsKey("g") && doc.containsKey("b")) {
      globalCurrentColor = CRGB(doc["r"], doc["g"], doc["b"]);
      disableShowcaseMode(); // User took control of color
      Serial.println("Color set to: " + String((int)doc["r"]) + "," + String((int)doc["g"]) + "," + String((int)doc["b"]));
    }
   
    server.send(200, "application/json", "{\"status\":\"success\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"No data\"}");
  }
}

void handleApiCircles() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
 
  hasAppConnected = true;
  lastAppContact = millis();
 
  if (server.hasArg("plain")) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
   
    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
   
    String command = doc["command"];
   
    if (command == "toggle_circle") {
      int circleId = doc["circle"];
      if (circleId >= 0 && circleId < NUM_CIRCLES) {
        circles[circleId].enabled = !circles[circleId].enabled;
      }
    }
    else if (command == "set_circle_pattern") {
      int circleId = doc["circle_id"];
      int patternId = doc["pattern_id"];
      if (circleId >= 0 && circleId < NUM_CIRCLES) {
        circles[circleId].currentPattern = patternId;
        disableShowcaseMode(); // User took control
      }
    }
   
    server.send(200, "application/json", "{\"status\":\"success\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"No data\"}");
  }
}

void handleApiSequences() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
 
  hasAppConnected = true;
  lastAppContact = millis();
 
  if (server.hasArg("plain")) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
   
    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
   
    String command = doc["command"];
   
    if (command == "set_sequence") {
      JsonArray circleIds = doc["circles"];
      JsonArray patterns = doc["patterns"];
      
      disableShowcaseMode(); // User took control with sequences
     
      for (JsonVariant circleIdVar : circleIds) {
        int id = circleIdVar.as<int>();
        if (id >= 0 && id < NUM_CIRCLES) {
          circles[id].sequenceLength = 0;
         
          int patternCount = 0;
          for (JsonVariant pattern : patterns) {
            if (patternCount < MAX_SEQUENCE_LENGTH) {
              circles[id].patternSequence[patternCount] = pattern.as<int>();
              patternCount++;
            }
          }
         
          circles[id].sequenceLength = patternCount;
          circles[id].currentSequenceIndex = 0;
          circles[id].isSequenceMode = (patternCount > 0);
          circles[id].lastSequenceUpdate = millis();
         
          if (patternCount > 0) {
            circles[id].currentPattern = circles[id].patternSequence[0];
          }
        }
      }
    }
    else if (command == "clear_sequence") {
      JsonArray circleIds = doc["circles"];
     
      for (JsonVariant circleIdVar : circleIds) {
        int id = circleIdVar.as<int>();
        if (id >= 0 && id < NUM_CIRCLES) {
          circles[id].sequenceLength = 0;
          circles[id].isSequenceMode = false;
          circles[id].currentSequenceIndex = 0;
          circles[id].currentPattern = globalCurrentPattern;
        }
      }
    }
   
    server.send(200, "application/json", "{\"status\":\"success\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"No data\"}");
  }
}

void enableAutoPilot() {
  isAutoPilot = true;
  isRunning = true;
  disableShowcaseMode(); // Auto pilot takes priority
  globalCurrentPattern = 2;
  autoPilotHue = 0;
  FastLED.setBrightness(30);
}

void disableAutoPilot() {
  isAutoPilot = false;
  FastLED.setBrightness(50);
}

void runAutoPilotPattern() {
  for (int circle = 0; circle < NUM_CIRCLES; circle++) {
    if (circles[circle].enabled) {
      for (int i = 0; i < LEDS_PER_CIRCLE; i++) {
        int hue = (autoPilotHue + (circle * 40) + (i * 10)) % 255;
        leds[circle][i] = CHSV(hue, 255, 200);
      }
    }
  }
 
  autoPilotHue += 2;
  if (autoPilotHue >= 255) {
    autoPilotHue = 0;
  }
 
  FastLED.show();
}

void checkAutoPilotTimeout() {
  // Modified: Return to showcase mode instead of auto pilot after timeout
  if (hasAppConnected && !isAutoPilot && !isShowcaseMode && (millis() - lastAppContact) > AUTO_PILOT_TIMEOUT) {
    Serial.println("📱 App timeout - returning to showcase mode");
    enableShowcaseMode();
  }
}

void updateSequences() {
  unsigned long currentTime = millis();
 
  for (int i = 0; i < NUM_CIRCLES; i++) {
    if (circles[i].isSequenceMode && circles[i].enabled &&
        circles[i].sequenceLength > 0) {
     
      if (currentTime - circles[i].lastSequenceUpdate >= SEQUENCE_INTERVAL) {
        circles[i].currentSequenceIndex =
          (circles[i].currentSequenceIndex + 1) % circles[i].sequenceLength;
        circles[i].currentPattern =
          circles[i].patternSequence[circles[i].currentSequenceIndex];
        circles[i].lastSequenceUpdate = currentTime;
      }
    }
  }
}

String rgbToHex(CRGB color) {
  char hex[8];
  sprintf(hex, "#%02X%02X%02X", color.r, color.g, color.b);
  return String(hex);
}

void loop() {
  // Enhanced main loop with better error handling
  dnsServer.processNextRequest();
  server.handleClient();
  checkAutoPilotTimeout();
 
  // Enhanced status checking like FuelCI
  if (millis() - lastStatusCheck > 30000) {
    checkAccessPointStatus();
    lastStatusCheck = millis();
  }
 
  if (isRunning) {
    if (isShowcaseMode) {
      // Showcase mode: cycle through patterns every 10 seconds
      updateShowcaseMode();
      if (millis() - lastUpdate > 100) {
        runPatterns();
        lastUpdate = millis();
      }
    }
    else if (isAutoPilot) {
      // Auto pilot mode: rainbow effect
      if (millis() - lastAutoPilotUpdate > 200) {
        runAutoPilotPattern();
        lastAutoPilotUpdate = millis();
      }
    } else {
      // Manual control mode
      updateSequences();
     
      if (millis() - lastUpdate > 100) {
        runPatterns();
        lastUpdate = millis();
      }
    }
  }
  
  delay(10); // Small delay to prevent watchdog issues
}

void runPatterns() {
  for (int circle = 0; circle < NUM_CIRCLES; circle++) {
    if (!circles[circle].enabled) {
      fill_solid(leds[circle], LEDS_PER_CIRCLE, CRGB::Black);
      continue;
    }
   
    int pattern = circles[circle].currentPattern;
   
    switch (pattern) {
      case 0: // Static color
        fill_solid(leds[circle], LEDS_PER_CIRCLE, globalCurrentColor);
        break;
       
      case 1: // Breathing effect
        {
          int brightness = (sin(millis() / 1000.0) + 1) * 127;
          CRGB color = globalCurrentColor;
          color.fadeToBlackBy(255 - brightness);
          fill_solid(leds[circle], LEDS_PER_CIRCLE, color);
        }
        break;
       
      case 2: // Rainbow
        fill_rainbow(leds[circle], LEDS_PER_CIRCLE, millis() / 20 + (circle * 40), 7);
        break;
       
      case 3: // Chase
        {
          fill_solid(leds[circle], LEDS_PER_CIRCLE, CRGB::Black);
          int pos = (millis() / 100) % LEDS_PER_CIRCLE;
          leds[circle][pos] = globalCurrentColor;
         
          for (int i = 1; i <= 3; i++) {
            int trailPos = (pos - i + LEDS_PER_CIRCLE) % LEDS_PER_CIRCLE;
            leds[circle][trailPos] = globalCurrentColor;
            leds[circle][trailPos].fadeToBlackBy(80 * i);
          }
        }
        break;
       
      case 4: // Twinkle
        {
          if (random(100) < 10) {
            int led = random(LEDS_PER_CIRCLE);
            leds[circle][led] = globalCurrentColor;
          }
          fadeToBlackBy(leds[circle], LEDS_PER_CIRCLE, 20);
        }
        break;
       
      case 5: // Wave
        {
          for (int i = 0; i < LEDS_PER_CIRCLE; i++) {
            int brightness = (sin((i + millis() / 50.0) * 0.3) + 1) * 127;
            leds[circle][i] = globalCurrentColor;
            leds[circle][i].fadeToBlackBy(255 - brightness);
          }
        }
        break;
       
      case 6: // Fire effect
        {
          for (int i = 0; i < LEDS_PER_CIRCLE; i++) {
            int heat = random(50, 255);
            leds[circle][i] = CHSV(random(0, 30), 255, heat);
          }
        }
        break;
       
      case 7: // Christmas colors
        {
          for (int i = 0; i < LEDS_PER_CIRCLE; i += 2) {
            leds[circle][i] = CRGB::Red;
            if (i + 1 < LEDS_PER_CIRCLE) {
              leds[circle][i + 1] = CRGB::Green;
            }
          }
        }
        break;
       
      case 8: // Circle wave
        {
          for (int i = 0; i < LEDS_PER_CIRCLE; i++) {
            int brightness = (sin((i + circle * 2 + millis() / 50.0) * 0.3) + 1) * 127;
            leds[circle][i] = globalCurrentColor;
            leds[circle][i].fadeToBlackBy(255 - brightness);
          }
        }
        break;
       
      case 9: // Alternating circles - Enhanced with more colors
        {
          // Cycle through 6 different color pairs every 2 seconds
          unsigned long colorTime = (millis() / 2000) % 6;
          CRGB color1, color2;
          
          switch(colorTime) {
            case 0: color1 = CRGB::Red; color2 = CRGB::Blue; break;
            case 1: color1 = CRGB::Green; color2 = CRGB::Orange; break;
            case 2: color1 = CRGB::Purple; color2 = CRGB::Yellow; break;
            case 3: color1 = CRGB::Cyan; color2 = CRGB::Magenta; break;
            case 4: color1 = CRGB::Pink; color2 = CRGB::Lime; break;
            case 5: color1 = CRGB::White; color2 = CRGB::Aqua; break;
            default: color1 = CRGB::Pink; color2 = CRGB::Purple; break;
          }
          
          // Alternate between the two colors every second, and by circle
          bool useColor1 = ((millis() / 1000) % 2 == circle % 2);
          CRGB selectedColor = useColor1 ? color1 : color2;
          fill_solid(leds[circle], LEDS_PER_CIRCLE, selectedColor);
        }
        break;
       
      default:
        fill_solid(leds[circle], LEDS_PER_CIRCLE, globalCurrentColor);
        break;
    }
  }
 
  FastLED.show();
}