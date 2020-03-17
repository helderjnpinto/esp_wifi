#include <EEPROM.h>

#include "ESPWIFI.h"

#ifdef ESPWIFI_CONFIG_USE_MDNS
# ifdef ESP8266
#  include <ESP8266mDNS.h>
# elif defined(ESP32)
#  include <ESPmDNS.h>
# endif
#endif

#define ESPWIFI_STATUS_ENABLED (this->_statusPin >= 0)

EspWifiParameter::EspWifiParameter()
{
}
EspWifiParameter::EspWifiParameter(
    const char* label, const char* id, char* valueBuffer, int length,
    const char* type, const char* placeholder, const char* defaultValue,
    const char* customHtml, boolean visible)
{
  this->label = label;
  this->_id = id;
  this->valueBuffer = valueBuffer;
  this->_length = length;
  this->type = type;
  this->placeholder = placeholder;
  this->defaultValue = defaultValue;
  this->customHtml = customHtml;
  this->visible = visible;
}
EspWifiParameter::EspWifiParameter(
    const char* id, char* valueBuffer, int length, const char* customHtml,
    const char* type)
{
  this->label = NULL;
  this->_id = id;
  this->valueBuffer = valueBuffer;
  this->_length = length;
  this->type = type;
  this->customHtml = customHtml;
  this->visible = true;
  this->errorMessage = NULL;
}

EspWifiSeparator::EspWifiSeparator()
    : EspWifiParameter(NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, true)
{
}

EspWifiSeparator::EspWifiSeparator(const char* label)
    : EspWifiParameter(label, NULL, NULL, 0, NULL, NULL, NULL, NULL, true)
{
}

////////////////////////////////////////////////////////////////

ESPWIFI::ESPWIFI(
    const char* defaultThingName, DNSServer* dnsServer, WebServer* server,
    const char* initialApPassword, const char* configVersion)
{
  strncpy(this->_thingName, defaultThingName, ESPWIFI_WORD_LEN);
  this->_dnsServer = dnsServer;
  this->_server = server;
  this->_initialApPassword = initialApPassword;
  this->_configVersion = configVersion;
  itoa(this->_apTimeoutMs / 1000, this->_apTimeoutStr, 10);

  this->_thingNameParameter = EspWifiParameter("Thing name", "iwcThingName", this->_thingName, ESPWIFI_WORD_LEN);
  this->_apPasswordParameter = EspWifiParameter("AP password", "iwcApPassword", this->_apPassword, ESPWIFI_WORD_LEN, "password");
  this->_wifiSsidParameter = EspWifiParameter("WiFi SSID", "iwcWifiSsid", this->_wifiSsid, ESPWIFI_WORD_LEN);
  this->_wifiPasswordParameter = EspWifiParameter("WiFi password", "iwcWifiPassword", this->_wifiPassword, ESPWIFI_WORD_LEN, "password");
  this->_apTimeoutParameter = EspWifiParameter("Startup delay (seconds)", "iwcApTimeout", this->_apTimeoutStr, ESPWIFI_WORD_LEN, "number", NULL, NULL, "min='1' max='600'", false);
  this->addParameter(&this->_thingNameParameter);
  this->addParameter(&this->_apPasswordParameter);
  this->addParameter(&this->_wifiSsidParameter);
  this->addParameter(&this->_wifiPasswordParameter);
  this->addParameter(&this->_apTimeoutParameter);
}

char* ESPWIFI::getThingName()
{
  return this->_thingName;
}

void ESPWIFI::setConfigPin(int configPin)
{
  this->_configPin = configPin;
}

void ESPWIFI::setStatusPin(int statusPin)
{
  this->_statusPin = statusPin;
}

void ESPWIFI::setupUpdateServer(
    HTTPUpdateServer* updateServer, const char* updatePath)
{
  this->_updateServer = updateServer;
  this->_updatePath = updatePath;
}

boolean ESPWIFI::init()
{
  // -- Setup pins.
  if (this->_configPin >= 0)
  {
    pinMode(this->_configPin, INPUT_PULLUP);
    this->_forceDefaultPassword = (digitalRead(this->_configPin) == LOW);
  }
  if (ESPWIFI_STATUS_ENABLED)
  {
    pinMode(this->_statusPin, OUTPUT);
    digitalWrite(this->_statusPin, ESPWIFI_STATUS_ON);
  }

  // -- Load configuration from EEPROM.
  this->configInit();
  boolean validConfig = this->configLoad();
  if (!validConfig)
  {
    // -- No config
    this->_apPassword[0] = '\0';
    this->_wifiSsid[0] = '\0';
    this->_wifiPassword[0] = '\0';
    this->_apTimeoutMs = ESPWIFI_DEFAULT_AP_MODE_TIMEOUT_MS;
  }
  else
  {
    this->_apTimeoutMs = atoi(this->_apTimeoutStr) * 1000;
  }

  // -- Setup mdns
#ifdef ESP8266
  WiFi.hostname(this->_thingName);
#elif defined(ESP32)
  WiFi.setHostname(this->_thingName);
#endif
#ifdef ESPWIFI_CONFIG_USE_MDNS
  MDNS.begin(this->_thingName);
  MDNS.addService("http", "tcp", 80);
#endif

  return validConfig;
}

//////////////////////////////////////////////////////////////////

bool ESPWIFI::addParameter(EspWifiParameter* parameter)
{
/*
#ifdef ESPWIFI_DEBUG_TO_SERIAL
  Serial.print("Adding parameter '");
  Serial.print(parameter->getId());
  Serial.println("'");
#endif
*/
  if (this->_firstParameter == NULL)
  {
    this->_firstParameter = parameter;
//    ESPWIFI_DEBUG_LINE(F("Adding as first"));
    return true;
  }
  EspWifiParameter* current = this->_firstParameter;
  while (current->_nextParameter != NULL)
  {
    current = current->_nextParameter;
  }

  current->_nextParameter = parameter;
  return true;
}

void ESPWIFI::configInit()
{
  int size = 0;
  EspWifiParameter* current = this->_firstParameter;
  while (current != NULL)
  {
    size += current->getLength();
    current = current->_nextParameter;
  }
#ifdef ESPWIFI_DEBUG_TO_SERIAL
  Serial.print("Config size: ");
  Serial.println(size);
#endif

  EEPROM.begin(
      ESPWIFI_CONFIG_START + ESPWIFI_CONFIG_VESION_LENGTH + size);
}

/**
 * Load the configuration from the eeprom.
 */
boolean ESPWIFI::configLoad()
{
  if (this->configTestVersion())
  {
    EspWifiParameter* current = this->_firstParameter;
    int start = ESPWIFI_CONFIG_START + ESPWIFI_CONFIG_VESION_LENGTH;
    while (current != NULL)
    {
      if (current->getId() != NULL)
      {
        this->readEepromValue(start, current->valueBuffer, current->getLength());
#ifdef ESPWIFI_DEBUG_TO_SERIAL
        const char* defaultMarker = "";
#endif
        if ((strlen(current->valueBuffer) == 0) && (current->defaultValue != NULL))
        {
          strncpy(current->valueBuffer, current->defaultValue, current->getLength());
#ifdef ESPWIFI_DEBUG_TO_SERIAL
          defaultMarker = " (using default)";
#endif
        }
#ifdef ESPWIFI_DEBUG_TO_SERIAL
        Serial.print("Loaded config '");
        Serial.print(current->getId());
        Serial.print("'= ");
# ifdef ESPWIFI_DEBUG_PWD_TO_SERIAL
        Serial.print("'");
        Serial.print(current->valueBuffer);
        Serial.print("'");
        Serial.println(defaultMarker);
# else
        if (strcmp("password", current->type) == 0)
        {
          Serial.print(F("<hidden>"));
          Serial.println(defaultMarker);
        }
        else
        {
          Serial.print("'");
          Serial.print(current->valueBuffer);
          Serial.print("'");
          Serial.println(defaultMarker);
        }
# endif
#endif

        start += current->getLength();
      }
      current = current->_nextParameter;
    }
    return true;
  }
  else
  {
    ESPWIFI_DEBUG_LINE(F("Wrong config version."));
    return false;
  }
}

void ESPWIFI::configSave()
{
  this->configSaveConfigVersion();
  EspWifiParameter* current = this->_firstParameter;
  int start = ESPWIFI_CONFIG_START + ESPWIFI_CONFIG_VESION_LENGTH;
  while (current != NULL)
  {
    if (current->getId() != NULL)
    {
#ifdef ESPWIFI_DEBUG_TO_SERIAL
      Serial.print("Saving config '");
      Serial.print(current->getId());
      Serial.print("'= ");
# ifdef ESPWIFI_DEBUG_PWD_TO_SERIAL
      Serial.print("'");
      Serial.print(current->valueBuffer);
      Serial.println("'");
# else
      if (strcmp("password", current->type) == 0)
      {
        Serial.print(F("<hidden>"));
      }
      else
      {
        Serial.print("'");
        Serial.print(current->valueBuffer);
        Serial.println("'");
      }
# endif
#endif

      this->writeEepromValue(start, current->valueBuffer, current->getLength());
      start += current->getLength();
    }
    current = current->_nextParameter;
  }
  EEPROM.commit();

  this->_apTimeoutMs = atoi(this->_apTimeoutStr) * 1000;

  if (this->_configSavedCallback != NULL)
  {
    this->_configSavedCallback();
  }
}

void ESPWIFI::readEepromValue(int start, char* valueBuffer, int length)
{
  for (int t = 0; t < length; t++)
  {
    *((char*)valueBuffer + t) = EEPROM.read(start + t);
  }
}
void ESPWIFI::writeEepromValue(int start, char* valueBuffer, int length)
{
  for (int t = 0; t < length; t++)
  {
    EEPROM.write(start + t, *((char*)valueBuffer + t));
  }
}

boolean ESPWIFI::configTestVersion()
{
  for (byte t = 0; t < ESPWIFI_CONFIG_VESION_LENGTH; t++)
  {
    if (EEPROM.read(ESPWIFI_CONFIG_START + t) != this->_configVersion[t])
    {
      return false;
    }
  }
  return true;
}

void ESPWIFI::configSaveConfigVersion()
{
  for (byte t = 0; t < ESPWIFI_CONFIG_VESION_LENGTH; t++)
  {
    EEPROM.write(ESPWIFI_CONFIG_START + t, this->_configVersion[t]);
  }
}

void ESPWIFI::setWifiConnectionCallback(std::function<void()> func)
{
  this->_wifiConnectionCallback = func;
}

void ESPWIFI::setConfigSavedCallback(std::function<void()> func)
{
  this->_configSavedCallback = func;
}

void ESPWIFI::setFormValidator(std::function<boolean()> func)
{
  this->_formValidator = func;
}

void ESPWIFI::setWifiConnectionTimeoutMs(unsigned long millis)
{
  this->_wifiConnectionTimeoutMs = millis;
}

////////////////////////////////////////////////////////////////////////////////

void ESPWIFI::handleConfig()
{
  if (this->_state == ESPWIFI_STATE_ONLINE)
  {
    // -- Authenticate
    if (!this->_server->authenticate(
            ESPWIFI_ADMIN_USER_NAME, this->_apPassword))
    {
      ESPWIFI_DEBUG_LINE(F("Requesting authentication."));
      this->_server->requestAuthentication();
      return;
    }
  }

  if (!this->_server->hasArg("iotSave") || !this->validateForm())
  {
    // -- Display config portal
    ESPWIFI_DEBUG_LINE(F("Configuration page requested."));
    String page = htmlFormatProvider->getHead();
    page.replace("{v}", "Config ESP");
    page += htmlFormatProvider->getScript();
    page += htmlFormatProvider->getStyle();
    page += htmlFormatProvider->getHeadExtension();
    page += htmlFormatProvider->getHeadEnd();

    page += htmlFormatProvider->getFormStart();
    char parLength[5];
    // -- Add parameters to the form
    EspWifiParameter* current = this->_firstParameter;
    while (current != NULL)
    {
      if (current->getId() == NULL)
      {
#ifdef ESPWIFI_DEBUG_TO_SERIAL
        Serial.println("Rendering separator");
#endif
        page += "</fieldset><fieldset>";
        if (current->label != NULL)
        {
          page += "<legend>";
          page += current->label;
          page += "</legend>";
        }
      }
      else if (current->visible)
      {
#ifdef ESPWIFI_DEBUG_TO_SERIAL
        Serial.print("Rendering '");
        Serial.print(current->getId());
        Serial.print("' with value: ");
# ifdef ESPWIFI_DEBUG_PWD_TO_SERIAL
        Serial.println(current->valueBuffer);
# else
        if (strcmp("password", current->type) == 0)
        {
          Serial.println(F("<hidden>"));
        }
        else
        {
          Serial.println(current->valueBuffer);
        }
# endif
#endif

        String pitem;
        if (current->label != NULL)
        {
          pitem = htmlFormatProvider->getFormParam(current->type);
          pitem.replace("{b}", current->label);
          pitem.replace("{t}", current->type);
          pitem.replace("{i}", current->getId());
          pitem.replace("{p}", current->placeholder == NULL ? "" : current->placeholder);
          snprintf(parLength, 5, "%d", current->getLength());
          pitem.replace("{l}", parLength);
          if (strcmp("password", current->type) == 0)
          {
            // -- Value of password is not rendered
            pitem.replace("{v}", "");
          }
          else if (this->_server->hasArg(current->getId()))
          {
            // -- Value from previous submit
            pitem.replace("{v}", this->_server->arg(current->getId()));
          }
          else
          {
            // -- Value from config
            pitem.replace("{v}", current->valueBuffer);
          }
          pitem.replace(
              "{c}", current->customHtml == NULL ? "" : current->customHtml);
          pitem.replace(
              "{e}",
              current->errorMessage == NULL ? "" : current->errorMessage);
          pitem.replace(
              "{s}",
              current->errorMessage == NULL ? "" : "de"); // Div style class.
        }
        else
        {
          pitem = current->customHtml;
        }

        page += pitem;
      }
      current = current->_nextParameter;
    }

    page += htmlFormatProvider->getFormEnd();

    if (this->_updatePath != NULL)
    {
      String pitem = htmlFormatProvider->getUpdate();
      pitem.replace("{u}", this->_updatePath);
      page += pitem;
    }

    // -- Fill config version string;
    {
      String pitem = htmlFormatProvider->getConfigVer();
      pitem.replace("{v}", this->_configVersion);
      page += pitem;
    }

    page += htmlFormatProvider->getEnd();

    this->_server->sendHeader("Content-Length", String(page.length()));
    this->_server->send(200, "text/html; charset=UTF-8", page);
  }
  else
  {
    // -- Save config
    ESPWIFI_DEBUG_LINE(F("Updating configuration"));
    char temp[ESPWIFI_WORD_LEN];

    EspWifiParameter* current = this->_firstParameter;
    while (current != NULL)
    {
      if ((current->getId() != NULL) && (current->visible))
      {
        if ((strcmp("password", current->type) == 0) &&
            (current->getLength() <= ESPWIFI_WORD_LEN))
        {
          // TODO: Passwords longer than ESPWIFI_WORD_LEN not supported.
          this->readParamValue(current->getId(), temp, current->getLength());
          if (temp[0] != '\0')
          {
            // -- Value was set.
            strncpy(current->valueBuffer, temp, current->getLength());
#ifdef ESPWIFI_DEBUG_TO_SERIAL
            Serial.print(current->getId());
            Serial.println(" was set");
#endif
          }
          else
          {
#ifdef ESPWIFI_DEBUG_TO_SERIAL
            Serial.print(current->getId());
            Serial.println(" was not changed");
#endif
          }
        }
        else
        {
          this->readParamValue(
              current->getId(), current->valueBuffer, current->getLength());
#ifdef ESPWIFI_DEBUG_TO_SERIAL
          Serial.print(current->getId());
          Serial.print("='");
# ifdef ESPWIFI_DEBUG_PWD_TO_SERIAL
          Serial.print(current->valueBuffer);
# else
          if (strcmp("password", current->type) == 0)
          {
            Serial.print(F("<hidden>"));
          }
          else
          {
            Serial.print(current->valueBuffer);
          }
# endif
          Serial.print(current->valueBuffer);
          Serial.println("'");
#endif
        }
      }
      current = current->_nextParameter;
    }

    this->configSave();

    String page = htmlFormatProvider->getHead();
    page.replace("{v}", "Config ESP");
    page += htmlFormatProvider->getScript();
    page += htmlFormatProvider->getStyle();
//    page += _customHeadElement;
    page += htmlFormatProvider->getHeadExtension();
    page += htmlFormatProvider->getHeadEnd();
    page += "Configuration saved. ";
    if (this->_apPassword[0] == '\0')
    {
      page += F("You must change the default AP password to continue. Return "
                "to <a href=''>configuration page</a>.");
    }
    else if (this->_wifiSsid[0] == '\0')
    {
      page += F("You must provide the local wifi settings to continue. Return "
                "to <a href=''>configuration page</a>.");
    }
    else if (this->_state == ESPWIFI_STATE_NOT_CONFIGURED)
    {
      page += F("Please disconnect from WiFi AP to continue!");
    }
    else
    {
      page += F("Return to <a href='/'>home page</a>.");
    }
    page += htmlFormatProvider->getHeadEnd();

    this->_server->sendHeader("Content-Length", String(page.length()));
    this->_server->send(200, "text/html; charset=UTF-8", page);
  }
}

void ESPWIFI::readParamValue(
    const char* paramName, char* target, unsigned int len)
{
  String value = this->_server->arg(paramName);
#ifdef ESPWIFI_DEBUG_TO_SERIAL
  Serial.print("Value of arg '");
  Serial.print(paramName);
  Serial.print("' is:");
  Serial.println(value);
#endif
  value.toCharArray(target, len);
}

boolean ESPWIFI::validateForm()
{
  // -- Clean previous error messages.
  EspWifiParameter* current = this->_firstParameter;
  while (current != NULL)
  {
    current->errorMessage = NULL;
    current = current->_nextParameter;
  }

  // -- Call external validator.
  boolean valid = true;
  if (this->_formValidator != NULL)
  {
    valid = this->_formValidator();
  }

  // -- Internal validation.
  int l = this->_server->arg(this->_thingNameParameter.getId()).length();
  if (3 > l)
  {
    this->_thingNameParameter.errorMessage =
        "Give a name with at least 3 characters.";
    valid = false;
  }
  l = this->_server->arg(this->_apPasswordParameter.getId()).length();
  if ((0 < l) && (l < 8))
  {
    this->_apPasswordParameter.errorMessage =
        "Password length must be at least 8 characters.";
    valid = false;
  }
  l = this->_server->arg(this->_wifiPasswordParameter.getId()).length();
  if ((0 < l) && (l < 8))
  {
    this->_wifiPasswordParameter.errorMessage =
        "Password length must be at least 8 characters.";
    valid = false;
  }

  return valid;
}

void ESPWIFI::handleNotFound()
{
  if (this->handleCaptivePortal())
  {
    // If captive portal redirect instead of displaying the error page.
    return;
  }
#ifdef ESPWIFI_DEBUG_TO_SERIAL
  Serial.print("Requested non-existing page '");
  Serial.print(this->_server->uri());
  Serial.print("' arguments(");
  Serial.print(this->_server->method() == HTTP_GET ? "GET" : "POST");
  Serial.print("):");
  Serial.println(this->_server->args());
#endif
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += this->_server->uri();
  message += "\nMethod: ";
  message += (this->_server->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += this->_server->args();
  message += "\n";

  for (uint8_t i = 0; i < this->_server->args(); i++)
  {
    message +=
        " " + this->_server->argName(i) + ": " + this->_server->arg(i) + "\n";
  }
  this->_server->sendHeader(
      "Cache-Control", "no-cache, no-store, must-revalidate");
  this->_server->sendHeader("Pragma", "no-cache");
  this->_server->sendHeader("Expires", "-1");
  this->_server->sendHeader("Content-Length", String(message.length()));
  this->_server->send(404, "text/plain", message);
}

/**
 * Redirect to captive portal if we got a request for another domain.
 * Return true in that case so the page handler do not try to handle the request
 * again. (Code from WifiManager project.)
 */
boolean ESPWIFI::handleCaptivePortal()
{
  String host = this->_server->hostHeader();
  String thingName = String(this->_thingName);
  thingName.toLowerCase();
  if (!isIp(host) && !host.startsWith(thingName))
  {
#ifdef ESPWIFI_DEBUG_TO_SERIAL
    Serial.print("Request for ");
    Serial.print(host);
    Serial.print(" redirected to ");
    Serial.println(this->_server->client().localIP());
#endif
    this->_server->sendHeader(
      "Location", String("http://") + toStringIp(this->_server->client().localIP()), true);
    this->_server->send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    this->_server->client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

/** Is this an IP? */
boolean ESPWIFI::isIp(String str)
{
  for (size_t i = 0; i < str.length(); i++)
  {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9'))
    {
      return false;
    }
  }
  return true;
}

/** IP to String? */
String ESPWIFI::toStringIp(IPAddress ip)
{
  String res = "";
  for (int i = 0; i < 3; i++)
  {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

/////////////////////////////////////////////////////////////////////////////////

void ESPWIFI::delay(unsigned long m)
{
  unsigned long delayStart = millis();
  while (m > millis() - delayStart)
  {
    this->doLoop();
    delay(1); // -- Note: 1ms might not be enough to perform a full yield. So
              // 'yeild' in 'doLoop' is eventually a good idea.
  }
}

void ESPWIFI::doLoop()
{
  doBlink();
  yield(); // -- Yield should not be necessary, but cannot hurt eather.
  if (this->_state == ESPWIFI_STATE_BOOT)
  {
    // -- After boot, fall immediately to AP mode.
    byte startupState = ESPWIFI_STATE_AP_MODE;
    if (this->_skipApStartup)
    {
      if (isWifiModePossible())
      {
        ESPWIFI_DEBUG_LINE(
            F("SkipApStartup is requested, but either no WiFi was set up, or "
              "configButton was pressed."));
      }
      else
      {
        // -- Startup state can be WiFi, if it is requested and also possible.
        ESPWIFI_DEBUG_LINE(F("SkipApStartup mode was applied"));
        startupState = ESPWIFI_STATE_CONNECTING;
      }
    }
    this->changeState(startupState);
  }
  else if (
      (this->_state == ESPWIFI_STATE_NOT_CONFIGURED) ||
      (this->_state == ESPWIFI_STATE_AP_MODE))
  {
    // -- We must only leave the AP mode, when no slaves are connected.
    // -- Other than that AP mode has a timeout. E.g. after boot, or when retry
    // connecting to WiFi
    checkConnection();
    checkApTimeout();
    this->_dnsServer->processNextRequest();
    this->_server->handleClient();
  }
  else if (this->_state == ESPWIFI_STATE_CONNECTING)
  {
    if (checkWifiConnection())
    {
      this->changeState(ESPWIFI_STATE_ONLINE);
      return;
    }
  }
  else if (this->_state == ESPWIFI_STATE_ONLINE)
  {
    // -- In server mode we provide web interface. And check whether it is time
    // to run the client.
    this->_server->handleClient();
    if (WiFi.status() != WL_CONNECTED)
    {
      ESPWIFI_DEBUG_LINE(F("Not connected. Try reconnect..."));
      this->changeState(ESPWIFI_STATE_CONNECTING);
      return;
    }
  }
}

/**
 * What happens, when a state changed...
 */
void ESPWIFI::changeState(byte newState)
{
  switch (newState)
  {
    case ESPWIFI_STATE_AP_MODE:
    {
      // -- In AP mode we must override the default AP password. Otherwise we stay
      // in STATE_NOT_CONFIGURED.
      if (isWifiModePossible())
      {
#ifdef ESPWIFI_DEBUG_TO_SERIAL
        if (this->_forceDefaultPassword)
        {
          Serial.println("AP mode forced by reset pin");
        }
        else
        {
          Serial.println("AP password was not set in configuration");
        }
#endif
        newState = ESPWIFI_STATE_NOT_CONFIGURED;
      }
      break;
    }
    default:
      break;
  }
#ifdef ESPWIFI_DEBUG_TO_SERIAL
  Serial.print("State changing from: ");
  Serial.print(this->_state);
  Serial.print(" to ");
  Serial.println(newState);
#endif
  byte oldState = this->_state;
  this->_state = newState;
  this->stateChanged(oldState, newState);
#ifdef ESPWIFI_DEBUG_TO_SERIAL
  Serial.print("State changed from: ");
  Serial.print(oldState);
  Serial.print(" to ");
  Serial.println(newState);
#endif
}

/**
 * What happens, when a state changed...
 */
void ESPWIFI::stateChanged(byte oldState, byte newState)
{
//  updateOutput();
  switch (newState)
  {
    case ESPWIFI_STATE_AP_MODE:
    case ESPWIFI_STATE_NOT_CONFIGURED:
      if (newState == ESPWIFI_STATE_AP_MODE)
      {
        this->blinkInternal(300, 90);
      }
      else
      {
        this->blinkInternal(300, 50);
      }
      setupAp();
      if (this->_updateServer != NULL)
      {
        this->_updateServer->setup(this->_server, this->_updatePath);
      }
      this->_server->begin();
      this->_apConnectionStatus = ESPWIFI_AP_CONNECTION_STATE_NC;
      this->_apStartTimeMs = millis();
      break;
    case ESPWIFI_STATE_CONNECTING:
      if ((oldState == ESPWIFI_STATE_AP_MODE) ||
          (oldState == ESPWIFI_STATE_NOT_CONFIGURED))
      {
        stopAp();
      }
      this->blinkInternal(1000, 50);
#ifdef ESPWIFI_DEBUG_TO_SERIAL
      Serial.print("Connecting to [");
      Serial.print(this->_wifiAuthInfo.ssid);
# ifdef ESPWIFI_DEBUG_PWD_TO_SERIAL
      Serial.print("] with password [");
      Serial.print(this->_wifiAuthInfo.password);
      Serial.println("]");
# else
      Serial.println(F("] (password is hidden)"));
# endif
#endif
      this->_wifiConnectionStart = millis();
      this->_wifiConnectionHandler(
          this->_wifiAuthInfo.ssid, this->_wifiAuthInfo.password);
      break;
    case ESPWIFI_STATE_ONLINE:
      this->blinkInternal(8000, 2);
      if (this->_updateServer != NULL)
      {
        this->_updateServer->updateCredentials(
            ESPWIFI_ADMIN_USER_NAME, this->_apPassword);
      }
      this->_server->begin();
      ESPWIFI_DEBUG_LINE(F("Accepting connection"));
      if (this->_wifiConnectionCallback != NULL)
      {
        this->_wifiConnectionCallback();
      }
      break;
    default:
      break;
  }
}

void ESPWIFI::checkApTimeout()
{
  if ((this->_wifiSsid[0] != '\0') && (this->_apPassword[0] != '\0') &&
      (!this->_forceDefaultPassword))
  {
    // -- Only move on, when we have a valid WifF and AP configured.
    if ((this->_apConnectionStatus == ESPWIFI_AP_CONNECTION_STATE_DC) ||
        ((this->_apTimeoutMs < millis() - this->_apStartTimeMs) &&
         (this->_apConnectionStatus != ESPWIFI_AP_CONNECTION_STATE_C)))
    {
      this->changeState(ESPWIFI_STATE_CONNECTING);
    }
  }
}

/**
 * Checks whether we have anyone joined to our AP.
 * If so, we must not change state. But when our guest leaved, we can
 * immediately move on.
 */
void ESPWIFI::checkConnection()
{
  if ((this->_apConnectionStatus == ESPWIFI_AP_CONNECTION_STATE_NC) &&
      (WiFi.softAPgetStationNum() > 0))
  {
    this->_apConnectionStatus = ESPWIFI_AP_CONNECTION_STATE_C;
    ESPWIFI_DEBUG_LINE(F("Connection to AP."));
  }
  else if (
      (this->_apConnectionStatus == ESPWIFI_AP_CONNECTION_STATE_C) &&
      (WiFi.softAPgetStationNum() == 0))
  {
    this->_apConnectionStatus = ESPWIFI_AP_CONNECTION_STATE_DC;
    ESPWIFI_DEBUG_LINE(F("Disconnected from AP."));
    if (this->_forceDefaultPassword)
    {
      ESPWIFI_DEBUG_LINE(F("Releasing forced AP mode."));
      this->_forceDefaultPassword = false;
    }
  }
}

boolean ESPWIFI::checkWifiConnection()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    if (this->_wifiConnectionTimeoutMs < millis() - this->_wifiConnectionStart)
    {
      // -- WiFi not available, fall back to AP mode.
      ESPWIFI_DEBUG_LINE(F("Giving up."));
      WiFi.disconnect(true);
      EspWifiWifiAuthInfo* newWifiAuthInfo = _wifiConnectionFailureHandler();
      if (newWifiAuthInfo != NULL)
      {
        // -- Try connecting with another connection info.
        this->_wifiAuthInfo.ssid = newWifiAuthInfo->ssid;
        this->_wifiAuthInfo.password = newWifiAuthInfo->password;
        this->changeState(ESPWIFI_STATE_CONNECTING);
      }
      else
      {
        this->changeState(ESPWIFI_STATE_AP_MODE);
      }
    }
    return false;
  }

  // -- Connected
#ifdef ESPWIFI_DEBUG_TO_SERIAL
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#endif

  return true;
}

void ESPWIFI::setupAp()
{
  WiFi.mode(WIFI_AP);

#ifdef ESPWIFI_DEBUG_TO_SERIAL
  Serial.print("Setting up AP: ");
  Serial.println(this->_thingName);
#endif
  if (this->_state == ESPWIFI_STATE_NOT_CONFIGURED)
  {
#ifdef ESPWIFI_DEBUG_TO_SERIAL
    Serial.print("With default password: ");
# ifdef ESPWIFI_DEBUG_PWD_TO_SERIAL
    Serial.println(this->_initialApPassword);
# else
    Serial.println(F("<hidden>"));
# endif
#endif
    this->_apConnectionHandler(this->_thingName, this->_initialApPassword);
  }
  else
  {
#ifdef ESPWIFI_DEBUG_TO_SERIAL
    Serial.print("Use password: ");
# ifdef ESPWIFI_DEBUG_PWD_TO_SERIAL
    Serial.println(this->_apPassword);
# else
    Serial.println(F("<hidden>"));
# endif
#endif
    this->_apConnectionHandler(this->_thingName, this->_apPassword);
  }

#ifdef ESPWIFI_DEBUG_TO_SERIAL
  Serial.print(F("AP IP address: "));
  Serial.println(WiFi.softAPIP());
#endif
  //  delay(500); // Without delay I've seen the IP address blank
  //  Serial.print(F("AP IP address: "));
  //  Serial.println(WiFi.softAPIP());

  /* Setup the DNS server redirecting all the domains to the apIP */
  this->_dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  this->_dnsServer->start(ESPWIFI_DNS_PORT, "*", WiFi.softAPIP());
}

void ESPWIFI::stopAp()
{
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
}

////////////////////////////////////////////////////////////////////

void ESPWIFI::blink(unsigned long repeatMs, byte dutyCyclePercent)
{
  if (repeatMs == 0)
  {
    this->stopCustomBlink();
  }
  else
  {
    this->_blinkOnMs = repeatMs * dutyCyclePercent / 100;
    this->_blinkOffMs = repeatMs * (100 - dutyCyclePercent) / 100;
  }
}

void ESPWIFI::fineBlink(unsigned long onMs, unsigned long offMs)
{
  this->_blinkOnMs = onMs;
  this->_blinkOffMs = offMs;
}

void ESPWIFI::stopCustomBlink()
{
  this->_blinkOnMs = this->_internalBlinkOnMs;
  this->_blinkOffMs = this->_internalBlinkOffMs;
}

void ESPWIFI::blinkInternal(unsigned long repeatMs, byte dutyCyclePercent)
{
  this->blink(repeatMs, dutyCyclePercent);
  this->_internalBlinkOnMs = this->_blinkOnMs;
  this->_internalBlinkOffMs = this->_blinkOffMs;
}

void ESPWIFI::doBlink()
{
  if (ESPWIFI_STATUS_ENABLED)
  {
    unsigned long now = millis();
    unsigned long delayMs =
        this->_blinkState == LOW ? this->_blinkOnMs : this->_blinkOffMs;
    if (delayMs < now - this->_lastBlinkTime)
    {
      this->_blinkState = 1 - this->_blinkState;
      this->_lastBlinkTime = now;
      digitalWrite(this->_statusPin, this->_blinkState);
    }
  }
}

boolean ESPWIFI::connectAp(const char* apName, const char* password)
{
  return WiFi.softAP(apName, password);
}
void ESPWIFI::connectWifi(const char* ssid, const char* password)
{
  WiFi.begin(ssid, password);
}
EspWifiWifiAuthInfo* ESPWIFI::handleConnectWifiFailure()
{
  return NULL;
}
