#include <Arduino.h>
#include <LittleFS.h>
#include <ZeroAllocJSON.h>
// Fixed caller-owned buffer; parser never owns or allocates storage.
static char buffer[1024];
static const char initial[] = R"({"name":"Device","enabled":true,"threshold":12.75})";
bool saveConfig(const char* bytes, size_t length) {
  if (ZeroAllocJSON(bytes,length).validate()!=ZeroAllocJSON::Status::Ok) return false;
  File file=LittleFS.open("/config.json","w");
  if (!file) return false;
  bool ok=file.write(reinterpret_cast<const uint8_t*>(bytes),length)==length;
  file.close();return ok;
}
void setup() {
  Serial.begin(115200);
  // Do not auto-format a filesystem that fails to mount.
  if (!LittleFS.begin(false)) {Serial.println("LittleFS mount failed; provision filesystem first");return;}
  if (!LittleFS.exists("/config.json") && !saveConfig(initial,sizeof(initial)-1)) {
    Serial.println("Save failed");return;
  }
  File file=LittleFS.open("/config.json","r");
  if (!file) {Serial.println("Open failed");return;}
  size_t length=file.size();
  if (length>sizeof(buffer)) {file.close();Serial.println("Config too large");return;}
  size_t received=file.readBytes(buffer,length);file.close();
  if (received!=length) {Serial.println("Short read");return;}
  ZeroAllocJSON cfg(buffer,length); // No terminator required.
  auto enabled=cfg.get<bool>("enabled");
  auto name=cfg.getString("name");
  if (!enabled || !name) {Serial.println("Invalid or incomplete config");return;}
  cfg.showJSON(Serial);Serial.println();
  Serial.write(reinterpret_cast<const uint8_t*>(name.value.ptr),name.value.length);
  Serial.println(enabled.value ? " enabled" : " disabled");
}
void loop() {}
