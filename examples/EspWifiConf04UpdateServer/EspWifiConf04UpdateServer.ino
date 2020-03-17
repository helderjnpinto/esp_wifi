
/**
 * Example: Update Server
 * Description:
 *   In this example we will provide a "firmware update" link in the
 *   config portal.
 *   (See ESP8266 ESP8266HTTPUpdateServer examples 
 *   to understand UpdateServer!)
 *   (ESP32: HTTPUpdateServer library is ported for ESP32 in this project.)
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

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "dem1"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN D2

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;

EspWifi espWifi(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");

  espWifi.setStatusPin(STATUS_PIN);
  espWifi.setConfigPin(CONFIG_PIN);
  espWifi.setupUpdateServer(&httpUpdater);

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
  s += "<title>EspWifi 04 Update Server</title></head><body>Hello world!";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

