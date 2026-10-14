# Thunderobot L2

用一块带屏幕的 ESP32-C6，把雷神屏幕挂灯接进 Zigbee 网络。板子用蓝牙连灯，自己作为 Zigbee 终端设备加入协调器。Home Assistant 里可以开关、调亮度、调色温。

屏幕上显示两路连接：蓝牙是否连上挂灯，Zigbee 是否加入网络。连上是绿色「已连接」，否则是红色「未连接」。

这次只控制主灯。背面 RGB 没有做到 Home Assistant 里。

## 硬件

| 角色 | 设备 |
| --- | --- |
| 桥 | 微雪 ESP32-C6-LCD-1.9（ESP32-C6FH8，8MB 闪存，1.9 寸 ST7789，170×320） |
| 灯 | THUNDEROBOT L2，蓝牙名 `THUNDEROBOT L2` |
| 协调器 | Zigbee2MQTT。Home Assistant 通过 MQTT 发现设备 |

屏幕接线按这块板的出厂定义：SCK `GPIO5`，MOSI `GPIO4`，DC `GPIO6`，CS `GPIO7`，RST `GPIO14`，背光 `GPIO15`（低电平点亮）。

板子只有一颗射频。蓝牙扫描和 Zigbee 会抢无线电，所以只在没连上灯时短时间扫描，连上后停止扫描。不要把它配成 Zigbee 路由器。

## 主灯协议

服务 `0xFFF0`，写特征 `0xFFF3`（Write Without Response）。

```text
7E 06 05 02 CC WW FF 10 EF
```

`CC` 是冷白 PWM，`WW` 是暖白 PWM，各自范围 `0x00`–`0x64`，不必加起来等于 100。两个都是 `0x00` 就是关灯。固件把 Zigbee 的色温（153–333 mired，约 6500K–3000K）和亮度（0–254）拆成这两路。

## 编译和烧录

需要 PlatformIO Core 6.2 或更新。板子用 USB 串口，分区表是 Zigbee 终端设备那份 `zigbee.csv`。

```powershell
python -m platformio run
python -m esptool --chip esp32c6 write-flash --flash-size 8MB 0x10000 .pio\build\esp32-c6-devkitc-1\firmware.bin
```

只写 `0x10000` 上的应用，不会清掉已经保存的 Zigbee 网络。第一次烧录如果分区表还没写过，用 `python -m platformio run -t upload`。Windows 控制台如果因为进度条字符中断上传，改用上面的 esptool 命令。

长按 BOOT 三秒会清除 Zigbee 网络数据并重启，用来重新配网。

## 接入 Zigbee2MQTT

1. 协调器打开配网。板子入网后型号是 Thunderobot `L2`。
2. 把 [`zigbee2mqtt/thunderobot-l2.mjs`](zigbee2mqtt/thunderobot-l2.mjs) 存进 Zigbee2MQTT 的外部转换器。前端或 MQTT 都可以：

```text
主题 zigbee2mqtt/bridge/request/converter/save
{"name":"thunderobot-l2.mjs","code":"<文件内容>"}
```

3. 重新访谈这台设备。Home Assistant 里会出现一盏色温灯，实体卡片里有亮度条和色温条。设备页的控制区只放开关，点开灯泡那一行才能看到滑条。

如果设备曾经以别的固件入过网，协调器里会留着旧的端点。先从 Zigbee2MQTT 删除它，再长按 BOOT 三秒，打开配网后重新加入。
