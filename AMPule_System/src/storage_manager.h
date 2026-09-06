#ifndef AMPULE_STORAGE_MANAGER_H
#define AMPULE_STORAGE_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Thin wrapper around LittleFS providing generic JSON read/write helpers
// plus first-boot seeding of demo data. All other managers persist through
// this module rather than touching the filesystem directly.
namespace StorageManager {

    bool begin();

    // Ensures every expected JSON file exists, writing DEMO seed data the
    // first time the firmware runs on a blank filesystem.
    void ensureSeedData();

    // Reads the JSON file at `path` into `doc`. Returns false (and leaves
    // `doc` empty) if the file is missing, unreadable or not valid JSON.
    bool readJson(const char *path, JsonDocument &doc);

    // Serializes `doc` to the JSON file at `path`, writing to a temporary
    // file first and renaming it into place to avoid corrupting the
    // existing file if power is lost mid-write.
    bool writeJson(const char *path, JsonDocument &doc);

    bool exists(const char *path);
}

#endif // AMPULE_STORAGE_MANAGER_H
