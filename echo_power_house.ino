//async with last voltage update and serial print ln using softap

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ESP32Servo.h>

// WiFi credentials
const char* ssid = "Multi power IOT";
const char* password = "1234567890";

// Analog Pins for voltage sources
#define WINDMILL_PIN 34
#define SOLAR_PANEL_VOLT_PIN 35
#define HYDRO_PIN 32
#define PIEZO_PIN 27


// Servo and LDR pins for solar tracking
#define LDR_LEFT_PIN 36
#define LDR_RIGHT_PIN 39
#define SERVO_PIN 18

// Constants for voltage scaling
const float VOLTAGE_SCALE = 2.0; // Scaling for 6.6V range

// Constants for solar tracking
const int LDR_THRESHOLD = 50; 
const int SERVO_MIN = 0;
const int SERVO_MAX = 180;
const int SERVO_STEP = 1;
const unsigned long SERVO_UPDATE_INTERVAL = 200;

// Variables for solar tracking
Servo servoMotor;
int currentAngle = 90; // Servo starts at midpoint
unsigned long lastUpdate = 0;

// Web server and WebSocket instances
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Variables to store last valid voltage readings
float lastWindmillVoltage = 0;
float lastSolarVoltage = 0;
float lastHydroVoltage = 0;
float lastPiezoVoltage = 0;

// Function to read and scale voltage
float readVoltage(int pin, float& lastValue) {
  int rawValue = analogRead(pin);
  float voltage = (rawValue * 3.3 / 4095) * VOLTAGE_SCALE;

  if (voltage > 0) {
    lastValue = voltage;
  }
  return lastValue;
}
// Function to send voltage updates to all WebSocket clients
void sendVoltageUpdates() {
  String json = "{";
  json += "\"windmill\": " + String(readVoltage(WINDMILL_PIN, lastWindmillVoltage)) + ", ";
  json += "\"solar\": " + String(readVoltage(SOLAR_PANEL_VOLT_PIN, lastSolarVoltage)) + ", ";
  json += "\"hydro\": " + String(readVoltage(HYDRO_PIN, lastHydroVoltage)) + ", ";
  json += "\"piezo\": " + String(readVoltage(PIEZO_PIN, lastPiezoVoltage)) + "}";
  ws.textAll(json);
}

// WebSocket event handler
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.println("WebSocket client connected");
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.println("WebSocket client disconnected");
  }
}

void setup() {
  // Serial Monitor initialization
  Serial.begin(115200);

  // WiFi setup
  WiFi.softAP(ssid, password);

  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Initialize servo
  ESP32PWM::allocateTimer(0);
  servoMotor.setPeriodHertz(50);
  servoMotor.attach(SERVO_PIN, 500, 2400);
  servoMotor.write(currentAngle);

  // LDR pins
  pinMode(LDR_LEFT_PIN, INPUT);
  pinMode(LDR_RIGHT_PIN, INPUT);

  // Voltage source pins
  pinMode(WINDMILL_PIN, INPUT);
  pinMode(SOLAR_PANEL_VOLT_PIN, INPUT);
  pinMode(HYDRO_PIN, INPUT);
  pinMode(PIEZO_PIN, INPUT);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    String html = "<html><head><title>Energy Source Monitoring System</title></head><body>";
    
    // Adding logo at the top
    html += "<img src='/jpncelogi.jpg' alt='JPNCE College Logo' style='display:block; margin:auto; width:150px; height:auto;'>";

    // Main heading
    html += "<h1 style='text-align:center;'>Energy Source Monitoring System</h1>";
    
    // Add Solar image and text
    html += "<div style='text-align:center; margin-bottom:20px;'>";
    html += "<img src='/solar_image.jpg' alt='Solar Panel' style='width:200px; height:auto;'>";
    html += "<p><b>Solar Panel Voltage:</b> <span id=\"solar\"></span> V</p>";
    html += "</div>";
    
    // Add Windmill image and text
    html += "<div style='text-align:center; margin-bottom:20px;'>";
    html += "<img src='/windmill_image.jpg' alt='Windmill' style='width:200px; height:auto;'>";
    html += "<p><b>Windmill Voltage:</b> <span id=\"windmill\"></span> V</p>";
    html += "</div>";
    
    // Add Hydro image and text
    html += "<div style='text-align:center; margin-bottom:20px;'>";
    html += "<img src='/hydro_image.jpg' alt='Hydro Generator' style='width:200px; height:auto;'>";
    html += "<p><b>Hydro Generator Voltage:</b> <span id=\"hydro\"></span> V</p>";
    html += "</div>";

    // Add Piezo image and text
    html += "<div style='text-align:center; margin-bottom:20px;'>";
    html += "<img src='/piezo_image.jpg' alt='Piezo Module' style='width:200px; height:auto;'>";
    html += "<p><b>Piezo Module Voltage:</b> <span id=\"piezo\"></span> V</p>";
    html += "</div>";
    
    // JavaScript for WebSocket updates
    html += "<script>";
    html += "const ws = new WebSocket('ws://' + location.host + '/ws');";
    html += "ws.onmessage = (event) => { const data = JSON.parse(event.data); ";
    html += "document.getElementById('windmill').innerText = data.windmill; ";
    html += "document.getElementById('solar').innerText = data.solar; ";
    html += "document.getElementById('hydro').innerText = data.hydro; ";
    html += "document.getElementById('piezo').innerText = data.piezo; };";
    html += "</script>";
    
    html += "</body></html>";
    request->send(200, "text/html", html);
});


  // WebSocket initialization
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  server.begin();
}

void loop() {
  // Solar tracking logic
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate >= SERVO_UPDATE_INTERVAL) {
    lastUpdate = currentMillis;

    int ldrLeft = analogRead(LDR_LEFT_PIN);
    int ldrRight = analogRead(LDR_RIGHT_PIN);

    int ldrDiff = ldrLeft - ldrRight;

    if (ldrDiff > LDR_THRESHOLD && currentAngle > SERVO_MIN) {
      currentAngle -= SERVO_STEP;
      if (currentAngle < SERVO_MIN) currentAngle = SERVO_MIN;
      servoMotor.write(currentAngle);
      Serial.println("Adjusting Left");
    } else if (ldrDiff < -LDR_THRESHOLD && currentAngle < SERVO_MAX) {
      currentAngle += SERVO_STEP;
      if (currentAngle > SERVO_MAX) currentAngle = SERVO_MAX;
      servoMotor.write(currentAngle);
      Serial.println("Adjusting Right");
    } else {
      Serial.println("No adjustment needed");
    }
  }
  // Send voltage updates to all WebSocket clients
  sendVoltageUpdates();
  // delay(10);
}