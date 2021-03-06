//
// Created by Lukas Tenbrink on 21.01.20.
//

#include <screen/ConcentricCoordinates.h>
#include <Setup.h>
#include <screen/behavior/Behaviors.h>
#include "VideoInterface.h"
#include "DeviceID.h"
#include "Logger.h"

VideoInterface::VideoInterface(Screen *screen, ArtnetServer *artnetServer) : screen(screen),
                                                                             artnetServer(artnetServer) {
}

void VideoInterface::info(AsyncResponseStream *stream) {
    const std::vector<ArtnetEndpoint *> *endpoints = artnetServer->endpoints();
    auto behaviors = NativeBehaviors::list;

    const String deviceID = DeviceID::getString();

    DynamicJsonDocument doc(
            JSON_OBJECT_SIZE(4)
            // Device ID
            + deviceID.length()
            // Behaviors
            + JSON_ARRAY_SIZE(behaviors.size())
            // Cartesian Screen
            + JSON_OBJECT_SIZE(3)
            // Concentric Screen
            + JSON_OBJECT_SIZE(3)
            + JSON_ARRAY_SIZE(8)
            + JSON_ARRAY_SIZE(8)
            + 512 // Capacity for other strings
    );

    doc["id"] = deviceID.begin();

    {
        auto list = doc.createNestedArray("behaviors");
        for (auto name : behaviors.keys) {
            list.add(name.begin());
        }
    }

    // ==============================================
    // ================Cartesian=====================
    // ==============================================

    {
        auto object = doc.createNestedObject("cartesian");

        object["net"] = (*endpoints)[0]->net;
        object["width"] = screen->cartesianResolution;
        object["height"] = screen->cartesianResolution;
    }

    // ==============================================
    // ================Concentric=====================
    // ==============================================

    {
        auto object = doc.createNestedObject("concentric");

        object["net"] = (*endpoints)[1]->net;

        auto names = object.createNestedArray("names");
        auto equations = object.createNestedArray("equations");

        names.add("rings");
        equations.add(String(screen->concentricResolution->count).begin());

        names.add("|ring");
        equations.add("rings");

        names.add("radius");
        equations.add(ConcentricCoordinates::radiusExpression(screen->concentricResolution->count).begin());

        names.add("resolution");
        equations.add((String(CONCENTRIC_RESOLUTION_ADD) + "*ring+" + String(CONCENTRIC_RESOLUTION_MIN)).begin());

        names.add("|i");
        equations.add("resolution");

        names.add("theta");
        equations.add("(i/resolution)*2*pi");

        names.add("x");
        equations.add("sin(theta)*0.5*radius+0.5");
        names.add("y");
        equations.add("cos(theta)*0.5*radius+0.5");
    }

    serializeJsonPretty(doc, *stream);
}

