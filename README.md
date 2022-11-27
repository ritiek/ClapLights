# ClapLights

Helps me turn on or off my hallway lights by clapping my hands together (yelling out my lungs works too)!

You'll need a few things to use this code as is (otherwise you may have to modify the code a bit to
adjust accordingly):
1. ESP32
2. Sound detection module
3. Touch sensor
4. Few jumpers to make the connections
5. Bulb flashed with Tasmota
6. Accessible WiFi SSID

I've also connected a capacitive touch sensor so if I tap on it, it'll lock the bulb state so it no longer
changes its state based on sound intensity (useful if sound detection module is picking up false-positives
in case the hallway gets noisier at times). Tapping the touch sensor again makes the bulb state affectable
by sound again.

You may have to experiment with loosening or tightening the screw present on your sound detection module to
calibrate it as per your environment noise.

<img src="https://i.imgur.com/x7qXT3P.jpg" width="300">

It optionally can also edit a message on a Discord server (every 30 secs by default) using the webhook API to
let me know if there's a powercut or if something's up back at home.


## Configuration

Modify `ClapLights.ino` to update sensor pins, WiFi SSID, API endpoint, Discord or NTP server configuration.
You can also disable the Discord integration here.


## Flashing

You can use the Arduino IDE to compile and upload the firmware to an ESP32.

Otherwise you can also use [arduino-cli](https://github.com/arduino/arduino-cli) like so:

```bash
$ arduino-cli compile -b esp32:esp32:nodemcu-32s
$ arduino-cli upload -b esp32:esp32:nodemcu-32s -p /dev/ttyUSB0
```
Monitor serial to make sure everything's working fine:
```bash
$ arduino-cli monitor -c baudrate=115200 -p /dev/ttyUSB0
```


## License

MIT
