# Oximeter F7 BLE Bridge

connects an ESP32 to a BLE F7 Oximeter and reports SpO2, PI and PR on serial console, on a web page and to an Influx database.
This way you dont need to stay close to the device all the time to monitor the health data.

![F7 and Atom](pics/f7-oximeter-and-m5-atom-lite.png)  
F7 oximeter and M5 atom lite at work

## Hardware
No tinkering required at all. The M5 Atom lite ESP32 pico board has all thats needed on board: WLAN, BLE, a button and a Neopixel.
If you use another ESP32, you will need to adjust the neopixel and button pins in the source (if needed).
The oximeter is the one that runs with the Andoid app Sapiential SpO2 and the iOS app Wearfit BO. It identifies itself via bluetooth as F7.

## Software
* All network data (WLAN, InfluxDB, optional Syslog) must be maintained in source - no online configuration yet.
* Meant to build and upload with PlatformIO: 
  pio run -t upload --upload-port /dev/ttyUSB0

After first upload of firmware via USB port serial OTA update of the firmware is possible via http://oximeter-??????/update

![F7 web page](pics/f7-oximeter-bridge-web-page.jpg)  
Example web page of the ESP32

## Features
* Get oximeter data on serial port along with some runtime information
* Use ESP32s own local webpage http://oximeter-??????/ to see oximeter data or to OTA upload a firmware update
* Pull the data as json string from http://oximeter-??????/json, e.g. to feed it into domoticz or another home automation system
* Scans for BLE advertisement packages an F7 oximeter sends at startup and connects to it
* Regularly polls data from the oximeter and forwards the data to an influx db 'f7'  
  - Just create the database on the server configured in source, e.g. with the CLI command 'influx' enter 'create databse f7'
  - The measurement table 'health' is automatically created and can be used to e.g. feed a grafana dashboard
* Publishes the status via the Neopixel colors:  
  - Single white blink at startup
  - Cyan blinking if not connected to WLAN
  - Violet blinking if posting to Influx database fails
  - Off while scanning for oximeter advertisements
  - Solid green to red if connected to the oximeter. Green means healthy normal data and red means very unhealthy data
  - The solid color is overlayed by blinking light green to light red, depending on a not optimal to very bad perfusion index
* Neopixel status display can be toggled with the button

![F7 on grafana dashboard](pics/f7-oximeter-bridge-grafana.jpg)  
Example grafana dashboard using the influx data

## TODO
* How to reconnect to an oximeter without the need to restart it  
  - Android app of F7 can reconnect a lost BLE connection (closed app) without restarting the oximeter.
* Use WlanManager to setup connection
* Make all parameters online configurable

## Links
I am not related to these shops, it is just to show what I use and know it is working. 
Both, M5 Atom Lite and Oximeter F7, cost only about 10€ each.
* [M5Stack Atom Lite](https://eckstein-shop.de/M5Stack-ATOM-Lite-ESP32-Development-Kit-ESP32-PICO-Chip-mit-WiFi-und-Bluetooth)
* [Option 1: SP001 - Fingertip Pulse Oximeter Bluetooth 4.0 Blood Oxygen Saturation Saturation SPO2 Heart Rate](https://a.aliexpress.com/_uXQZXl)
* [Option 2: SPO2 PI PR bluetooth - Bluetooth Finger Pulse Oximeter Sleep monitoring data analysis Fingertip OLED](https://a.aliexpress.com/_vWpxsn)
