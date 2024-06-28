#include <Arduino.h>
#include <FastLED.h>
#include <EEPROM.h>
#include "painlessMesh.h"
#include <Arduino_JSON.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MCP23X17.h>

#include "indexM.h" //HTML webpage contains Config items with javascripts
#include "ap.h"     //HTML webpage for changing the main WIFI connection (not the mesh)
#include "logo.h"  //Splash screen logo

// MESH Details for painlessMesh
#define MESH_PREFIX "CUELIGHT"   // name for your MESH
#define MESH_PASSWORD "CUElight" // password for your MESH
#define MESH_PORT 5555           // default port
int nodeNumber = 2;              // Number for this node

// String to send to other nodes with sensor readings
String readings;
Scheduler userScheduler; // to control your personal task
painlessMesh mesh;
WiFiClient wifiClient;

// Define the HTTP server
AsyncWebServer server(80);
IPAddress myIP(0,0,0,0);

IPAddress getlocalIP();

// CHANGE THESE FOR Access Point CONNECTION FOR WHEN CONNECTION FAILS
char ap_ssid[] = "Cue Light Main AP"; // SSID to host network on if no connection is established
char ap_pass[] = "password";          // Password for network that is hosted if there is no network connection established
int connectionTimeout = 20;           // This number * 500 eqauls the number of ms to wait before assuming no network connection

int mode; // connection state

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, 9, 10);

// PINS AND BUTTONS
int StatusLEDpin = 2; // GPIO pin for WIFI status LED D1

// LEDs
const int LED_Pin = 4;  // LED Data Pin
const int NUM_LEDS = 3; // Number of Pixel LEDs connected

// Button pins
const int BUTTON_PINS[] = {0, 1, 2};
const int NUM_BUTTONS = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);
const int SETUP_BUTTONS[] = {5, 6, 7}; // Pins for setup buttons (change, down, up)
bool SButtonState[3] = {true};
int lastSButtonState[3] = {true};
int settingsState = 0; // Which setting mode is in view

Adafruit_MCP23X17 mcp;

// Button states
bool buttonStates[NUM_BUTTONS] = {false};
unsigned long lastButtonTimes[NUM_BUTTONS] = {0};
bool lastButtonStates[NUM_BUTTONS] = {LOW}; // Initialize to LOW


unsigned long previousTimeButton[NUM_BUTTONS] = {0};
unsigned long previousMillis[NUM_BUTTONS] = {0};      // variable to store the time at which the LED was last toggled
unsigned long previousBlinkMillis[NUM_BUTTONS] = {0}; // variable to store the time at which the LED was last toggled
unsigned long previousTimeSButton = 0;
unsigned long previousMillis2 = 0;    // variable to store the time at which the button was last toggled in settings.
unsigned long buttondebounce = 100;                   // button debounce, in milliseconds
unsigned long BLINKinterval = 200;                    // interval at which to toggle the LED, in milliseconds
unsigned long GOinterval = 5000;                      // interval at which to turn off the GO LED, in milliseconds
unsigned long settingsTimeout = 7000; // Time before resetting to main screen
bool ledState[NUM_BUTTONS] = {false};                 // current state of the LED 0=off 1=warning 2=warningACK 3=standby 4=standbyGroup 5=go

// Define the LED strip
CRGB leds[NUM_LEDS];
byte brightness;   // Brightness 0-255
float brightnessP; // Brightness Percent

// Stings that hold the HEX colors
String warningColorHex;
String warningAColorHex;
String standbyColorHex;
String goColorHex;

// Define the colors for the four states
CRGB warningColor;
CRGB warningAColor;
CRGB standbyColor;
CRGB goColor;

// Define the state of the LED strip
int state[NUM_BUTTONS] = {0};
bool buttonPressed[NUM_BUTTONS] = {false}; // flag variable to track button press

// Define the EEPROM addresses for the configuration data
const int ssidEEPROM = 0;
const int passwordEEPROM = 32;
const int idEEPROM = 64;
const int LEDbrightEEPROM = 96; // 1 byte needed
const int warningColorEEPROM = 128;
const int warningAColorEEPROM = 138;
const int standbyColorEEPROM = 148;
const int goColorEEPROM = 158;

bool tryConnection();// NOT USED
void setMode();// NOT USED
void settings();                                 // Used for adjusting unit number, brightness
void sHeadSet();                                 // Heading for settings
void sHeadInfo();                                // Heading for settings
void handleRoot(AsyncWebServerRequest *request);
void apUpdate(AsyncWebServerRequest *request);

// painlessMESH
//  User stub
void sendMessage();       // Prototype so PlatformIO doesn't complain
String getStates();       // Prototype for sending sensor readings
void sendConfigMessage(); // Prototype so PlatformIO doesn't complain
String getConfig();       // Prototype for sending sensor readings
SimpleList<uint32_t> nodes;

// Create tasks: to send messages and get readings;
Task taskSendMessage(TASK_SECOND * 10, TASK_FOREVER, &sendConfigMessage);

String getConfig()
{
  JSONVar jsonConfig;
  jsonConfig["node"] = nodeNumber;
  jsonConfig["warningColor"] = getConfigValue(warningColorEEPROM).substring(1).c_str();
  jsonConfig["standbyColor"] = getConfigValue(standbyColorEEPROM).substring(1).c_str();
  jsonConfig["goColor"] = getConfigValue(goColorEEPROM).substring(1).c_str();
  readings = JSON.stringify(jsonConfig);
  return readings;
}
String getStates()
{
  JSONVar jsonConfig;
  jsonConfig["node"] = nodeNumber;
  jsonConfig["warningColor"] = getConfigValue(warningColorEEPROM).substring(1).c_str();
  jsonConfig["standbyColor"] = getConfigValue(standbyColorEEPROM).substring(1).c_str();
  jsonConfig["goColor"] = getConfigValue(goColorEEPROM).substring(1).c_str();
  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    String unitKey = "unit" + String(i);
    jsonConfig[unitKey] = state[i];
  }
  readings = JSON.stringify(jsonConfig);
  return readings;
}

void sendConfigMessage()
{
  String msg = getConfig();
  mesh.sendBroadcast(msg);
}

void sendMessage()
{
  String msg = getStates();
  mesh.sendBroadcast(msg);
}

// Needed for painless library
void receivedCallback(uint32_t from, String &msg)
{
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  JSONVar myObject = JSON.parse(msg.c_str());
  int node = myObject["unitnum"];
  Serial.print("Unit Responding: ");
  Serial.println(node);
  state[node] = myObject["responce"];
  sendMessage();
}

void newConnectionCallback(uint32_t nodeId)
{
  Serial.printf("New Connection, nodeId = %u\n", nodeId);
  Serial.printf("New Connection, %s\n", mesh.subConnectionJson(true).c_str());
}

void changedConnectionCallback()
{
  Serial.printf("Changed connections\n");
  nodes = mesh.getNodeList();

  Serial.printf("Num nodes: %d\n", nodes.size());
  Serial.printf("Connection list:");

  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end())
  {
    Serial.printf(" %u", *node);
    node++;
  }
  Serial.println();
}

void nodeTimeAdjustedCallback(int32_t offset)
{
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

IPAddress getlocalIP() {
  return IPAddress(mesh.getStationIP());
}


String scanprocessor(const String &var)
{
  if (var == "SCAN")
    return mesh.subConnectionJson(false);
  return String();
}


// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 1040)
const int epd_bitmap_allArray_LEN = 1;
const unsigned char* epd_bitmap_allArray[1] = {
  epd_bitmap_MST_Acronym_Logo_White
};


void setup()
{
  // Init Serial Monitor
  Serial.begin(115200);
  // uncomment appropriate mcp.begin
  if (!mcp.begin_I2C()) {
  //if (!mcp.begin_SPI(CS_PIN)) {
    Serial.println("Error.");
    while (1);
  }
  for (uint8_t  i = 0; i < NUM_BUTTONS; i++)
  {
    mcp.pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    buttonStates[i] = digitalRead(BUTTON_PINS[i]) == LOW;
  }
 
//    mcp.pinMode(BUTTON_TEST, INPUT_PULLUP);

  // Initialize setup button pins
  for (uint8_t  i = 0; i < 3; i++)
  {
    pinMode(SETUP_BUTTONS[i], INPUT_PULLUP);
    SButtonState[i] = digitalRead(SETUP_BUTTONS[i]) == LOW;
  }

 // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Clear the buffer
  display.clearDisplay();
  display.setTextColor(WHITE);

  pinMode(StatusLEDpin, OUTPUT);

  // Initialize the EEPROM
  EEPROM.begin(512);

  // Initialize the LED strip
  FastLED.addLeds<WS2811, LED_Pin, RGB>(leds, NUM_LEDS);

  // Initialize the colors for the three states from the saved values
  long hexValue = strtol(getConfigValue(warningColorEEPROM).substring(1).c_str(), NULL, 16);
  warningColor = CRGB((hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF);

  long hexValue1 = strtol(getConfigValue(warningAColorEEPROM).substring(1).c_str(), NULL, 16);
  warningAColor = CRGB((hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF); // CURRENTLY MATCHES WARNING COLOR  WEBSITE ONLY HAS 3 OPTIONS

  long hexValue2 = strtol(getConfigValue(standbyColorEEPROM).substring(1).c_str(), NULL, 16);
  standbyColor = CRGB((hexValue2 >> 16) & 0xFF, (hexValue2 >> 8) & 0xFF, hexValue2 & 0xFF);

  long hexValue3 = strtol(getConfigValue(goColorEEPROM).substring(1).c_str(), NULL, 16);
  goColor = CRGB((hexValue3 >> 16) & 0xFF, (hexValue3 >> 8) & 0xFF, hexValue3 & 0xFF);

/*  // Initialize button pins
  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    buttonStates[i] = digitalRead(BUTTON_PINS[i]) == LOW;
  }
  */
  // painlessMESH
  // mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes(ERROR | MESH_STATUS | STARTUP | CONNECTION ); // set before init() so that you can see startup messages

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  // mesh.stationManual(getConfigValue(ssidEEPROM), getConfigValue(passwordEEPROM));

  mesh.setRoot(true);
  // This node and all other nodes should ideally know the mesh contains a root, so call this on all nodes
  mesh.setContainsRoot(true);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();

  server.onNotFound(notFound);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              String s = MAIN_page;
              s.replace("@@warncolor@@", getConfigValue(warningColorEEPROM));
              s.replace("@@standcolor@@", getConfigValue(standbyColorEEPROM));
              s.replace("@@gocolor@@", getConfigValue(goColorEEPROM));
             // s.replace("@@brightness@@", getConfigValue(LEDbrightEEPROM));
              s.replace("@@password@@", getConfigValue(passwordEEPROM));
              s.replace("@@mac@@", WiFi.macAddress());
              s.replace("@@id@@", getConfigValue(idEEPROM));
              //s.replace("@@brightness@@", getConfigValue(LEDbrightEEPROM));
              request->send(200, "text/html", s); });
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              String brightness = request->hasArg("BRIGHTNESS") ? request->arg("BRIGHTNESS") : "";
                /*if (brightness != getConfigValue(LEDbrightEEPROM))
                  {
                    updateConfigValue(LEDbrightEEPROM, brightness.c_str());
                    Serial.println(" Updated Brightness");
                  }*/
              warningColorHex = request->arg("WARN");
                if (warningColorHex != getConfigValue(warningColorEEPROM))
                  {
                    updateConfigValue(warningColorEEPROM, warningColorHex.c_str());
                    long hexValue = strtol(warningColorHex.substring(1).c_str(), NULL, 16);
                    warningColor = CRGB((hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF);
                    Serial.println(" Warning Color Updated");
                  }
              standbyColorHex = request->arg("STAND");
                if (standbyColorHex != getConfigValue(standbyColorEEPROM))
                  {
                    updateConfigValue(standbyColorEEPROM, standbyColorHex.c_str());
                    long hexValue = strtol(standbyColorHex.substring(1).c_str(), NULL, 16);
                    standbyColor = CRGB((hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF);
                    Serial.println(" Stand By Color Updated");
                   }
              goColorHex = request->arg("GO");
                if (goColorHex != getConfigValue(goColorEEPROM))
                  {
                    updateConfigValue(goColorEEPROM, goColorHex.c_str());
                    long hexValue = strtol(goColorHex.substring(1).c_str(), NULL, 16);
                    goColor = CRGB((hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF);
                    Serial.println(" Go Color Updated");
                  } });
  server.on("/ap", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              String s = AP_page;
              s.replace("@@mac@@", WiFi.macAddress());
              request->send(200, "text/html", s); });
  server.on("/ap", HTTP_POST, [](AsyncWebServerRequest *request)
            {            
              String ssid = request->arg("SSID");
              String password = request->arg("PASSWORD");

              // Update the values in EEPROM
              updateConfigValue(ssidEEPROM, ssid.c_str());
              updateConfigValue(passwordEEPROM, password.c_str());

              // Restart the device to apply the new configuration
              ESP.restart(); });
  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "application/json", mesh.subConnectionJson(false)); });
  server.begin();
    brightness = getConfigValue(LEDbrightEEPROM).toInt();
}

/*
bool tryConnection()
{
  // Connect to the WiFi network using the SSID and password from EEPROM
  WiFi.mode(WIFI_STA);
  //WiFi.begin(getConfigValue(ssidEEPROM), getConfigValue(passwordEEPROM));
  mesh.stationManual(getConfigValue(ssidEEPROM), getConfigValue(passwordEEPROM));
  Serial.print("Connecting to ");
  Serial.print(getConfigValue(ssidEEPROM));

  int timeout = 0;
  while (timeout < connectionTimeout)
  { // start of loop that continually checks wifi status up to a user-defined amount
    if (WiFi.status() == WL_CONNECTED)
    {
      return true; // if wireless is connceted return true
    }
    digitalWrite(StatusLEDpin, LOW);
    Serial.print(".");
    delay(250);
    digitalWrite(StatusLEDpin, HIGH);
    delay(250);
    timeout++;

  }
  Serial.println("");
  WiFi.disconnect(); // if connection attempts reaches connection_timeout assume no connection was made; disconnect and return false
  return false;
}

void setMode()
{
  bool success = tryConnection(); // determine connection state
  if (!success)
  { // if unable to connect start an access point with user-defined ssid and pass and set root webpage to ap configuration
    mode = 0;

    // If not, start a network with a default SSID and password
    WiFi.softAP(ap_ssid, ap_pass);
    Serial.println("Connection failed starting access point");
    Serial.println(ap_ssid);
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    server.on("/ap", HTTP_GET, apUpdate);
    digitalWrite(StatusLEDpin, HIGH);
    server.begin();
  }
  else
  { // if connection is succesful set appropriate page forwards
    mode = 1;
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", "Hello, world.  This is in SetMode"); });
    //  server.on("/", HTTP_GET, handleRoot);
    server.on("/ap", HTTP_GET, apUpdate);
     Serial.println();
    Serial.println("Successful");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(StatusLEDpin, LOW);
    server.begin();
  }
}

*/
void loop()
{
/*    if (!mcp.digitalRead(BUTTON_TEST)) {
    Serial.println("Button Pressed!");
  }
*/  
  mesh.update();
  if(myIP != getlocalIP()){
    myIP = getlocalIP();
    Serial.println("My IP is " + myIP.toString());
  }
  std::list<uint32_t> nodeList = mesh.getNodeList();
  
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(4, 0);
  display.println("StandbyGo Cue Lights");
  display.setCursor(0,30);
  display.print("Number of nodes: ");
  display.setTextSize(2);
  display.setCursor(0,40);
  display.print(nodeList.size());
  display.setTextSize(1);
  display.setCursor(0,55);
  display.print(state[0]);
  display.setCursor(10,55);
  display.print(state[1]);
  display.setCursor(20,55);
  display.print(state[2]);

  settings();
  
  unsigned long currentMillis = millis(); // get the current time

SButtonState[0] = digitalRead(SETUP_BUTTONS[0]);
  SButtonState[1] = digitalRead(SETUP_BUTTONS[1]);
  SButtonState[2] = digitalRead(SETUP_BUTTONS[2]);
  /*if (currentMillis - previousTimeButton > buttondebounce && digitalRead(SETUP_BUTTONS[2]) == LOW)
  {
    ESP.restart();
  }*/
  if (currentMillis - previousTimeSButton > buttondebounce && SButtonState[0] != lastSButtonState[0])
  {
    if (SButtonState[0] == LOW)
    {
      settingsState++;
    }
    previousMillis2 = currentMillis;
    previousTimeSButton = currentMillis;
    if (settingsState > 5)
    {
      settingsState = 0;
    }
    lastSButtonState[0] = SButtonState[0];
  }
  

 // byte brightness = getConfigValue(LEDbrightEEPROM).toInt();
  FastLED.setBrightness(brightness);
  for (uint8_t  i = 0; i < NUM_BUTTONS; i++)
  {
    lastButtonStates[i] = mcp.digitalRead(BUTTON_PINS[i]);
    // if the button is pressed and the current state is less than 5, increase the state by 1
    if (currentMillis - previousTimeButton[i] > buttondebounce && lastButtonStates[i] == LOW && !buttonPressed[i])
    {
      previousTimeButton[i] = currentMillis;
      if (state[i] == 1)
      {
        state[i] = 3;
      }
      else
      {
        state[i]++;
      }
      buttonPressed[i] = true;
      if (state[i] > 5)
      {
        state[i] = 0;
      }
      sendMessage();
      Serial.print("State increased to ");
      Serial.println(state[i]);
        for (int z = 0; z < NUM_BUTTONS; z++)
  {
    Serial.print(state[z]);
  }
  Serial.println();
    }

    // if the button is not pressed, reset the flag variable
    if (lastButtonStates[i] == HIGH)
    {
      buttonPressed[i] = false;
    }
    else
    {
      previousMillis[i] = currentMillis;
      buttonPressed[i] = true;
    }
    // Update the LED strip based on the state
    switch (state[i])
    {
    case 0:
      // Off state
      leds[i] = CRGB(0, 0, 0);
      break;
    case 1:
      // Warning state
      FastLED.setBrightness(brightness);
     if (currentMillis - previousBlinkMillis[i] >= BLINKinterval){
      ledState[i] = !ledState[i];  // toggle the LED state
      previousBlinkMillis[i] = currentMillis; // update the previous time
      if (!ledState[i])
      {
        leds[i] = warningColor; // turn on the LED
      }
      else
      {
        leds[i] = CRGB(0, 0, 0); // turn off the LED
      }}
      break;
    case 2:
      // Warning Acknowledged state
       leds[i] = warningColor;
      // fill_solid(leds, NUM_LEDS, warningColor);
      break;
    case 3:
      // Standby state
      leds[i] = standbyColor;
      // fill_solid(leds, NUM_LEDS, standbyColor);
      break;
    case 4:
      // Standby Group state
      FastLED.setBrightness(brightness);
     if (currentMillis - previousBlinkMillis[i] >= BLINKinterval/2){
      ledState[i] = !ledState[i];  // toggle the LED state
      previousBlinkMillis[i] = currentMillis; // update the previous time
      if (!ledState[i])
      {
        leds[i] = standbyColor; // turn on the LED
      }
      else
      {
        leds[i] = CRGB(0, 0, 0); // turn off the LED
      }}
      // fill_solid(leds, NUM_LEDS, standbyColor);
      break;      
    case 5:
      //  Go state
      if (currentMillis - previousMillis[i] < GOinterval)
      {
        leds[i] = goColor;
        // fill_solid(leds, NUM_LEDS, goColor);
      }
      else
      {
        state[i] = 0;
        sendMessage();
      }
      break;
    }
    // Show the updated LED strip
    FastLED.show();
      display.display(); 
  }
}

void settings()
{
  unsigned long currentMillis = millis(); // get the current time

  if (currentMillis - previousMillis2 >= settingsTimeout)
  {
    previousMillis2 = currentMillis; // update the time at which the LED was last toggled
    settingsState = 0;               // reset settings
  }
  switch (settingsState)
  {
  case 0:
  { // Blank screen
  }
  break;
  case 1:
  { // Unit Number
    sHeadSet();
    display.print("Change Unit #: ");
    /*display.print(unitnum);
    if ((currentMillis - previousTimeSButton > buttondebounce && SButtonState[1] != lastSButtonState[1]) && unitnum > 0)
    {
      if (SButtonState[1] == LOW)
      {
        unitnum--;
      }
      previousTimeSButton = currentMillis;
      previousMillis2 = currentMillis;
      lastSButtonState[1] = SButtonState[1];
    }
    if ((currentMillis - previousTimeSButton > buttondebounce && SButtonState[2] != lastSButtonState[2]) && unitnum < 15)
    {
      if (SButtonState[2] == LOW)
      {
        unitnum++;
      }
      previousTimeSButton = currentMillis;
      previousMillis2 = currentMillis;
      lastSButtonState[2] = SButtonState[2];
    }
    if ((getConfigValue(UnitNumEEPROM).toInt() != unitnum) && getConfigValue(UnitNumEEPROM) != NULL)
    {
      updateConfigValue(UnitNumEEPROM, String(unitnum).c_str());
    }*/
    break;
  }
  case 2:
  { // Brightness
    sHeadSet();
    display.print("Brightness: ");
    float brightnessP = (float(getConfigValue(LEDbrightEEPROM).toInt()) / 255) * 100;
    display.print(brightnessP, 1);
    display.print("%(");
    display.print(brightness);
    display.print(")");

    if (currentMillis - previousTimeSButton > buttondebounce)
    {
      if (SButtonState[1] != lastSButtonState[1] && brightness > 0 && SButtonState[1] == LOW)
      {
        brightness -= 5;
        previousMillis2 = currentMillis;
        previousTimeSButton = currentMillis;
      }
      if (SButtonState[2] != lastSButtonState[2] && brightness < 255 && SButtonState[2] == LOW)
      {
        brightness += 5;
        previousMillis2 = currentMillis;
        previousTimeSButton = currentMillis;
      }
      lastSButtonState[1] = SButtonState[1];
      lastSButtonState[2] = SButtonState[2];
    }
    if ((getConfigValue(LEDbrightEEPROM).toInt() != brightness))
    {
      updateConfigValue(LEDbrightEEPROM, String(brightness).c_str());
    }
    break;
  }
  case 3:
  { // Color Order
    sHeadSet();
    display.print("Color Order: ");
    display.print("Soon?");
    break;
  }

  case 4:
  { // View IP
    sHeadInfo();
    display.print(" IP: ");
    display.print(getlocalIP());
    break;
  }
  case 5:
  { // View number of nodes connected in the mesh
    std::list<uint32_t> nodeList = mesh.getNodeList();
    sHeadInfo();
    display.print(" Total nodes: ");
    display.println(nodeList.size());
    break;
  }
  break;
  }
}

void sHeadSet()
{
  display.drawLine(0, 13, 41, 13, WHITE);
  display.drawLine(86, 13, 128, 13, WHITE);
  display.setCursor(43, 10);
  display.println("SETTING");
}
void sHeadInfo()
{
  display.drawLine(0, 13, 30, 13, WHITE);
  display.drawLine(97, 13, 128, 13, WHITE);
  display.setCursor(31, 10);
  display.println("INFORMATION");
}

// Read a string value from EEPROM
String getConfigValue(int address)
{
  String value = "";
  for (int i = address; i < address + 32; i++)
  {
    char c = char(EEPROM.read(i));
    if (c == '\0')
    {
      break;
    }
    value += c;
  }
  return value;
}

// Write a string value to EEPROM
void updateConfigValue(int address, const char *value)
{
  int i = 0;
  while (i < 32 && value[i] != '\0')
  {
    EEPROM.write(address + i, value[i]);
    i++;
  }
  EEPROM.write(address + i, '\0');
  EEPROM.commit();
}
