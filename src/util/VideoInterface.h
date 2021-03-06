//
// Created by Lukas Tenbrink on 21.01.20.
//

#ifndef LED_FAN_VIDEOINTERFACE_H
#define LED_FAN_VIDEOINTERFACE_H

#include <ArduinoJson.h>
#include <screen/Screen.h>
#include <network/ArtnetServer.h>
#include <ESPAsyncWebServer.h>

class VideoInterface {
public:
    Screen *screen;
    ArtnetServer *artnetServer;

    VideoInterface(Screen *screen, ArtnetServer *artnetServer);

    void info(AsyncResponseStream *stream);
};


#endif //LED_FAN_VIDEOINTERFACE_H
