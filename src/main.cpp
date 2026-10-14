#include <Arduino.h>
#ifndef ZIGBEE_MODE_ED
#error "Build with ZIGBEE_MODE_ED"
#endif

#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEUtils.h>
#include <Zigbee.h>

#include "status_screen.h"

namespace {

constexpr uint16_t kService = 0xFFF0;
constexpr uint16_t kWriteChar = 0xFFF3;
constexpr uint16_t kMiredCold = 153;  // 6500 K
constexpr uint16_t kMiredWarm = 333;  // 3000 K
constexpr uint8_t kEndpoint = 10;
constexpr char kLampName[] = "THUNDEROBOT L2";

ZigbeeColorDimmableLight zbLight(kEndpoint);

BLEClient* ble = nullptr;
BLERemoteCharacteristic* lampChar = nullptr;
bool zigbeeUp = false;

bool haveCmd = false;
volatile bool cmdDirty = false;
bool cmdOn = false;
uint8_t cmdLevel = 254;
uint16_t cmdMireds = kMiredWarm;

uint16_t kelvinToMireds(uint16_t kelvin) { return 1000000 / kelvin; }

void sendLamp() {
  if (lampChar == nullptr || ble == nullptr || !ble->isConnected()) {
    static uint32_t lastMiss = 0;
    if (millis() - lastMiss > 5000) {
      lastMiss = millis();
      log_w("lamp not connected, command waiting");
    }
    return;
  }
  uint16_t mireds = cmdMireds;
  if (mireds < kMiredCold) {
    mireds = kMiredCold;
  }
  if (mireds > kMiredWarm) {
    mireds = kMiredWarm;
  }
  uint8_t level = (uint16_t)cmdLevel * 100 / 255;
  uint8_t warm = 0;
  uint8_t cool = 0;
  if (cmdOn && level > 0) {
    float warmMix = (float)(mireds - kMiredCold) / (float)(kMiredWarm - kMiredCold);
    warm = (uint8_t)(level * warmMix + 0.5f);
    if (warm > level) {
      warm = level;
    }
    cool = level - warm;
  }
  uint8_t frame[9] = {0x7E, 0x06, 0x05, 0x02, cool, warm, 0xFF, 0x10, 0xEF};
  lampChar->writeValue(frame, sizeof(frame), false);
  log_i("lamp cool=%u warm=%u on=%d level=%u mireds=%u", cool, warm, cmdOn, cmdLevel, mireds);
  cmdDirty = false;
}

void onTemp(bool state, uint8_t level, uint16_t mireds) {
  cmdOn = state;
  cmdLevel = level;
  cmdMireds = mireds;
  haveCmd = true;
  cmdDirty = true;
  log_i("zigbee on=%d level=%u mireds=%u", state, level, mireds);
}

void onRgb(bool state, uint8_t, uint8_t, uint8_t, uint8_t level) {
  onTemp(state, level, cmdMireds);
}

bool connectDevice(BLEAdvertisedDevice& device) {
  if (ble == nullptr) {
    ble = BLEDevice::createClient();
  }
  if (ble->isConnected()) {
    ble->disconnect();
  }
  Serial.printf("连接 %s %s\n", device.getName().c_str(), device.getAddress().toString().c_str());
  if (!ble->connect(&device)) {
    Serial.println("蓝牙连接失败");
    return false;
  }
  BLERemoteService* service = ble->getService(BLEUUID(kService));
  if (service == nullptr) {
    Serial.println("没有 FFF0 服务");
    ble->disconnect();
    return false;
  }
  lampChar = service->getCharacteristic(BLEUUID(kWriteChar));
  if (lampChar == nullptr) {
    Serial.println("没有 FFF3 特征");
    ble->disconnect();
    return false;
  }
  Serial.println("挂灯已连接");
  if (haveCmd) {
    cmdDirty = true;
  }
  return true;
}

bool scanOnce() {
  BLEScan* scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(160);
  scan->setWindow(80);
  BLEScanResults* results = scan->start(2, false);
  bool found = false;
  if (results != nullptr) {
    for (int i = 0; i < results->getCount(); i++) {
      BLEAdvertisedDevice device = results->getDevice(i);
      if (!device.haveName() || device.getName() != kLampName) {
        continue;
      }
      found = connectDevice(device);
      break;
    }
  }
  scan->clearResults();
  return found;
}

void pollResetButton() {
  if (digitalRead(BOOT_PIN) != LOW) {
    return;
  }
  uint32_t start = millis();
  while (digitalRead(BOOT_PIN) == LOW) {
    delay(50);
    if (millis() - start > 3000) {
      Serial.println("Zigbee 恢复出厂并重启");
      delay(200);
      Zigbee.factoryReset();
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(BOOT_PIN, INPUT_PULLUP);
  screenBegin();

  zbLight.setLightColorCapabilities(ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP);
  zbLight.setLightColorTemperatureRange(kelvinToMireds(6500), kelvinToMireds(3000));
  zbLight.onLightChangeTemp(onTemp);
  zbLight.onLightChangeRgb(onRgb);
  zbLight.setManufacturerAndModel("Thunderobot", "L2");
  Zigbee.addEndpoint(&zbLight);

  Serial.println("启动 Zigbee 终端设备");
  if (!Zigbee.begin()) {
    Serial.println("Zigbee 启动失败，重启");
    delay(1000);
    ESP.restart();
  }
  uint32_t waitStart = millis();
  while (!Zigbee.connected() && millis() - waitStart < 20000) {
    Serial.print(".");
    delay(250);
  }
  Serial.println();
  zigbeeUp = Zigbee.connected();
  Serial.println(zigbeeUp ? "已加入 Zigbee 网络" : "还没加入，协调器打开配网后会自动加入");
  screenShow(false, zigbeeUp);
  zbLight.setLightColorTemperature(kMiredWarm);

  BLEDevice::init("");
}

void loop() {
  pollResetButton();

  if (ble != nullptr && !ble->isConnected()) {
    lampChar = nullptr;
  }

  static uint32_t nextScan = 0;
  bool linked = ble != nullptr && ble->isConnected() && lampChar != nullptr;
  if (!linked && millis() >= nextScan) {
    nextScan = millis() + 8000;
    scanOnce();
  }

  if (cmdDirty) {
    sendLamp();
  }
  screenShow(linked, Zigbee.connected());
  delay(50);
}
