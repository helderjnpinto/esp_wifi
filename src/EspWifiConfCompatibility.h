#ifdef ESP8266
# define WebServer ESP8266WebServer
# define HTTPUpdateServer ESP8266HTTPUpdateServer
#endif



/**
 * ESP32 doesn't implement a HTTPUpdateServer. However it seams, that to code
 * from ESP8266 covers nearly all the same functionality.
 * So we need to implement our own HTTPUpdateServer for ESP32, and code is
 * reused from
 * https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPUpdateServer/src/
 * version: 41de43a26381d7c9d29ce879dd5d7c027528371b
 */
#ifdef ESP32

#ifndef __HTTP_UPDATE_SERVER_H
#define __HTTP_UPDATE_SERVER_H

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <StreamString.h>
#include <Update.h>

#define emptyString F("")

class WebServer;

class HTTPUpdateServer
{
  public:
    HTTPUpdateServer(bool serial_debug=false);

    void setup(WebServer *server)
    {
      setup(server, emptyString, emptyString);
    }

    void setup(WebServer *server, const String& path)
    {
      setup(server, path, emptyString, emptyString);
    }

    void setup(WebServer *server, const String& username, const String& password)
    {
      setup(server, "/update", username, password);
    }

    void setup(WebServer *server, const String& path, const String& username, const String& password);

    void updateCredentials(const String& username, const String& password)
    {
      _username = username;
      _password = password;
    }

  protected:
    void _setUpdaterError();

  private:
    bool _serial_output;
    WebServer *_server;
    String _username;
    String _password;
    bool _authenticated;
    String _updaterError;
};
#endif

#endif
