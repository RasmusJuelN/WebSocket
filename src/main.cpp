// Import required libraries
#ifdef ESP32
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#else
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#endif
#include <OneWire.h>
#include <DallasTemperature.h>
// #include <SPIFFS.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Arduino_JSON.h>
#include "JSON.h"
#include <SPIFFS.h>

// Data wire is connected to GPIO 4
// #define ONE_WIRE_BUS 4
#define SENSOR_PIN 4
OneWire ds(4);

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(SENSOR_PIN);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensor(&oneWire);

AsyncEventSource events("/events");
JSONVar readings;

// Variables to store temperature values
String temperatureC = "";

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 3000;

// Replace with your network credentials
const char *ssid = "AndroidAP";
const char *password = "ralle123";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Create the necessary objects for NTP synchronization
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

DeviceAddress sensor1 = {0x28, 0xFF, 0x64, 0x1E, 0x31, 0x97, 0x87, 0xBC};

String getSensorReadings(){
  sensor.requestTemperatures();
  readings["sensor1"] = String(sensor.getTempC(sensor1));

  String jsonString = JSON.stringify(readings);
  return jsonString;
}


void initSDCard(){
  if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

String readDSTemperatureC()
{
  // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  sensor.requestTemperatures();
  float tempC = sensor.getTempCByIndex(0);

  if (tempC == -127.00)
  {
    Serial.println("Failed to read from DS18B20 sensor");
    return "--";
  }
  else
  {
    Serial.print("Temperature Celsius: ");
    Serial.println(tempC);
  }
  return String(tempC);
}

void initWifi()
{
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // Print ESP Local IP Address
  // TODO: Explain the string concatenation in code comments
  Serial.println("\n" + WiFi.localIP().toString());
}

void initSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
    Serial.println("Websocket client connection received");

  else if (type == WS_EVT_DISCONNECT)
    Serial.println("Client disconnected");

  else if (type == WS_EVT_DATA)
    Serial.println("Data received");

    char *receivedData = reinterpret_cast<char *>(data);

    if (strncmp(receivedData, "getReadings", len) == 0)
    {
      String temperature = readDSTemperatureC();
      client->text(temperature);
    }
}

void initWebSocket()
{
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
}

File dataFile;

void writeToSDCard(String data)
{
  dataFile = SD.open("/temperature_log.csv", FILE_APPEND);

  if (dataFile)
  {
    dataFile.println(data);
    dataFile.close();
  }
  else
  {
    Serial.println("Error opening temperature log file");
  }
}

void initNTP()
{
  timeClient.begin();
  timeClient.setTimeOffset(3600); // Set the time offset in seconds (if needed)
}

void handleDataRequest(AsyncWebServerRequest *request) {
    File file = SD.open("/temperature_log.csv");
    if (file) {
        // Read the contents of the CSV file and send it as JSON
        String jsonData = "[";

        while (file.available()) {
            String line = file.readStringUntil('\n');
            if (line.length() > 0) {
                // Assuming the CSV format is "timestamp,temperature"
                int commaIndex = line.indexOf(',');
                String timestamp = line.substring(0, commaIndex);
                String temperature = line.substring(commaIndex + 1);

                Serial.print("Timestamp: ");
                Serial.println(timestamp);
                Serial.print("Temperature: ");
                Serial.println(temperature);

                // Create a data point with {x, y} properties
                jsonData += "{\"x\":\"" + timestamp + "\",\"y\":" + temperature + "},";
            }
        }
        jsonData.remove(jsonData.length() - 1); // Remove the trailing comma
        jsonData += "]";
        file.close();

        request->send(200, "application/json", jsonData);
    } else {
        request->send(404, "text/plain", "File not found");
    }
}

void setup()
{
  // Serial port for debugging purposes
  Serial.begin(115200);
  Serial.println();

  // Start up the DS18B20 library
  sensor.begin();

  // temperatureC = readDSTemperatureC();

  initWifi();
  initSPIFFS();
  initWebSocket();
  initSDCard();
  initNTP();
 

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SD, "/index.html", "text/html"); });

  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = getSensorReadings();
    request->send(200, "application/json", json);
    json = String();
  });

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  server.on("/download-csv", HTTP_GET, [](AsyncWebServerRequest *request){
    File file = SD.open("/temperature_log.csv");
    if (file) {
        // Set the content type to CSV
        request->send(200, "text/csv", file.readString());
        file.close();
    } else {
        request->send(404, "text/plain", "File not found");
    }
  });

  server.on("/clear-csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Open the file in write mode, which clears its contents
    File file = SD.open("/temperature_log.csv", FILE_WRITE);
    
    if (file) {
        file.close();
        request->send(200, "text/plain", "CSV file cleared");
    } else {
        request->send(500, "text/plain", "Error clearing CSV file");
    }
  });

  server.serveStatic("/", SD, "/"); // TODO Can I remove this?

  dataFile = SD.open("/temperature_log.csv", FILE_WRITE);
  if (dataFile)
  {
    dataFile.println("Time,Temperature (Celsius)"); // Add header to the CSV file
    dataFile.close();
  }
  else
  {
    Serial.println("Error opening temperature log file");
  }

  server.begin();
}

void loop()
{

  byte i;
  byte addr[8];
  
  // if (!ds.search(addr)) {
  //   Serial.println(" No more addresses.");
  //   Serial.println();
  //   ds.reset_search();
  //   delay(250);
  //   return;
  // }
  // Serial.print(" ROM =");
  // for (i = 0; i < 8; i++) {
  //   Serial.write(' ');
  //   Serial.print(addr[i], HEX);
  // }

  if ((millis() - lastTime) > timerDelay)
  {
    temperatureC = readDSTemperatureC();

    events.send("ping",NULL,millis());
    events.send(getSensorReadings().c_str(),"new_readings" ,millis());
    // Log temperature to CSV file with timestamp
    timeClient.forceUpdate();
    String currentTime = timeClient.getFormattedTime();
    String logData = currentTime + "," + temperatureC;
    writeToSDCard(logData);

    ws.textAll(temperatureC);
    lastTime = millis();
  }
}