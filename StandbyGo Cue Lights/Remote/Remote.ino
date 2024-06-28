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

#include "index.h" //HTML webpage contents with javascripts
#include "ap.h"    //HTML webpage for changing the WIFI connection
#include "logo.h"  //Splash screen logo

int unitnum;

// MESH Details for painlessMesh
#define MESH_PREFIX "CUELIGHT"   // name for your MESH
#define MESH_PASSWORD "CUElight" // password for your MESH
#define MESH_PORT 5555           // default port

// String to send to other nodes with sensor readings
String readings;
Scheduler userScheduler; // to control your personal task
painlessMesh mesh;
WiFiClient wifiClient;

// Define the HTTP server
AsyncWebServer server(80);
IPAddress myIP(0, 0, 0, 0);

IPAddress getlocalIP();

// CHANGE THESE TO DESIRED VALUES
char ap_ssid[] = "Cue Light Receiver AP"; // SSID to host network on if no connection is established
char ap_pass[] = "password";              // Password for network that is hosted if there is no network connection established
int connectionTimeout = 20;               // This number * 500 eqauls the number of ms to wait before assuming no network connection

// SCREEN SETUP
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// PINS AND BUTTONS
int StatusLEDpin = 2;                     // GPIO pin for WIFI status LED D1
int buttonPin = D3;                       // GPIO pin for responce
const int LED_Pin = D8;                   // LED Data Pin
const int NUM_LEDS = 2;                   // Number of Pixel LEDs connected
const int SETUP_BUTTONS[] = {D5, D6, D7}; // Pins for setup buttons (change, down, up)
bool SButtonState[3] = {true};
int lastSButtonState[3] = {true};
int settingsState = 0; // Which setting mode is in view

unsigned long previousTimeButton = 0;
unsigned long previousMillis = 0;     // variable to store the time at which the LED was last toggled. Handles Blink for warning
unsigned long previousMillis2 = 0;    // variable to store the time at which the button was last toggled in settings.
unsigned long buttondebounce = 100;   // button debounce, in milliseconds
unsigned long interval = 200;         // interval at which to toggle the LED, in milliseconds
unsigned long GOinterval = 5000;      // interval at which to toggle the LED, in milliseconds.  NOT USED (turned off by master)
unsigned long settingsTimeout = 7000; // Time before resetting to main screen
bool ledState = false;                // current state of the LED

// Define the LED strip
CRGB leds[NUM_LEDS];
byte brightness;   // Brightness 0-255
float brightnessP; // Brightness Percent

// Define the colors for the four states
CRGB warningColor;
CRGB warningAColor;
CRGB standbyColor;
CRGB goColor;

// Define the state of the LED strip
int state = 0;              // 0=off 1=warning 2=warningACK 3=standby 4=standbyGroup 5=go
bool buttonPressed = false; // flag variable to track button press of the responce button

// Define the EEPROM addresses for the configuration data
const int ssidEEPROM = 0;
const int passwordEEPROM = 32;
const int idEEPROM = 64;
const int masterIPAddress = 96;
const int LEDbrightEEPROM = 128; // 1 byte needed
const int UnitNumEEPROM = 132;   // 1 byte needed

int mode; // connection state of the mesh

bool tryConnection();                            // NOT USED
void setMode();                                  // NOT USED
void settings();                                 // Used for adjusting unit number, brightness
void sHeadSet();                                 // Heading for settings
void sHeadInfo();                                // Heading for settings
void handleRoot(AsyncWebServerRequest *request); // NOT USED
void apUpdate(AsyncWebServerRequest *request);   // NOT USED

// painlessMESH
//  User stub
void sendMessage();   // Prototype so PlatformIO doesn't complain
String getReadings(); // Gets repsonce from button press
SimpleList<uint32_t> nodes;
//
// Create tasks: to send messages and get readings;
// Task taskSendMessage(TASK_SECOND * 5, TASK_FOREVER, &sendMessage);
//
String getReadings()
{
  JSONVar jsonReadings;
  jsonReadings["unitnum"] = unitnum;
  jsonReadings["responce"] = 2; // 2=Warning acknologed
  readings = JSON.stringify(jsonReadings);
  return readings;
}

void sendMessage()
{
  String msg = getReadings();
  mesh.sendBroadcast(msg);
}

// Needed for painless library
void receivedCallback(uint32_t from, String &msg)
{
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  JSONVar myObject = JSON.parse(msg.c_str());
  int node = myObject["node"];
  long hexValue = strtol(myObject["warningColor"], NULL, 16);
  warningColor = CRGB((hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF);
  long hexValue2 = strtol(myObject["standbyColor"], NULL, 16);
  standbyColor = CRGB((hexValue2 >> 16) & 0xFF, (hexValue2 >> 8) & 0xFF, hexValue2 & 0xFF);
  long hexValue3 = strtol(myObject["goColor"], NULL, 16);
  goColor = CRGB((hexValue3 >> 16) & 0xFF, (hexValue3 >> 8) & 0xFF, hexValue3 & 0xFF);
  if (myObject["unit" + String(unitnum)] == NULL)
  {
    // do nothing
  }
  else
  {
    state = myObject["unit" + String(unitnum)];
  }
  Serial.print("Nodes: ");
  Serial.println(node);

  // THIS NEEDS WORK TO DISPLAY THEM COLOR CODES CORRECTLY IN SERIAL MONITOR
  Serial.print("Warning Color: ");
  hexValue = (warningColor.r << 16) | (warningColor.g << 8) | warningColor.b;
  String hexString = String("#" + String(hexValue, HEX));
  Serial.println(hexString);

  Serial.print("Standby Color: ");
  hexValue2 = (standbyColor.r << 16) | (standbyColor.g << 8) | standbyColor.b;
  String hexString1 = String("#" + String(hexValue2, HEX));
  Serial.println(hexString1);

  Serial.print("Go Color: ");
  hexValue3 = (goColor.r << 16) | (goColor.g << 8) | goColor.b;
  String hexString2 = String("#" + String(hexValue3, HEX));
  Serial.println(hexString2);

  Serial.print("State: ");
  Serial.println(state);
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

IPAddress getlocalIP()
{
  return IPAddress(mesh.getStationIP());
}

void setup()
{
  pinMode(buttonPin, INPUT);
  pinMode(StatusLEDpin, OUTPUT);
  digitalWrite(StatusLEDpin, HIGH);
  // Initialize the EEPROM
  EEPROM.begin(512);
  Serial.begin(115200); // Start Serial
  // Initialize setup button pins
  for (uint8_t i = 0; i < 3; i++)
  {
    pinMode(SETUP_BUTTONS[i], INPUT_PULLUP);
    SButtonState[i] = digitalRead(SETUP_BUTTONS[i]) == LOW;
  }

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  // Clear the display buffer
  display.clearDisplay();
  display.setTextColor(WHITE);

  // Initialize the LED strip
  FastLED.addLeds<WS2811, LED_Pin, GRB>(leds, NUM_LEDS);

  // Initialize the colors for the three states from the master
  warningColor = CRGB::DarkBlue;
  warningAColor = CRGB::Blue;
  standbyColor = CRGB::Red;
  goColor = CRGB::Green;

  // setMode();      // Try to connect to ssid in memory, if unsucceful start AP
  // server.begin(); // begin http server

  // painlessMESH
  // mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION); // set before init() so that you can see startup messages

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  //  userScheduler.addTask(taskSendMessage);
  //  taskSendMessage.enable();

  mesh.setContainsRoot(true);

  settingsState = 0;
  if (getConfigValue(UnitNumEEPROM) != NULL)
  {
    unitnum = getConfigValue(UnitNumEEPROM).toInt();
  }
  else
  {
    unitnum = 0;
  }
  brightness = getConfigValue(LEDbrightEEPROM).toInt();
}
/*
bool tryConnection()
{
  // Connect to the WiFi network using the SSID and password from EEPROM
  WiFi.mode(WIFI_STA);
  WiFi.begin(getConfigValue(ssidEEPROM), getConfigValue(passwordEEPROM));
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
    server.on("/ap", apUpdate);
    digitalWrite(StatusLEDpin, HIGH);
  }
  else
  { // if connection is succesful set appropriate page forwards
    mode = 1;
    Serial.println("Successful");
    Serial.print("IP Address: ");
    digitalWrite(StatusLEDpin, LOW);
  }
}

*/
void loop()
{
  mesh.update();
  display.clearDisplay();
  std::list<uint32_t> nodeList = mesh.getNodeList();
  if (nodeList.size() > 0)
  {
    digitalWrite(StatusLEDpin, LOW); // Turn on the LED
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(4, 0);
    display.println("StandbyGo Cue Lights");
    if (settingsState < 1)
    {
      display.setCursor(43, 10);
      display.print("Unit: ");
      display.print(unitnum);
    }
  }
  else
  {
    digitalWrite(StatusLEDpin, HIGH); // Turn off the LED
    display.setTextSize(1);
    display.setCursor(25, 5);
    display.print("CONNECTING...");
    display.drawBitmap(0, 25, epd_bitmap_MST_Full_Logo_White, 128, 64, WHITE);
  }
  //  giveInfo();
  if (myIP != getlocalIP())
  {
    myIP = getlocalIP();
    Serial.println("My IP is " + myIP.toString());
  }
  settings();

  unsigned long currentMillis = millis(); // get the current time
  // Check for incoming HTTP requests
  //  server.handleClient();
  /*    if ((getConfigValue(LEDbrightEEPROM).toInt() != brightness))
      {
        //updateConfigValue(LEDbrightEEPROM, String(brightness).c_str());
        brightness = getConfigValue(LEDbrightEEPROM).toInt();
        Serial.print("BRIGHTNESS: ");
        Serial.println(brightness);
      }
  */

  // byte brightness = getConfigValue(LEDbrightEEPROM).toInt();
  FastLED.setBrightness(brightness);

  SButtonState[0] = digitalRead(SETUP_BUTTONS[0]);
  SButtonState[1] = digitalRead(SETUP_BUTTONS[1]);
  SButtonState[2] = digitalRead(SETUP_BUTTONS[2]);
  /*if (currentMillis - previousTimeButton > buttondebounce && digitalRead(SETUP_BUTTONS[2]) == LOW)
  {
    ESP.restart();
  }*/
  if (currentMillis - previousTimeButton > buttondebounce && SButtonState[0] != lastSButtonState[0])
  {
    if (SButtonState[0] == LOW)
    {
      settingsState++;
    }
    previousMillis2 = currentMillis;
    previousTimeButton = currentMillis;
    if (settingsState > 5)
    {
      settingsState = 0;
    }
    lastSButtonState[0] = SButtonState[0];
  }

  // read the state of the button
  int buttonState = digitalRead(buttonPin);

  // if the state is 1 and button pressed, Send responce
  if (buttonState == LOW && !buttonPressed && state == 1)
  {
    buttonPressed = true;
    Serial.println("Responce Sent");
    sendMessage();
  }

  // if the button is not pressed, reset the flag variable
  if (buttonState == HIGH)
  {
    buttonPressed = false;
  }
  // Update the LED strip based on the state

  switch (state)
  {
  case 0:
    // Off state
    fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));
    break;
  case 1:
    // Warning state
    if (currentMillis - previousMillis >= interval)
    {
      previousMillis = currentMillis; // update the time at which the LED was last toggled
      ledState = !ledState;           // toggle the LED state

      if (ledState)
      {
        fill_solid(leds, NUM_LEDS, warningColor); // turn on the LED
      }
      else
      {
        fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0)); // turn off the LED
      }
    }
    display.setTextSize(2);
    display.setCursor(22, 35);
    display.print("WARNING");
    break;
  case 2:
    // Warning Acknowledged state
    fill_solid(leds, NUM_LEDS, warningAColor);
    display.setTextSize(2);
    display.setCursor(22, 35);
    display.print("Warning");
    display.setTextSize(1);
    display.setCursor(28, 51);
    display.print("acknowledged");
    break;
  case 3:
  case 4:
    // Standby state
    fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));
    fill_solid(leds, NUM_LEDS, standbyColor);
    display.setTextSize(2);
    display.setCursor(16, 35);
    display.print("STAND BY");
    break;
  case 5:
    previousMillis2 = currentMillis;
    // Go state
    fill_solid(leds, NUM_LEDS, goColor);
    display.fillRoundRect(20, 30, 88, 34, 10, WHITE);
    display.setTextColor(BLACK, WHITE); // 'inverted' text
    display.setTextSize(4);
    display.setCursor(40, 33);
    display.print("GO");
    break;
  }
  // Show the updated LED strip
  FastLED.show();
  display.display();
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
    display.print(unitnum);
    if ((currentMillis - previousTimeButton > buttondebounce && SButtonState[1] != lastSButtonState[1]) && unitnum > 0)
    {
      if (SButtonState[1] == LOW)
      {
        unitnum--;
      }
      previousTimeButton = currentMillis;
      previousMillis2 = currentMillis;
      lastSButtonState[1] = SButtonState[1];
    }
    if ((currentMillis - previousTimeButton > buttondebounce && SButtonState[2] != lastSButtonState[2]) && unitnum < 15)
    {
      if (SButtonState[2] == LOW)
      {
        unitnum++;
      }
      previousTimeButton = currentMillis;
      previousMillis2 = currentMillis;
      lastSButtonState[2] = SButtonState[2];
    }
    if ((getConfigValue(UnitNumEEPROM).toInt() != unitnum) && getConfigValue(UnitNumEEPROM) != NULL)
    {
      updateConfigValue(UnitNumEEPROM, String(unitnum).c_str());
    }
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

    if (currentMillis - previousTimeButton > buttondebounce)
    {
      if (SButtonState[1] != lastSButtonState[1] && brightness > 0 && SButtonState[1] == LOW)
      {
        brightness -= 5;
        previousMillis2 = currentMillis;
        previousTimeButton = currentMillis;
      }
      if (SButtonState[2] != lastSButtonState[2] && brightness < 255 && SButtonState[2] == LOW)
      {
        brightness += 5;
        previousMillis2 = currentMillis;
        previousTimeButton = currentMillis;
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
