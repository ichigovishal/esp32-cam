#include "storage.h"

#include <algorithm>
#include <Arduino.h>
#include <SD_MMC.h>
#include <dirent.h>
#include <driver/sdmmc_host.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"

static bool s_ready = false;
static char s_error[64] = "";

bool storageMount() {
  if (!SD_MMC.begin(SD_MOUNTPOINT, /*mode1bit=*/true, /*format_if_mount_failed=*/false,
                    SDMMC_FREQ_DEFAULT, /*maxOpenFiles=*/10)) {
    snprintf(s_error, sizeof(s_error), "SD_MMC mount failed");
    return false;
  }
  if (SD_MMC.cardType() == CARD_NONE) {
    snprintf(s_error, sizeof(s_error), "no SD card detected");
    SD_MMC.end();
    return false;
  }
  if (!SD_MMC.exists(PRINTS_DIR)) {
    SD_MMC.mkdir(PRINTS_DIR);
  }
  s_ready = true;
  s_error[0] = '\0';
  Serial.printf("[sd] mounted, total=%lluMB free=%lluMB\n",
                (unsigned long long)(storageTotalBytes() / (1024 * 1024)),
                (unsigned long long)(storageFreeBytes() / (1024 * 1024)));
  return true;
}

bool storageReady() {
  return s_ready;
}

const char* storageLastError() {
  return s_error;
}

uint64_t storageTotalBytes() {
  return s_ready ? SD_MMC.totalBytes() : 0;
}

uint64_t storageUsedBytes() {
  return s_ready ? SD_MMC.usedBytes() : 0;
}

uint64_t storageFreeBytes() {
  if (!s_ready) return 0;
  uint64_t total = SD_MMC.totalBytes();
  uint64_t used = SD_MMC.usedBytes();
  return total > used ? total - used : 0;
}

bool storageValidSessionName(const String& s) {
  if (s.isEmpty() || s.length() > 64) return false;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

bool storageValidFileName(const String& s) {
  if (s.isEmpty() || s.length() > 64 || s.indexOf("..") >= 0) return false;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.';
    if (!ok) return false;
  }
  return true;
}

static String sessionPath(const String& name) {
  return String(SD_MOUNTPOINT) + PRINTS_DIR + "/" + name;
}

bool storageListSessions(std::vector<String>& names, const String& exclude) {
  names.clear();
  if (!s_ready) return false;

  DIR* dir = opendir(sessionPath(String("")).c_str());
  if (dir == nullptr) return false;

  struct dirent* e;
  while ((e = readdir(dir)) != nullptr) {
    if (e->d_name[0] == '.') continue;
    String name(e->d_name);
    if (!storageValidSessionName(name)) continue;
    if (!exclude.isEmpty() && name == exclude) continue;
    names.push_back(name);
  }
  closedir(dir);

  std::sort(names.begin(), names.end());
  return true;
}

uint32_t storageCountFrames(const String& sessionDir) {
  if (!s_ready) return 0;
  DIR* dir = opendir(sessionPath(sessionDir).c_str());
  if (dir == nullptr) return 0;
  uint32_t n = 0;
  struct dirent* e;
  while ((e = readdir(dir)) != nullptr) {
    if (strncmp(e->d_name, "frame_", 6) == 0) n++;
  }
  closedir(dir);
  return n;
}

static bool removeTree(const String& path) {
  DIR* dir = opendir(path.c_str());
  if (dir == nullptr) {
    return unlink(path.c_str()) == 0;
  }
  struct dirent* e;
  while ((e = readdir(dir)) != nullptr) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
    String child = path + "/" + e->d_name;
    removeTree(child);
  }
  closedir(dir);
  return rmdir(path.c_str()) == 0;
}

bool storageDeleteSession(const String& name) {
  if (!s_ready || !storageValidSessionName(name)) return false;
  bool ok = removeTree(sessionPath(name));
  if (ok) {
    Serial.printf("[sd] deleted old session %s\n", name.c_str());
  } else {
    Serial.printf("[sd] failed to delete %s\n", name.c_str());
  }
  return ok;
}

bool storageEnforceSpace(const String& exclude) {
  if (!s_ready) return true;
  uint64_t total = SD_MMC.totalBytes();
  if (total == 0) return true;
  uint64_t limit = (uint64_t)((double)total * (double)STORAGE_MAX_USED);

  int guard = 0;
  while (SD_MMC.usedBytes() >= limit && guard++ < 64) {
    std::vector<String> sessions;
    storageListSessions(sessions, exclude);
    if (sessions.empty()) break;
    if (!storageDeleteSession(sessions.front())) break;
  }
  return SD_MMC.usedBytes() < limit;
}
