//
// Created by Lukas Tenbrink on 20.01.20.
//

#ifndef LED_FAN_SCREEN_H
#define LED_FAN_SCREEN_H

static const int MICROS_INPUT_ACTIVE = 5000 * 1000;

#include <util/IntRoller.h>
#include <sensor/RotationSensor.h>
#include <screen/behavior/NativeBehavior.h>
#include <util/Image.h>
#include "Renderer.h"

class Blade {
public:
    struct Pixel {
        float radius;
        int ringIndex;
        int pixelIndex;
        int bladeIndex;

        PRGB *color;

        // Cached for performance
        int concentricResolution;
        PRGB *concentricPointer;
    };

    float rotationOffset;
    int pixelCount;
    Pixel *pixels;

    Blade(int pixelCount, float rotationOffset);
};

class Screen {
public:
    enum Mode {
        none, cartesian, concentric, count
    };

    enum CartesianSampling {
        nearest, bilinear
    };

    RotationSensor *rotationSensor;
    Renderer *renderer;

    int bladeCount;
    Blade **blades;
    Blade::Pixel **pixels;

    unsigned long lastUpdateTimestamp;

    Mode getMode() const;
    void setMode(Mode mode);

    // Multi-purpose buffer for any input mode
    PRGB *buffer;
    int bufferSize;

    int cartesianResolution;
    float cartesianCenter;
    BilinearTraverser *bilinearTraverser;
    CartesianSampling cartesianSampling = nearest;

    IntRoller *concentricResolution;

    NativeBehavior *behavior = nullptr;
    unsigned long inputTimestamps[Mode::count];

    Screen(Renderer *renderer, int cartesianResolution, IntRoller *concentricResolution);

    void readConfig();

    void update(unsigned long delayMicros);

    void draw(unsigned long delayMicros);
    void drawCartesian();
    void drawConcentric();

    int noteInput(Mode mode);

    void determineMode(unsigned long microseconds);
    void setRadialCorrection(float ratio);

    int getPixelCount() {
        return renderer->pixelCount;
    }

    float getBrightness() const {
        return renderer->getBrightness();
    };
    void setBrightness(float brightness);

    float getResponse() const;;
    void setResponse(float response);;

protected:
    Mode _mode = none;

    float _radialCorrection;

    bool hasSignal(unsigned long microseconds) const;
};


#endif //LED_FAN_SCREEN_H
