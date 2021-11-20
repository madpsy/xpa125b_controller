// *********** START CONFIG ***********

// WiFi config
const char* ssid = "";
const char* password = "";

// MQTT config
bool mqtt_enabled = false;
const char* mqttserver = "";
const char* mqttuser = "";
const char* mqttpass = "";

// default mode
String mode = "analogue";

// TX blocker config in seconds
int tx_limit = 300; // 300 = 5 mins 
int tx_block_time = 60; // 60 = 1 minute

// rigctl timeout in milliseconds
int rigctl_timeout = 250;

// *********** END CONFIG ***********

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <Regexp.h>

ESP8266WebServer server(80);

WiFiClient mqttClient;
WiFiClient rigctlClient;
PubSubClient pubsubClient;

int ptt_pin = D1;
int band_pin = D2;
int tx_pin = D3;
int yaesu_band_pin = A0;

String serialValue;
char serialEOL = '\n';

int current_band = 0;
int previous_band = 0;
int tx_timer = 0;
int tx_seconds = 0;
int tx_previous_seconds = 0;
int tx_block_timer = 0;
int tx_block_seconds = 0;
int tx_block_previous_seconds = 0;
int yaesu_band_voltage = 0;
unsigned long current_tx_millis = 0;
unsigned long previous_tx_millis = 0;
unsigned long current_block_millis = 0;
unsigned long previous_block_millis = 0;
bool rx_state = true;
bool previous_state = 0;
bool current_state = 0;
bool serialonly = 0;
bool current_analogue_rx = 0;
bool previous_analogue_rx = 0;
bool current_rigctl_rx = 0;
bool previous_rigctl_rx = 0;
bool rigctl_address_set = false;
bool rigctl_port_set = false;
String frequency = "0";
String previous_frequency = "0";
String rigctl_address;
String rigctl_port;
IPAddress remote_ip;
IPAddress rigctl_ipaddress;
int rigctl_portnumber;

String curState = "rx";
String curBand = "0";

char* Topic;
byte* buffer;
String message;
boolean Rflag=false;
int r_len;

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
  // pubsubClient.connected condition here prevents a crash which can happen if already connected
  if (((mqtt_enabled == true) && (!pubsubClient.connected()) && (WiFi.status() == WL_CONNECTED))) {
    delay(100); // stops it trying to reconnect as fast as the loop can go
    Serial.println("Attempting MQTT connection");
    if (pubsubClient.connect("xpa125b", mqttuser, mqttpass)) {
      Serial.println("MQTT connected"); 
      pubsubClient.subscribe("xpa125b/#");
    } else {
      Serial.println(pubsubClient.state());
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
  if ((rigctl_address_set) && (rigctl_port_set)) {
   if (rigctlClient.connect(rigctl_ipaddress, rigctl_portnumber)) {
    rigctlClient.stop();
    Serial.print("connection to rigctl server succeeded ");
    Serial.println(rigctl_address + ":" + rigctl_port);
    return true;
   } else {
    Serial.print("connection to rigctl server failed ");
    Serial.println(rigctl_address + ":" + rigctl_port);
    return false;
   }
  } else {
    Serial.println("rigctl server not set");
    return false;
  }
}

String sendRigctlCommand(char* command) {
    
  if ((rigctl_address_set) && (rigctl_port_set)) {
    if (!rigctlClient.connect(rigctl_ipaddress, rigctl_portnumber)) {
      Serial.println("rigctl connection failed");
    } else {
      rigctlClient.print(command);
      rigctlClient.print("\n");
    
      unsigned long timeout = millis();
      while (rigctlClient.available() == 0) {
       if (millis() - timeout > rigctl_timeout) {
         //Serial.print("rigctl timeout for command: ");
         //Serial.println(command);
         rigctlClient.stop();
         return("error");
        }
     }

      String response;
      while(rigctlClient.available()){
        char ch = static_cast<char>(rigctlClient.read());
       response += String(ch);
      }
      //rigctlClient.stop();
      int length = response.length();
      response.remove(length - 1, 1);
      return(response);
    }
  }
}

void handleRoot() {
  String message = "<html><head><title>XPA125B</title></head><body>";
  message += "<script>";
  message += "var xhr = new XMLHttpRequest();";
  message += "function mouseDown() {";
  message += "document.getElementById('demo').value = 'PTT TX';";
  message += "xhr.open('POST', '/setstate', true);";
  message += "xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded; charset=UTF-8');";
  message += "xhr.send('state=tx');";
  message += "}";
  message += "function mouseUp() {";
  message += "document.getElementById('demo').value = 'PTT RX';";
  message += "xhr.open('POST', '/setstate', true);";
  message += "xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded; charset=UTF-8');";
  message += "xhr.send('state=rx');";
  message += "}";
  message += "</script>";
  message += "Xiegu XPA125B Controller</br></br>";
  message += "<form action='/setmode' method='post' target='response'>";
  message += "<select name='mode'>";
  message += "<option value='rigctl'>Rigctl</option>";
  message += "<option value='analogue'>Analogue</option>";
  message += "<option value='serial'>Serial</option>";
  message += "<option value='http'>HTTP</option>";
  message += "<option value='mqtt'>MQTT</option>";
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
  message +="<input id='demo' type='button' value='PTT RX' onmouseup='mouseUp();' onmousedown='mouseDown();'></br></br>";
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
  message += "Last Response: ";
  message += "<iframe name='response' id='response' scrolling='no' frameBorder='0' width=400 height=25></iframe></br>";
  message += "Current State: ";
  message += "<iframe src='/status' scrolling='no' frameBorder='0' width=700 height=25></iframe>";
  message += "</br></br>";
  message += "Valid serial commands (115200 baud):</br></br>";
  message += "serialonly [true|false] (disables analogue and wifi entirely)</br>";
  message += "setmode [analogue|serial|http|mqtt|rigctl]</br>";
  message += "setstate [rx|tx]</br>";
  message += "setband [160|80|60|40|30|20|17|15|12|11|10]</br>";
  message += "setfreq [frequency in Hz]</br>";
  message += "setmqtt [enable|disable]</br>";
  message += "setrigctl [address] [port]</br></br>";
  message += "Valid HTTP POST paths:</br></br>";
  message += "/setmode mode=[analogue|serial|http|mqtt|rigctl]</br>";
  message += "/setstate state=[rx|tx]</br>";
  message += "/setband band=[160|80|60|40|30|20|17|15|12|11|10]</br>";
  message += "/setfreq freq=[frequency in Hz]</br>";
  message += "/setmqtt mqtt=[enable|disable] (only available via http)</br>";
  message += "/setrigctl address=[rigctl IP address] port=[rigctl port] (http only)</br></br>";
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
  message += "When serialonly is enabled neither http/mqtt (wifi is disabled) nor analogue can be used</br>";
  message += "You can always use 'setmode' with serial/http/mqtt reguardless of current mode except when serialonly is enabled, in which case it only works via serial</br>";
  message += "In analogue mode only the Yaesu standard voltage input is used for band selection and rx/tx is only via the control cable (default mode on boot)</br>";
  message += "In serial mode we only accept band/freq selection and rx/tx via serial</br>";
  message += "In mqtt mode we only accept band/freq selection and rx/tx via mqtt messages</br>";
  message += "In http mode we only accept band/freq selection and rx/tx via http messages</br>";
  message += "In rigctl mode we only accept band/freq selection and rx/tx via rigctl (server connection must succeed for this mode to activate)</br></br>";
  message += "Example rigctld run command (TS-2000 has ID 2014):</br></br>";
  message += "rigctld.exe -r COM18 -m 2014 -s 57600 -t 51111</br></br>";
  message += "If MQTT is disabled and the mode is changed to MQTT then it will be automatically enabled</br></br>";
  message += "If TX time exceeds ";
  message += (tx_limit / 100);
  message += " seconds then TX will be blocked for ";
  message += (tx_block_time / 100);
  message += " seconds. After the block releases you must send another TX event to start again - this includes analogue (i.e. release PTT). Note that 'seconds' is only rough due to non-exact timing in the code.</br></br>";
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
  message += "&nbsp TX Time: ";
  message += tx_seconds;
  message += "&nbsp TX Blocker: ";
  message += tx_block_seconds;
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
  String seconds = String(tx_seconds);
  server.send(200, "text/plain; charset=UTF-8", seconds);
}

void getTxBlockTimer() {
  String seconds = String(tx_block_seconds);
  server.send(200, "text/plain; charset=UTF-8", seconds);
}

void setMode(String value) {
   if ((value == "rigctl") && (!testRigctlServer())) {
    return;
   }
   mode = value;
   frequency = "0";
   char charMode[9];
   mode.toCharArray(charMode, 9);
   Serial.print("mode ");
   Serial.println(mode);
   pubsubClient.publish("xpa125b/mode", charMode, true);
   if ((mode == "mqtt") && (mqtt_enabled == 0)) {
     setMQTT("enable");
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

    if ( bandInt == 160 ) {
      pwm_value = 5;
    } else if ( bandInt == 80 ) {
      pwm_value = 40;
    } else if ( bandInt == 60 ) {
      pwm_value = 70;
    } else if ( bandInt == 40 ) {
      pwm_value = 95;
    } else if ( bandInt == 30 ) {
      pwm_value = 120;
    } else if ( bandInt == 20 ) {
      pwm_value = 150;
    } else if ( bandInt == 17 ) {
      pwm_value = 180;
    } else if ( bandInt == 15 ) {
      pwm_value = 210;
    } else if ( bandInt == 12 ) {
      pwm_value = 230;
    } else if ( bandInt == 11 ) {
      pwm_value = 255;
    } else if ( bandInt == 10 ) {
      pwm_value = 255;
    } else {
      pwm_value = 0;
    }

    analogWrite(band_pin, pwm_value);
    current_band = bandInt;
    if ( current_band != previous_band ) {
      Serial.print("band ");
      Serial.println(bandChar);
      pubsubClient.publish("xpa125b/band", bandChar, true);
    }
    previous_band = bandInt; 
}

void setBandVoltage(int voltage) {
  if ((voltage >= 5) && (voltage <= 20)) {
    setBand("160");
  } else if ((voltage >= 20) && (voltage <= 30)) {
    setBand("80");
  } else if ((voltage >= 30) && (voltage <= 40)) {
    setBand("60");
  } else if ((voltage >= 40) && (voltage <= 50)) {
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
 if ((freq != previous_frequency) && (freq != "error" )) {  
   frequency = freq;
   char charFreq[10];
   freq.toCharArray(charFreq, 10);
   pubsubClient.publish("xpa125b/frequency", charFreq, false);
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
      pubsubClient.publish("xpa125b/state", "rx");
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
      pubsubClient.publish("xpa125b/state", "tx");
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
    } else {
      Serial.println("\nWiFi failed to connect");
    }
    if (MDNS.begin("xpa125b")) {
      Serial.println("MDNS responder started as xpa125b[.local]");
    }
    mqttConnect();
  } else if (state == "disable") {
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disabled");
  }
}

void setup(void) {
  Serial.begin(115200);
  //Serial.setDebugOutput(true);
  Serial.println("");

  wifi("enable");
  
  // set to 160m and 0Hz by default
  setBand("160");
  setFreq("0");

  pinMode(band_pin, OUTPUT);
  pinMode(ptt_pin, OUTPUT);
  pinMode(tx_pin, INPUT);
  pinMode(yaesu_band_pin, INPUT);
  
  analogWriteFreq(30000);

  if (mqtt_enabled == true) {
    pubsubClient.subscribe("xpa125b/#");
    // update mqtt so it knows we are on the default startup state of RX
    pubsubClient.publish("xpa125b/state", "rx", true);
    // update mqtt so it knows we are on the default startup band of 160m
    pubsubClient.publish("xpa125b/band", "160", true);
    // update mqtt so it knows we are on the default startup mode of analogue
    pubsubClient.publish("xpa125b/mode", "analogue", true);
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
  

  server.on("/setmode", []() {
    if ((server.method() == HTTP_POST) && (server.argName(0) == "mode")) {
       httpSetMode(server.arg(0)); 
    } else {
      server.send(405, "text/html; charset=UTF-8", "Must send a POST with argument 'mode' and a value");
    }
  });
  
  server.on("/setstate", []() {
    if (mode == "http") {
      if ((server.method() == HTTP_POST) && (server.argName(0) == "state")) {
        String state = server.arg(0);
        setState(state);
        server.send(200, "text/html; charset=UTF-8", state);
      } else {
        server.send(405, "text/html; charset=UTF-8", "Must send a POST with argument 'state' and a value");
      }
    } else {
      server.send(403, "text/html; charset=UTF-8", "HTTP control disabled");
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

  server.onNotFound(handleNotFound);

  webServer(true);

  Serial.print("mode ");
  Serial.println(mode);
  Serial.print("band ");
  Serial.println(current_band);
  Serial.print("frequency ");
  Serial.println(frequency);
  Serial.print("state ");
  Serial.println(current_state);

  // update mqtt with txtime / txblocktimer to 0 on start
  pubsubClient.publish("xpa125b/txtime", "0", false);
  pubsubClient.publish("xpa125b/txblocktimer", "0", false);

}

void loop(void) {
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
   } else if ((command == "setstate") && (mode == "serial")) {
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
      wifi("disable");
     } else if (value == "false") {
      serialonly = false;
      wifi("enable");
     }
   } 
 }  

 rx_state = digitalRead(tx_pin);
 if (((rx_state == LOW) && (mode == "analogue") && (serialonly == false))) {
    current_analogue_rx = true;
    if (current_analogue_rx != previous_analogue_rx) {
      setState("tx");
      previous_analogue_rx = true;
    }
 } else if (((rx_state == HIGH) && (mode == "analogue") && (serialonly == false))) {
    current_analogue_rx = false;
    if (current_analogue_rx != previous_analogue_rx) {
      setState("rx");
      previous_analogue_rx = false;
    }
 }

 yaesu_band_voltage = analogRead(yaesu_band_pin);
 if ((mode == "analogue") && (serialonly == false)) {
    setBandVoltage(yaesu_band_voltage);
 }

 // mqtt subscribed messages
 if(Rflag) {
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
   } else if ((topic == "setstate") && (mode == "mqtt")) {
     setState(message);
   } else if ( topic == "setmode" ) {
     setMode(message);
   }
 }

 if ( curState == "tx" ) {
  current_tx_millis = millis();
  int difference = (current_tx_millis - previous_tx_millis);
  int second = (difference / 1000);
  if (second != tx_previous_seconds) {
    tx_seconds++;
    char charSeconds[4];
    String strSeconds = String(tx_seconds);
    strSeconds.toCharArray(charSeconds, 4);
    pubsubClient.publish("xpa125b/txtime", charSeconds, false);
    tx_previous_seconds=second;
  }
 } else {
  tx_seconds=0;
  if ( tx_seconds != tx_previous_seconds ) {
    pubsubClient.publish("xpa125b/txtime", "0", false);
  }
  previous_tx_millis = current_tx_millis;
 }

 if ( tx_seconds >= tx_limit ) {
  Serial.println("txblocktimer start");
  setState("rx");
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
    char charSeconds[4];
    String strSeconds = String(tx_block_seconds);
    strSeconds.toCharArray(charSeconds, 4);
    pubsubClient.publish("xpa125b/txblocktimer", charSeconds, false);
    if ( tx_block_timer == 0 ) {
    Serial.println("txblocktimer end");
    } 
   }
 } else {
   tx_block_seconds=0;
   previous_block_millis = current_block_millis; 
 }

 if (mode == "rigctl") {
  setFreq(sendRigctlCommand("f"));
  String result=sendRigctlCommand("t");
  if (result == "1") {
    current_rigctl_rx = false;
    if (current_rigctl_rx != previous_rigctl_rx) {
      setState("tx");
      previous_rigctl_rx = false;
    }
  } else if (result == "0") {
    current_rigctl_rx = true;
    if (current_rigctl_rx != previous_rigctl_rx) {
      setState("rx");
      previous_rigctl_rx = true;
    }
  }
 }
 
 delay(10); // so we dont go full throttle
}
