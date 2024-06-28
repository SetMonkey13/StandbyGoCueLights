#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FastLED.h>
#include <EEPROM.h>

#include "indexM.h" //HTML webpage contents with javascripts
#include "ap.h"    //HTML webpage for changing the WIFI connection

#define NUM_BUTTONS 6

String ssid;
String pass;
String ip;
String localip;

// CHANGE THESE TO DESIRED VALUES
char ap_ssid[] = "Cue Light Main AP"; // SSID to host network on if no connection is established
char ap_pass[] = "password";              // Password for network that is hosted if there is no network connection established
int connectionTimeout = 20;               // This number * 500 eqauls the number of ms to wait before assuming no network connection

int StatusLEDpin = 2;                     // GPIO pin for status LED D1
int buttonPin = D3;                       // GPIO pin for trigger D8
const int LED_Pin = 4;                    // LED D2
const int NUM_LEDS = 2;                   // Number of Pixel LEDs connected

unsigned long previousMillis = 0; // variable to store the time at which the LED was last toggled
unsigned long previousMillis2 = 0; // variable to store the time at which the LED was last toggled
unsigned long interval = 200;     // interval at which to toggle the LED, in milliseconds
unsigned long GOinterval = 5000;  // interval at which to toggle the LED, in milliseconds
bool ledState = false;            // current state of the LED

IPAddress serverAddress;
int mode; // connection state

// Define the LED strip
CRGB leds[NUM_LEDS];

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
int state = 0;
bool buttonPressed = false; // flag variable to track button press

// Define the HTTP server
ESP8266WebServer server(80);

// Define the EEPROM addresses for the configuration data
const int ssidEEPROM = 0;
const int passwordEEPROM = 32;
const int idEEPROM = 64;
const int LEDbrightEEPROM = 96; // 1 byte needed
const int warningColorEEPROM = 128;
const int warningAColorEEPROM = 138;
const int standbyColorEEPROM = 148;
const int goColorEEPROM = 158;

bool tryConnection();
void setMode();
void apUpdate();
void handleRoot();
void giveInfo();
void getBroadcastAddress();

void setup()
{
  pinMode(buttonPin, INPUT);
  pinMode(StatusLEDpin, OUTPUT);
  // Initialize the EEPROM
  EEPROM.begin(512);

  Serial.begin(115200); // Start Serial

  // Initialize the LED strip
  FastLED.addLeds<WS2811, LED_Pin, RGB>(leds, NUM_LEDS);

  // Initialize the colors for the three states from the master
  warningColor = CRGB::MidnightBlue;
  warningAColor = CRGB::Blue;
  standbyColor = CRGB::Red;
  goColor = CRGB::Green;

  setMode(); // Try to connect to ssid in memory, if unsucceful start AP
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
    server.on("/", giveInfo);
    Serial.println();
    Serial.println("Successful");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(StatusLEDpin, LOW);
  }
}

void apUpdate()
{
  String s = AP_page;
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
  s.replace("@@warncolor@@", getConfigValue(warningColorEEPROM));
  s.replace("@@standcolor@@", getConfigValue(standbyColorEEPROM));
  s.replace("@@gocolor@@", getConfigValue(goColorEEPROM));
  s.replace("@@brightness@@", getConfigValue(LEDbrightEEPROM));
  s.replace("@@password@@", getConfigValue(passwordEEPROM));
  s.replace("@@mac@@", WiFi.macAddress());
  s.replace("@@id@@", getConfigValue(idEEPROM));
  s.replace("@@brightness@@", getConfigValue(LEDbrightEEPROM));
  s.replace("@@state@@", String(state));
  server.send(200, "text/html", s);
  String info = String(state);
  server.send(200, "text/html", info);

  // Handle the POST request to update the configuration
  if (server.method() == HTTP_POST)
  {
    Serial.println("--Button pressed--");
    String brightness = server.arg("BRIGHTNESS");
    warningColorHex = server.arg("WARN");
    if (warningColorHex != getConfigValue(warningColorEEPROM)) {
      updateConfigValue(warningColorEEPROM, warningColorHex.c_str());
      Serial.println(" Warning Color Updated");
    }
    standbyColorHex = server.arg("STAND");
    if (standbyColorHex != getConfigValue(standbyColorEEPROM)) {
      updateConfigValue(standbyColorEEPROM, standbyColorHex.c_str());
      Serial.println(" Stand By Color Updated");
    }
    goColorHex = server.arg("GO");
    if (goColorHex != getConfigValue(goColorEEPROM)) {
      updateConfigValue(goColorEEPROM, goColorHex.c_str());
      Serial.println(" Go Color Updated");
    }
    if (brightness != getConfigValue(LEDbrightEEPROM)) {
      updateConfigValue(LEDbrightEEPROM, brightness.c_str());
      Serial.println(" Updated Brightness");
    }
  }
}

void giveInfo()
{ // serve up general information about the device's current settings
  String info = String(state);
  server.send(200, "text/html", info);
}
void loop() {

  giveInfo();
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
/*
  // Define the button pins
  const int buttonPins[NUM_BUTTONS] = {3, 4, 5, 6, 7, 8};

  // Define the states and IP addresses of the remote devices
  const int numRemotes = 3;
  const char* remoteColors[numRemotes] = {"blue", "red", "green"};
  const IPAddress remoteIPs[numRemotes] = {IPAddress(192, 168, 1, 10), IPAddress(192, 168, 1, 11), IPAddress(192, 168, 1, 12)};
  const int remoteLEDIndices[numRemotes] = {0, 1, 2};

  // Define the button state variables
  bool buttonStates[NUM_BUTTONS] = {false};

  void setup() {
  // Initialize the LED strip
  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.show();

  // Initialize the button pins
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  }

  void loop() {
  // Check if any of the buttons have been pressed
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (digitalRead(buttonPins[i]) == LOW && !buttonStates[i]) {
      buttonStates[i] = true;
      for (int j = 0; j < numRemotes; j++) {
        if (i == j) {
          // Control the LED on the main device
          if (i == 0) {
            leds[0] = CRGB::Blue;
          } else if (i == 1) {
            leds[0] = CRGB::Red;
          } else if (i == 2) {
            fill_solid(leds, NUM_LEDS, CRGB::Green);
            FastLED.show();
            delay(5000);
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            FastLED.show();
          }
        } else {
          // Control the LED on the remote device
          sendUDPMessage(remoteColors[i], remoteIPs[i]);
          leds[remoteLEDIndices[i]] = CRGB::Blue;
        }
      }
      FastLED.show();
    } else if (digitalRead(buttonPins[i]) == HIGH) {
      buttonStates[i] = false;
    }
  }
  }

*/

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
