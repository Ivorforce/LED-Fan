//
// Created by Lukas Tenbrink on 21.01.20.
//

#include <SPIFFS.h>
#include "TextFiles.h"
#include "Logger.h"

bool TextFiles::has(String path) {
    return SPIFFS.exists(path);
}

bool TextFiles::write(String path, String s) {
    File file = SPIFFS.open(path, FILE_WRITE);

    if(!file){
        SerialLog.print("There was an error opening the file: ");
        SerialLog.print(path).ln();
        return false;
    }

    file.print(s);
    file.close();

    return true;
}

String TextFiles::read(String path) {
    File file = SPIFFS.open(path, FILE_READ);

    if(!file){
        return "";
    }

    String string = file.readString();
    file.close();
    return string;
}

bool TextFiles::hasConf(String path) {
    return has(CFG_PATH + path);
}

bool TextFiles::writeConf(String path, String s) {
    return write(CFG_PATH + path, s);
}

String TextFiles::readConf(String path) {
    return read(CFG_PATH + path);
}
