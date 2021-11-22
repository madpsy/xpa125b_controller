# XPA125B Amplifier Network & Serial Controller
Xiegu XPA125B (https://xiegu.eu/product/xpa125-100w-solid-state-linear-amplifier/) network control interface designed for a D1 Mini (https://www.wemos.cc/en/latest/d1/d1_mini.html). This allows you to use virtually any rig, including SDRs, with this amplifier. Both PTT and automatic band selection are supported. Although WiFi is required for the APIs you can also operate without network access in `analogue` or `serial` mode. This would enable you to use a Yaesu rig and have the benefit of automatic band selection. The primary intended use of the controller is to hook directly into `rigctld` (https://hamlib.github.io/) meaning any rig which it supports will work (albeit requiring the use of WiFi). The latency is ~100ms in network modes which is perfectly adequate for almost all digital work as well as phone. In serial and analogue modes the latency is even less.

Written using the Arduino IDE. Required 3rd party libraries included for convience.

Supported APIs/protocols:

+ Analogue (Yaesu standard band voltages)
+ Serial
+ Web Interface
+ REST
+ MQTT
+ Rigctld

Even if a particular API isn't the current active mode for control purposes it can still be consumed for state. For example, providing MQTT is configured and enabled it will still publish events such as band/frequency and PTT changes. This allows the current state to be easily consumed by other MQTT compatible software. Similarly for REST, you can always `GET /state` to find out the current PTT state.

# Use Cases

This project has grown arms and legs and is now a powerful tool way beyond its initial intended use. The most useful feature is providing multiple APIs on top of rigctl to allow integration into almost any custom software.

+ Use any Yaesu radio with an XPA125B Amplifier without network
+ Use any hamlib compatible radio with an XPA125B Amplifier
+ Provide a Web/REST/MQTT/Serial API interface to rigctl
+ Allow automation via Node-RED to any hamlib compatible radio
+ Expand to support Icom and other manufactures if desired

# Configuration

The top of the file contains config options which need set first:

+ `ssid` - your WiFi SSID
+ `password` - your WiFi password

If you want to enable MQTT set the following:

+ `mqtt_enabled` = true
+ `mqttserver` = <host>  (MQTT server hostname or IP address)
+ `mqttuser` = <username>  (MQTT username)
+ `mqttpass` = <password>  (MQTT password)

# Rigctl

To make rigctl default on boot first ensure WiFi credentials are filled in and also set:

+  `mode` = rigctl
+  `rigctl_default_enable` = true
+  `rigctl_default_address` = <IP address> (rigctl IP address)
+  `rigctl_default_port` = <port> (rigctl port)
  
As an example, if you want to use SDR Console as the rig, first enable CAT control and attach it to one end of a pair of virtual com cables, then run rigctld:

`rigctld.exe -r COM18 -m 2014 -s 57600 -t 51111`
  
 You need to ensure the port is allowed through the firewall.
  
 In the above we are using `COM18` as the other end of the virtual com cable. `2014` is the rig ID for a Kenwood TS-2000 which SDR Console emulates. The port `51111` is what you would then set `rigctl_default_port` to.
  
  Other software such as SparkSDR (https://www.sparksdr.com/) has rigctld built in so no need to run the daemon - simply point the controller to the IP/port of SparkSDR directly (you must enable CAT control). This is the ideal way to run the controller and perfect for radios such as the Hermes-Lite (http://www.hermeslite.com/).
  
 You can control basic functions of the rig ('rig' could also be SDR Console/SparkSDR etc) via the REST and MQTT APIs. Namely frequency, mode and PTT. For example, to set the current frequency of the rig via REST:
  
  `curl -s -d 'freq=7074000' http://xpa125b.local/setrigctlfreq`
  
 In essense this operates as a REST/MQTT/Serial API translation layer to rigctl.
 
 If you are using `rigctld` on Windows you need a recent version of Hamlib due to a bug I discovered while developing this controller. More details here: https://github.com/Hamlib/Hamlib/issues/873.
  
# MQTT

As well as allowing control of the amplifier by publishing MQTT events the system is always updating current state to MQTT. You can use this feature to act as a bridge between Rigctl and MQTT even when no amplifier is involved. To do this:
  
  1. Configure MQTT server/username/password in the config (ensure `mqtt_enabled` is `true`)
  2. Configure Rigctl settings in the config and optionally make it the default mode
  
Now every state change will be published as an event to MQTT to be consumed by other software, such as Node-RED (https://nodered.org/) to trigger other actions. You could go wild and use this to have a big LED matrix display showing the current frequency and PTT state.
  
# Web Interface
  
A simple web interface is available on port 80 which allows access to basic functionality as well as documentation. This uses REST so anything which can be done here can also be achieved via the API.
  
![web](https://raw.githubusercontent.com/madpsy/xpa125b_controller/main/webinterface.png "wen")
  
# Valid serial commands (115200 baud):

+ serialonly [true|false] (disables analogue and wifi entirely)
+ setmode [analogue|serial|http|mqtt|rigctl|none]
+ setstate [rx|tx]
+ setband [160|80|60|40|30|20|17|15|12|11|10]
+ setfreq [frequency in Hz]
+ setrigmode mode=[rigmode] (USB/FM etc)
+ setmqtt [enable|disable]
+ setrigctl [address] [port]
+ setrigctlfreq freq=[frequency in Hz] (only available in rigctl mode)
+ setrigctlmode mode=[mode] ('mode' depends on radio - only available in rigctl mode)
+ setrigctlptt ptt=[0|1] (only available in rigctl mode)
  
Note: There are no commands to get current states over serial. The reason being all state changes are printed when they occur so you need to parse and track them as they appear.

# Valid HTTP POST paths:

+ /setmode mode=[analogue|serial|http|mqtt|rigctl|none]
+ /setstate state=[rx|tx]
+ /setband band=[160|80|60|40|30|20|17|15|12|11|10]
+ /setfreq freq=[frequency in Hz]
+ /setrigmode mode=[rigmode] (USB/FM etc)
+ /setmqtt mqtt=[enable|disable] (only available via http)
+ /setrigctl address=[rigctl IP address] port=[rigctl port] (http only)
+ /setrigctlfreq freq=[frequency in Hz] (rigctl only)
+ /setrigctlmode mode=[mode] ('mode' depends on radio - rigctl only)
+ /setrigctlptt ptt=[0|1] (rigctl only)
  
Note: When you use `setfreq` the system will translate this into a band automatically. The idea is it makes it a lot easier than having to do the translation yourself.

# Valid HTTP GET paths:

+ /mode (show current mode)
+ /state (show current PTT state)
+ /band (show current band)
+ /frequency (show current frequency - must have been set)
+ /txtime (show tx time in seconds)
+ /txblocktimer (show tx countdown block timer in seconds)
+ /network (show network details)
+ /mqtt (show if mqtt is enabled - only available via http)
+ /rigctl (show rigctl server and performs connection test - only available via http)
+ /rigmode (show mode the rigctl radio is set to (FM, USB etc)
+ /status (show status summary in HTML - only available via http)

MQTT topic prefix is 'xpa125b' followed by the same paths as above (where the message is the values in [])

# Example API calls

Note: mDNS should be xpa125b.local

+ `curl -s -d 'mode=http' http://xpa125b.local/setmode`
+ `curl -s http://xpa125b.local/txtime`
+ `mosquitto_pub -h hostname -u username -P password -t xpa125b/setmode -m http`
+ `mosquitto_sub -h hostname -u username -P password -t xpa125b/txtime`

When `serialonly` is enabled neither http/mqtt (wifi is disabled) nor analogue can be used. You can always use 'setmode' with serial/http/mqtt reguardless of current mode except when serialonly is enabled, in which case it only works via serial
  
Note: When using `setfreq` it automatically sets the correct band. Therefore, use either `setfreq` or `setband` but not both.

+ In analogue mode only the Yaesu standard voltage input is used for band selection and rx/tx is only via the control cable
+ In serial mode we only accept band/freq selection and rx/tx via serial
+ In mqtt mode we only accept band/freq selection and rx/tx via mqtt messages
+ In http mode we only accept band/freq selection and rx/tx via http messages
+ In rigctl mode we only accept band/freq selection and rx/tx via rigctl. You can control the rig itself in this mode via http/mqtt/serial. Server connection must succeed for this mode to activate)
+ In none mode then no control is possible

If MQTT is disabled and the mode is changed to MQTT then it will be automatically enabled.

# Serial Example

As mentioned previously you can run the controller without any network connectivity at all. In fact you can still interface with rigctl without network access by using the serial interface. Simply plug the controller into a USB port on the system you wish to run the scripts on.
  
Example scripts written in Bash showing how this may be achieved can be found in the `examples/serial` directory. In the configuration you may leave everything default (blank) and only change the default mode to serial (`mode = "serial"`). This isn't strictly neccessary as the script sets this when it starts just to be sure. Because we haven't configured the WiFi credentials there won't be any network access.

Assuming rigctld is available on `localhost` port `51111` and the serial interface of the controller is available at `/dev/ttyUSB0` you would run:

`./amp_serial_control.sh localhost 51111 /dev/ttyUSB0`

In a different terminal you can watch the serial output by running:
  
`./amp_serial_messages.sh /dev/ttyUSB0`
  
Now all frequency, mode and PTT state changes will be passed to the controller and it will work just as it does when connecting directly to rigctl over the network. Hopefully this demonstrates how flexible the controller can be for scenarios where WiFi connectivity is unavailable or undesirable. You can still enable WiFi with serial mode if you want the control to remain via serial but still have access to the other APIs and optionally also publish events to MQTT.
  
Note: The example scripts are very bare bones and intended to demonstrate the feature. One improvement would be to not open a new TCP connection to rigctld every time but instead keep it open and send commands over the same session.

# Analogue Mode

Analogue is the most basic mode of operation. This simply reads the incoming band voltage from a Yaesu compatible radio and outputs the correct voltage to the amplifier. PTT is handled in the same way. As well as automatic band control the other advantage of using the controller like this is the TX Block Timer.
  
In this mode you don't need WiFi or serial. As the default mode is already set to `analogue` simply compile as is and the controller will work straight away. You could even power it from a USB battery pack.

You can still enable WiFi and/or MQTT with this mode if desired. This allows you to have access to the other APIs and web interface aswel as push MQTT events but have control entirely handled locally.
  
# TX Block
  
If TX time exceeds 300 seconds (default) then TX will be blocked for 60 seconds (default). After the block releases you must send another TX event to start again - this includes analogue (i.e. release PTT). Note that 'seconds' is only rough due to non-exact timing in the code.

In every mode this tells the amp to switch to RX. In rigctl mode this also tells the radio itself to stop TX'ing.
  
To configure the timings, just set the following in the config (in seconds):
 
+ `tx_limit` (maximum allowed TX time)
+ `tx_block_time` (how long to block TX for)

# Home Assistant / Node-RED
  
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
  name: "XPA125B Rig Mode"
  state_topic: "xpa125b/rigctlmode"
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
  
Node-RED has built in support for MQTT. You can subscribe to the relevant topics and easily create automations based on these. Of course you can also send commands assuming you are in MQTT mode. 

This is particularly useful if you want to trigger other events based on the current state. An example might be to illuminate a light when you are transmitting or even send a notification if the TX Block Timer is activated.
  
In my case if TX Block is activated (i.e. `txblocktimer` is > 0) then the power to the amplifier is removed via a 'smart' plug. The basis being if TX exceeds the threshold then something must be amiss with the system so best to cut the power and find out what went wrong.
  
# To Do
  
+ Reduce the excessive use of `Strings` in the code
+ There is virtually no input validation. Therefore all input values are trusted. This can be a pro or a con.
+ Add support for Icom CI-V via `analogue` mode (Icom radios are currently only supported via rigctl)
+ Provide schematics for the interface board to the XPA125B amplifier
+ Add instructions how to adapt for other amplifiers
