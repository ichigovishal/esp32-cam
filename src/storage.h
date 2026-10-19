#pragma once

#include <stdint.h>

#include <Arduino.h>
#include <vector>

bool storageMount();
bool storageReady();
const char* storageLastError();
uint64_t storageTotalBytes();
uint64_t storageUsedBytes();
uint64_t storageFreeBytes();
bool storageListSessions(std::vector<String>& names, const String& exclude = String());
uint32_t storageCountFrames(const String& sessionDir);
bool storageDeleteSession(const String& name);
bool storageEnforceSpace(const String& exclude = String());
bool storageValidSessionName(const String& s);
bool storageValidFileName(const String& s);
