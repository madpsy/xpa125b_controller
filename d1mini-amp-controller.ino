// *********** START CONFIG ***********

// WiFi config
bool wifi_enabled = false
const char* ssid = "";
const char* password = "";

// MQTT config
bool mqtt_enabled = false
const char* mqttserver = "";
const char* mqttuser = "";
const char* mqttpass = "";

// default mode
String default_mode = "yaesu";

// always use analog control cable for PTT
bool hybrid = false;

// enable bluetooth (required for Icom)
bool hl_05_enabled = false;

// enable MAX3232 (required for Elecraft radio or Hardrock-50 amplifier)
bool max3232_enabled = false;
long int max3232_baud = 38400;

// amplifier type
const char* amplifier = "xpa125b";

// rigctl config
bool rigctl_default_enable = false;
String rigctl_default_address = "";
String rigctl_default_port = "";
int rigctl_timeout = 500;
int rigctl_delay = 0;
int rigctl_debug = false;

// TX blocker config in seconds
int tx_limit = 300; // 300 = 5 mins 
int tx_block_time = 60; // 60 = 1 minute

// *********** END CONFIG ***********

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <Regexp.h>
#include <SoftwareSerial.h>

ESP8266WebServer server(80);

WiFiClient mqttClient;
WiFiClient rigctlClient;
PubSubClient pubsubClient;

int ptt_pin = D1;
int band_pin = D2;
int tx_pin = D3;
int yaesu_band_pin = A0;
// serial adapters - use either an hc-05 for the IC-705 or a MAX3232 for Elecraft radio/Hardrock-50 amp - both share the same pins
// bluetooth - tx pin overlaps with SunSDR/Yaesu so you can either use bluetooth (for Icom-705) or SunSDR/Yaesu
int hc05_rx_pin = D4;
int hc05_tx_pin = D5;
// MAX3232 - tx pin overlaps with SunSDR/Yaesu so you can either use MAX3232 (for Elecraft radio / Hardrock-50 amp) or SunSDR/Yaesu
int max3232_rx_pin = D4;
int max3232_tx_pin = D5;
// SunSDR EXT CTRL and Yaesu Band Data - requires level converter 5VDC to 3V3
// X8 on the SunSDR and 'TX GND' on Yaesu (LINEAR mode) is PTT (no need for level converter)
int band_data_1 = D5;
int band_data_2 = D6;
int band_data_3 = D7;
int band_data_4 = D8;

String serialValue;
char serialEOL = '\n';

int current_band = 0;
int previous_band = 0;
int tx_timer = 0;
int tx_seconds = 0;
int tx_seconds_true = 0;
int tx_previous_seconds = 0;
int tx_block_timer = 0;
int tx_block_seconds = 0;
int tx_block_seconds_true = 0;
int tx_block_previous_seconds = 0;
int yaesu_band_voltage = 0;
char bt_buffer[32];
char bt_com = 0;
unsigned char hr_band = 11;
unsigned char prev_hr_band = 11;
unsigned char bt_ptr = 0;
unsigned long bt_kHz = 0;
unsigned long bt_MHz = 0;
unsigned long bt_Hz = 0;
String bt_freq;
unsigned long current_tx_millis = 0;
unsigned long previous_tx_millis = 0;
unsigned long current_block_millis = 0;
unsigned long previous_block_millis = 0;
bool rx_state = true;
bool previous_state = 0;
bool current_state = 0;
bool serialonly = 0;
bool current_yaesu_rx = 0;
bool previous_yaesu_rx = 0;
bool current_rigctl_rx = 0;
bool previous_rigctl_rx = 0;
bool rigctl_address_set = false;
bool rigctl_port_set = false;
String mode = "none";
String frequency = "0";
String previous_frequency = "0";
String rigctl_address;
String rigctl_port;
String current_rig_mode = "none";
String previous_rig_mode = "none";
IPAddress remote_ip;
IPAddress rigctl_ipaddress;
int rigctl_portnumber;

String curState = "rx";
String curBand = "0";
String curMode = "none";
String previousMode = "none";

char* elecraft_response;
int elecraft_value;

char* Topic;
byte* buffer;
String message;
boolean Rflag=false;
int r_len;
char ser_buffer[32];

SoftwareSerial BTserial(hc05_rx_pin, hc05_tx_pin);
SoftwareSerial MAX3232(max3232_rx_pin, max3232_tx_pin);

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

String getRemoteIP() {
  // remote_ip variable is of type 'IPAddress' and can be used elsewhere if needed
  remote_ip = server.client().remoteIP();
  // this function actually returns a string 
  return server.client().remoteIP().toString();
}

char* MAX3232serialRead(char term_char) {
  const unsigned int MAX_MESSAGE_LENGTH = 12;
  while (MAX3232.available() >0) {
   static char message[MAX_MESSAGE_LENGTH];
   static unsigned int message_pos = 0;
   char inByte = Serial.read();
   if ( inByte != term_char && (message_pos < MAX_MESSAGE_LENGTH - 1) ) {
     message[message_pos] = inByte;
     message_pos++;
   } else {
    message_pos = 0;
    return (message);
   }
  }
}

int get_elecraft_value(char* message) {
  String stringMessage = String(message);
  stringMessage.remove(0, 2);
  return stringMessage.toInt();
}

void callback(char* topic, byte* payload, unsigned int length) {
  message="";
  Topic=topic;
  Rflag=true; //will use in main loop
  r_len=length; //will use in main loop
  for (int i=0;i<length;i++) {
    message+=(char)payload[i];
  }
}

void webServer(bool value) {
  if (value == true) {
    server.begin();
    Serial.println("HTTP server started");
  } else {
    server.stop();
    Serial.println("HTTP server stopped");
  }
}

void mqttConnect() {
  pubsubClient.setClient(mqttClient);
  pubsubClient.setServer(mqttserver,1883);
  pubsubClient.setCallback(callback);
  // pubsubClient.connected condition here prevents a crash which can happen if already connected
  if (((mqtt_enabled == true) && (!pubsubClient.connected()) && (WiFi.status() == WL_CONNECTED))) {
    delay(100); // stops it trying to reconnect as fast as the loop can go
    Serial.println("Attempting MQTT connection");
    if (pubsubClient.connect("xpa125b", mqttuser, mqttpass)) {
      Serial.println("MQTT connected"); 
      pubsubClient.subscribe("xpa125b/#");
    }
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/html; charset=UTF-8", message);
}

bool setupBandData() {
  if (hl_05_enabled == false) {
    pinMode(band_data_1,INPUT_PULLUP);
    pinMode(band_data_1,INPUT_PULLUP);
    pinMode(band_data_1,INPUT_PULLUP);
    pinMode(band_data_1,INPUT_PULLUP);
    Serial.println("Band data pins configured");
    return true;
  } else {
    Serial.println("Cannot setup band data when bluetooth is snabled as the TX pin overlaps");
    return false;
  }
}

bool setupMAX3232() {
  if (hl_05_enabled == false) {
    if (max3232_enabled == true) {
      MAX3232.begin(max3232_baud);
      Serial.print("MAX3232 configured at baudrate ");
      Serial.println(max3232_baud);
      return true;
    } else {
      Serial.println("You must set max3232_enabled to true");
      return false;
    }
  } else {
    Serial.println("Cannot setup MAX3232 when bluetooth is enabled as the TX pin overlaps");
    return false;
  }
}

bool setupAmplifier(String value) {
  if (value == "xpa125b") {
    amplifier = "xpa125b";
    Serial.println("Amplifier set to xpa125b");
    return true;
  } else if (value == "minipa50") {
    amplifier = "xpa125b";
    Serial.println("Amplifier set to xpa125b");
    return true;
  } else if (value == "hardrock50") {
    if (setupMAX3232()) {
      amplifier = "hardrock50";
      Serial.println("Amplifier set to hardrock50");
      return true;
    } else {
      amplifier = "none";
      Serial.print("Failed to set amplifier set to hardrock50");
      return false;
    }
  }
}

void getMode() {
  server.send(200, "text/html; charset=UTF-8", mode);
}

void getState() {
  server.send(200, "text/html; charset=UTF-8", curState);
}

void getBand() {
  server.send(200, "text/html; charset=UTF-8", curBand);
}

void getFrequency() {
  server.send(200, "text/html; charset=UTF-8", frequency);
}

void httpGetRigMode() {
  server.send(200, "text/html; charset=UTF-8", current_rig_mode);
}

String getRigctlAddress() {
  if (!rigctl_address_set) {
    return getRemoteIP();
  } else {
    return rigctl_address;
  }
}

String getRigctlPort() {
  if (!rigctl_port_set) {
    return "51111";
  } else {
    return rigctl_port;
  }
}

String getRigctlServer() {
  if ((rigctl_address_set) && (rigctl_port_set)) {
    return rigctl_address + ":" + rigctl_port;
  } else {
    return "null:null";
  }
}

void httpGetRigctlServer() {
  String result;
  if (testRigctlServer()) {
    result = "success";
  } else {
    result = "failed";
  }
  server.send(200, "text/html; charset=UTF-8", getRigctlServer() + " " + result);
}

void setRigctlAddress(String address) {
  rigctl_address = address;
  rigctl_address_set = true;
  rigctl_ipaddress.fromString(address);
  Serial.print("rigctl_address ");
  Serial.println(rigctl_address);
}

void setRigctlPort(String port) {
  rigctl_port = port;
  rigctl_port_set = true;
  rigctl_portnumber = port.toInt();
  Serial.print("rigctl_port ");
  Serial.println(rigctl_port);
}

bool testRigctlServer() {
  if (((wifi_enabled == true) && (rigctl_address_set) && (rigctl_port_set))) {
   if (rigctlClient.connect(rigctl_ipaddress, rigctl_portnumber)) {
    if ((sendRigctlCommand("t") == "0") || (sendRigctlCommand("t") == "1")) {
      Serial.print("connection to rigctl server succeeded ");
      Serial.println(rigctl_address + ":" + rigctl_port);
      return true;
    } else {
      Serial.print("connection to rigctl succeeded but PTT status failed to return ");
      Serial.println(rigctl_address + ":" + rigctl_port);
      if (mode == "rigctl") {
        setMode("none");
      }
      return false;
    }
    rigctlClient.stop();
   } else {
    Serial.print("connection to rigctl server failed ");
    Serial.println(rigctl_address + ":" + rigctl_port);
    if (mode == "rigctl") {
      setMode("none");
    }
    return false;
   }
  } else {
    Serial.println("rigctl server not set");
    return false;
  }
}

bool connectRigctl() {
  if (((wifi_enabled == true) &&  (rigctl_address_set) && (rigctl_port_set))) {
    //Serial.println("connecting to rigctl");
    if (rigctlClient.connect(rigctl_ipaddress, rigctl_portnumber)) {
      if (rigctl_debug) {
        Serial.println("rigctl connection success");
      }
      return true;
    } else {
      Serial.println("rigctl connection failed");
      return false;
    }
  } else {
    Serial.println("wifi disabled or rigctl address and port not set");
    return false;
  }
}

String sendRigctlCommand(char* command) {
  
  if (!rigctlClient.connected()) {
      connectRigctl();
  }
  if (rigctlClient.connected()) {
        rigctlClient.print(command);
        rigctlClient.print("\n");
        unsigned long timeout = millis();
        while (rigctlClient.available() == 0) {
         if (millis() - timeout > rigctl_timeout) {
          // we must stop the client after timeout (causing a reconnect) otherwise data backs up in the buffer and is read on the next
          // iteration, causing e.g. the ptt status to be concatenated with the frequency
          // set rigctl_debug to true if rigctl appears to be doing nothing yet the connection test claims success
          if (rigctl_debug) {
            Serial.println("rigctl timed out"); 
          }
          rigctlClient.stop();
          return("error");
          }
        }

        String response;
        while(rigctlClient.available()){
          char ch = static_cast<char>(rigctlClient.read());
          response += String(ch);
          if (ch == '\n') {
             while(rigctlClient.available()){
               // gobble the rest up but do nothing with it
               // this was originally for when getting the current mode ('m') as it also returns the filter width but we don't care about that
               static_cast<char>(rigctlClient.read());
             }
          }
        }
        int length = response.length();
        response.remove(length - 1, 1);
        delay(rigctl_delay);
        //Serial.println(response);
        return(response);
    } else {
      return "error";
    }
}

void setRigctlFreq(String frequency) {
     Serial.print("rigctlfreq: ");
     Serial.println(frequency);
     String cmd  = "F "; cmd += (frequency);
     char cmdChar[20];
     cmd.toCharArray(cmdChar, 20);
     sendRigctlCommand(cmdChar);
}

void httpSetRigctlFreq(String value) {
  server.send(200, "text/html; charset=UTF-8", value);
  setRigctlFreq(value);
}

void setRigMode(String rigmode) {
    current_rig_mode = rigmode;
    if (current_rig_mode != previous_rig_mode) {
       char modeChar[20];
       current_rig_mode.toCharArray(modeChar, 20);
       if (pubsubClient.connected()) pubsubClient.publish("xpa125b/rigmode", modeChar, true);
       Serial.print("rigmode ");
       Serial.println(current_rig_mode);
       previous_rig_mode = rigmode;
    }
}

void httpSetRigMode(String value) {
  server.send(200, "text/html; charset=UTF-8", value);
  setRigMode(value);
}

void setRigctlMode(String mode) {
     String cmd  = "M "; cmd += (mode);
     char cmdChar[10];
     cmd.toCharArray(cmdChar, 10);
     sendRigctlCommand(cmdChar);
}

void httpSetRigctlMode(String value) {
  server.send(200, "text/html; charset=UTF-8", value);
  setRigctlMode(value);
}

void setRigctlPtt(String ptt) {
  if ((ptt != "0") && (tx_block_timer != 0)) {
     return;
  } else {
     Serial.print("rigctlptt: ");
     Serial.println(ptt);
     String cmd  = "T "; cmd += (ptt);
     char cmdChar[5];
     cmd.toCharArray(cmdChar, 5);
     sendRigctlCommand(cmdChar);
  }
}

void httpSetRigctlPtt(String value) {
  server.send(200, "text/html; charset=UTF-8", value);
  setRigctlPtt(value);
}

void handleRoot() {
  String message = "<html><head><title>XPA125B</title></head><body>";
  message += "<script>";
  message += "var xhr = new XMLHttpRequest();";
  message += "function mouseDown() {";
  message += "document.getElementById('ptt').value = 'PTT TX';";
  message += "xhr.open('POST', '/setrigctlptt', true);";
  message += "xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded; charset=UTF-8');";
  message += "xhr.send('ptt=1');";
  message += "}";
  message += "function mouseUp() {";
  message += "document.getElementById('ptt').value = 'PTT RX';";
  message += "xhr.open('POST', '/setrigctlptt', true);";
  message += "xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded; charset=UTF-8');";
  message += "xhr.send('ptt=0');";
  message += "}";
  message += "</script>";
  message += "Xiegu XPA125B Controller</br></br>";
  message += "<form action='/setmode' method='post' target='response'>";
  message += "<select name='mode'>";
  message += "<option value='rigctl'>Rigctl</option>";
  message += "<option value='yaesu'>Yaesu</option>";
  message += "<option value='yaesu817'>Yaesu 817/818</option>";
  message += "<option value='icom'>Icom</option>";
  message += "<option value='sunsdr'>SunSDR</option>";
  message += "<option value='elecraft'>Elecraft</option>";
  message += "<option value='serial'>Serial</option>";
  message += "<option value='http'>HTTP</option>";
  message += "<option value='mqtt'>MQTT</option>";
  message += "<option value='none'>None</option>";
  message += "</select>";
  message += "<button name='mode'>Set Mode</button>";
  message += "</form>";
  message += "<form action='/setband' method='post' target='response'>";
  message += "<select name='band'>";
  message += "<option value='160'>160m</option>";
  message += "<option value='80'>80m</option>";
  message += "<option value='60'>60m</option>";
  message += "<option value='40'>40m</option>";
  message += "<option value='30'>30m</option>";
  message += "<option value='20'>20m</option>";
  message += "<option value='17'>17m</option>";
  message += "<option value='15'>15m</option>";
  message += "<option value='12'>12m</option>";
  message += "<option value='11'>11m</option>";
  message += "<option value='10'>10m</option>";
  message += "</select>";
  message += "<button name='band'>Set Band</button>";
  message += "</form>";
  message += "<form action='/setmqtt' method='post' target='response'>";
  message += "<button name='mqtt' value='enable'>Enable MQTT</button>";
  message += "</form>";
  message += "<form action='/setmqtt' method='post' target='response'>";
  message += "<button name='mqtt' value='disable'>Disable MQTT</button>";
  message += "</form>";
  message += "Rigctl Server Settings:</br></br>";
  message += "<form action='/setrigctl' method='post' target='response'>";
  message += "<input type='text' size='15' maxlength='50' name='address' value='";
  message += getRigctlAddress();
  message += "'>";
  message += "<input type='text' size='5' maxlength='5' name='port' value='";
  message += getRigctlPort();
  message += "'>";
  message += "<button>Set/Test</button>";
  message += "</form>";
  message += "<form action='/setrigctlmode' method='post' target='response'>";
  message += "<select name='mode'>";
  message += "<option value='USB'>USB</option>";
  message += "<option value='LSB'>LSB</option>";
  message += "<option value='FM'>FM</option>";
  message += "<option value='AM'>AM</option>";
  message += "<option value='DigiU'>DigiU</option>";
  message += "<option value='FT8'>FT8</option>";
  message += "</select>";
  message += "<button name='mode'>Rigctl Mode</button>";
  message += "</form>";
  message += "<form action='/setrigctlfreq' method='post' target='response'>";
  message += "<input type='text' size='10' maxlength='10' name='freq' value='";
  message += frequency;
  message += "'>";
  message += "<button name='freq'>Rigctl Frequency</button>";
  message += "</form>";
  message +="<input id='ptt' type='button' value='PTT RX' onmouseup='mouseUp();' onmousedown='mouseDown();'></br></br>";
  message += "Last Response: ";
  message += "<iframe name='response' id='response' scrolling='no' frameBorder='0' width=400 height=25></iframe></br>";
  message += "Current State: ";
  message += "<iframe src='/status' scrolling='no' frameBorder='0' width=700 height=25></iframe>";
  message += "</br></br>";
  message += "Valid serial commands (115200 baud):</br></br>";
  message += "serialonly [true|false] (disables yaesu and wifi entirely)</br>";
  message += "setmode [yaesu|yaesu817|icom|sunsdr|elecraft|serial|http|mqtt|rigctl|none]</br>";
  message += "setstate [rx|tx]</br>";
  message += "setband [160|80|60|40|30|20|17|15|12|11|10]</br>";
  message += "setfreq [frequency in Hz]</br>";
  message += "setrigmode mode=[rigmode] (USB/FM etc)</br>";
  message += "setmqtt [enable|disable]</br>";
  message += "setrigctl [address] [port]</br>";
  message += "setrigctlfreq freq=[frequency in Hz] (rigctl only)</br>";
  message += "setrigctlmode mode=[mode] ('mode' depends on radio - rigctl only)</br>";
  message += "setrigctlptt ptt=[0|1] (rigctl only)</br></br>";
  message += "Valid HTTP POST paths:</br></br>";
  message += "/setmode mode=[yaesu|yaesu817|icom|sunsdr|elecraft|serial|http|mqtt|rigctl|none]</br>";
  message += "/setstate state=[rx|tx]</br>";
  message += "/setband band=[160|80|60|40|30|20|17|15|12|11|10]</br>";
  message += "/setfreq freq=[frequency in Hz]</br>";
  message += "/setrigmode mode=[rigmode] (USB/FM etc)</br>";
  message += "/setmqtt mqtt=[enable|disable] (only available via http)</br>";
  message += "/setrigctl address=[rigctl IP address] port=[rigctl port] (http only)</br>";
  message += "/setrigctlfreq freq=[frequency in Hz] (rigctl only)</br>";
  message += "/setrigctlmode mode=[mode] ('mode' depends on radio - rigctl only)</br>";
  message += "/setrigctlptt ptt=[0|1] (rigctl only)</br></br>";
  message += "Valid HTTP GET paths:</br></br>";
  message += "<a href='/mode'>/mode</a> (show current mode)</br>";
  message += "<a href='/state'>/state</a> (show current state)</br>";
  message += "<a href='/band'>/band</a> (show current band)</br>";
  message += "<a href='/frequency'>/frequency</a> (show current frequency - must have been set)</br>";
  message += "<a href='/txtime'>/txtime</a> (show tx time in seconds)</br>";
  message += "<a href='/txblocktimer'>/txblocktimer</a> (show tx countdown block timer in seconds)</br>";
  message += "<a href='/network'>/network</a> (show network details)</br>";
  message += "<a href='/mqtt'>/mqtt</a> (show if mqtt is enabled - only available via http)</br>";
  message += "<a href='/rigctl'>/rigctl</a> (show rigctl server and performs connection test - only available via http)</br>";
  message += "<a href='/rigmode'>/rigmode</a> (show mode the radio is set to (FM, USB etc)</br>";
  message += "<a href='/status'>/status</a> (show status summary in HTML - only available via http)</br></br>";
  message += "MQTT topic prefix is 'xpa125b' followed by the same paths as above (where the message is the values in [])</br></br>";
  message += "Examples: (Note: mDNS should be xpa125b.local)</br></br>";
  message += "curl -s -d 'mode=http' http://";
  message += WiFi.localIP().toString().c_str();
  message += "/setmode</br>";
  message += "curl -s http://";
  message += WiFi.localIP().toString().c_str();
  message += "/txtime</br>";  
  message += "mosquitto_pub -h hostname -u username -P password -t xpa125b/setmode -m http</br>";
  message += "mosquitto_sub -h hostname -u username -P password -t xpa125b/txtime</br></br>";
  message += "When serialonly is enabled neither http/mqtt (wifi is disabled) nor yaesu can be used</br>";
  message += "You can always use 'setmode' with serial/http/mqtt reguardless of current mode except when serialonly is enabled, in which case it only works via serial</br>";
  message += "In yaesu mode only the Yaesu band data is used for band selection and rx/tx is only via the control cable</br>";
  message += "In yaesu817 mode only the Yaesu standard voltage is used for band selection and rx/tx is only via the control cable</br>";
  message += "In icom mode only a Bluetooth attached Icom radio is used for band selection and rx/tx is only via the control cable</br>";
  message += "In sunsdr mode only the band data is used for band selection and rx/tx is only via the control cable</br>";
  message += "In elecraft mode we only accept band/freq selection and rx/tx via the serial port on the radio</br>";
  message += "In serial mode we only accept band/freq selection and rx/tx via serial</br>";
  message += "In mqtt mode we only accept band/freq selection and rx/tx via mqtt messages</br>";
  message += "In http mode we only accept band/freq selection and rx/tx via http messages</br>";
  message += "In rigctl mode we only accept band/freq selection and rx/tx via rigctl. You can control the rig in this mode via http/mqtt/serial. Server connection must succeed for this mode to activate.</br>";
  message += "In none mode then no control is possible</br></br>";
  message += "If 'hybrid' is set to true in the config then the control cable will be used for PTT in every mode</br></br>";
  message += "Example rigctld run command (TS-2000 has ID 2014):</br></br>";
  message += "rigctld.exe -r COM18 -m 2014 -s 57600 -t 51111</br></br>";
  message += "If MQTT is disabled and the mode is changed to MQTT then it will be automatically enabled</br></br>";
  message += "If TX time exceeds ";
  message += tx_limit;
  message += " seconds then TX will be blocked for ";
  message += tx_block_time;
  message += " seconds. After the block releases you must send another TX event to start again - this includes yaesu (i.e. release PTT).</br></br>";
  message += "</body></html>";
  server.send(200, "text/html; charset=UTF-8", message);
}

void getStatus() {
  String message = "<html><head><meta http-equiv='refresh' content='1'></head><body>";
  message += "Mode: ";
  message += mode;
  message += "&nbsp Band: ";
  message += curBand;
  message += "&nbsp Frequency: ";
  message += frequency;
  message += "&nbsp State: ";
  message += curState;
  message += "&nbsp Rig Mode: ";
  message += current_rig_mode;
  message += "&nbsp TX Time: ";
  message += String(tx_seconds_true);
  message += "&nbsp TX Blocker: ";
  message += String(tx_block_seconds_true);
  message += "&nbsp MQTT: ";
  String mqttvalue = (mqtt_enabled ? "enabled" : "disabled");
  message += mqttvalue;
  message += "</body></html>";
  server.send(200, "text/html; charset=UTF-8", message);
}

void getNetwork() {
  String message;
  message = "SSID: ";
  message += WiFi.SSID();
  message += "\nRSSI: ";
  message += WiFi.RSSI();
  message += "\nIP: ";
  message += WiFi.localIP().toString().c_str();
  message += "\nDNS: ";
  message += WiFi.dnsIP().toString().c_str();
  message += "\nGateway: ";
  message += WiFi.gatewayIP().toString().c_str();
  message += "\nMAC: ";
  message += WiFi.macAddress();
  message += "\nmDNS: xpa125b.local";
  message += "\nRemote IP: ";
  message += getRemoteIP();
  message += "\n";
  server.send(200, "text/plain; charset=UTF-8", message);
}

void getTxTime() {
  String seconds = String(tx_seconds_true);
  server.send(200, "text/plain; charset=UTF-8", seconds);
}

void getTxBlockTimer() {
  String seconds = String(tx_block_seconds_true);
  server.send(200, "text/plain; charset=UTF-8", seconds);
}

void setMode(String value) {
   curMode = value;
   if (curMode != previousMode) {
    if ((value == "rigctl") && (!testRigctlServer())) {
      return;
    }
    if (((value == "sunsdr") || (value == "yaesu")) && (!setupBandData())) {
      return;
    }
    if ((value == "elecraft") && (!setupMAX3232())) {
      return;
    }
    mode = value;
    //frequency = "0";
    //previous_frequency = "0";
    char charMode[9];
    mode.toCharArray(charMode, 9);
    Serial.print("mode ");
    Serial.println(mode);
    if (pubsubClient.connected()) pubsubClient.publish("xpa125b/mode", charMode, true);
    if ((mode == "mqtt") && (mqtt_enabled == 0)) {
      setMQTT("enable");
    }
    previousMode = value;
  }
}

void setBand(String band) {
    curBand = band;
    int bandInt = band.toInt();
    int band_len = band.length() + 1;
    char bandChar[band_len];
    band.toCharArray(bandChar, band_len);

    // a PWM value of 255 shuld be full cycle, therefore, 3V3. However, because we have an RC filter in the controller to prevent oscillation, this reduces the final outputted voltage seen by the amp.
    // voltages as per the docs (Mhz | mV):

    // 1.8 | 230
    // 3.8 | 460
    // 5.0 | 690
    // 7.0 | 920
    // 10.0 | 1150
    // 14.0 | 1380
    // 18.0 | 1610
    // 21.0 | 1840
    // 24.0 | 2070
    // 28.0 | 2300
    // 50.0 | 2350

    // given full PWM cycle is triggering the 10m/28MHz band and not the 6m band we know that the max voltage with the filter in place limits us to around 2300mV and we will never be able to
    // enable the 6m band with the current design. Not really an issue as this amp is intended to be used with the hermes lite which can't do 6m anyway.
    
    int pwm_value;
    int yaesu_pwm_value; // minipa50 uses yaesu standard voltages so we name var accordingly

    switch (bandInt) {
      case 160:
        pwm_value = 5;
        yaesu_pwm_value = 8;
        hr_band = 10;
        break;
      case 80:
        pwm_value = 40;
        yaesu_pwm_value = 30;
        hr_band = 9;
        break;
      case 60:
        pwm_value = 70;
        yaesu_pwm_value = 80;
        hr_band = 8;
        break;
      case 40:
        pwm_value = 95;
        yaesu_pwm_value = 80;
        hr_band = 7;
        break;
      case 30:
        pwm_value = 120;
        yaesu_pwm_value = 100;
        hr_band = 6;
        break;
      case 20:
        pwm_value = 150;
        yaesu_pwm_value = 130;
        hr_band = 5;
        break;
      case 17:
        pwm_value = 210;
        yaesu_pwm_value = 200;
        hr_band = 4;
        break;
      case 15:
        pwm_value = 230;
        yaesu_pwm_value = 210;
        hr_band = 3;
        break;
      case 12:
        pwm_value = 255;
        yaesu_pwm_value = 230;
        hr_band = 2;
        break;
      case 11:
        pwm_value = 255;
        yaesu_pwm_value = 230;
        hr_band = 1;
        break;
      case 10:
        pwm_value = 255;
        yaesu_pwm_value = 230;
        hr_band = 1;
        break;
      case 6:
        pwm_value = 255;
        yaesu_pwm_value = 255;
        hr_band = 0;
        break;
      default:
        pwm_value = 0;
        yaesu_pwm_value = 0;
        hr_band = 11;
        break;
    }

    if (amplifier == "xpa125b") {
      analogWrite(band_pin, pwm_value);
    } else if (amplifier == "minipa50") {
      analogWrite(band_pin, yaesu_pwm_value);
    } else if (amplifier = "hardrock50") {
      if (hr_band != prev_hr_band) {
        sprintf(ser_buffer, "HRBN%u;\n", hr_band);
        MAX3232.write(ser_buffer);
        prev_hr_band = hr_band;
      }
    }
    current_band = bandInt;
    if ( current_band != previous_band ) {
      Serial.print("band ");
      Serial.println(bandChar);
      if (pubsubClient.connected()) pubsubClient.publish("xpa125b/band", bandChar, true);
    }
    previous_band = bandInt; 
}

void setBandVoltage(int voltage) {

  // Yaesu standard voltages used in the 817/818
  // (Mhz | mV)

  // 1.8  | 330
  // 3.5  | 670
  // 7.0  | 1000
  // 10.0 | 1330
  // 14.0 | 1670
  // 18.0 | 2000
  // 21.0 | 2330
  // 24.5 | 2670
  // 28.0 | 3000
  // 50.0 | 3330
  
  if ((voltage >= 5) && (voltage <= 20)) {
    setBand("160");
  } else if ((voltage >= 20) && (voltage <= 30)) {
    setBand("80");
  } else if ((voltage >= 30) && (voltage <= 40)) {
    setBand("60");
  } else if ((voltage >= 30) && (voltage <= 40)) {
    setBand("40");
  } else if ((voltage >= 50) && (voltage <= 60)) {
    setBand("30");
  } else if ((voltage >= 70) && (voltage <= 80)) {
    setBand("20");
  } else if ((voltage >= 80) && (voltage <= 90)) {
    setBand("17");
  } else if ((voltage >= 90) && (voltage <= 100)) {
    setBand("15");
  } else if ((voltage >= 110) && (voltage <= 120)) {
    setBand("12");
  } else if ((voltage >= 120) && (voltage <= 130)) {
    setBand("10");
  }
}


bool regexMatch(char* value, char* regex) {
  MatchState ms;
  ms.Target(value);
  int result = ms.Match(regex);
  if(result>0){
    return true;
  } else {
    return false;
  }
}

void setFreq(String freq) {
 if (((freq != previous_frequency) && (freq != "error" ) && (freq != "0")))  {  
   frequency = freq;
   char charFreq[10];
   // create an int var for if/when I change this logic to use comparison between two values rather than regex, such as:
   // if ((freqInt >= 14000000) && (freqInt <= 14999999))
   int freqInt = frequency.toInt();
   freq.toCharArray(charFreq, 10);
   if (pubsubClient.connected()) pubsubClient.publish("xpa125b/frequency", charFreq, false);
   Serial.print("frequency ");
   Serial.println(frequency);
   if (regexMatch(charFreq, "^1......$")) {
    setBand("160");
   } else if (regexMatch(charFreq, "^3......$")) {
    setBand("80");
   } else if (regexMatch(charFreq, "^3......$")) {
    setBand("80");
   } else if (regexMatch(charFreq, "^5......$")) {
    setBand("60");
   } else if (regexMatch(charFreq, "^7......$")) {
    setBand("40");
   } else if (regexMatch(charFreq, "^10......$")) {
    setBand("30");
   } else if (regexMatch(charFreq, "^14......$")) {
    setBand("20");
   } else if (regexMatch(charFreq, "^18......$")) {
    setBand("17");
   } else if (regexMatch(charFreq, "^21......$")) {
    setBand("15");
   } else if (regexMatch(charFreq, "^24......$")) {
    setBand("12");
   } else if (regexMatch(charFreq, "^27......$")) {
    setBand("11");
   } else if (regexMatch(charFreq, "^28......$")) {
    setBand("10");
   } else if (regexMatch(charFreq, "^5.......$")) {
    setBand("6");
   } else {
    Serial.print("No matching band found for ");
    Serial.println(frequency);
   }
   previous_frequency = frequency;
  }
}

void setState(String state) {
  if ( state == "rx" ) {
    digitalWrite(ptt_pin, LOW);
    current_state = 0;
    curState = "rx";
    if (current_state != previous_state) {
      Serial.println("state rx");
      if (pubsubClient.connected()) pubsubClient.publish("xpa125b/state", "rx");
    }
    previous_state = 0;
    tx_timer = 0;
    tx_seconds = 0;
  } else if (( state == "tx" ) && ( tx_block_timer == 0 )) {
    digitalWrite(ptt_pin, HIGH);
    current_state = 1;
    curState = "tx";
    if (current_state != previous_state) {
      Serial.println("state tx");
      if (pubsubClient.connected()) pubsubClient.publish("xpa125b/state", "tx");
    }
    previous_state = 1;
  }
}

void setMQTT(String value) {
  if (value == "enable") {
    mqtt_enabled = true;
    mqttConnect;
    Serial.println("MQTT enabled");
  } else if (value == "disable") {
    mqtt_enabled = false;
    pubsubClient.disconnect();
    Serial.println("MQTT disabled");
  }
}

void httpSetMode(String value) {
  server.send(200, "text/html; charset=UTF-8", value);
  setMode(value);
}

void httpSetBand(String band) {
      server.send(200, "text/html; charset=UTF-8", band);
      setBand(band);
}

void httpSetFreq(String freq) {
      server.send(200, "text/html; charset=UTF-8", freq);
      setFreq(freq);
}

void httpSetMQTT(String value) {
  server.send(200, "text/html; charset=UTF-8", value);
  setMQTT(value);
}

void wifi(String state) {
  if ((state == "enable") && (WiFi.status() != WL_CONNECTED)) {
    WiFi.mode(WIFI_STA);
    WiFi.hostname("xpa125b");
    WiFi.begin(ssid, password);
    int count = 0;
    while ((WiFi.status() != WL_CONNECTED) && (count < 20)) {
      delay(500);
      Serial.print(".");
      count++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.print("Connected to ");
      Serial.println(ssid);
      Serial.print("RSSI ");
      Serial.println(WiFi.RSSI());
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("DNS address: ");
      Serial.println(WiFi.dnsIP());
      Serial.print("Gateway: ");
      Serial.println(WiFi.gatewayIP());
      Serial.print("MAC address: ");
      Serial.println(WiFi.macAddress());
      if (MDNS.begin("xpa125b")) {
        Serial.println("MDNS responder started as xpa125b[.local]");
      }
      mqttConnect();
    } else {
      Serial.println("\nWiFi failed to connect");;
    }
  } else if ((state == "disable") && (WiFi.status() != WL_DISCONNECTED)) {
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disabled");
  }
}

void setup(void) {
  Serial.begin(115200);
  delay(500); // computer serial port takes time to be available after reset
  Serial.println("XPA125B controller started");

  if (hl_05_enabled == true) {
    BTserial.begin(9600);
  }
  
  if (wifi_enabled == true) {
    wifi("enable");
  }

  setupAmplifier(amplifier);

  if (hybrid == true) {
    Serial.println("Hybrid mode is enabled");
  } else {
    Serial.println("Hybrid mode is disabled");
  }
  
  // set to 160m and 0Hz by default
  setBand("160");
  setFreq("0");

  pinMode(band_pin, OUTPUT);
  pinMode(ptt_pin, OUTPUT);
  pinMode(tx_pin, INPUT);
  pinMode(yaesu_band_pin, INPUT);
  
  analogWriteFreq(30000);

  if (mqtt_enabled == true) {
    if (pubsubClient.connected()) pubsubClient.subscribe("xpa125b/#");
    if (pubsubClient.connected()) pubsubClient.publish("xpa125b/state", "rx", true);
    if (pubsubClient.connected()) pubsubClient.publish("xpa125b/band", "160", true);
    if (pubsubClient.connected()) pubsubClient.publish("xpa125b/txtime", "0", false);
    if (pubsubClient.connected()) pubsubClient.publish("xpa125b/txblocktimer", "0", false);
    char modeChar[20];
    mode.toCharArray(modeChar, 20);
    if (pubsubClient.connected()) pubsubClient.publish("xpa125b/mode", modeChar, true);
  }

  if (rigctl_default_enable) {
    setRigctlAddress(rigctl_default_address);
    setRigctlPort(rigctl_default_port);
  }

  server.on("/", handleRoot);

  server.on("/status", getStatus);

  server.on("/network", []() {
        getNetwork();
  });

  server.on("/mode", []() {
        getMode();
  });

  server.on("/state", []() {
        getState();
  });

  server.on("/band", []() {
        getBand();
  });

  server.on("/frequency", []() {
        getFrequency();
  });

  server.on("/txtime", []() {
        getTxTime();
  });

  server.on("/txblocktimer", [] () {
        getTxBlockTimer();
  });

  server.on("/mqtt", [] () {
       String value = (mqtt_enabled ? "enabled" : "disabled");
       server.send(200, "text/html; charset=UTF-8", value);
  });

  server.on("/rigctl", [] () {
       httpGetRigctlServer();
  });

  server.on("/rigmode", [] () {
       httpGetRigMode();
  });

  server.on("/setmode", []() {
    if ((server.method() == HTTP_POST) && (server.argName(0) == "mode")) {
       httpSetMode(server.arg(0)); 
    } else {
      server.send(405, "text/html; charset=UTF-8", "Must send a POST with argument 'mode' and a value");
    }
  });
  
  server.on("/setstate", []() {
    if ((mode == "http") && (hybrid == false)) {
      if ((server.method() == HTTP_POST) && (server.argName(0) == "state")) {
        String state = server.arg(0);
        setState(state);
        server.send(200, "text/html; charset=UTF-8", state);
      } else {
        server.send(405, "text/html; charset=UTF-8", "Must send a POST with argument 'state' and a value");
      }
    } else {
      server.send(403, "text/html; charset=UTF-8", "HTTP control disabled or hybrid is enabled");
    }
  });
  
  server.on("/setband", []() {
    if (mode == "http") {
     if ((server.method() == HTTP_POST) && (server.argName(0) == "band")) {
       String band = server.arg(0);
       httpSetBand(band);
     } else {
        server.send(405, "text/html; charset=UTF-8", "Must send a POST with argument 'band' and a value");
     }
    } else {
      server.send(403, "text/html; charset=UTF-8", "HTTP control disabled");
    }
  });

  server.on("/setfreq", []() {
    if (mode == "http") {
     if ((server.method() == HTTP_POST) && (server.argName(0) == "freq")) {
       String freq = server.arg(0);
       httpSetFreq(freq);
     } else {
        server.send(405, "text/html; charset=UTF-8", "Must send a POST with argument 'freq' and a value");
     }
    } else {
      server.send(403, "text/html; charset=UTF-8", "HTTP control disabled");
    }
  });

  server.on("/setrigmode", []() {
    if (mode == "http") {
     if ((server.method() == HTTP_POST) && (server.argName(0) == "mode")) {
       String rigmode = server.arg(0);
       httpSetRigMode(rigmode);
     } else {
        server.send(405, "text/html; charset=UTF-8", "Must send a POST with argument 'mode' and a value");
     }
    } else {
      server.send(403, "text/html; charset=UTF-8", "HTTP control disabled");
    }
  });

  server.on("/setmqtt", []() {
    if ((server.method() == HTTP_POST) && (server.argName(0) == "mqtt")) {
       httpSetMQTT(server.arg(0));
     } else {
        server.send(405, "text/html; charset=UTF-8", "Must send a POST with argument 'mqtt' and a value");
     }
  });

  server.on("/setrigctl", []() {
    if (((server.method() == HTTP_POST) && (server.argName(0) == "address") && (server.argName(1) == "port"))) {
       setRigctlAddress(server.arg(0));
       setRigctlPort(server.arg(1));
       httpGetRigctlServer();
     } else {
        server.send(405, "text/html; charset=UTF-8", "Must send a POST with arguments 'address' and 'port' with values");
     }
  });

  
  server.on("/setrigctlfreq", []() {
    if (mode == "rigctl") {
     if ((server.method() == HTTP_POST) && (server.argName(0) == "freq")) {
       String freq = server.arg(0);
       httpSetRigctlFreq(freq);
     } else {
        server.send(405, "text/html; charset=UTF-8", "Must send a POST with argument 'freq' and a value");
     }
    } else {
      server.send(403, "text/html; charset=UTF-8", "rigctl is not current mode");
    }
  });

  server.on("/setrigctlmode", []() {
    if (mode == "rigctl") {
     if ((server.method() == HTTP_POST) && (server.argName(0) == "mode")) {
       String mode = server.arg(0);
       httpSetRigctlMode(mode);
     } else {
        server.send(405, "text/html; charset=UTF-8", "Must send a POST with argument 'mode' and a value");
     }
    } else {
      server.send(403, "text/html; charset=UTF-8", "rigctl is not current mode");
    }
  });

  server.on("/setrigctlptt", []() {
    if (mode == "rigctl") {
     if ((server.method() == HTTP_POST) && (server.argName(0) == "ptt")) {
       String ptt = server.arg(0);
       httpSetRigctlPtt(ptt);
     } else {
        server.send(405, "text/html; charset=UTF-8", "Must send a POST with argument 'ptt' and a value");
     }
    } else {
      server.send(403, "text/html; charset=UTF-8", "rigctl is not current mode");
    }
  });

  server.onNotFound(handleNotFound);

  webServer(true);
  setMode(default_mode);
  Serial.print("band ");
  Serial.println(current_band);
  Serial.print("frequency ");
  Serial.println(frequency);
  Serial.print("state ");
  Serial.println(current_state);
}

void loop(void) {

  if (wifi_enabled == true) {
    wifi("enable");
  } else {
    wifi("disable");
  }
   
  server.handleClient();
  MDNS.update();

  mqttConnect();
  pubsubClient.loop();

 if (Serial.available()) {
   serialValue = Serial.readStringUntil(serialEOL);
   String command = getValue(serialValue,' ',0);
   String value = getValue(serialValue,' ',1);
   if (command == "setmode") {
     setMode(value);
   } else if ((command == "setband") && (mode == "serial")) {
     setBand(value);
   } else if ((command == "setfreq") && (mode == "serial")) {
     setFreq(value);
   } else if ((command == "setrigmode") && (mode == "serial")) {
     setRigMode(value);
   } else if ((command == "setrigctlfreq") && (mode == "rigctl")) {
     setRigctlFreq(value);
   } else if ((command == "setrigctlmode") && (mode == "rigctl")) {
     setRigctlMode(value);
   } else if ((command == "setrigctlptt") && (mode == "rigctl")) {
     setRigctlPtt(value);
   } else if (((command == "setstate") && (mode == "serial") && (hybrid == false))) {
     setState(value);
   } else if (command == "setmqtt") {
     setMQTT(value);
   } else if (command == "setrigctl") {
    setRigctlAddress(getValue(serialValue,' ',1));
    setRigctlPort(getValue(serialValue,' ',2));
    Serial.println("rigctl_server " + getRigctlServer());
   } else if (command == "serialonly") {
     if (value == "true") {
      serialonly = true;
      setMode("serial");
      wifi_enabled = false;
     } else if (value == "false") {
      serialonly = false;
      wifi_enabled = true;
     }
   } 
 }  

 if ((hybrid == true || mode == "yaesu" || mode == "yaesu817" || mode == "icom" || mode == "sunsdr") && (mode != "none")) {
  delay(10); // digital pin needs time to settle between reads
  rx_state = digitalRead(tx_pin);
  if ((rx_state == LOW) && (serialonly == false)) {
      current_yaesu_rx = true;
      if (current_yaesu_rx != previous_yaesu_rx) {
        setState("tx");
        previous_yaesu_rx = true;
      }
  } else if ((rx_state == HIGH) && (serialonly == false)) {
     current_yaesu_rx = false;
     if (current_yaesu_rx != previous_yaesu_rx) {
       setState("rx");
       previous_yaesu_rx = false;
     }
   }
 }

 if ((mode == "yaesu817") && (serialonly == false)) {
    delay(10); // ADC needs time to settle between reads
    yaesu_band_voltage = analogRead(yaesu_band_pin);
    setBandVoltage(yaesu_band_voltage);
 }

 // mqtt subscribed messages
 if(Rflag) {
   //Serial.println("MQTT message recieved");
   // reset Rflag
   Rflag=false;
   // convert from char array to String
   String topic = String(Topic);

   topic.remove(0,8); // strip "xpa125b/" from topic
   
   if (topic == "setband") {
     if (mode == "mqtt") {
        setBand(message);
     }
   } else if ( topic == "setfreq" ) {
     if (mode == "mqtt") {
        setFreq(message);
     }
   } else if ((((topic == "setstate") && (mode == "mqtt") && (hybrid == false)))) {
     setState(message);
   } else if ( topic == "setmode" ) {
     setMode(message);
   } else if ((topic == "setrigmode") && (mode == "mqtt")) {
     setRigMode(message);
   } else if ((topic == "setrigctlfreq") && (mode == "rigctl")) {
     setRigctlFreq(message);
   } else if ((topic == "setrigctlmode") && (mode == "rigctl")) {
     setRigctlMode(message);
   } else if ((topic == "setrigctlptt") && (mode == "rigctl")) {
     setRigctlPtt(message);
   }
 }

 if ( curState == "tx" ) {
  current_tx_millis = millis();
  int difference = (current_tx_millis - previous_tx_millis);
  int second = (difference / 1000);
  if (second != tx_previous_seconds) {
    tx_seconds++;
    tx_seconds_true = (tx_seconds - 1);
    char charSeconds[4];
    String strSeconds = String(tx_seconds_true);
    strSeconds.toCharArray(charSeconds, 4);
    if (pubsubClient.connected()) pubsubClient.publish("xpa125b/txtime", charSeconds, false);
    Serial.print("txtime ");
    Serial.println(charSeconds);
    tx_previous_seconds=second;
  }
 } else {
  tx_seconds=0;
  tx_seconds_true=0;
  if ( tx_seconds != tx_previous_seconds ) {
    if (pubsubClient.connected()) pubsubClient.publish("xpa125b/txtime", "0", false);
    tx_previous_seconds=0;
  }
  previous_tx_millis = current_tx_millis;
 }

 if ( tx_seconds >= tx_limit ) {
  Serial.println("txblocktimer start");
  setState("rx");
  if (mode == "rigctl") {
    setRigctlPtt("0");
  }
  tx_block_timer = tx_block_time; // set block timer 
 }
 
 if ( tx_block_timer >= 1 ) {
   current_block_millis = millis();
   int difference = (current_block_millis - previous_block_millis);
   int second = (difference / 1000);
   if (second != tx_block_previous_seconds) {
    tx_block_timer--;
    tx_block_seconds = tx_block_timer;
    tx_block_previous_seconds=second;
    tx_block_seconds_true = (tx_block_seconds + 1);
    char charSeconds[4];
    String strSeconds = String(tx_block_seconds_true);
    strSeconds.toCharArray(charSeconds, 4);
    if (pubsubClient.connected()) pubsubClient.publish("xpa125b/txblocktimer", charSeconds, false);
    Serial.print("txblocktimer ");
    Serial.println(charSeconds);
    if ( tx_block_timer == 0 ) {
    Serial.println("txblocktimer end");
    if (pubsubClient.connected()) pubsubClient.publish("xpa125b/txblocktimer", "0", false);
    } 
   }
 } else {
   tx_block_seconds=0;
   tx_block_seconds_true=0;
   previous_block_millis = current_block_millis; 
 }

 if ((mode == "rigctl") && (wifi_enabled == true)) {
  String f_result=sendRigctlCommand("f");
  if (f_result != "error") {
    setFreq(f_result);
  }
  String m_result=sendRigctlCommand("m");
  if (m_result != "error") {
    setRigMode(m_result);
  }

  if (hybrid == false) {
    String t_result=sendRigctlCommand("t");
    if (t_result == "1") {
      current_rigctl_rx = false;
      if (current_rigctl_rx != previous_rigctl_rx) {
        setState("tx");
        previous_rigctl_rx = false;
      }
    } else if (t_result == "0") {
      current_rigctl_rx = true;
      if (current_rigctl_rx != previous_rigctl_rx) {
        setState("rx");
        previous_rigctl_rx = true;
      }
    }
  }
 } 

 // Change the Bluetooth Data setting on the IC-705 to CIV Data (Echo Back) and connect the controller ('XPA128B') in the Bluetooth menu. Security code: 1234.
 // The BT name of the HC-05 is set with 'AT+NAME=XPA128B' and password with 'AT+PSWD="12345"'
 // There is a button near the EN pin of HC-05 module. Press that button, apply power and then release. You are now in AT command mode.
 // Continuous blinking of LED indicates it is in data mode and delayed blinking (2 seconds) indicates the module is in AT command mode.
 if ((mode == "icom") && (hl_05_enabled == true)) {
  if (BTserial.available())
  {  
      int bt_c = BTserial.read() & 0x00ff;
      //Serial.write(bt_c);
      //sprintf(ser_buffer, "%u\n", bt_c);
      //Serial.write(ser_buffer);
 
      if (bt_c == 0xfe){
        bt_com = 1;
        bt_ptr = 0;
      }
      else if(bt_c == 0xfd){
        bt_com = 2;
      }
      else if (bt_com == 1){
        bt_buffer[bt_ptr] = bt_c;
        if (++bt_ptr > 31) bt_ptr = 31;
      }
  }
  if (bt_com == 2){
    bt_com = 0;
    bt_ptr = 0;
    if (bt_buffer[2] == 0){
      bt_kHz = (bt_buffer[4] & 0xf0) >> 4;
      bt_kHz += 10 * (bt_buffer[5] & 0x0f);
      bt_kHz += 100 * ((bt_buffer[5] & 0xf0) >> 4);
      bt_MHz = bt_buffer[6] & 0x0f;
      bt_MHz += 10 * ((bt_buffer[6] & 0xf0) >> 4);
      bt_MHz += 100 * (bt_buffer[7] & 0x0f);
      bt_Hz = bt_MHz * 1000 + bt_kHz;
      bt_Hz = bt_Hz * 1000;
      bt_freq = String(bt_Hz);
      setFreq(bt_freq);
    }
  }
 }

 if ((mode == "sundsr") && (serialonly == false)) {
  delay(10);
  int x3 = digitalRead(band_data_1);
  int x4 = digitalRead(band_data_2);
  int x5 = digitalRead(band_data_3);
  int x6 = digitalRead(band_data_4);
  
  if ((x3 == LOW) && (x4 == LOW) && (x5 == LOW) && (x6 == HIGH)) {
    setBand("160");
  } else if ((x3 == LOW) && (x4 == LOW) && (x5 == HIGH) && (x6 == LOW)) {
    setBand("80");
  } else if ((x3 == LOW) && (x4 == LOW) && (x5 == HIGH) && (x6 == HIGH)) {
    setBand("60");
  } else if ((x3 == LOW) && (x4 == HIGH) && (x5 == LOW) && (x6 == LOW)) {
    setBand("40");
  } else if ((x3 == LOW) && (x4 == HIGH) && (x5 == LOW) && (x6 == HIGH)) {
    setBand("30");
  } else if ((x3 == LOW) && (x4 == HIGH) && (x5 == HIGH) && (x6 == LOW)) {
    setBand("20");
  } else if ((x3 == LOW) && (x4 == HIGH) && (x5 == HIGH) && (x6 == HIGH)) {
    setBand("17");
  } else if ((x3 == HIGH) && (x4 == LOW) && (x5 == LOW) && (x6 == LOW)) {
    setBand("15");
  } else if ((x3 == HIGH) && (x4 == LOW) && (x5 == LOW) && (x6 == HIGH)) {
    setBand("12");
  } else if ((x3 == HIGH) && (x4 == LOW) && (x5 == HIGH) && (x6 == LOW)) {
    setBand("10");
  } else if ((x3 == HIGH) && (x4 == LOW) && (x5 == HIGH) && (x6 == HIGH)) {
    setBand("6");
  } 
 }

 if ((mode == "yaesu") && (serialonly == false)) {
  delay(10);
  int A = digitalRead(band_data_1);
  int B = digitalRead(band_data_2);
  int C = digitalRead(band_data_3);
  int D = digitalRead(band_data_4);
  
  if ((A == HIGH) && (B == LOW) && (C == LOW) && (D == LOW)) {
    setBand("160");
  } else if ((A == LOW) && (B == HIGH) && (C == LOW) && (D == LOW)) {
    setBand("80");
  } else if ((A == HIGH) && (B == HIGH) && (C == LOW) && (D == LOW)) {
    setBand("60");
  } else if ((A == HIGH) && (B == HIGH) && (C == LOW) && (D == LOW)) {
    setBand("40");
  } else if ((A == LOW) && (B == LOW) && (C == HIGH) && (D == LOW)) {
    setBand("30");
  } else if ((A == HIGH) && (B == LOW) && (C == HIGH) && (D == LOW)) {
    setBand("20");
  } else if ((A == LOW) && (B == HIGH) && (C == HIGH) && (D == LOW)) {
    setBand("17");
  } else if ((A == HIGH) && (B == HIGH) && (C == HIGH) && (D == LOW)) {
    setBand("15");
  } else if ((A == LOW) && (B == LOW) && (C == LOW) && (D == HIGH)) {
    setBand("12");
  } else if ((A == HIGH) && (B == LOW) && (C == LOW) && (D == HIGH)) {
    setBand("10");
  } else if ((A == LOW) && (B == HIGH) && (C == LOW) && (D == HIGH)) {
    setBand("6");
  } 
 }

 if ((mode == "elecraft") && (serialonly == false)) {
  
  MAX3232.print("FA;"); // request VFO A frequency
  elecraft_response = MAX3232serialRead(';');
  elecraft_value = get_elecraft_value(elecraft_response);
  setFreq(String(elecraft_value));

  MAX3232.print("MD;"); // request VFO A mode
  String response_mode;
  elecraft_response = MAX3232serialRead(';');
  elecraft_value = get_elecraft_value(elecraft_response);
  
  switch (elecraft_value) {
      case 1:
        response_mode = "LSB";
        break;
      case 2:
        response_mode = "USB";
        break;
      case 3:
        response_mode = "CW";
        break;
      case 4:
        response_mode = "FM";
        break;
      case 5:
        response_mode = "AM";
        break;
      case 6:
        response_mode = "DATA";
        break;
      case 7:
        response_mode = "CW-REV";
        break;
      // no '8' for whatever reason
      case 9:
        response_mode = "DATA-REV";
        break;
      default:
        response_mode = "unknown";
        break;
  }
  setRigMode(response_mode);

  MAX3232.print("TQ;"); // request transmit state
  elecraft_response = MAX3232serialRead(';');
  elecraft_value = get_elecraft_value(elecraft_response);
  if ( elecraft_value == 1 ) {
    setState("tx");
  } else {
    setState("rx");
  }
 }
}