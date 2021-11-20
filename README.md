# XPA125B Amplifier Controller
Xiegu XPA125B control interface designed for a D1 Mini. This allows you to use virtual any rig, including SDRs, with this amplifier. Both PTT and automatic band selection is supported.

Supported protocols:

+ Analogue (Yaesu standard band voltages)
+ Serial
+ Web Interface
+ REST
+ MQTT
+ Rigctl

The top of the file contains config options which need set first:

+ ssid - your WiFi SSID
+ password - your WiFi password

If you want to enable MQTT set  to true and set the following:

+ mqtt_enabled = true
+ mqttserver = <host>  (MQTT server hostname or IP address)
+ mqttuser = <username>  (MQTT username)
+ mqttpass = <password>  (MQTT password)

To make rigctl default on boot, ensure WiFi credentials are filled in and also set:

+  mode = rigctl
+  rigctl_default_enable = true
+  rigctl_default_address = <IP address> (rigctl IP address)
+  rigctl_default_port = <port> (rigctl port)
  
As an example, if you want to use SDR Console as the rig, first enable CAT control and attach it to one end of a pair of virtual com cables, then run rigctld:

`rigctld.exe -r COM18 -m 2014 -s 57600 -t 51111`
  
 In the above we are using `COM18` as the other end of the virtual com cable. `2014` is the rig ID for a Kenwood TS-2000 which SDR Console emulates. The port `51111` is what you would then set `rigctl_default_port` to.
