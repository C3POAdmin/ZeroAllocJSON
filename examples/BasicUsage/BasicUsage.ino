#include <Arduino.h>
#include <ZeroAllocJSON.h>
const char config[] = R"({"name":"Front Gate","starttime":1790000000,"enabled":true,"threshold":12.75})";
ZeroAllocJSON cfg(config, sizeof(config)-1);
void setup() {
  Serial.begin(115200);
  if (cfg.validate() != ZeroAllocJSON::Status::Ok) { Serial.println("Invalid config"); return; }
  auto start = cfg.get<uint64_t>("starttime");
  auto enabled = cfg.get<bool>("enabled");
  auto threshold = cfg.get<float>("threshold");
  auto name = cfg.getString("name");
  if (!start || !enabled || !threshold || !name) { Serial.println("Required setting invalid"); return; }
  Serial.write(reinterpret_cast<const uint8_t*>(name.value.ptr), name.value.length);
  Serial.println();
  cfg.showJSON(Serial); Serial.println();
  cfg.showVariable("starttime", Serial); Serial.println();
#ifdef ESP32
  // Diagnostic only: other ESP32 tasks can change free heap independently.
  size_t before = ESP.getFreeHeap();
  volatile bool observed = false;
  for (int i=0; i<100; ++i) observed = cfg.get<bool>("enabled").value;
  size_t after = ESP.getFreeHeap();
  Serial.printf("Heap before=%u after=%u enabled=%d\n", unsigned(before), unsigned(after), observed);
#endif
}
void loop() {}
