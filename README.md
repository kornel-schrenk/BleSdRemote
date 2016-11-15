# BleSdRemote

## Description

Nowadays some high end phones do not have an extra SD card slot, therefore its users are limited to the  internal storage space of the device. If you need more, you have to spend more money or use the manufacturer's proprietary cloud solution as an alternative extension.

Is there a way to solve this problem? I hope so! Only some *extra* storage space has to be *attached* to these mobile phones.

However this attachment can be problematic! Fortunately [Bluetooth Low Energy](https://en.wikipedia.org/wiki/Bluetooth_low_energy) (BLE) is supported by today's all major mobile phone operating systems: iOS, Android and Windows Phone. The idea is to add extra storage space to these mobile phones through BLE, which *demonstrates* that this extension is **possible**!

The solution consist of two parts:
1. An external SD card extension, which is driven by an [Arduino](http://www.arduino.org/) microcontroller.
2. An [Android](https://www.android.com/) application, which provides access to the remote SD card content.

The repository contains the [Arduino](http://www.arduino.org/) code. The related [Android](https://www.android.com/) code can be found in the [BleSdRemoteDroid](https://github.com/kornel-schrenk/BleSdRemoteDroid) GitHub repository.

## Breadboard view

![alt text](../master/BleSdRemote_bb.png "Fritzing breadboard view")

## Repository Owner 

* [Kornel Schrenk](http://www.schrenk.hu/about/)

## License

This software is available under [MIT](../master/LICENSE) license.
