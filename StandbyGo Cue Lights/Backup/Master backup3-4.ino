#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FastLED.h>
#include <EEPROM.h>
#include <espnow.h>

#include "indexM.h" //HTML webpage contents with javascripts
#include "ap.h"     //HTML webpage for changing the WIFI connection

// REPLACE WITH RECEIVER MAC Address
uint8_t broadcastAddress1[] = {0x4C, 0x75, 0x25, 0x1A, 0x24, 0xF1};

// Structure example to send data
// Must match the receiver structure
typedef struct test_struct
{
  int u;
  String v;
  String w;
  String x;
  String y;
} test_struct;

// Create a struct_message called test to store variables to be sent
test_struct test;

unsigned long lastTime = 0;
unsigned long timerDelay = 20; // send readings timer

String ssid;
String pass;
String ip;
String localip;

// CHANGE THESE TO DESIRED VALUES
char ap_ssid[] = "Cue Light Main AP"; // SSID to host network on if no connection is established
char ap_pass[] = "password";          // Password for network that is hosted if there is no network connection established
int connectionTimeout = 20;           // This number * 500 eqauls the number of ms to wait before assuming no network connection

int StatusLEDpin = 2; // GPIO pin for status LED D1

// LED setup
const int LED_Pin = 4;  // LED D2
const int NUM_LEDS = 2; // Number of Pixel LEDs connected

// Button pins
const int BUTTON_PINS[] = {D3, D5};
const int NUM_BUTTONS = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);

// Button states
bool buttonStates[NUM_BUTTONS] = {false};
unsigned long lastButtonTimes[NUM_BUTTONS] = {0};
bool lastButtonStates[NUM_BUTTONS] = {LOW}; // Initialize to LOW

unsigned long previousTimeButton;
unsigned long previousMillis = 0;  // variable to store the time at which the LED was last toggled
unsigned long previousMillis2 = 0; // variable to store the time at which the LED was last toggled
unsigned long interval = 200;      // interval at which to toggle the LED, in milliseconds
unsigned long GOinterval = 5000;   // interval at which to toggle the LED, in milliseconds
bool ledState = false;             // current state of the LED

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
  // Init Serial Monitor
  Serial.begin(115200);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Init ESP-NOW
  if (esp_now_init() != 0)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  //  esp_now_register_send_cb(OnDataSent);

  // Register peer
  esp_now_add_peer(broadcastAddress1, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

  pinMode(StatusLEDpin, OUTPUT);
  // Initialize the EEPROM
  EEPROM.begin(512);

  Serial.begin(115200); // Start Serial

  // Initialize the LED strip
  FastLED.addLeds<WS2811, LED_Pin, RGB>(leds, NUM_LEDS);

  // Initialize the colors for the three states from the master
  // warningColor = CRGB::MidnightBlue;
  long hexValue = strtol(getConfigValue(warningColorEEPROM).substring(1).c_str(), NULL, 16);
  warningColor = CRGB((hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF);

  // warningAColor = CRGB::Blue;
  long hexValue1 = strtol(getConfigValue(warningAColorEEPROM).substring(1).c_str(), NULL, 16);
  warningAColor = CRGB((hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF); //CURRENTLY MATCHES WARNING COLOR  WEBSITE ONLY HAS 3 OPTIONS

  // standbyColor = CRGB::Red;
  long hexValue2 = strtol(getConfigValue(standbyColorEEPROM).substring(1).c_str(), NULL, 16);
  standbyColor = CRGB((hexValue2 >> 16) & 0xFF, (hexValue2 >> 8) & 0xFF, hexValue2 & 0xFF);

  // goColor = CRGB::Green;
  long hexValue3 = strtol(getConfigValue(goColorEEPROM).substring(1).c_str(), NULL, 16);
  goColor = CRGB((hexValue3 >> 16) & 0xFF, (hexValue3 >> 8) & 0xFF, hexValue3 & 0xFF);

  // Initialize button pins
  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }

  // Initialize button states
  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    buttonStates[i] = digitalRead(BUTTON_PINS[i]) == LOW;
  }
  /*
    // Initialize color indices
    for (int i = 0; i < NUM_LEDS; i++) {
      currentColorIndices[i] = 0;
    }
  */
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
    // server.on("/", giveInfo);
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
    if (warningColorHex != getConfigValue(warningColorEEPROM))
    {
      updateConfigValue(warningColorEEPROM, warningColorHex.c_str());
      long hexValue = strtol(warningColorHex.substring(1).c_str(), NULL, 16);
      warningColor = CRGB((hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF);
      Serial.println(" Warning Color Updated");
    }
    standbyColorHex = server.arg("STAND");
    if (standbyColorHex != getConfigValue(standbyColorEEPROM))
    {
      updateConfigValue(standbyColorEEPROM, standbyColorHex.c_str());
      long hexValue = strtol(standbyColorHex.substring(1).c_str(), NULL, 16);
      standbyColor = CRGB((hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF);
      Serial.println(" Stand By Color Updated");
    }
    goColorHex = server.arg("GO");
    if (goColorHex != getConfigValue(goColorEEPROM))
    {
      updateConfigValue(goColorEEPROM, goColorHex.c_str());
      long hexValue = strtol(goColorHex.substring(1).c_str(), NULL, 16);
      goColor = CRGB((hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF);
      Serial.println(" Go Color Updated");
    }
    if (brightness != getConfigValue(LEDbrightEEPROM))
    {
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

void handleButtonState()
{
  String buttonState = server.arg("state");
  Serial.print("Button state sent: ");
  Serial.println(buttonState);
  server.send(200, "text/plain", "Button state sent");
}

void loop()
{

  if ((millis() - lastTime) > timerDelay)
  {
    // Set values to send
    test.u = state;
    test.v = getConfigValue(warningColorEEPROM);
    test.w = getConfigValue(warningAColorEEPROM);
    test.x = getConfigValue(standbyColorEEPROM);
    test.y = getConfigValue(goColorEEPROM);

    // Send message via ESP-NOW
    esp_now_send(0, (uint8_t *)&test, sizeof(test));

    lastTime = millis();
  }

  // giveInfo();
  unsigned long currentMillis = millis(); // get the current time
  // Check for incoming HTTP requests
  server.handleClient();
  byte brightness = getConfigValue(LEDbrightEEPROM).toInt();
  FastLED.setBrightness(brightness);

  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    int buttonState = digitalRead(BUTTON_PINS[0]);

    // if the button is pressed and the current state is less than 4, increase the state by 1
    if (currentMillis - previousTimeButton > interval && buttonState == LOW && !buttonPressed && state <= 4)
    {
      previousTimeButton = currentMillis;
      state++;
      buttonPressed = true;
      Serial.print("State increased to ");
      Serial.println(state);
    }

    // if the button is pressed and the current state is 4, reset the state to 0
    else if (state > 4)
    {
      previousTimeButton = currentMillis;
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
      fill_solid(leds, NUM_LEDS, warningColor);
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
}
/*
void updateLEDs() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = colors[currentColorIndices[i]];
  }
  FastLED.show();
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

/*// Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  char macStr[18];
  Serial.print("Packet to:");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
         mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
  Serial.print(" send status: ");
  if (sendStatus == 0){
    Serial.println("Delivery success");
  }
  else{
    Serial.println("Delivery fail");
  }
}*/
