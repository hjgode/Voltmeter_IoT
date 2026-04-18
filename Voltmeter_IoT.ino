#include <WiFi.h>
#include <ArduinoOTA.h>

#include <ESPAsyncWebServer.h>

//#include <PubSubClient.h>
#include <MQTTClient.h>

WiFiClient wifiClient;
//PubSubClient client(wifiClient);
MQTTClient mqtt = MQTTClient(256);
#define CLIENT_ID "ESP32-001"  // CHANGE IT AS YOU DESIRE

const char MQTT_BROKER_ADRRESS[] = "192.168.0.40";  // CHANGE TO MQTT BROKER'S IP ADDRESS
const int MQTT_PORT = 1883;

//time
#include <time.h>

//delay
unsigned long lastExecutedMillis_1 = 0; // vairable to save the last executed time for code block 1

//IPAddress mqttserver=(192,168,0,40);
#define PUBLISH_TOPIC "esp32-001/voltage"
#define PUBLISH_TOPIC_RAW "esp32-001/rawADC"

// Wi-Fi credentials for AP mode
const char* ssid = "Horst1";
const char* password = "1234567890123";

// ADC Pin
const int adcPin = 34; // GPIO34, ADC1 Ch6

// Voltage Divider for 24V Range
const float R1 = 100000.0; // Resistor R1 value in ohms
const float R2 = 10000.0;  // Resistor R2 value in ohms
const float Voffset = 1.3;

// ADC Parameters
const float ADC_MAX_VOLTAGE = 3.3; // Max input voltage to the ADC
const int ADC_RESOLUTION = 4096;  // 12-bit ADC resolution

// Voltage Range Settings
bool is24VRange = true; // Start with 24V or 3.3V range

// Create server
AsyncWebServer server(80);

// HTML, CSS, and JS for the Web UI
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>IndusVoltmeter</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            margin: 0;
            padding: 0;
            background-color: #f4f4f9;
        }
        .container {
            margin: 2rem auto;
            max-width: 500px;
            padding: 20px;
            background: #ffffff;
            box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.2);
            border-radius: 10px;
        }
        h1 {
            color: #333;
        }
        .voltage-display {
            font-size: 2rem;
            color: #555;
        }
        .toggle-button {
            display: inline-block;
            margin-top: 1.5rem;
            padding: 10px 20px;
            font-size: 1rem;
            color: #fff;
            background: #0078D4;
            border: none;
            border-radius: 5px;
            cursor: pointer;
        }
        .toggle-button:hover {
            background: #005a9e;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>IndusVoltmeter</h1>
        <p class="voltage-display">Voltage: <span id="voltage">--</span> V</p>
        <button id="rangeToggle" class="toggle-button" disabled>Switch to 3V3 Range</button>
    </div>
    <script>
        const voltageElement = document.getElementById("voltage");
        const toggleButton = document.getElementById("rangeToggle");
        let range = "3.3"; // Start with the 3.3V range

        // Function to fetch voltage data
        async function fetchVoltage() {
            const response = await fetch("/voltage");
            const data = await response.json();
            voltageElement.textContent = data.voltage.toFixed(2);
        }

        // Function to toggle range
        toggleButton.addEventListener("click", async () => {
            range = range === "3.3" ? "24" : "3.3";
            const response = await fetch(`/setrange?range=${range}`);
            const data = await response.json();
            toggleButton.textContent = `Switch to ${data.nextRange}V Range`;
        });

        // Periodically fetch voltage
        setInterval(fetchVoltage, 1000);
    </script>
</body>
</html>
)rawliteral";

void connectToMQTT() {
  // Connect to the MQTT broker
  mqtt.begin(MQTT_BROKER_ADRRESS, MQTT_PORT, wifiClient);

  // Create a handler for incoming messages
//  mqtt.onMessage(messageHandler);

  Serial.print("ESP32 - Connecting to MQTT broker");

  while (!mqtt.connect(CLIENT_ID, "", "")) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (!mqtt.connected()) {
    Serial.println("ESP32 - MQTT broker Timeout!");
    return;
  }
/*
  // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  if (mqtt.subscribe(SUBSCRIBE_TOPIC))
    Serial.print("ESP32 - Subscribed to the topic: ");
  else
    Serial.print("ESP32 - Failed to subscribe to the topic: ");

  Serial.println(SUBSCRIBE_TOPIC);
*/
  Serial.println("ESP32  - MQTT broker Connected!");
}
void connectWifi(){
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());  
    //WiFi.setAutoReconnect(true);
}

//time
void setTimeZone(){
  setenv("TZ","WET0WEST,M3.5.0/1,M10.5.0",1);
  tzset();  
}

void getTime(){
  configTime(0, 0, "pool.ntp.org");    // First connect to NTP server, with 0 TZ offset
  setTimeZone();
}

void printLocalTime(){
  struct tm timeinfo;
  /*
  %A  Full weekday name
  %B  Full month name
  %d  Day of the month
  %Y  Year
  %H  Hour in 24h format
  %I  Hour in 12h format
  %M  Minute
  %S  Second
  */
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time 1");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
}

float getVoltage(){
    float rawADC = analogRead(adcPin);
    float measuredVoltage = (rawADC * ADC_MAX_VOLTAGE / ADC_RESOLUTION);
    float actualVoltage = is24VRange ? (measuredVoltage * ((R1 + R2) / R2)) : measuredVoltage;
    actualVoltage = actualVoltage >0 ? actualVoltage += Voffset : actualVoltage ;

    Serial.print("Raw ADC: ");      //3v3   1v5
    Serial.println(rawADC);         //224   23
    Serial.print("Voltage: ");  
    Serial.println(actualVoltage);  //1.99  0.2

    return actualVoltage;
}

float getRawADC(){
    float rawADC = analogRead(adcPin);
    Serial.print("Raw ADC: ");      //3v3   1v5
    Serial.println(rawADC);         //224   23

    return rawADC;
}

bool timeElapsed(int mydelay){
   unsigned long currentMillis = millis(); 
   if (currentMillis - lastExecutedMillis_1 >= mydelay) {
    lastExecutedMillis_1 = currentMillis; // save the last executed time
    return true;
   }
   return false;  
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Configure ADC
  analogReadResolution(12); // Set ADC resolution to 12 bits
  analogSetAttenuation(ADC_11db); // Configure ADC for 0-3.3V range

  // start Wifi Client
  connectWifi();
  /*
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
  */  
    ArduinoOTA.begin();  // Starts OTA

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
/*
  // Start AP Mode
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
*/
  
  // Serve the HTML Page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", htmlPage);
  });

  // Endpoint to Get Voltage
  server.on("/voltage", HTTP_GET, [](AsyncWebServerRequest* request) {
/*
    float rawADC = analogRead(adcPin);
    float measuredVoltage = (rawADC * ADC_MAX_VOLTAGE / ADC_RESOLUTION);
    float actualVoltage = is24VRange ? (measuredVoltage * ((R1 + R2) / R2)) : measuredVoltage;
    actualVoltage = actualVoltage >0 ? actualVoltage += Voffset : actualVoltage ;
*/
    float actualVoltage = getVoltage();    
    String json = "{\"voltage\": " + String(actualVoltage, 2) + "}";
    request->send(200, "application/json", json);
  });

  // Endpoint to Set Voltage Range
  server.on("/setrange", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (request->hasParam("range")) {
      String range = request->getParam("range")->value();
      is24VRange = (range == "24");
    }

    String nextRange = is24VRange ? "3.3" : "24";
    String json = "{\"nextRange\": \"" + nextRange + "\"}";
    request->send(200, "application/json", json);
  });

  // Start server
  server.begin();
}

void loop() {
  ArduinoOTA.handle();  // Handles OTA
  
  // Main loop can be empty since everything is handled by the server
/*
  float rawADC = analogRead(adcPin);
  float measuredVoltage = (rawADC * ADC_MAX_VOLTAGE / ADC_RESOLUTION);
  float actualVoltage = is24VRange ? (measuredVoltage * ((R1 + R2) / R2)) : measuredVoltage;
  Serial.print("Raw ADC: ");      //3v3   1v5
  Serial.println(rawADC);         //224   23
  Serial.print("Voltage: ");  
  Serial.println(actualVoltage + Voffset);  //1.99  0.2
*/

  //only execute every x seconds (millis)
  if (timeElapsed(5000)) {
    if (WiFi.status() != WL_CONNECTED){
      connectWifi();
    }
    float actualVoltage=getVoltage();
    float rawADC=getRawADC();
    if (!mqtt.connected()) {
      Serial.println("reconnect mqtt...");
      connectToMQTT();
    }
    else{
      Serial.println("publish...");

      String strVoltage=String(actualVoltage,1);
      String strPublishTopic=String(PUBLISH_TOPIC);
      String strRawADC=String(rawADC);
      String strPublishTopicRaw=String(PUBLISH_TOPIC_RAW);
      
      mqtt.publish(strPublishTopic.c_str(), strVoltage.c_str()); //, strlen(strVoltage.c_str()), true); //retain=true
      mqtt.publish(strPublishTopicRaw.c_str(), strRawADC.c_str()); //, strlen(strRawADC.c_str()), true); //retain=true
/*
      char tempString[8];
      dtostrf(actualVoltage, 4, 2, tempString);
      mqtt.publish(PUBLISH_TOPIC,tempString, true); //retain=true
*/      
    }
  }
  mqtt.loop();

  //sleep(5);
}
