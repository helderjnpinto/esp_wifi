/**
 * Example: MQTT Demo Application
 * Description:
 *   All EspWifi specific aspects of this example are described in
 *   previous examples, so please get familiar with EspWifi before
 *   starting this example. So nothing new will be explained here, 
 *   but a complete demo application will be build.
 *   It is also expected from the reader to have a basic knowledge over
 *   MQTT to understand this code.
 *   
 *   This example starts an MQTT client with the configured
 *   connection settings.
 *   Will post the status changes of the D2 pin in channel "/test/status".
 *   Receives messages appears in channel "/test/action", and writes them to serial.
 *   This example also provides the firmware update option.
 *   (See previous examples for more details!)
 * 
 * Software setup for this example:
 *   This example utilizes Joel Gaehwiler's MQTT library.
 *   https://github.com/256dpi/arduino-mqtt
 * 
 * Hardware setup for this example:
 *   - An LED is attached to LED_BUILTIN pin with setup On=LOW.
 *   - [Optional] A push button is attached to pin D2, the other leg of the
 *     button should be attached to GND.
 */

#include <MQTT.h>
#include <EspWifi.h>

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "testThing";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "smrtTHNG8266";

#define STRING_LEN 128

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "mqt1"

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
void mqttMessageReceived(String &topic, String &payload);

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;
WiFiClient net;
MQTTClient mqttClient;

char mqttServerValue[STRING_LEN];
char mqttUserNameValue[STRING_LEN];
char mqttUserPasswordValue[STRING_LEN];

EspWifi espWifi(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
EspWifiParameter mqttServerParam = EspWifiParameter("MQTT server", "mqttServer", mqttServerValue, STRING_LEN);
EspWifiParameter mqttUserNameParam = EspWifiParameter("MQTT user", "mqttUser", mqttUserNameValue, STRING_LEN);
EspWifiParameter mqttUserPasswordParam = EspWifiParameter("MQTT password", "mqttPass", mqttUserPasswordValue, STRING_LEN, "password");

boolean needMqttConnect = false;
boolean needReset = false;
int pinState = HIGH;
unsigned long lastReport = 0;
unsigned long lastMqttConnectionAttempt = 0;

void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");

  espWifi.setStatusPin(STATUS_PIN);
  espWifi.setConfigPin(CONFIG_PIN);
  espWifi.addParameter(&mqttServerParam);
  espWifi.addParameter(&mqttUserNameParam);
  espWifi.addParameter(&mqttUserPasswordParam);
  espWifi.setConfigSavedCallback(&configSaved);
  espWifi.setFormValidator(&formValidator);
  espWifi.setWifiConnectionCallback(&wifiConnected);
  espWifi.setupUpdateServer(&httpUpdater);

  // -- Initializing the configuration.
  boolean validConfig = espWifi.init();
  if (!validConfig)
  {
    mqttServerValue[0] = '\0';
    mqttUserNameValue[0] = '\0';
    mqttUserPasswordValue[0] = '\0';
  }

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []{ espWifi.handleConfig(); });
  server.onNotFound([](){ espWifi.handleNotFound(); });

  mqttClient.begin(mqttServerValue, net);
  mqttClient.onMessage(mqttMessageReceived);
  
  Serial.println("Ready.");
}

void loop() 
{
  // -- doLoop should be called as frequently as possible.
  espWifi.doLoop();
  mqttClient.loop();
  
  if (needMqttConnect)
  {
    if (connectMqtt())
    {
      needMqttConnect = false;
    }
  }
  else if ((espWifi.getState() == ESPWIFI_STATE_ONLINE) && (!mqttClient.connected()))
  {
    Serial.println("MQTT reconnect");
    connectMqtt();
  }

  if (needReset)
  {
    Serial.println("Rebooting after 1 second.");
    espWifi.delay(1000);
    ESP.restart();
  }

  unsigned long now = millis();
  if ((500 < now - lastReport) && (pinState != digitalRead(CONFIG_PIN)))
  {
    pinState = 1 - pinState; // invert pin state as it is changed
    lastReport = now;
    Serial.print("Sending on MQTT channel '/test/status' :");
    Serial.println(pinState == LOW ? "ON" : "OFF");
    mqttClient.publish("/test/status", pinState == LOW ? "ON" : "OFF");
  }
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
  s += "<title>EspWifi 06 MQTT App</title></head><body>MQTT App demo";
  s += "<ul>";
  s += "<li>MQTT server: ";
  s += mqttServerValue;
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void wifiConnected()
{
  needMqttConnect = true;
}

void configSaved()
{
  Serial.println("Configuration was updated.");
  needReset = true;
}

boolean formValidator()
{
  Serial.println("Validating form.");
  boolean valid = true;

  int l = server.arg(mqttServerParam.getId()).length();
  if (l < 3)
  {
    mqttServerParam.errorMessage = "Please provide at least 3 characters!";
    valid = false;
  }

  return valid;
}

boolean connectMqtt() {
  unsigned long now = millis();
  if (1000 > now - lastMqttConnectionAttempt)
  {
    // Do not repeat within 1 sec.
    return false;
  }
  Serial.println("Connecting to MQTT server...");
  if (!connectMqttOptions()) {
    lastMqttConnectionAttempt = now;
    return false;
  }
  Serial.println("Connected!");

  mqttClient.subscribe("/test/action");
  return true;
}

/*
// -- This is an alternative MQTT connection method.
boolean connectMqtt() {
  Serial.println("Connecting to MQTT server...");
  while (!connectMqttOptions()) {
    espWifi.delay(1000);
  }
  Serial.println("Connected!");

  mqttClient.subscribe("/test/action");
  return true;
}
*/

boolean connectMqttOptions()
{
  boolean result;
  if (mqttUserPasswordValue[0] != '\0')
  {
    result = mqttClient.connect(espWifi.getThingName(), mqttUserNameValue, mqttUserPasswordValue);
  }
  else if (mqttUserNameValue[0] != '\0')
  {
    result = mqttClient.connect(espWifi.getThingName(), mqttUserNameValue);
  }
  else
  {
    result = mqttClient.connect(espWifi.getThingName());
  }
  return result;
}

void mqttMessageReceived(String &topic, String &payload)
{
  Serial.println("Incoming: " + topic + " - " + payload);
}

