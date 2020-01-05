// EspMqttDHTdallasPIRbeep
// main.ino
// 6/21/2019
// by: Truglodite
//
// Connects to mqtt broker, pubs temp, humidity, and pir @qos 0, and subs to
// ota & beep @qos 1. User configs are in configuration.h... comment out the
// line below!!!
////////////////////////////////////////////////////////////////////////////////
#define privacy // uncomment, or rename configuration.h to privacy.h (4github).

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServerSecure.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
extern "C" {
  #include "user_interface.h"
}
#include <avr/pgmspace.h>

#ifdef privacy
  #include "privacy.h"
#endif
#ifndef privacy
  #include "configuration.h"
#endif

// Globals /////////////////////////////////////////////////////////////////////
bool firmwareUp = 0;                 // boolsh!t
bool beeperActive = 0;
bool connectTimerFlag = 0;
bool otaMessageSent = 0;
bool subbed = 0;
bool ipAddressSet = 0;
bool isPirSet = 0;
bool pirBroker = 0;
bool pirLocal = 0;

unsigned long dhtReadTime = 0;       // Somewhere in time...
unsigned long dallasReadTime = 0;
unsigned long lastPubTime = 0;
unsigned long connectStartTime = 0;
unsigned long otaStartTime = 0;
unsigned long lastPirHigh = 0;
unsigned long beepEventTime = 0;
unsigned long subStartTime = 0;

float humidA = 0.0;                  // big fat f's
float humid = 0.0;
float dallasTemp = 0.0;
float dhtSamples = 0.0;
float dallasSamples=0.0;

u_int retriesDHT = 0;                // never negative (whole) numbers
u_int nBeeps = 0;
u_int state = 0;
u_int pubCount = 0;

// stringy mess of charred o's
char otaTopic[sizeof(mqttClientID) + 1 + sizeof(otaTopicD) + 1] = {0};
char pirTopic[sizeof(mqttClientID) + 1 + sizeof(pirTopicD) + 1] = {0};
char tempTopic[sizeof(mqttClientID) + 1 + sizeof(tempTopicD) + 1] = {0};
char humidTopic[sizeof(mqttClientID) + 1 + sizeof(humidTopicD) + 1] = {0};
char beeperTopic[sizeof(mqttClientID) + 1 + sizeof(beeperTopicD) + 1] = {0};
char notifyOTAready[sizeof(notifyOTAreadyB) + 45 + sizeof(update_path) + 1] = {0};

DeviceAddress dallasAddress;         // others
IPAddress myIP;

//BearSSL::ESP8266WebServerSecure httpServer(443);  // deprecated
ESP8266WebServerSecure httpServer(443);  // secure web server init
ESP8266HTTPUpdateServerSecure httpUpdater; // Browser OTA init
WiFiClient espClient;                // Wifi-PubSubClient init

DHT dht(dhtPin,dhtType);             // DHT init
OneWire oneWire(dallasPin);          // OneWire init
DallasTemperature dallas(&oneWire);  // Dallas sensors init

// Mosquitto callback (subs) ///////////////////////////////////////////////////
void callback(char* topic, byte* payload, unsigned int length) {
  #ifdef debug
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  #endif

  // "otaTopic"
  if(!strcmp(topic, otaTopic)) {
    if((char)payload[0] == '1') {  // first letter is 1
      #ifdef debug
        Serial.println("Turning on OTA!");
      #endif
      firmwareUp = 1;
    }
    else if((char)payload[0] == '0') {
      #ifdef debug
        Serial.println("Turning off OTA!");
      #endif
      firmwareUp = 0;
    }
  }
  // "beeperTopic: N" (N is the first payload character)
  else if(!strcmp(topic, beeperTopic)) {
    // convert byte* payload to a useable int
    char format[16];
    snprintf(format, sizeof format, "%%%ud", length);
    int payload_value = 0;
    if (sscanf((const char *) payload, format, &payload_value) == 1)  {
      #ifdef debug
        Serial.print("Recieved beeps: ");
        Serial.println(payload_value);
      #endif
    }
    else  {
      #ifdef debug
        Serial.println("Beeper int conversion error");
      #endif
    }
    nBeeps = payload_value;
  }
  // "pirTopic: 1"
  else if(!strcmp(topic, pirTopic)) {
    if((char)payload[0] == '1') {
      pirBroker = 1;
      isPirSet = 1;
    }
    else if((char)payload[0] == '0') {
      pirBroker = 0;
      isPirSet = 1;
    }
  }
  return;
}

PubSubClient client(brokerIP, brokerPort, callback, espClient);  // ANNOYING!!!

// Read DHT (non-blocking) /////////////////////////////////////////////////////
void readDHT()  {
  // DHT read and print
  #ifdef debug
    Serial.println("Reading DHT Sensor...");
  #endif
  humidA = dht.readHumidity();        // RH %
  dhtReadTime = millis();
  if(isnan(humidA)) {// Drop the reading(s) if we get any NAN's
    retriesDHT ++;
    #ifdef debug
      Serial.print("DHT Read Fail... fails in a row: ");
      Serial.println(retriesDHT);
    #endif
    if(retriesDHT > dhtRetriesMax)  { // Too many NAN's in a row, reboot...
      #ifdef debug
        Serial.println("Too many NANs, sending notification and restarting...");
      #endif
      if(!client.publish(humidTopic,notifyDHTfail))  {
        #ifdef debug
          Serial.println("dht_nan pub failed");
        #endif
        ESP.restart();
        yield();
      }
      ESP.restart();
      delay(500);
    }
    return;
  }
  else {                              // Cool, no NAN's, count it!
    retriesDHT = 0;
    // Overflow safety
    if(MAXFLOAT - humid > humidA) {
      humid += humidA;
      dhtSamples++;
    }
    else {  // overflow
      humid = humidA;
      dhtSamples=1;
    }
    #ifdef debug
      Serial.print("DHT ReadTime: ");
      Serial.print(dhtReadTime);
      Serial.println(" msec");
      Serial.print("DHT Samples: ");
      Serial.println(dhtSamples);
      Serial.print("R.H. Sum: ");
      Serial.print(humid);
      Serial.println(" %");
    #endif
  }
  return;
}

// Dallas temp readings (non-blocking) ////////////////////////////////////////
void readDallas()  {
  #ifdef debug
    Serial.println("Reading Dallas temp...");
  #endif
  float tempTemp = dallas.getTempCByIndex(0);
  // Overflow safety
  if(MAXFLOAT - dallasTemp > tempTemp) {
    dallasTemp += tempTemp;// Read temp conversion
    dallasSamples++;
  }
  else {  // overflow
    dallasTemp = tempTemp;
    dallasSamples=1;
  }
  dallas.requestTemperatures();      // Send command to start next reading & conversion
  dallasReadTime = millis();
  #ifdef debug
    Serial.print("Dallas ReadTime: ");
    Serial.print(dallasReadTime);
    Serial.println(" msec");
    Serial.print("Dallas Samples: ");
    Serial.println(dallasSamples);
    Serial.print("Dallas Sum: ");
    Serial.print(dallasTemp);
    Serial.println(" C");
  #endif
  return;
}

// Publish data to server //////////////////////////////////////////////////////
void publishData() {
  #ifdef debug
    Serial.print("Sending data to broker... ");
  #endif
  // Convert floats to strings
  // dtostrf(floatIn, minWidth, digitsAfterDecimal, charOut);
  char humidStr[7] = {0};  // declare strings
  char dallasTempStr[7] = {0};
  dtostrf(humid, 4, 1, humidStr); // convert: 6 wide, precision 1 (-999.9)
  dtostrf(dallasTemp, 4, 1, dallasTempStr);
  if(!client.publish(humidTopic, humidStr))  {
    #ifdef debug
      Serial.println("humid pub failed");
    #endif
    ESP.restart();
    yield();
  }
  if(!client.publish(tempTopic, dallasTempStr))  {
    #ifdef debug
      Serial.println("temp pub failed");
    #endif
    ESP.restart();
    yield();
  }
  yield();
  lastPubTime = millis();
  #ifdef debug
    Serial.print(humidTopic);
    Serial.println(humidStr);
    Serial.print(tempTopic);
    Serial.println(dallasTempStr);
  #endif
  return;
}

// Check connection to Wifi & broker //////////////////////////////////////////
void checkConnection() {
  // Broker not connected
  subbed = 0;  // resubscribe when reconnected
  // connection timer not started
  if(!connectTimerFlag) {
    connectTimerFlag = 1;
    connectStartTime = millis();
    #ifdef debug
      Serial.println("Reconnecting...");
    #endif
    return;
  }
  // Wifi not connected
  if(WiFi.status() != WL_CONNECTED) {
    ipAddressSet = 0;
    // wifi timed out
    if(millis() - connectStartTime > connectTimeout) {
      #ifdef debug
        Serial.println("Wifi timeout... restarting.");
      #endif
      ESP.restart();
      delay(500);
      return;
    }
    yield();
  }
  // broker timed out
  else if(millis() - connectStartTime > connectTimeout)  {
    #ifdef debug
      Serial.println("Broker timeout... restarting.");
    #endif
    ESP.restart();
    delay(500);
  }
  // broker not connected, not timed out
  else {
    client.connect(mqttClientID, mqttClientUser, mqttClientPass);
    yield();
  }
  return;
}

// Obvious by name...///////////////////////////////////////////////////////////
float celsiusToFarenheit(float celsius)  {
  float farenheit = 1.8*celsius + 32.0;
  return farenheit;
}

// Beeper (Non-blocking) ///////////////////////////////////////////////////////
void beeper(void) {
  if(nBeeps)  { // we have some beeping left to do
    unsigned long msecs = millis(); // grab time once per loop should be enough
    if(msecs - beepEventTime > beepDelay) { //time to do something with the horn
      if(!beeperActive)  { // horn is off
        beepEventTime = msecs; // reset timer
        digitalWrite(beepPin,HIGH); // sound the horn
        beeperActive = 1;
      }
      else if(beeperActive) { // horn is on
        beepEventTime = msecs; // reset timer
        digitalWrite(beepPin,LOW); // kill the horn
        beeperActive = 0;
        nBeeps--; // done with one beep
      }
    }
  }
  return;
}

// PIR State Machine ///////////////////////////////////////////////////////////
// Mission: Make sure the MQTT broker doesn't miss a PIR state change!!!
void pirMachine(void) {
switch(state) {
// (coming from loop, broker connected, pirLocal updated)
//  state =
//  0: pub pir "1", start timer, reset isPirSet, increment pubCount -> 1:
//  1: wait for sub, verify against pub: mismatched -> 0:, matched -> 2:
//  2: Wait for PIR timeout (timer reset in main loop) -> 3:
//  3: pub pir "0", start timer, reset isPirSet, increment pubCount++  -> 4:
//  4: wait for sub, verify against pub: mismatched -> 3:, matched -> 0:

  case 0:{
    if(pirLocal) {
      if(!client.publish(pirTopic, "1"))  {
        #ifdef debug
          Serial.println("pir_on pub failed");
        #endif
        ESP.restart();
      }
      isPirSet = 0;
      pubCount++;
      subStartTime = millis();
      state = 1;
    }
    break;
  }

  case 1:{ // verify broker PIR data was updated
    // just client.loop() until sub is ready
    if(isPirSet)  {
      if(pirBroker) {  // all data are matching
        #ifdef debug
          Serial.println("data matches broker, continuing...");
        #endif
        pubCount = 0;
        state = 2;
      }
      // broker doesn't match, and it's time to republish
      else if(millis() - subStartTime > repubDelay) {
        if(pubCount >= repubsMax)  {  // too many repubs
          #ifdef debug
            Serial.println("Err: too many repubs");
          #endif
          ESP.restart();
        }
        #ifdef debug
          Serial.println("Second sub mismatch, republishing");
        #endif
        state = 0;
      }
    }
    break;
  }

  case 2:{ // Wait for PIR timeout
    // note: lastPirHigh is updated in main the loop
    if(millis()-lastPirHigh > pirTimeout)  {  // PIR timeout, move on
      #ifdef debug
        Serial.println("PIR timed out: state = 3");
      #endif
      state = 3;
      return;
    }
    yield(); //delay to prevent watchdog timer reset
    break;
  }

  case 3:{  // pub pir=0
    #ifdef debug
      Serial.print("Sending: [");
      Serial.print(pirTopic);
      Serial.println("]: 0");
    #endif
    if(!client.publish(pirTopic, "0", true)) {
      #ifdef debug
        Serial.println("pir send failed");
      #endif
    }
    yield();
    isPirSet = 0; // set flag to get new pir state from broker
    state = 4;
    pubCount++;
    #ifdef debug
      Serial.print("Sent, waiting for sub update...");
    #endif
    subStartTime = millis();
    break;
  }

  case 4:{  // verify pir=0 with broker
    if(isPirSet)  { // wait for sub to arrive
      if(!pirBroker) {  // broker=0, we have a match
        #ifdef debug
          Serial.println("PIR sub = pub... state=0");
        #endif
        state = 0;
        pubCount = 0;
      }
      // doesn't match, and it is time to republish
      else if(millis() - subStartTime > repubDelay) {
        if(pubCount > repubsMax)  {  // too many repubs
          #ifdef debug
            Serial.println("Err: too many repubs");
          #endif
          ESP.restart();
        }
        #ifdef debug
          Serial.println("3rd sub mismatch, republishing");
        #endif
        state = 3;
      }
    }
    // sub timeout
    else if(millis() - subStartTime > subTimeout*1000) {
      #ifdef debug
        Serial.println("3rd sub timeout");
      #endif
      ESP.restart();
    }
    break;
  }
} // end of switch
} // end of function

// Setup ///////////////////////////////////////////////////////////////////////
void setup() {
  pinMode(pirPin,INPUT);             // Setup PIR input pin
  pinMode(beepPin,OUTPUT);           // Setup beep output pin
  digitalWrite(beepPin,LOW);         // Make sure beeper is "off"

  #ifdef debug
    Serial.begin(115200);
    Serial.println("Debug Enabled...");
  #endif

  // Form our topic strings
  sprintf(otaTopic, "%s/%s", mqttClientID, otaTopicD);
  sprintf(pirTopic, "%s/%s", mqttClientID, pirTopicD);
  sprintf(tempTopic, "%s/%s", mqttClientID, tempTopicD);
  sprintf(humidTopic, "%s/%s", mqttClientID, humidTopicD);
  sprintf(beeperTopic, "%s/%s", mqttClientID, beeperTopicD);

  WiFi.mode(WIFI_STA);                 // Wifi mode config
  wifi_station_set_hostname(hostName); // Set Wifi host name
  #ifdef customMac
    wifi_set_macaddr(STATION_IF, mac);   // Set wifi mac
  #endif
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    #ifdef debug
      Serial.print(".");
    #endif
  }
  myIP = WiFi.localIP();
  sprintf(notifyOTAready, "%s%s%s", notifyOTAreadyB, myIP.toString().c_str(), update_path);
  #ifdef debug
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  #endif

  client.setServer(brokerIP, brokerPort);
  client.setCallback(callback);
  //randomSeed(micros());
  checkConnection();                 // Connect to Wifi and broker

  client.subscribe(beeperTopic, 1); // perhaps not necessary?
  client.subscribe(otaTopic, 1);
  subbed = 1;

  //Initialize OTA server
  #ifdef debug
    Serial.print("Setting up OTA: ");
  #endif
  configTime(timeZone * 3600, 0, ntpServer1, ntpServer2);
  MDNS.begin(hostName);
  // httpServer.setRSACert(new BearSSL::X509List(serverCert), new BearSSL::PrivateKey(serverKey));
  httpServer.getServer().setRSACert(new BearSSL::X509List(serverCert), new BearSSL::PrivateKey(serverKey));
  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();
  MDNS.addService("https", "tcp", 443);
  #ifdef debug
    Serial.println("Ready");
  #endif

  #ifdef debug                       // DHT setup
    Serial.println("Intializing DHT Sensor...");
  #endif
  dht.begin();

  #ifdef debug                       // Dallas setup
    Serial.println("Intializing Dallas Sensor(s)...");
  #endif
  dallas.begin();
  #ifdef debug
    if (dallas.isParasitePowerMode()) Serial.println("Parasite Power: ON");
    else Serial.println("Parasite Power: OFF");
  #endif
  if (!dallas.getAddress(dallasAddress, 0)) Serial.println("Unable to find address for Device 0");
  dallas.setResolution(dallasAddress, dallasResolution);// set new resolution to tempResolution
  #ifdef debug                      // Report new resolution for each device
    Serial.print("Device 0 New resolution:");
    Serial.print(dallas.getResolution(dallasAddress), DEC);
    Serial.println(" bits");
  #endif
  dallas.setWaitForConversion(false);// Non-blocking, we'll handle timing manually
} // End setup

// LOOP ///////////////////////////////////////////////////////////////////////
void loop() {
  if (!client.connected()) {  // broker not connected
    checkConnection();
  }
  else if(!ipAddressSet) {  // broker connected, but ip not set
    myIP = WiFi.localIP();
    sprintf(notifyOTAready, "%s%s%s", notifyOTAreadyB, myIP.toString().c_str(), update_path);
    ipAddressSet = 1;
    #ifdef debug
      Serial.printf("Connected: %s\n",myIP.toString().c_str());
    #endif
    connectTimerFlag = 0; // reset timer flag
  }
  // must have got disconnected at some point... resubscribe
  else if(!subbed)  {
    client.subscribe(beeperTopic, 1);
    client.subscribe(otaTopic, 1);
    client.subscribe(pirTopic, 1);
    subbed = 1;
  }

  client.loop();  // Process PubSubClient
  beeper();       // Process beeps

  // FW button OFF: normal routine......................................
  if(!firmwareUp) {
    if(otaMessageSent)  { // reset ota message for next time
      if(!client.publish(otaTopic, "OTA turned off"))  {
        #ifdef debug
          Serial.println("ota_off pub failed");
        #endif
        ESP.restart();
      }
      otaMessageSent = 0;         // reset OTA notification flag
    }

    pirLocal = digitalRead(pirPin); // read PIR
    if(pirLocal) {  // bag it
      lastPirHigh = millis();  // tag it
    }
    pirMachine();  // welcome to the machine... we know where you've been

    // "non-blocking" read functions (at the mercy of stock libraries)
    if(millis() - dhtReadTime > dhtPeriod) {
      readDHT();
    }
    if(millis() - dallasReadTime > dallasPeriod) {
      readDallas();
    }

    // Pub time!!! ...oh, data pub, nm
    if(millis() - lastPubTime > uploadPeriod) {
      #ifdef debug
        Serial.println("Calculating Averages...");
      #endif

      humid = humid / dhtSamples;
      dallasTemp = dallasTemp / dallasSamples;

      #ifdef unitsFarenheit          // a big fat F is better than a C?
        dallasTemp = celsiusToFarenheit(dallasTemp);
      #endif

      #ifdef debug
        Serial.print("R.H. (n = ");
        Serial.print(dhtSamples);
        Serial.print("): ");
        Serial.print(humid);
        Serial.println(" %");
        Serial.print("temp (n = ");
        Serial.print(dallasSamples);
        Serial.print("): ");
        Serial.print(dallasTemp);
        Serial.println(" F");
      #endif

      publishData();                  // Publish data to broker

      Serial.println("Resetting averaging filter...");
      dhtSamples = 0.0;              // Reset our averaging buffers
      dallasSamples = 0.0;
      humid = 0.0;
      dallasTemp = 0.0;
    }
  }

  // FW OTA ON: handle start OTA server...............................
  else if(firmwareUp) {
    if(!otaMessageSent) {
      #ifdef debug
        Serial.println(notifyOTAready);
      #endif
      if(!client.publish(otaTopic, notifyOTAready))  {
        #ifdef debug
          Serial.println("ota notify failed");
        #endif
        ESP.restart();
      }
      otaMessageSent = 1;
      otaStartTime = millis();
    }
    if(millis() - otaStartTime > otaTimeout){
      #ifdef debug
        Serial.println("OTA timeout... resuming.");
      #endif
      firmwareUp = 0;
      otaMessageSent = 0;  // reset this for next time
      if(!client.publish(otaTopic, "OTA timeout"))  {
        #ifdef debug
          Serial.println("ota__timeout notify failed");
        #endif
        ESP.restart();
      }
    }
    httpServer.handleClient();
    MDNS.update();
  }
}
// End Of LOOP
////////////////////////////////////////////////////////////////////////////////
