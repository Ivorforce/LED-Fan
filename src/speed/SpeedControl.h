//
// Created by Lukas Tenbrink on 17.04.20.
//

#ifndef LED_FAN_SPEEDCONTROL_H
#define LED_FAN_SPEEDCONTROL_H


#include <sensor/RotationSensor.h>
#include <util/PWMPin.h>
#include <cmath>

class SpeedControl {
public:
    PWMPin *forwardPin;
    PWMPin *backwardPin;

    RotationSensor *rotationSensor;

    float maxSpeedRotationsPerSecond = 0;

    unsigned long microsPerUpdate = 1000 * 200;
    unsigned long microsUntilUpdate = 0;

    float accelerationLimit = 0;

    SpeedControl(PWMPin *forwardPin, PWMPin *backwardPin, RotationSensor *rotationSensor, float maxSpeedRotationsPerSecond);

    void setDesiredSpeed(float speed);
    float getDesiredSpeed() { return desiredSpeed; };

    float getSpeed() { return speed; };

    void update(unsigned long millisDelay);

    void stop();

private:
    float desiredSpeed = 0;
    float speed = NAN;
    unsigned long microsAccelerating = 0;

    void setSpeed(float speed);

    void flush();
};


#endif //LED_FAN_SPEEDCONTROL_H
