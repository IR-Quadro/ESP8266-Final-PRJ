#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_AHTX0.h>
#include <LiquidCrystal_I2C.h>
#include "FS.h"

IPAddress local_ip(192,168,1,1);
IPAddress getway(192,168,1,1);
IPAddress subnet(255,255,255,0);

Adafruit_AHTX0 aht;

ESP8266WebServer server(80);
LiquidCrystal_I2C lcd(0x3F ,16,2);

String savedUsername = "admin";
String savedPassword = "1234";
bool isAuthenticated = false;

float temperatureVal = 0.00;
float humidityVal = 0.00;


void saveCredentials(String username, String password) {
    File file = SPIFFS.open("/config.txt", "w");
    if (!file) {
        Serial.println("Failed to open file for writing!");
        return;
    }
    file.println(username);
    file.println(password);
    file.close();
}

void loadCredentials() {
    File file = SPIFFS.open("/config.txt", "r");
    if (!file) {
        Serial.println("No saved credentials, using default.");
        return;
    }
    savedUsername = file.readStringUntil('\n');
    savedPassword = file.readStringUntil('\n');
    savedUsername.trim();
    savedPassword.trim();
    file.close();
}

//===================================> Root (Main) <=======================================//

void handleRoot() {
        File file = SPIFFS.open("/root.html", "r");
    if (!file) {
        server.send(500, "text/plain", "File not found");
        return;
    }

    server.streamFile(file, "text/html");
    file.close();
}

//===================================> Login <=======================================//

void handleLogin() {
    File file = SPIFFS.open("/login.html", "r");
    if (!file) {
        server.send(500, "text/plain", "File not found");
        return;
    }

    // Stream the file content to the client
    server.streamFile(file, "text/html");
    file.close();
}

//===================================> Check User-Pass <=======================================//

void handleCheck() {
    if (server.hasArg("username") && server.hasArg("password")) {
        String username = server.arg("username");
        String password = server.arg("password");

        if (username == savedUsername && password == savedPassword) {
            isAuthenticated = true;
            server.sendHeader("Location", "/dashboard");
            server.send(302, "text/plain", "Redirecting...");
        } 
        else
        {
            server.send(403, "text/html", "<h3>Access Denied!</h3><a href='/login'>Try Again</a>");
        }
    }
}

//===================================> Dashboard <=======================================//

void handleDashboard() {
    if (!isAuthenticated) {
        server.sendHeader("Location", "/login");
        server.send(302, "text/plain", "Redirecting...");
        return;
    }

    File file = SPIFFS.open("/dashboard.html", "r");
    if (!file) {
        server.send(500, "text/plain", "File not found");
        return;
    }

    server.streamFile(file, "text/html");
    file.close();
}

//===================================> Control <=======================================//

void handleControll(){
    if (!isAuthenticated) {
        server.sendHeader("Location", "/login");
        server.send(302, "text/plain", "Redirecting...");
        return;
    }  

    String html = "<h2>Demo Panel for Relay Control</h2><p><a href='/dashboard'>Back to Dashboard</a></p>";

        server.send(200, "text/html", html);

}

//===================================> Sensor <=======================================//

void handleSensor(){
  if (!isAuthenticated) {
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "Redirecting...");
    return;
}

    File file = SPIFFS.open("/sensor.html", "r");
    if (!file) {
        server.send(500, "text/plain", "File not found");
        return;
    }

    String html = "";
    while (file.available()) {
        html += (char)file.read();
    }
    file.close();

    html.replace("{{temperature}}", String(temperatureVal));
    html.replace("{{humidity}}", String(humidityVal));

    server.send(200, "text/html", html);
}

//===================================> Sensor Data <=======================================//

void handleData(){
  if (!isAuthenticated) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }

  String json = "{\"temperatureVal\":" + String(temperatureVal) + ",\"humidityVal\":" + String(humidityVal) + "}";
  server.send(200, "application/json", json);
}

//===================================> Setting (change User-Pass) <=======================================//

void handleSettings() {
    if (!isAuthenticated) {
        server.sendHeader("Location", "/login");
        server.send(302, "text/plain", "Redirecting...");
        return;
    }

    File file = SPIFFS.open("/settings.html", "r");
    if (!file) {
        server.send(500, "text/plain", "File not found");
        return;
    }

    server.streamFile(file, "text/html");
    file.close();
}

//===================================> Process change User-Pass <=======================================//

void handleChange() {
    if (!isAuthenticated) {
        server.sendHeader("Location", "/login");
        server.send(302, "text/plain", "Redirecting...");
        return;
    }

    if (server.hasArg("newuser") && server.hasArg("newpass")) {
        savedUsername = server.arg("newuser");
        savedPassword = server.arg("newpass");
        saveCredentials(savedUsername, savedPassword);

        isAuthenticated = false; 
        server.sendHeader("Location", "/login");
        server.send(302, "text/plain", "Redirecting...");
    }
}

//===================================> LogOut <=======================================//

void handleLogout() {
    isAuthenticated = false;
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "Redirecting...");
}


void setup() {
    Serial.begin(115200);
    
    lcd.init();
    lcd.clear();         
    lcd.backlight();
    
    WiFi.softAP("ESP8266", "12345678");
    WiFi.softAPConfig(local_ip, getway, subnet);
        
    lcd.clear();
    lcd.setCursor(0, 0); 
    lcd.print(local_ip);
    
    if (!SPIFFS.begin()) {
        Serial.println("SPIFFS Mount Failed!");
        
        lcd.setCursor(0, 1); 
        lcd.print("SPIFFS Failed!");
        return;
    }

    aht.begin();
    delay(1000);
    
    loadCredentials();

    server.on("/", handleRoot);
    server.on("/login", handleLogin);
    server.on("/check", HTTP_POST, handleCheck);
    server.on("/dashboard", handleDashboard);
    server.on("/controll", handleControll);
    server.on("/sensor", handleSensor);
    server.on("/settings", handleSettings);
    server.on("/data", handleData);
    server.on("/change", HTTP_POST, handleChange);
    server.on("/logout", handleLogout);
    
    server.begin();
    Serial.println("HTTP server started.");
}

void loop() {
  
    server.handleClient();

    static unsigned long lastUpdate = 0;
    
    if (millis() - lastUpdate > 3000)
    {
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);

      temperatureVal = temp.temperature;
      humidityVal = humidity.relative_humidity;
    
      lcd.setCursor(0,1);
      lcd.print(temp.temperature);
      lcd.print((char)223);
      lcd.print("C   ");

      lcd.setCursor(9,1);
      lcd.print(humidity.relative_humidity);
      lcd.print("%");

      lastUpdate = millis();
    }    
}
