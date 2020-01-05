# EspMqttDHTdallasPIRbeep
*Code for an ESP8266 MQTT node, with temperature, humidity, PIR motion, and beeper.*
## Description
This code is written for use with an ESP8266, a DHTXX (humidity), a DS18B01 (temp), a logic output PIR sensor (motion), and a logic input beeper. Upon bootup it connects to the MQTT broker and periodically publishes temp and humidity based on continuously averaged sensor data.

It continuously reads the PIR sensor and publishes "1" to the "pir" topic upon motion. By default the code obeys a 5second no-motion timeout before it sends a "0". When the device receives an "ota" payload of "1", it triggers the OTA https server. When OTA is triggered, a message is published containing the URL for the update. OTA and reconnection routines obey timeouts for reliable operation. A "beeper" payload of "n" feeds n beep counts to the beeper. When a beep count is received, the beeper will emit n short beeps (beep count must be <10).

PIR on and off pubs are implemented with "manual qos1" for enhanced reliability; a subscription to "pir" is refreshed after pub, and if the broker data is does not match, the value is republished (>=1sec after pub by default). Mismatch repubs may occur up to 5 times before restart by default. The repetitive temp and humid values are simply published at qos 0.

This code is as non-blocking as possible for this author to implement, however no modifications were done to the included libraries. These libraries can (and often do) contain blocking code. :(
## Pubs & Subs
*Note: Pubs and subs topics use the format "MY_MQTT_UNIQUE_ID/topic". The "MY_MQTT_UNIQUE_ID" is hardcoded in the configuration file.*

S/P | Topic | Data
--- | ------ | ---------------
Sub | "ota" | Firmware Upload [0 = normal, 1 = upload]
Sub | "beeper" | N beeps (integer)
Sub | "pir" | Motion Status [0 = normal, 1 = motion]
Pub | "pir" | Motion Status [0 = normal, 1 = motion]
Pub | "temp" | Temperature 1 - Dallas [F]
Pub | "humid" | Relative Humidity [%]
## ESP8266 Pinout
ESP pin | Connect to
------ | -------------------
io13 | PIR output pin
io12 | DHT sensor data pin
io14 | Dallas temp sensor (non-parasitic power)
io16 | Beeper (w/ NPN... high = beep)
io0 | 4k7 High (Low = UART flash bootloader)
io2 | 4k7 High
io15 | 4k7 Low
EN | 4k7 High
RST | 4k7 High
## Install
Use platformio, arduino ide, or other to edit configuration.h, compile, and upload to your esp8266, nodemcu, wemos d1, etc board. To create self signed SSL certificates in Windows (for browser OTA), install openssl, 'cd' to some directory you'll remember, and use the one liner below at the command prompt.
```
openssl req -x509 -nodes -newkey rsa:2048 -keyout privkey.pem -sha256 -out fullchain.pem -days 4000 -subj "/C=US/ST=CA/L=province/O=Anytown/CN=CommonName"
```
## Notes
Since this code is intended to always be connected to Wifi, it is important to choose a PIR sensor that is not sensitive to RF interference. The author has had much success using "AM312" type PIR sensors. The common 501 sensor for the most part will not work with this project due to false triggering from RFI if it is located anywhere near the ESP board.

The code's configuration allows adjusting frequency of published temp and humid. The sensors are polled as fast as possible to increase the accuracy of the averaging filter used. The temp and humid buffers have overflow protection by resetting to 0. So for best performance, configure such that [uploadPeriod / dallasPeriod * someHotSummerTempValueInCelsius < MAXFLOAT]... and do the same for DHT polling as needed.
