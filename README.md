# Test task for the position of embedded developer in WebbyLab

This poject connects to the Wi-Fi and selected broker URI using `idf.py menuconfig` (using mqtt tcp transport) and as a demonstration subscribes/unsubscribes and send a message on certain topic to control led and reqest device status.

It uses ESP-MQTT library which implements mqtt client to connect to mqtt broker.

It can also display device and connection information on the OLED screen.

## How to use 

### Hardware Required

This example can be executed on any ESP32 board, the only required interface is WiFi and connection to internet.

You also need to have an SSD1306 128x64 i2c screen to use the terminal function.
```
OLED sda ---> GPIO 21 esp32
OLED scl ---> GPIO 22 esp32
```

### Configure the project

* Open the project configuration menu (`idf.py menuconfig`)
* Set GPIO pins for you I2C bus. 
* Configure Wi-Fi or Ethernet under "Example Connection Configuration" menu. See "Establishing Wi-Fi or Ethernet Connection" section in [examples/protocols/README.md](../../README.md) for more details.
* Broker URI used in the following format
```
mqtt://username:password@hostname:1884 

exemple:
mqtt://felmar:2173@192.168.1.227:1883
```
MQTT over TCP, port 1884, with username and password
* When using Make build system, set `Default serial port` under `Serial flasher config`.

Note: all need components contain in project dir

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Project MQTT convention
We have three topics for device management:  
  
/root/monitor  
/root/control/  
/root/control/led  

He is subscribed to two topics and listens to certain commands:  
`/root/control/`  
`/root/control/led`  

On the third topic, he publishes answers:  
`/root/monitor`  

Commands accepted by the /control branch:  
`getinfo` - is responsible for requesting information about the device, namely the Wi-Fi signal level;  
Commands accepted by the /control/led branch:  
`on` - turn on the LED;  
`off` - turn off the LED;  

