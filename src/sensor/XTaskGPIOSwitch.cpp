//
// Created by Lukas Tenbrink on 22.02.20.
//

#include "XTaskGPIOSwitch.h"

#include <util/XTaskTimer.h>

void XTaskGPIOSwitch::update(int *visitCheckpoint, unsigned long *visitTime) {
    if (lastCheckpoint >= 0) {
        *visitCheckpoint = lastCheckpoint;
        *visitTime = lastVisitTime;

        lastCheckpoint = 0;
    }
}

XTaskGPIOSwitch::XTaskGPIOSwitch(const std::vector<int> &pins, double decay, int delay) : SyncGPIOSwitch(pins, decay) {
    timer = new XTaskTimer(
            delay,
        "RSENSOR",
        10,
        [this](unsigned long time){
            SyncGPIOSwitch::update(&lastCheckpoint, &lastVisitTime);
        }
    );
}
