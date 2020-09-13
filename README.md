
# esp32_led_panel

Welcome to the esp32_led_panel project!  This quick personal project was 
created for my family's "Maker Weekend".  The project uses the ESP32-DevKit-C
along with the 32x16 panel extracted from the Pixel Purse Mc2.


To build this project:

* Install the [Arduino IDE](https://www.arduino.cc/)
* Install the ESP32 boards - [Instructions](https://github.com/espressif/arduino-esp32)
* Install the [SmartMatrix](https://github.com/pixelmatix/SmartMatrix/tree/teensy4) teensy branch to Documents/Arduino.
* Apply the SmartMatrix changes from [smartmatrix.diff](smartmatrix.diff).
* Open esp32_led_panel.ino.
* Select the ESP32-DevKit target.
* Select your ESP32 com port.
* Set your WiFi credentials in wifi_credentials.h
* Upload!


## License

All pyjoulescope code is released under the permissive Apache 2.0 license.
See the [License File](LICENSE.txt) for details.
