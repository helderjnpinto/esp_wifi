/**
 * Example: Callbacks
 * Description:
 *   This example shows, what callbacks EspWifi provides.
 *   (See previous examples for more details!)
 * 
 * Hardware setup for this example:
 *   - An LED is attached to LED_BUILTIN pin with setup On=LOW.
 *   - [Optional] A push button is attached to pin D2, the other leg of the
 *     button should be attached to GND.
 */

#include <EspWifi.h>

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "testThing";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "smrtTHNG8266";

#define STRING_LEN 128

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "dem3"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN D2

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

// -- Callback method declarations.
void wifiConnected();
void configSaved();
boolean formValidator();
void messageReceived(String &topic, String &payload);

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;

char stringParamValue[STRING_LEN];

EspWifi espWifi(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
EspWifiParameter stringParam = EspWifiParameter("String param", "stringParam", stringParamValue, STRING_LEN);

void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");

  espWifi.setStatusPin(STATUS_PIN);
  espWifi.setConfigPin(CONFIG_PIN);
  espWifi.addParameter(&stringParam);
  espWifi.setConfigSavedCallback(&configSaved);
  espWifi.setFormValidator(&formValidator);
  espWifi.setWifiConnectionCallback(&wifiConnected);

  // -- Initializing the configuration.
  boolean validConfig = espWifi.init();
  if (!validConfig)
  {
    stringParamValue[0] = '\0';
  }

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []{ espWifi.handleConfig(); });
  server.onNotFound([](){ espWifi.handleNotFound(); });

  Serial.println("Ready.");
}

void loop() 
{
  // -- doLoop should be called as frequently as possible.
  espWifi.doLoop();
}

/**
 * Handle web requests to "/" path.
 */
void handleRoot()
{
  // -- Let EspWifi test and handle captive portal requests.
  if (espWifi.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>EspWifi 05 Callbacks</title></head><body>Hello world!";
  s += "<ul>";
  s += "<li>String param value: ";
  s += stringParamValue;
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void wifiConnected()
{
  Serial.println("WiFi was connected.");
}

void configSaved()
{
  Serial.println("Configuration was updated.");
}

boolean formValidator()
{
  Serial.println("Validating form.");
  boolean valid = true;

  int l = server.arg(stringParam.getId()).length();
  if (l < 3)
  {
    stringParam.errorMessage = "Please provide at least 3 characters for this test!";
    valid = false;
  }

  return valid;
}

