# Test task MC-2236


This poject connects to the Wi-Fi and selected broker URI using `idf.py menuconfig` (using mqtt tcp transport) and as a demonstration subscribes/unsubscribes and send a message on certain topic to control led and reqest device status in 2Smart Cloud platfom.

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
mqtt://username:password@hostname:1883 

username - is a sha256 hash from user email in 2Smart Cloud mobile app;
password - is acces token from 2Smart Cloud;
hostname - cloud.2smart.com
1883 - mqtt tcp port
```
MQTT over TCP, port 1883, with username and password
* When using Make build system, set `Default serial port` under `Serial flasher config`.

Note: In the code, you must set defines according to your mqtt login and test device ID

![add_custom](./media/1.jpg)

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Project MQTT convention
First we must configure the device:  
  
```
esp_mqtt_client_publish(client, DEVICE "/$name", "MQTT-2SC", 0, 1, 0);
esp_mqtt_client_publish(client, DEVICE "/$fw/name", "draft", 0, 1, 0);
esp_mqtt_client_publish(client, DEVICE "/$fw/version", "2", 0, 1, 0);
esp_mqtt_client_publish(client, DEVICE "/$localip", "192.168.1.228", 0, 1, 0);
esp_mqtt_client_publish(client, DEVICE "/$mac", "3C:71:BF:FF:78:40", 0, 1, 0);
esp_mqtt_client_publish(client, DEVICE "/$implementation", "ESP32", 0, 1, 0);
esp_mqtt_client_publish(client, DEVICE "/$state", "ready", 0, 1, 0);
```

For the correct operation of the device, you need to configure the lines for yourself:

```
DEVICE "/$fw/name", "draft",
DEVICE "/$fw/version", "2", 
DEVICE "/$localip", "192.168.1.228",
DEVICE "/$mac", "3C:71:BF:FF:78:40", 
```

Config Wi-Fi init topics
Init nodes led and signal
Config led node
Add led-state propertti to led node
Config led-state propertti
Config signal node
Add get-signal and set-signal propertties to signal node
Config get-signal propertti
Config set-signal propertti

Subscribe to "/led/led-state/set" topic for control BLINK_GPIO led
Subscribe to "/signal/get-signal/set" topic to get signal val

Start heartbeat_timer - every 8 sec publish "ping" in "/$heartbeat"

## The result of the device

![add_custom](./media/2.jpg)
![add_custom](./media/3.jpg)
![add_custom](./media/4.jpg)

З.Ы. Почему то постоянно глючит джира :с

![add_custom](./media/5.jpg)