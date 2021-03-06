//
// Created by Lukas Tenbrink on 27.04.20.
//

#include "App.h"

#ifdef FastLED_LED_TYPE
#include "util/spi/ESP32SPI.h"
#include <screen/FastLEDRenderer.h>
#endif

#include <SPIFFS.h>
#include <screen/ConcentricCoordinates.h>
#include <network/Network.h>
#include <screen/Apa102Renderer.h>

#include <util/Logger.h>
#include <util/LUT.h>
#include <screen/behavior/Behaviors.h>

#if ROTATION_SENSOR_TYPE == ROTATION_SENSOR_TYPE_HALL_SYNC
#include <sensor/SyncGPIOSwitch.h>
#elif  ROTATION_SENSOR_TYPE == ROTATION_SENSOR_TYPE_HALL_XTASK
#include <sensor/XTaskGPIOSwitch.h>
#elif  ROTATION_SENSOR_TYPE == ROTATION_SENSOR_TYPE_INTERRUPT
#include <sensor/InterruptGPIOSwitch.h>
#endif

#if ROTATION_EXTRAPOLATION == ROTATION_EXTRAPOLATION_STEP
#include <util/extrapolation/StepExtrapolator.h>
#elif ROTATION_EXTRAPOLATION == ROTATION_EXTRAPOLATION_REGRESSION
#include <util/extrapolation/LinearRegressionExtrapolator.h>
#include <screen/behavior/FancyPing.h>

#endif

#define MICROSECONDS_PER_FRAME (1000 * 1000 / MAX_FRAMES_PER_SECOND)

App::App() {
    SerialLog.print("Booting LLED Fan Firmware").ln();

    // Don't init Device ID until asked via HTTP
    // Because WiFi gives excellent entropy
//    DeviceID::get();

    // Mount file system
    SPIFFS.begin(false);

    // Initialize WiFi first, we want to connect ASAP
    Network::setHostname(WIFI_HOSTNAME);
    Network::setSoftApSetup(new WifiSetup(HOST_NETWORK_SSID, HOST_NETWORK_PASSWORD));
    Network::readConfig();
    // Start connecting
    Network::checkStatus();

    // Init Statics
    LUT::initSin(LUT_SIN_COUNT);
    NativeBehaviors::init();

    // Clock Synchronizer
    regularClock = new RegularClock(
            MICROSECONDS_PER_FRAME, 50
    );

    // Initialize Screen

    std::vector<int> rotationSensorPins = {ROTATION_SENSOR_PINS};
    rotationSensor = new RotationSensor(
#if ROTATION_SENSOR_TYPE == ROTATION_SENSOR_TYPE_HALL_SYNC
            new SyncGPIOSwitch(rotationSensorPins, MICROSECONDS_PER_FRAME / 1000.0 / 1000.0 / 10.0),
#elif  ROTATION_SENSOR_TYPE == ROTATION_SENSOR_TYPE_HALL_XTASK
            new XTaskGPIOSwitch(rotationSensorPins, MICROSECONDS_PER_FRAME / 1000.0 / 1000.0 / 10.0, ROTATION_SENSOR_MS),
#elif  ROTATION_SENSOR_TYPE == ROTATION_SENSOR_TYPE_INTERRUPT
            new InterruptGPIOSwitch(rotationSensorPins),
#endif
            // if separate checkpoints, each n
            CHECKPOINT_HISTORY_COUNT * (ROTATION_SENSOR_SEPARATE_CHECKPOINTS ? rotationSensorPins.size() : 1),
#if ROTATION_EXTRAPOLATION == ROTATION_EXTRAPOLATION_STEP
            new StepExtrapolator()
#elif ROTATION_EXTRAPOLATION == ROTATION_EXTRAPOLATION_REGRESSION
            new LinearRegressionExtrapolator()
#endif
    );
    rotationSensor->minCheckpointTime = 1000 * 1000 / (MAX_ROTATIONS_PER_SECOND * rotationSensorPins.size());
    rotationSensor->pauseInterval = ROTATION_PAUSED_MS * 1000;
    rotationSensor->separateCheckpoints = ROTATION_SENSOR_SEPARATE_CHECKPOINTS;
    // Add 10 to always be positive
    rotationSensor->rotationOffset = ROTATION_SENSOR_OFFSET + 10;
#ifdef ROTATION_SENSOR_FLIP
    rotationSensor->rotationFlip = true;
#endif
#ifdef ROTATION_SENSOR_CRITICAL_CHECKPOINT
    rotationSensor->criticalCheckpoint = ROTATION_SENSOR_CRITICAL_CHECKPOINT;
#endif

#ifdef FastLED_LED_TYPE
    auto controller = new FastLED_LED_TYPE<LED_DATA_PIN, LED_CLOCK_PIN, COLOR_ORDER, DATA_RATE_MHZ(LED_CLOCK_SPEED_MHZ)>();
    auto renderer = new FastLEDRenderer(LED_COUNT, LED_OVERFLOW_WALL, controller);
#else
    auto renderer = new Apa102Renderer(LED_COUNT, LED_OVERFLOW_WALL);
#endif
    renderer->setColorCorrection(0xFFB0F0);
    renderer->setMaxDynamicColorRescale(MAX_DYNAMIC_COLOR_RESCALE);

    screen = new Screen(
        renderer,
        int(LED_COUNT * CARTESIAN_RESOLUTION_FACTOR),
        ConcentricCoordinates::resolution(LED_COUNT, CONCENTRIC_RESOLUTION_ADD, CONCENTRIC_RESOLUTION_MIN)
    );
    screen->setRadialCorrection(LED_BRIGHTNESS_CORRECTION);
    screen->rotationSensor = rotationSensor;
    screen->cartesianSampling = Screen::CARTESIAN_SAMPLING_MODE;
    // Startup Animation
    screen->behavior = new FancyPing();

#ifdef MAX_AMPERE
    float peakAmpereDrawn = LED_COUNT * AMPERE_PER_LED;
    if (peakAmpereDrawn > MAX_AMPERE) {
        float allowedRatio = float(MAX_AMPERE) / peakAmpereDrawn;
        // Each LED has a lightness of 3 (r+g+b)
        screen->renderer->setMaxLightness(float(LED_COUNT * 3) * allowedRatio);
    }
#endif

    auto motorForwardPin = new PWMPin(MOTOR_FORWARD_PIN, 0);
    auto motorBackwardPin = new PWMPin(MOTOR_BACKWARD_PIN, 1);
    motorForwardPin->setup(MOTOR_PWM_FREQUENCY, MOTOR_PWM_RESOLUTION);
    motorBackwardPin->setup(MOTOR_PWM_FREQUENCY, MOTOR_PWM_RESOLUTION);
    speedControl = new SpeedControl(
            motorForwardPin,
            motorBackwardPin,
            rotationSensor,
            MOTOR_ROTATIONS_PER_SECOND
    );

#if ROTATION_SENSOR_TYPE == ROTATION_SENSOR_TYPE_HALL_XTASK
    server->hallTimer = ((XTaskGPIOSwitch *) rotationSensor->visitor)->timer;
#endif

    pairPin = PAIR_PIN;
    pinMode(pairPin, INPUT_PULLUP);

    // Initialize Server
    artnetServer = new ArtnetServer(screen, speedControl);
    updater = new Updater();

    server = new HttpServer(this);
}

void App::run() {
    auto delayMicros = regularClock->sync();
    rotationSensor->update();

    screen->update(delayMicros);
    speedControl->update(delayMicros);

    if (delayMicros > timeUntilSlowUpdate) {
        timeUntilSlowUpdate = 1000 * 1000 * 2;

        if (digitalRead(pairPin) == LOW) {
            Network::pair();
        }

        bool isNetworkConnected = Network::checkStatus();

        if (speedControl->getDesiredSpeed() > 0 && !isNetworkConnected) {
            speedControl->stop();
            SerialLog.print("No WiFi! Stopping the motor, just in case.").ln();
        }
    }
    else
        timeUntilSlowUpdate -= delayMicros;

    updater->handle();
}
