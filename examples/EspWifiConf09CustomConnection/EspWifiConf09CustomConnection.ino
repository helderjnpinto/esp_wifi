/**
 * Example: Custom connection
 * Description:
 *   This example is for advanced users only!
 *   In this example custom connection handler methods are defined
 *   to override the default connecting behavior.
 *   Also, three custom parameters are introduced, that are used
 *   at the connection.
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
#define CONFIG_VERSION "dem9"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN 2

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

// -- Callback method declarations.
void configSaved();
boolean formValidator();
boolean connectAp(const char* apName, const char* password);
void connectWifi(const char* ssid, const char* password);

DNSServer dnsServer;
WebServer server(80);

char ipAddressValue[STRING_LEN];
char gatewayValue[STRING_LEN];
char netmaskValue[STRING_LEN];

EspWifi espWifi(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
EspWifiParameter ipAddressParam = EspWifiParameter("IP address", "ipAddress", ipAddressValue, STRING_LEN, "text", NULL, "192.168.3.222");
EspWifiParameter gatewayParam = EspWifiParameter("Gateway", "gateway", gatewayValue, STRING_LEN, "text", NULL, "192.168.3.0");
EspWifiParameter netmaskParam = EspWifiParameter("Subnet mask", "netmask", netmaskValue, STRING_LEN, "text", NULL, "255.255.255.0");

IPAddress ipAddress;
IPAddress gateway;
IPAddress netmask;

void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");

  espWifi.setStatusPin(STATUS_PIN);
  espWifi.setConfigPin(CONFIG_PIN);
  espWifi.addParameter(&ipAddressParam);
  espWifi.addParameter(&gatewayParam);
  espWifi.addParameter(&netmaskParam);
  espWifi.setConfigSavedCallback(&configSaved);
  espWifi.setFormValidator(&formValidator);
  espWifi.setApConnectionHandler(&connectAp);
  espWifi.setWifiConnectionHandler(&connectWifi);

  // -- Initializing the configuration.
  boolean validConfig = espWifi.init();

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
  s += "<title>EspWifi 09 Custom Connection</title></head><body>Hello world!";
  s += "<ul>";
  s += "<li>IP address: ";
  s += ipAddressValue;
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

  if (!ipAddress.fromString(server.arg(ipAddressParam.getId())))
  {
    ipAddressParam.errorMessage = "Please provide a valid IP address!";
    valid = false;
  }
  if (!netmask.fromString(server.arg(netmaskParam.getId())))
  {
    netmaskParam.errorMessage = "Please provide a valid netmask!";
    valid = false;
  }
  if (!gateway.fromString(server.arg(gatewayParam.getId())))
  {
    gatewayParam.errorMessage = "Please provide a valid gateway address!";
    valid = false;
  }

  return valid;
}

boolean connectAp(const char* apName, const char* password)
{
  // -- Custom AP settings
  return WiFi.softAP(apName, password, 4);
}
void connectWifi(const char* ssid, const char* password)
{
  ipAddress.fromString(String(ipAddressValue));
  netmask.fromString(String(netmaskValue));
  gateway.fromString(String(gatewayValue));

  if (!WiFi.config(ipAddress, gateway, netmask)) {
    Serial.println("STA Failed to configure");
  }
  Serial.print("ip: ");
  Serial.println(ipAddress);
  Serial.print("gw: ");
  Serial.println(gateway);
  Serial.print("net: ");
  Serial.println(netmask);
  WiFi.begin(ssid, password);
}
