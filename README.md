# XPA125B Amplifier Network Controller
Xiegu XPA125B (https://xiegu.eu/product/xpa125-100w-solid-state-linear-amplifier/) network control interface designed for a D1 Mini (https://www.wemos.cc/en/latest/d1/d1_mini.html). This allows you to use virtually any rig, including SDRs, with this amplifier. Both PTT and automatic band selection is supported. Although WiFi is supported the controller can also operate without it in `analogue` mode. This would enable you to use a Yaesu rig and have the benefit of automatic band selection. The primary intended use of the controller is to hook directly into `rigctld` https://hamlib.github.io/ meaning any rig which it supports will work (albeit requiring the use of WiFi). The latency is ~100ms which is perfectly adiquate for almost all digital work aswel as phone.

Written using the Arduino IDE. Required 3rd party libraries included for convience.

Supported APIs/protocols:

+ Analogue (Yaesu standard band voltages)
+ Serial
+ Web Interface
+ REST
+ MQTT
+ Rigctld

# Configuration

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
  
  Other software such as SparkSDR (https://www.sparksdr.com/) has rigctld built in so no need to run the daemon - simply point the controller to the IP/port of SparkSDR directly. This is the ideal way to run the controller.
  
# Web Interface
  
A simple web interface is available on port 80 which allows access to basic functionality aswel as documentation. This uses REST so anything which can be done here can also be achieved via the API.
  
![web](https://raw.githubusercontent.com/madpsy/xpa125b_controller/main/webinterface.png "wen")
  
# Valid serial commands (115200 baud):

+ serialonly [true|false] (disables analogue and wifi entirely)
+ setmode [analogue|serial|http|mqtt|rigctl]
+ setstate [rx|tx]
+ setband [160|80|60|40|30|20|17|15|12|11|10]
+ setfreq [frequency in Hz]
+ setmqtt [enable|disable]
+ setrigctl [address] [port]

# Valid HTTP POST paths:

+ /setmode mode=[analogue|serial|http|mqtt|rigctl]
+ /setstate state=[rx|tx]
+ /setband band=[160|80|60|40|30|20|17|15|12|11|10]
+ /setfreq freq=[frequency in Hz]
+ /setmqtt mqtt=[enable|disable] (only available via http)
+ /setrigctl address=[rigctl IP address] port=[rigctl port] (http only)

# Valid HTTP GET paths:

+ /mode (show current mode)
+ /state (show current state)
+ /band (show current band)
+ /frequency (show current frequency - must have been set)
+ /txtime (show tx time in seconds)
+ /txblocktimer (show tx countdown block timer in seconds)
+ /network (show network details)
+ /mqtt (show if mqtt is enabled - only available via http)
+ /rigctl (show rigctl server and performs connection test - only available via http)
+ /status (show status summary in HTML - only available via http)

MQTT topic prefix is 'xpa125b' followed by the same paths as above (where the message is the values in [])

Examples: (Note: mDNS should be xpa125b.local)

+ `curl -s -d 'mode=http' http://192.168.0.180/setmode`
+ `curl -s http://192.168.0.180/txtime`
+ `mosquitto_pub -h hostname -u username -P password -t xpa125b/setmode -m http`
+ `mosquitto_sub -h hostname -u username -P password -t xpa125b/txtime`

When `serialonly` is enabled neither http/mqtt (wifi is disabled) nor analogue can be used
You can always use 'setmode' with serial/http/mqtt reguardless of current mode except when serialonly is enabled, in which case it only works via serial

+ In analogue mode only the Yaesu standard voltage input is used for band selection and rx/tx is only via the control cable
+ In serial mode we only accept band/freq selection and rx/tx via serial
+ In mqtt mode we only accept band/freq selection and rx/tx via mqtt messages
+ In http mode we only accept band/freq selection and rx/tx via http messages
+ In rigctl mode we only accept band/freq selection and rx/tx via rigctl (server connection must succeed for this mode to activate)

If MQTT is disabled and the mode is changed to MQTT then it will be automatically enabled

If TX time exceeds 300 seconds then TX will be blocked for 60 seconds. After the block releases you must send another TX event to start again - this includes analogue (i.e. release PTT). Note that 'seconds' is only rough due to non-exact timing in the code.

# Home Assistant
  
If you use Home Assistant (https://www.home-assistant.io/) you can add sensors via MQTT as follows:
  
```
#########################################################################
# XPA125B
#########################################################################
- platform: mqtt
  name: "XPA125B State"
  state_topic: "xpa125b/state"
- platform: mqtt
  name: "XPA125B Band"
  state_topic: "xpa125b/band"
- platform: mqtt
  name: "XPA125B Frequency"
  state_topic: "xpa125b/frequency"
  unit_of_measurement: "Hz"
- platform: template
  sensors:
      xpa125b_frequency_mhz:
        friendly_name: "XPA125B Frequency MHz"
        unit_of_measurement: 'MHz'
        icon_template: mdi:sine-wave
        value_template: >-
                {{ (states('sensor.xpa125b_frequency') | float  / 1000000 | round(6)) }}
- platform: mqtt
  name: "XPA125B Mode"
  state_topic: "xpa125b/mode"
- platform: mqtt
  name: "XPA125B TX Time"
  state_topic: "xpa125b/txtime"
  unit_of_measurement: "s"
- platform: mqtt
  name: "XPA125B TX Block Timer"
  state_topic: "xpa125b/txblocktimer"
  unit_of_measurement: "s"
```

  Then create some cards on a dashboard and it will look something like this:
  
  ![ha](https://raw.githubusercontent.com/madpsy/xpa125b_controller/main/ha.png "ha")
