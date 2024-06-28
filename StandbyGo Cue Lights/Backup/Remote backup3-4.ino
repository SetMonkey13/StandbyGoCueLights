#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FastLED.h>
#include <EEPROM.h>
#include <espnow.h>

#include "index.h" //HTML webpage contents with javascripts
#include "ap.h"    //HTML webpage for changing the WIFI connection

String ssid;
String pass;
String ip;
String localip;

// CHANGE THESE TO DESIRED VALUES
char ap_ssid[] = "Cue Light Receiver AP"; // SSID to host network on if no connection is established
char ap_pass[] = "password";              // Password for network that is hosted if there is no network connection established
int connectionTimeout = 20;               // This number * 500 eqauls the number of ms to wait before assuming no network connection

int StatusLEDpin = 2;   // GPIO pin for status LED D1
int buttonPin = D3;     // GPIO pin for trigger D8
const int LED_Pin = 4;  // LED D2
const int NUM_LEDS = 2; // Number of Pixel LEDs connected

unsigned long previousMillis = 0;  // variable to store the time at which the LED was last toggled
unsigned long previousMillis2 = 0; // variable to store the time at which the LED was last toggled
unsigned long interval = 200;      // interval at which to toggle the LED, in milliseconds
unsigned long GOinterval = 5000;   // interval at which to toggle the LED, in milliseconds
bool ledState = false;             // current state of the LED

IPAddress serverAddress;

// Define the LED strip
CRGB leds[NUM_LEDS];

// Define the colors for the four states
CRGB warningColor;
CRGB warningAColor;
CRGB standbyColor;
CRGB goColor;

// Define the state of the LED strip
int state = 0;
bool buttonPressed = false; // flag variable to track button press

// Define the HTTP server
ESP8266WebServer server(80);

// Define the EEPROM addresses for the configuration data
const int ssidEEPROM = 0;
const int passwordEEPROM = 32;
const int idEEPROM = 64;
const int masterIPAddress = 96;
const int LEDbrightEEPROM = 128; // 1 byte needed
const int UnitNumEEPROM = 132;   // 1 byte needed

int mode; // connection state

bool tryConnection();
void setMode();
void apUpdate();
void handleRoot();
void giveInfo();
void getBroadcastAddress();

void setup()
{
  // Init ESP-NOW
  if (esp_now_init() != 0)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);

  pinMode(buttonPin, INPUT);
  pinMode(StatusLEDpin, OUTPUT);
  // Initialize the EEPROM
  EEPROM.begin(512);

  Serial.begin(115200); // Start Serial

  // Initialize the LED strip
  FastLED.addLeds<WS2811, LED_Pin, RGB>(leds, NUM_LEDS);

  // Initialize the colors for the three states from the master
  warningColor = CRGB::DarkBlue;
  warningAColor = CRGB::Blue;
  standbyColor = CRGB::Red;
  goColor = CRGB::Green;

  setMode();      // Try to connect to ssid in memory, if unsucceful start AP
  server.begin(); // begin http server
}

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
    server.on("/", handleRoot);
    server.on("/ap", apUpdate);
    server.on("/state", giveInfo);
    Serial.println();
    Serial.println("Successful");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.macAddress());
    digitalWrite(StatusLEDpin, LOW);
  }
}

void apUpdate()
{
  String s = AP_page;
  s.replace("@@mac@@", WiFi.macAddress());
  server.send(200, "text/html", s);

  // Handle the POST request to update the configuration
  if (server.method() == HTTP_POST)
  {
    String ssid = server.arg("SSID");
    String password = server.arg("PASSWORD");
    
    // Update the values in EEPROM
    updateConfigValue(ssidEEPROM, ssid.c_str());
    updateConfigValue(passwordEEPROM, password.c_str());

    // Restart the device to apply the new configuration
    ESP.restart();
  }
}

void handleRoot()
{
  String s = MAIN_page;
  s.replace("@@ssid@@", getConfigValue(ssidEEPROM));
  s.replace("@@password@@", getConfigValue(passwordEEPROM));
  s.replace("@@mac@@", WiFi.macAddress());
  s.replace("@@id@@", getConfigValue(idEEPROM));
  s.replace("@@unitnum@@", getConfigValue(UnitNumEEPROM));
  s.replace("@@brightness@@", getConfigValue(LEDbrightEEPROM));
  s.replace("@@state@@", String(state));
  server.send(200, "text/html", s);
  String info = String(state);
  server.send(200, "text/html", info);

  // Handle the POST request to update the configuration
  if (server.method() == HTTP_POST)
  {
    Serial.println("--Button pressed--");
    String id = server.arg("ID");
    String unitnum = server.arg("UNITNUM");
    String brightness = server.arg("BRIGHTNESS");

    // Update the values in EEPROM
    if (id != getConfigValue(idEEPROM))
    {
      id.replace(" ", "_");
      updateConfigValue(idEEPROM, id.c_str());
      Serial.println(" Updated ID");
    }
    if (brightness != getConfigValue(LEDbrightEEPROM))
    {
      updateConfigValue(LEDbrightEEPROM, brightness.c_str());
      Serial.println(" Updated Brightness");
    }
    if (unitnum != getConfigValue(UnitNumEEPROM))
    {
      updateConfigValue(UnitNumEEPROM, unitnum.c_str());
      Serial.println(" Updated Unit Number");
    }
  }
}

void giveInfo()
{ // serve up general information about the device's current settings
  String info = String(state);
  server.send(200, "text/html", info);
}

void loop()
{
  //  giveInfo();
  unsigned long currentMillis = millis(); // get the current time
  // Check for incoming HTTP requests
  server.handleClient();
  byte brightness = getConfigValue(LEDbrightEEPROM).toInt();
  FastLED.setBrightness(brightness);
  // read the state of the button
  int buttonState = digitalRead(buttonPin);

  // if the button is pressed and the current state is less than 4, increase the state by 1
  if (buttonState == LOW && !buttonPressed && state < 4)
  {
    state++;
    buttonPressed = true;
    Serial.print("State increased to ");
    Serial.println(state);
  }

  // if the button is pressed and the current state is 4, reset the state to 0
  else if (buttonState == LOW && !buttonPressed && state == 4)
  {
    state = 0;
    buttonPressed = true;
    Serial.print("State reset to ");
    Serial.println(state);
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
    break;
  case 2:
    // Warning Acknowledged state
    fill_solid(leds, NUM_LEDS, warningAColor);
    break;
  case 3:
    // Standby state
    fill_solid(leds, NUM_LEDS, standbyColor);
    break;
  case 4:
    previousMillis2 = currentMillis;
    // Go state
    fill_solid(leds, NUM_LEDS, goColor);
    break;
  }
  // Show the updated LED strip
  FastLED.show();
}

// Structure example to receive data
// Must match the sender structure
typedef struct test_struct
{
  int u;
  String v;
  String w;
  String x;
  String y;
} test_struct;

// Create a struct_message called myData
test_struct myData;

// Callback function that will be executed when data is received
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len)
{
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("State: ");
  Serial.println(myData.u);
  state = myData.u;
  Serial.print("Warning Color: ");
  Serial.println(myData.v);
long hexValue = (warningColor.r << 16) | (warningColor.g << 8) | warningColor.b;
  String hexString = String("#" + String(hexValue, HEX));
  if (hexString != myData.v)
  {
    long hexValue = strtol(myData.v.substring(1).c_str(), NULL, 16);
    warningColor = CRGB((hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF);
  }

  Serial.print("WarningA Color: ");
  Serial.println(myData.w);
  Serial.print("Standing Color: ");
  Serial.println(myData.x);
    long hexValue1 = (standbyColor.r << 16) | (standbyColor.g << 8) | standbyColor.b;
  String hexString1 = String("#" + String(hexValue1, HEX));
  if (hexString1 != myData.x)
  {
    long hexValue1 = strtol(myData.x.substring(1).c_str(), NULL, 16);
    standbyColor = CRGB((hexValue1 >> 16) & 0xFF, (hexValue1 >> 8) & 0xFF, hexValue1 & 0xFF);
  }
     Serial.print("Go Color: ");
     Serial.println(myData.y);
    long hexValue2 = (goColor.r << 16) | (goColor.g << 8) | goColor.b;
  String hexString2 = String("#" + String(hexValue2, HEX));
  if (hexString2 != myData.y)
  {
    long hexValue2 = strtol(myData.y.substring(1).c_str(), NULL, 16);
    goColor = CRGB((hexValue2 >> 16) & 0xFF, (hexValue2 >> 8) & 0xFF, hexValue2 & 0xFF);
  }
  Serial.println();
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
