# ESP RainMaker

> This branch will not support esp-idf v4.x releases anymore as they have reached their end of life. Please check out `idf_4_x_compat` branch if your project requires it.

> Note: For major changes, please refer [this file](CHANGES.md).

## Introduction

ESP RainMaker is an end-to-end solution offered by Espressif to enable remote control and monitoring for products based on ESP32 series of SoCs (e.g., ESP32, ESP32-S2, ESP32-S3, ESP32-C2, ESP32-C3, ESP32-C6, ESP32-H2, ESP32-C5 (preview), etc.) without any configuration required in the Cloud. <br>

The primary components of this solution are:

- Claiming Service (to get the Cloud connectivity credentials)
- RainMaker Agent (i.e. this repo, to develop the firmware)
- RainMaker Cloud (backend, offering remote connectivity)
- RainMaker Phone App/CLI (Client utilities for remote access)


The key features of ESP RainMaker are:

1. Ability to define own devices and parameters, of any type, in the firmware.
2. Zero configuration required on the Cloud.
3. Phone apps that dynamically render the UI as per the device information.

## Get ESP RainMaker

Please clone this repository using the below command:

```
git clone --recursive https://github.com/espressif/esp-rainmaker.git
```

> Note the --recursive option. This is required to pull in the various dependencies into esp-rainmaker. In case you have already cloned the repository without this option, execute this to pull in the submodules: `git submodule update --init --recursive`

Please check the ESP RainMaker documentation [here](https://rainmaker.espressif.com/docs/get-started.html) to get started.

Each example has its own README with additional information about using the example.

## Supported ESP-IDF versions

ESP RainMaker can work with ESP IDF 4.1 and above.

## Phone Apps

### Android

- [Google PlayStore](https://play.google.com/store/apps/details?id=com.espressif.rainmaker)
- [Direct APK](https://github.com/espressif/esp-rainmaker/wiki)
- [Source Code](https://github.com/espressif/esp-rainmaker-android)

### iOS
- [Apple App Store](https://apps.apple.com/app/esp-rainmaker/id1497491540)
- [Source Code](https://github.com/espressif/esp-rainmaker-ios)

## Discussions

[ESP32 Forum](https://www.esp32.com/viewforum.php?f=41)

[![Gitter Chat](https://badges.gitter.im/esp-rainmaker/community.svg)](https://gitter.im/esp-rainmaker/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

---

<a href="https://espressif.github.io/esp-launchpad/?solution=rainmaker">
    <img alt="Try it with ESP Launchpad" src="https://espressif.github.io/esp-launchpad/assets/try_with_launchpad.png" width="250" height="70">
</a>
