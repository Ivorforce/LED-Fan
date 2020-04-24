//
// Created by Lukas Tenbrink on 25.04.20.
//

#ifndef LED_FAN_PING_H
#define LED_FAN_PING_H


#include "NativeBehavior.h"

class Ping : public NativeBehavior {
public:
    unsigned long timeLeft;

    Ping(int timeLeft);

    virtual bool update(CRGB *leds, int ledCount, unsigned long delay);
};


#endif //LED_FAN_PING_H
