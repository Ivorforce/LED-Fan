//
// Created by Lukas Tenbrink on 20.01.20.
//

#include "Screen.h"

#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define LED_PIN 25

Screen::Screen(int ledCount): count(ledCount) {
    this->leds = new CRGB[ledCount];
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, ledCount)
            .setCorrection(TypicalLEDStrip);
}

void Screen::draw(float rotation) {
    int start = scrapMode == 0 ? 0 : scrapMode == 1 ? 85 : 170;
    fill_rainbow(leds, count, start + (int)(rotation * 255), 7);
    FastLED.show();
}

void Screen::drawError() {
    fill_solid(leds, count, CRGB::Red);
    FastLED.show();
}
