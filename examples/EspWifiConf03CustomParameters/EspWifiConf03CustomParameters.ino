/**
 * Example: Custom parameters
 * Description:
 *   In this example it is shown how to attach your custom parameters
 *   to the config portal. Your parameters will be maintained by 
 *   EspWifi. This means, they will be loaded from/saved to EEPROM,
 *   and will appear in the config portal.
 *   Note the configSaved and formValidator callbacks!
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
#define NUMBER_LEN 32

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "dem2"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN D2

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

// -- Callback method declarations.
void configSaved();
boolean formValidator();

DNSServer dnsServer;
WebServer server(80);

char stringParamValue[STRING_LEN];
char intParamValue[NUMBER_LEN];
char floatParamValue[NUMBER_LEN];

EspWifi espWifi(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
EspWifiParameter stringParam = EspWifiParameter("String param", "stringParam", stringParamValue, STRING_LEN);
EspWifiSeparator separator1 = EspWifiSeparator();
EspWifiParameter intParam = EspWifiParameter("Int param", "intParam", intParamValue, NUMBER_LEN, "number", "1..100", NULL, "min='1' max='100' step='1'");
// -- We can add a legend to the separator
EspWifiSeparator separator2 = EspWifiSeparator("Calibration factor");
EspWifiParameter floatParam = EspWifiParameter("Float param", "floatParam", floatParamValue, NUMBER_LEN, "number", "e.g. 23.4", NULL, "step='0.1'");

void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");

  espWifi.setStatusPin(STATUS_PIN);
  espWifi.setConfigPin(CONFIG_PIN);
  espWifi.addParameter(&stringParam);
  espWifi.addParameter(&separator1);
  espWifi.addParameter(&intParam);
  espWifi.addParameter(&separator2);
  espWifi.addParameter(&floatParam);
  espWifi.setConfigSavedCallback(&configSaved);
  espWifi.setFormValidator(&formValidator);
  espWifi.getApTimeoutParameter()->visible = true;

  // -- Initializing the configuration.
  espWifi.init();

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
  s += "<title>EspWifi 03 Custom Parameters</title></head><body>Hello world!";
  s += "<ul>";
  s += "<li>String param value: ";
  s += stringParamValue;
  s += "<li>Int param value: ";
  s += atoi(intParamValue);
  s += "<li>Float param value: ";
  s += atof(floatParamValue);
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
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

