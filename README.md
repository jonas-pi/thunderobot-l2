# Thunderobot L2

用一块 ESP32-C6，把雷神屏幕挂灯接进 Zigbee 网络。板子用蓝牙连灯，自己作为 Zigbee 终端设备加入协调器。Home Assistant 里可以开关、调亮度、调色温。

屏幕暂未点亮（暂无需求）。固件里留了在 1.9 寸屏上显示蓝牙和 Zigbee 连接状态的代码，目前不需要点亮。

这次只控制主灯。背面 RGB 的帧写在下面，没有做到 Home Assistant 里。

## 硬件

| 角色 | 设备 |
| --- | --- |
| 桥 | 微雪 ESP32-C6-LCD-1.9（ESP32-C6FH8，8MB 闪存，1.9 寸 ST7789，170×320） |
| 灯 | THUNDEROBOT L2，蓝牙名 `THUNDEROBOT L2` |
| 协调器 | Zigbee2MQTT。Home Assistant 通过 MQTT 发现设备 |

屏幕暂未点亮，下面的接线只是按这块板的出厂定义记下来，供以后要用时参考：SCK `GPIO5`，MOSI `GPIO4`，DC `GPIO6`，CS `GPIO7`，RST `GPIO14`，背光 `GPIO15`（低电平点亮）。

板子只有一颗射频。蓝牙扫描和 Zigbee 会抢无线电，所以只在没连上灯时短时间扫描，连上后停止扫描。不要把它配成 Zigbee 路由器。

## 蓝牙帧

服务 `0xFFF0`，写特征 `0xFFF3`（Write Without Response）。下面除了单独标明的，都在灯上对过。

### 主灯

```text
7E 06 05 02 CC WW FF 10 EF
```

`CC` 是冷白 PWM，`WW` 是暖白 PWM，各自范围 `0x00`–`0x64`，不必加起来等于 100。

| 字节 | 效果 |
| --- | --- |
| `CC=00 WW=64` | 暖白，最亮 |
| `CC=64 WW=00` | 冷白，最亮 |
| `CC=14 WW=00` | 冷白，变暗 |
| `CC=00 WW=00` | 关灯 |

亮度靠降低 `CC`/`WW`，不要用后面那条 RGB 亮度帧，那条调不暗主灯。固件把 Zigbee 色温（153–333 mired，约 6500K–3000K）和亮度（0–254）拆成这两路。

### RGB

先发开启，颜色帧才会生效。开启也会把灯里存着的灯效接着播出来。

| 作用 | 帧 |
| --- | --- |
| 开启 | `7E 07 04 FF 00 01 02 01 EF` |
| 关闭 | `7E 07 04 00 00 00 02 01 EF` |
| 颜色 | `7E 07 05 03 RR GG BB 10 EF` |
| 灯效 | `7E 07 03 XX 03 FF FF 00 EF` |
| 灯效速度 | `7E 07 02 SS FF FF FF 00 EF` |
| 亮度 | `7E 04 01 XX 01 FF 02 01 EF` |

`RR GG BB` 和 `XX`、`SS` 都是单字节。颜色按红、橙、黄、绿、青、蓝、紫对过。亮度和速度的范围是 `0x00`–`0x64`。关闭帧是和一组停灯效的帧一起发出去的，没有单独再试一次。

`XX` 共 22 档，名称来自 ELK-BLEDOB / LotusLamp X，实灯表现和这张表一致：

| XX | 灯效 | XX | 灯效 |
| --- | --- | --- | --- |
| `87` | RGB 跳变 | `92` | 红绿渐变 |
| `88` | 七色跳变 | `93` | 红蓝渐变 |
| `89` | RGB 渐变 | `94` | 绿蓝渐变 |
| `8A` | 七色渐变 | `95` | 七色频闪 |
| `8B` | 红渐变 | `96` | 红频闪 |
| `8C` | 绿渐变 | `97` | 绿频闪 |
| `8D` | 蓝渐变 | `98` | 蓝频闪 |
| `8E` | 黄渐变 | `99` | 黄频闪 |
| `8F` | 青渐变 | `9A` | 青频闪 |
| `90` | 紫渐变 | `9B` | 紫频闪 |
| `91` | 白渐变 | `9C` | 白频闪 |

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
