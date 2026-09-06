#ifndef AMPULE_WEB_SERVER_H
#define AMPULE_WEB_SERVER_H

// Hosts the local dashboard (static files from LittleFS) and the REST API
// on the ESP32 itself. No external server, Internet access, or companion
// device is required — everything is served from http://192.168.4.1.
namespace WebServerManager {

    void begin();
    void handleClient(); // call every loop() iteration; never blocks
}

#endif // AMPULE_WEB_SERVER_H
