//
// Created by Lukas Tenbrink on 20.01.20.
//

#include <cmath>
#include <util/Logger.h>
#include <util/cluster/FastCluster.h>
#include <util/Math.h>
#include <esp32-hal.h>
#include "RotationSensor.h"

#include "Setup.h"

RotationSensor::RotationSensor(GPIOVisitor *visitor, int historySize, Extrapolator *extrapolator) :
    checkpointTimestamps(new IntRoller(historySize)),
    checkpointIndices(new IntRoller(historySize)),
    visitor(visitor),
    extrapolator(extrapolator)  {
}

void RotationSensor::update() {
    didUpdate = false;

    int checkpoint = -1;
    unsigned long time;
    visitor->update(&checkpoint, &time);

    if (checkpoint < 0) {
        if (isPaused || (int(micros()) - checkpointTimestamps->last()) < pauseInterval)
            return;

        checkpointTimestamps->fill(0);
        checkpointIndices->fill(-1);
        _isReliable = false;
        isPaused = true;
        didUpdate = true;

        return;
    }

    isPaused = false;
    unsigned long checkpointTime = time - checkpointTimestamps->last();

    int checkpointCount = visitor->checkpointCount;
    // 0->1 = 1; 0->0 = count
    int diff = std::abs(checkpoint - checkpointIndices->last());
    int elapsedCheckpoints = diff == 0 ? checkpointCount : std::min(diff, checkpointCount - diff);

    if (checkpointTime < minCheckpointTime * elapsedCheckpoints) {
        return;
    }

    registerCheckpoint(time, checkpoint);
    didUpdate = true;
}

void RotationSensor::registerCheckpoint(unsigned long time, int checkpoint) {
    int historySize = checkpointIndices->count;
    int checkpointCount = visitor->checkpointCount;

    checkpointIndices->push(checkpoint);
    checkpointTimestamps->push((int) time);

    int n = (int) historySize - checkpointIndices->countOccurrences(-1);

    if (n < minCheckpointPasses) {
        // Too little data to make meaningful extrapolation
        _isReliable = false;
        return;
    }

    // If we have <= 2 checkpoints, can't estimate direction. Just assume 1.
    auto direction = historySize > 2 ? estimatedDirection() : 1;
    if (direction == 0) {
        // Unsure about direction - probably unreliable!
        _isReliable = false;
        return;
    }

    if (criticalCheckpoint>= 0 && checkpoint != criticalCheckpoint) {
        // Recalculating won't actually do anything different
        return;
    }

    referenceTime = checkpoint;

    // Raw Data Collection
    std::vector<float> x{};
    x.reserve(n);
    std::vector<int> estimatedY{};
    estimatedY.reserve(n);

    for (int j = 0; j < historySize; ++j) {
        int checkpointIndex = (*checkpointIndices)[j];

        if (checkpointIndex < 0)
            continue; // Not set yet

        x.push_back((*checkpointTimestamps)[j] - referenceTime);
        estimatedY.push_back(checkpointIndex);
    }

    float estimatedCheckpointDiff = FastCluster::center(
        FastCluster::stepDiffs(x),
        float(10 * 1000)
    );

    // Try to estimate if we missed any checkpoints
    std::vector<float> y(n);

    if (estimatedCheckpointDiff <= 0) {
        // Not sure what happened... but don't take the chance.
        _isReliable = false;
        return;
    }

    // Go back in reverse, setting reached checkpoint as baseline Y
    y[n - 1] = estimatedY[n - 1];
    for (int j = n - 2; j >= 0; --j) {
        int expectedSteps = (estimatedY[j + 1] - estimatedY[j] + checkpointCount * direction) % checkpointCount;
        float estimatedSteps = (x[j + 1] - x[j]) / estimatedCheckpointDiff * direction;

        // Accept +- multiples of checkpointCount
        y[j] = y[j + 1] - (round((estimatedSteps - expectedSteps) / checkpointCount) * checkpointCount + expectedSteps);
    }

    if (criticalCheckpoint >= 0) {
        for (int i = n - 1; i >= 0; --i) {
            if (estimatedY[i] != criticalCheckpoint) {
                x.erase(x.begin() + i);
                y.erase(y.begin() + i);
            }
        }
    }
    else if (separateCheckpoints) {
        // Expand any available checkpoint -> (checkpoint + 1) to full rotation
        // -- Practically 'deleting' any other segments from time history
        // -- Regress, and then 'unexpand' the segment into its intended size
        int segmentStart = checkpoint;

        // Always keep last x and y, since it's our current reference
        for (int i = n - 2; i >= 0; --i) {
            if (i == 0 || std::lround(estimatedY[i - 1]) != segmentStart || (std::lround(y[i] - y[i - 1]) != 1)) {
                // Uninteresting; remove!
                x.erase(x.begin() + i);
                y.erase(y.begin() + i);
                continue;
            }

            // Found a separatable segment, determine its length
            // i + 1 is our last reference, sync the current segment up to it, deleting any time in between
            x[i] = x[i + 1] - (x[i] - x[i - 1]);
            // Since we're only considering in-segment differences, set y to reflect 1 segment slopes
            y[i] = y[i + 1] - 1;
        }
    }

    // We erased some values; gotta recompute
    if (x.size() < minCheckpointPasses) {
        _isReliable = false;
        return;
    }

    extrapolator->adjust(x, y);
    auto rotationsPerSecond = this->rotationsPerSecond();
    auto rotationsPerSecondP = std::abs(rotationsPerSecond);

    // Speed is sensible?
    _isReliable = rotationsPerSecondP < 100 && rotationsPerSecondP > 0.5;
}

int RotationSensor::estimatedDirection() {
    int historySize = checkpointIndices->count;

    int direction = 0;
    for (unsigned int i = 1; i < checkpointIndices->count; i++) {
        int diff = (((*checkpointIndices)[i] - (*checkpointIndices)[i - 1]) + historySize) % historySize;
        if (diff == 0)
            continue;

        if (diff < historySize / 2)
            direction++; // Moved < 180°; probably forwards
        else if (diff > historySize / 2) {
            direction--; // Moved > 180°; probably backwards
        }
    }
    return signum(direction);
}

float RotationSensor::estimatedRotation(unsigned long time) {
    if (fixedRotation >= 0)
        return fixedRotation;

    if (!_isReliable || isPaused)
        return NAN;

    float position = extrapolator->extrapolate(int(time) - referenceTime);

    if (std::isnan(position))
        return NAN;

    float rotation = position / (float) visitor->checkpointCount;

    if (rotation > 3.5 || rotation < -3.5)
        // Missed checkpoints for > 3 rotations...
        // Or went back too far
        return NAN;

    // Add 10 beforehand so we're always positive
    return std::fmod(rotationOffset + (rotationFlip ? -rotation : rotation), 1.0f);
}

float RotationSensor::rotationsPerSecond() {
    if (isPaused)
        return 0;

    return extrapolator->slope() * 1000 * 1000 / visitor->checkpointCount;
}

bool RotationSensor::isReliable() {
    return _isReliable || fixedRotation >= 0;
}

String RotationSensor::stateDescription() {
    return visitor->stateDescription();
}
