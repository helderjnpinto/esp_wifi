/**
 * EspWifi11Advanced.ino -- EspWifi is an ESP8266/ESP32
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

/**
 * Example: Advanced runtime options
 * Description:
 *   The purpose of this example is to show the advanced non-GUI
 *   options that can be used by application developers.
 *   This example opens a serial terminal where we can send
 *   commands to the code, mimicing an application, that needs
 *   to change EspWifi behavior runtime according to some
 *   internal logic.
 *   Please see this source code for available commands!
 * 
 * Hardware setup for this example:
 *   - An LED is attached to LED_BUILTIN pin with setup On=LOW.
 *     This is hopefully already attached by default.
 *   - Serial monitor is set up with baud 115200 in bi-direction mode.
 */

#include <EspWifi.h>

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "testThing";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "smrtTHNG8266";

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

#define COMMAND_PARAMETER_LENGTH 30

void handleRoot();
void processCommand();

DNSServer dnsServer;
WebServer server(80);

EspWifi EspWifi(thingName, &dnsServer, &server, wifiInitialApPassword);

char commandParameter[COMMAND_PARAMETER_LENGTH];
char labelBuffer[COMMAND_PARAMETER_LENGTH];

void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");

  // -- Initializing the configuration.
  EspWifi.setStatusPin(STATUS_PIN);
  EspWifi.init();

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []{ EspWifi.handleConfig(); });
  server.onNotFound([](){ EspWifi.handleNotFound(); });

  Serial.println("Ready.");
}

void loop() 
{
  // -- doLoop should be called as frequently as possible.
  EspWifi.doLoop();

  if (Serial.available())
  {
    processCommand();
  }
}

/**
 * Handle web requests to "/" path.
 */
void handleRoot()
{
  // -- Let EspWifi test and handle captive portal requests.
  if (EspWifi.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>EspWifi 02 Status and Reset</title></head><body>Hello world!";
  s += "Go to <a href='config'>configure page</a> to change settings.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void processCommand()
{
  char command = Serial.read();
  delay(10);

  byte commandParameterIndex = 0;
  while(Serial.available())
  {
    char c = Serial.read();
    if (c < 32)
    {
      continue;
    }
    if (commandParameterIndex < (COMMAND_PARAMETER_LENGTH - 2))
    {
      commandParameter[commandParameterIndex] = c;
      commandParameterIndex += 1;
    }
  }
  commandParameter[commandParameterIndex] = '\0';

  switch (command)
  {
    case 'a':
      // -- a1 - force AP mode
      // -- a0 - stop forcing AP mode
      if (commandParameter[0] == '1')
      {
        EspWifi.forceApMode(true);
        Serial.println(F(">> Force AP mode started"));
      }
      else if (commandParameter[0] == '0')
      {
        EspWifi.forceApMode(false);
        Serial.println(F(">> Force AP mode stopped"));
      }
      break;
    case 'b':
      // -- b1 - start blink in "percend" mode
      // -- b2 - start blink in "fine" mode
      // -- b0 - stop blinking (started by the options above)
      if (commandParameter[0] == '1')
      {
        EspWifi.blink(2000, 70);
        Serial.println(F(">> Started custom blinking with repeat of 2000ms and 70% on state."));
      }
      else if (commandParameter[0] == '2')
      {
        EspWifi.fineBlink(100, 600);
        Serial.println(F(">> Started custom blinking with repeat of 100ms ON and 600ms OFF state."));
      }
      else if (commandParameter[0] == '0')
      {
        EspWifi.stopCustomBlink();
        Serial.println(F(">> Custom blinking stopped."));
      }
      break;
    case 's':
      // -- s - Get current state
      Serial.print(F(">> State is :"));
      Serial.println(EspWifi.getState());
      break;
    case 't':
      // -- t1 - Sets the AP timeout to 60 secs
      // -- t0 - Sets the AP timeout to the default
      if (commandParameter[0] == '1')
      {
        EspWifi.setApTimeoutMs(60000);
        Serial.println(F(">> AP timeout was changed to 60secs."));
      }
      else if (commandParameter[0] == '0')
      {
        EspWifi.setApTimeoutMs(EspWifi_DEFAULT_AP_MODE_TIMEOUT_MS);
        Serial.print(F(">> AP timeout was changed to default (ms):"));
        Serial.println(EspWifi_DEFAULT_AP_MODE_TIMEOUT_MS);
      }
      break;
    case 'n':
      // -- n<name> - Sets the thing name
      strncpy(
        EspWifi.getThingNameParameter()->valueBuffer,
        commandParameter,
        EspWifi.getThingNameParameter()->getLength());
      EspWifi.configSave();
      Serial.print(F(">> Thing name was changed to '"));
      Serial.print(commandParameter);
      Serial.println(F("'"));
      break;
    case 'l':
      // -- l<label> - Sets the label for config item thing-name.
      strncpy(
        labelBuffer,
        commandParameter,
        COMMAND_PARAMETER_LENGTH);
      EspWifi.getThingNameParameter()->label = labelBuffer;
      Serial.print(F(">> Thing name label was temporary changed to '"));
      Serial.print(labelBuffer);
      Serial.println(F("'"));
      break;
  }

}
