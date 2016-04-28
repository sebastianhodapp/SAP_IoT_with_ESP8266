/*
 *  Temperature Sensor connected to the SAP HANA IoT Edition
 *
 *  Configuration:
 *  1. Connect to power
 *  2. Connect with laptop to Access Point
 *  3. Open IP 192.168.1.4 in browser
 *  4. Scan for WiFi or enter credentials manually
 *  5. Enter device id & oauth token
 */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include "DHT.h"

// ========== start configuration ==========
#define DEBUG 1  // Toggle comment to switch on/off debugging via serial console
#define DHTTYPE DHT22   // Can be: "DHT11" | "DHT22" (AM2302) | "DHT21" (AM2301)

const char* host = "iotmms########trial.hanatrial.ondemand.com"; // Your SAP HANA Host
String message_type_id = "#####################";                // Your message ID

char device_id[40]   = "########-####-####-####-###########";  // Optional: You can preset your Device ID
char oauth_token[36] = "################################";      // Optional: You can preset your OAuth Token
// ========== end configuration ============

//Debug Definitions
#ifdef DEBUG
 #define DEBUG_PRINT(x)  Serial.println (x)
#else
 #define DEBUG_PRINT(x)
#endif

const int LED = 2;  // LED
Ticker ticker;      // Ticker

// HCP Parameter
String url = "/com.sap.iotservices.mms/v1/api/http/data/";
const int httpsPort = 443;

// DHT Setup
#define DHTPIN D4         // Connection PIN of DHT sensor. For WeMos SHields it's D4
DHT dht(DHTPIN, DHTTYPE); // Initialize sensor

void setup() {
  Serial.begin(115200);
  Serial.println();

  // Set LED Pins as Output and pull low for further status indication
  pinMode(BUILTIN_LED, OUTPUT);
  ticker.attach(0.6, status_led);

  // Connect to WiFi
  WiFiManager wifiManager;
  WiFiManagerParameter custom_device_id("device", "Device ID", device_id, 36);
  wifiManager.addParameter(&custom_device_id);
  WiFiManagerParameter  custom_oauth_token("token", "OAuth Token", oauth_token, 32);
  wifiManager.addParameter(&custom_oauth_token);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.autoConnect();

  if (!wifiManager.autoConnect()) {
    DEBUG_PRINT("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }
  
  DEBUG_PRINT("WiFi connected");

  DEBUG_PRINT("Start DHT");
  dht.begin();
  delay(5000);
}

void loop() {
  send_message();
  delay(30000);
}

void send_message(){
  // Read humidity
  float h = dht.readHumidity();
  
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    DEBUG_PRINT("Failed to read from DHT sensor!");
    return;
  }

  float hic = dht.computeHeatIndex(t, h, false); // Compute heat index in Celsius (isFahreheit = false)
  
  String message = "{\"mode\":\"async\",\"messageType\":\"" + message_type_id + "\",\"messages\":[{\"humidity\":" + String(h) + ",\"temperature\":" + String(t) + ", \"heatindex\":"+ String(hic) +"}]}";
  
  DEBUG_PRINT("Humidity: ");
  DEBUG_PRINT(h);
  DEBUG_PRINT(" %\t");
  DEBUG_PRINT("Temperature: ");
  DEBUG_PRINT(t);
  DEBUG_PRINT(" *C ");
  DEBUG_PRINT("Heat index: ");
  DEBUG_PRINT(hic);
  DEBUG_PRINT(" *C ");
  DEBUG_PRINT(message);

  WiFiClientSecure client;
  DEBUG_PRINT("Connecting to: ");
  DEBUG_PRINT(host);

   if (!client.connect(host, httpsPort)) {
     DEBUG_PRINT("connection failed");
    return;
  }
  
  DEBUG_PRINT("requesting URL: ");
  DEBUG_PRINT(url);
  
  // using HTTP/1.0 enforces a non-chunked response
  client.print(String("POST ") + url + device_id + " HTTP/1.0\r\n" +
               "Host: " + host + "\r\n" +
               "Content-Type: application/json;charset=utf-8\r\n" +
               "Authorization: Bearer " + oauth_token + "\r\n" +
               "Content-Length: " + message.length() + "\r\n\r\n" +
               message + "\r\n\r\n");
               
  DEBUG_PRINT("Request sent");
  DEBUG_PRINT("Reply:");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    DEBUG_PRINT(line);
  }
}

void status_led(){
  int state = digitalRead(LED);  // get the current state of GPIO1 pin
  digitalWrite(LED, !state);     // set pin to the opposite state
}

// Helper Function: Start Configuration mode if WiFi not maintained
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Enter Config Mode");
  Serial.println(WiFi.softAPIP());
  ticker.attach(0.2, status_led); //Make led toggle faster to indicate config mode
}
