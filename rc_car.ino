#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

// WiFi credentials
const char* ssid = "ddm4534";
const char* password = "12345678";

// Pin definitions
const int LEFT_MOTOR_PIN = 12;   // GPIO 12 - Left rear wheel ESC
const int RIGHT_MOTOR_PIN = 13;  // GPIO 13 - Right rear wheel ESC
const int SERVO_PIN = 14;        // GPIO 14 - Steering servo
const int BUZZER_PIN = 5;        // D1

// Motor and servo objects
Servo leftMotor;
Servo rightMotor;
Servo steeringServo;

// Control parameters
const int MOTOR_NEUTRAL = 1500;  // ESC neutral position (1500µs)
const int MOTOR_MIN = 1000;      // ESC minimum (1000µs)
const int MOTOR_MAX = 2000;      // ESC maximum (2000µs)
const int SERVO_CENTER = 90;     // Servo center position
const int SERVO_MAX_LEFT = 60;   // Maximum left turn (90-90)
const int SERVO_MAX_RIGHT = 120; // Maximum right turn (90+90)

// Current control values
int currentSpeed = 0;    // -100 to 100
int currentTurn = 0;     // -100 to 100
bool emergencyStop = false;

// Web server
ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("RC Car Controller Starting...");

  pinMode(BUZZER_PIN, OUTPUT);

  tone(BUZZER_PIN, 440, 500);
  
  // Initialize motors and servo
  initializeMotors();
  initializeServo();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Setup web server routes
  setupWebServer();
  
  Serial.println("RC Car Controller Ready!");
  Serial.print("Control panel available at: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
  
  // Apply emergency stop if activated
  if (emergencyStop) {
    stopMotors();
  } else {
    updateMotors();
    updateSteering();
  }
  
  delay(20); // Small delay for stability
}

void initializeMotors() {
  Serial.println("Initializing ESCs...");
  
  // Attach ESCs to PWM pins
  leftMotor.attach(LEFT_MOTOR_PIN, MOTOR_MIN, MOTOR_MAX);
  rightMotor.attach(RIGHT_MOTOR_PIN, MOTOR_MIN, MOTOR_MAX);
  
  // Send neutral signal for ESC initialization
  leftMotor.writeMicroseconds(MOTOR_NEUTRAL);
  rightMotor.writeMicroseconds(MOTOR_NEUTRAL);
  
  // Wait for ESC initialization (important!)
  delay(3000);
  
  Serial.println("ESCs initialized");
}

void initializeServo() {
  Serial.println("Initializing steering servo...");
  
  steeringServo.attach(SERVO_PIN);
  steeringServo.write(SERVO_MAX_LEFT);
  delay(500);
  steeringServo.write(SERVO_MAX_RIGHT);
  delay(500);
  steeringServo.write(SERVO_CENTER); // Point straight ahead
  
  delay(500);
  Serial.println("Servo initialized - pointing straight");
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    tone(BUZZER_PIN, 523, 250);
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  tone(BUZZER_PIN, 440, 250);
  delay(250);
  tone(BUZZER_PIN, 523, 250);
  delay(250);
  tone(BUZZER_PIN, 659, 250);
  delay(250);
}

void setupWebServer() {
  // Serve the main control page
  server.on("/", handleRoot);
  
  // API endpoints for control
  server.on("/control", HTTP_POST, handleControl);
  server.on("/stop", HTTP_POST, handleEmergencyStop);
  server.on("/status", HTTP_GET, handleStatus);
  
  server.begin();
  Serial.println("Web server started");
}

void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=no'>
    <title>RC Car Controller</title>
    <link rel='stylesheet' href='https://pcwu2022.github.io/rc_car_ino/data/style.css'>
</head>
<body>
    <div class='container'>
        <h1>🏎️ RC Car Controller</h1>
        
        <div class='status'>
            <div>Speed: <span id='speed'>0</span>%</div>
            <div>Turn: <span id='turn'>0</span>%</div>
            <div>Status: <span id='status'>Ready</span></div>
        </div>
        
        <div class='joystick-container' id='joystickContainer'>
            <div class='joystick' id='joystick'></div>
        </div>
        
        <button class='emergency-stop' onclick='emergencyStop() '>🛑 EMERGENCY STOP</button>
    </div>

    <script src='https://pcwu2022.github.io/rc_car_ino/data/script.js'></script>
</body>
</html>
  )";
  
  server.send(200, "text/html", html);
}

void handleControl() {
  if (server.hasArg("speed") && server.hasArg("turn")) {
    int newSpeed = server.arg("speed").toInt();
    int newTurn = server.arg("turn").toInt();
    
    // Clamp values to valid range
    currentSpeed = constrain(newSpeed, -100, 100);
    currentTurn = constrain(newTurn, -100, 100);
    
    emergencyStop = false; // Clear emergency stop when new commands received
    
    Serial.print("Speed: ");
    Serial.print(currentSpeed);
    Serial.print(", Turn: ");
    Serial.println(currentTurn);
    
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleEmergencyStop() {
  emergencyStop = true;
  currentSpeed = 0;
  currentTurn = 0;
  
  Serial.println("EMERGENCY STOP ACTIVATED!");
  server.send(200, "text/plain", "Emergency stop activated");
  tone(BUZZER_PIN, 440, 1000);
}

void handleStatus() {
  String status = "{";
  status += "\"speed\":" + String(currentSpeed) + ",";
  status += "\"turn\":" + String(currentTurn) + ",";
  status += "\"emergency\":" + String(emergencyStop ? "true" : "false");
  status += "}";
  
  server.send(200, "application/json", status);
}

void updateMotors() {
  // Calculate differential steering
  int leftSpeed = currentSpeed;
  int rightSpeed = currentSpeed;
  
  // Apply differential steering for turning
  if (currentTurn > 0) { // Turning right
    leftSpeed = currentSpeed;
    rightSpeed = currentSpeed - (abs(currentTurn) * currentSpeed / 100);
  } else if (currentTurn < 0) { // Turning left
    rightSpeed = currentSpeed;
    leftSpeed = currentSpeed - (abs(currentTurn) * currentSpeed / 100);
  }
  
  // Convert speed percentage to ESC microseconds
  int leftPWM = map(leftSpeed, -100, 100, MOTOR_MIN, MOTOR_MAX);
  int rightPWM = map(rightSpeed, -100, 100, MOTOR_MIN, MOTOR_MAX);
  
  // Apply to motors
  leftMotor.writeMicroseconds(leftPWM);
  rightMotor.writeMicroseconds(rightPWM);
}

void updateSteering() {
  // Convert turn percentage to servo angle
  int servoAngle = map(currentTurn, -100, 100, SERVO_MAX_LEFT, SERVO_MAX_RIGHT);
  steeringServo.write(servoAngle);
}

void stopMotors() {
  leftMotor.writeMicroseconds(MOTOR_NEUTRAL);
  rightMotor.writeMicroseconds(MOTOR_NEUTRAL);
}