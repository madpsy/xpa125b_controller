# XPA125B Amplifier Network Controller
Xiegu XPA125B (https://xiegu.eu/product/xpa125-100w-solid-state-linear-amplifier/) network control interface designed for a D1 Mini (https://www.wemos.cc/en/latest/d1/d1_mini.html). This allows you to use virtually any rig, including SDRs, with this amplifier. Both PTT and automatic band selection is supported. Although WiFi is supported the controller can also operate without it in `analogue` mode. This would enable you to use a Yaesu rig and have the benefit of automatic band selection. The primary intended use of the controller is to hook directly into `rigctld` https://hamlib.github.io/ meaning any rig which it supports will work (albeit requiring the use of WiFi). The latency is ~100ms which is perfectly adiquate for almost all digital work aswel as phone.

Supported protocols:

+ Analogue (Yaesu standard band voltages)
+ Serial
+ Web Interface
+ REST
+ MQTT
+ Rigctld

The top of the file contains config options which need set first:

+ ssid - your WiFi SSID
+ password - your WiFi password

If you want to enable MQTT set the following:

+ mqtt_enabled = true
+ mqttserver = <host>  (MQTT server hostname or IP address)
+ mqttuser = <username>  (MQTT username)
+ mqttpass = <password>  (MQTT password)

To make rigctl default on boot first ensure WiFi credentials are filled in and also set:

+  mode = rigctl
+  rigctl_default_enable = true
+  rigctl_default_address = <IP address> (rigctl IP address)
+  rigctl_default_port = <port> (rigctl port)
  
As an example, if you want to use SDR Console as the rig, first enable CAT control and attach it to one end of a pair of virtual com cables, then run rigctld:

`rigctld.exe -r COM18 -m 2014 -s 57600 -t 51111`
  
 In the above we are using `COM18` as the other end of the virtual com cable. `2014` is the rig ID for a Kenwood TS-2000 which SDR Console emulates. The port `51111` is what you would then set `rigctl_default_port` to.
