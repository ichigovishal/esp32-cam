#pragma once

#include <Arduino.h>
#include <functional>

void wifiConnect(uint32_t timeoutMs);
bool wifiReady();
void webBegin(std::function<String()> getStatusJson,
              std::function<bool()> onStart,
              std::function<bool()> onStop,
              std::function<void()> onReboot);
void webLoop();
