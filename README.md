# xpa125b_controller
Xiegu XPA125B Yaesu/Serial/REST/Web/MQTT/rigctl control interface.

The Yaesu reference is to the standard voltages Yaesu use for band selection.

Can be used with any radio which is supported by rigctl or you could write your own using whichever API you prefer.

The top of the file contains config options which need set first:

ssid - your WiFi SSID
password - your WiFi password

If you want to enable MQTT set mqtt_enabled to true and set the following:

mqttserver
mqttuser
mqttpass

Designed for a D1 Mini.
