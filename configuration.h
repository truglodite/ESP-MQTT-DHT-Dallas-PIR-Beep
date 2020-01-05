#pragma once
///////////////////////////////////////////////////////////////////////////////
// configuration.h
// by: Truglodite
// updated: 6/21/2019
//
// General configuration for EspMqttDHTdallasPIRbeep. Comment out
// the first line (not the pragma) in the main ino!!!
///////////////////////////////////////////////////////////////////////////////
#ifndef privacy
//#define debug             // Uncomment to enable serial debug prints
#define hostName        "hostName"
#define dhtType         DHT11      // DHT22, DHT11, etc...
#define customMac           // comment this and the next line to use default mac
uint8_t mac[6]   {0xDE,0xAD,0xBE,0xEF,0xFE,0xED};

#define ssid            "mySSID"
#define pass            "myWifiPassword"

// Mosquitto //////////////////////////////////////////////////////////////////
// Broker & Client info
IPAddress brokerIP(192,168,0,1);
#define brokerPort      1883
#define mqttClientID    "MY_MQTT_UNIQUE_ID"
#define mqttClientUser  "MY_MQTT_USERNAME"
#define mqttClientPass  "MY_MQTT_PASSWORD"

// Topics
// default pub/sub topic example: "MY_MQTT_UNIQUE_ID/ota"
#define otaTopicD     "ota"    // FW OTA: 0 = Normal, 1 = On
#define beeperTopicD  "beeper" // number of beeps to do
#define pirTopicD     "pir"    // binary motion (0=off, 1=on)
#define tempTopicD    "temp"   // Dallas temperature [F]
#define humidTopicD   "humid"  // Relative Humidity [%]
#define repubsMax     5        // max times to repub pir mismatch before restart

// Special payloads
#define notifyOTAreadyB      "https://"  // Template for OTA ready notification text (just to store the required chars)
#define notifyDHTfail        "DHT nan"   // Failed DHT read notification text

// OTA ////////////////////////////////////////////////////////////////////////
#define update_username  "username"  // OTA username
#define update_password  "password"  // OTA password
#define update_path      "/firmware" // OTA directory (must begin with "/")

#define ntpServer1    "pool.time.org"    // primary time server
#define ntpServer2    "time.nist.gov"    // secondary time server
#define timeZone      -8                 // time zone (+/- hours)
/*
Install openssl and use this great one-liner to create your self signed
key/cert pair with 2k RSA encryption:

openssl req -x509 -nodes -newkey rsa:2048 -keyout serverKey.pem -sha256 -out serverCert.pem -days 4000 -subj "/C=US/ST=CA/L=province/O=Anytown/CN=CommonName"

Paste the contents of the files between the 'BEGIN...' and 'END...' lines below:
*/
static const char serverCert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

static const char serverKey[] PROGMEM =  R"EOF(
-----BEGIN PRIVATE KEY-----

-----END PRIVATE KEY-----
)EOF";

// Pins ///////////////////////////////////////////////////////////////////////
#define pirPin                      13   // Physical pin: PIR output
#define dhtPin                      12   // DHT sensor data pin (default io12)
#define dallasPin                   14   // Dallas sensor data pin
#define beepPin                     16   // Beeper output pin

#define dallasResolution            12   // bits (9 default, up to 12)
#define dhtRetriesMax               5    // max DHT nan's before restart
#define unitsFarenheit                   // Comment for Celsius

// Timers (milliseconds) //////////////////////////////////////////////////////
#define dallasPeriod    800    // between start of conv. and read (>760 @12bit)
#define dhtPeriod       2000   // between DHT readings
#define pirTimeout      5000  // after PIR off before pub sent
#define connectTimeout  60000  // before rebooting during failed connections
#define uploadPeriod    60000  // between pubs
#define otaTimeout      300000 // wait for OTA upload before reboot
#define beepDelay       100    // delay between beeps
#define subTimeout      8000   // to wait for pir 'resub' before continuing
#define repubDelay      1000   // milliseconds to wait between repubs

#endif
