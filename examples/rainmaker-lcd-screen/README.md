# Wi-Fi controlled LCD screen using ESP RainMaker

This is a Wi-Fi controlled LCD Screen. Put it up on your desk, door, shop, grocery aisles or conference rooms. Easy to change using an iOS/Android phone app.

<img src="https://raw.githubusercontent.com/wiki/kedars/rainmaker-lcd-screen/images/LCD_screen.jpeg" width="300"/>

## Requirements
* Hardware
  * ESP32 Wrover Kit
* Software
  * ESP-RainMaker: Set up your development host as mentioned here: https://rainmaker.espressif.com/docs/get-started.html

## Build and Flash
```bash
$ git clone https://github.com/kedars/rainmaker-lcd-screen
$ cd rainmaker-lcd-screen
$ RAINMAKER_PATH=/path/to/where/esp-rainmaker/exists idf.py build flash monitor
```

## Using the screen
* Download the ESP RainMaker [iOS](https://apps.apple.com/app/esp-rainmaker/id1497491540)/[Android](https://play.google.com/store/apps/details?id=com.espressif.rainmaker) phone app
* Boot up the ESP32 WROVER Kit
* Configure the ESP32 WROVER Kit using the phone app
* Enter the status you want to display by hitting the _Edit_ button

<img src="https://raw.githubusercontent.com/wiki/kedars/rainmaker-lcd-screen/images/LCD_app.jpeg" width="300"/>


