#include <Arduino.h>
#include <Stream.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

//AWS
#include "sha256.h"
#include "Utils.h"

//WEBSockets
#include <Hash.h>
#include <WebSocketsClient.h>

//MQTT PAHO
#include <SPI.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>

//AWS MQTT Websocket
#include "Client.h"
#include "AWSWebSocketClient.h"
#include "CircularByteBuffer.h"

//WiFiManager
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

//JSON
#include <ArduinoJson.h>

//EEPROM
#include <EEPROM.h>


//AWS IOT config, change these:
char aws_endpoint[]    = "";
char aws_key[]         = "";
char aws_secret[]      = "";
char aws_region[]      = "us-east-2";
int port = 443;

char* awsTopicGet             = "$aws/things/ESP8266/shadow/get";
char* awsTopicGetAccepted     = "$aws/things/ESP8266/shadow/get/accepted";
char* awsTopicUpdate          = "$aws/things/ESP8266/shadow/update";
char* awsTopicUpdateDelta     = "$aws/things/ESP8266/shadow/update/delta";
char* awsTopicUpdateAccepted  = "$aws/things/ESP8266/shadow/update/accepted";


char *configSsid = "RaumfeldSwitch";
char *configPassword = "1231231234";


WiFiManager wifiManager;


int LED1 = D2;
int LED2 = D3;
int BTN1 = D1;
int BTN2 = D4;
volatile int button1IsPressed = false;
volatile int button2IsPressed = false;
volatile unsigned long alteZeit = 0, entprellZeit = 200;

//MQTT config
const int maxMQTTpackageSize = 512;
const int maxMQTTMessageHandlers = 1;

ESP8266WiFiMulti WiFiMulti;

AWSWebSocketClient awsWSclient(1000);

IPStack ipstack(awsWSclient);
MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers> *client = NULL;

//# of connections
long connection = 0;

//count arrived MQTT-messages
int arrivedcount = 0;



//Chinch Voltage between (0-255 -> 0-1V)
int inputLevel = 0;

//threshold. If inputLevel >= threshold: PowerOn, else PowerOff
int threshold = 0;

//delay in milliseconds until power off after under threshold
int delayOff = 3000;

//switch to always PowerOn and ignore threshold
bool alwaysOn = false;

// indicates if the sound is on or ofd
bool isOn = false;

volatile unsigned long delayStartTime = 0;

int eeAddressStart = 0;

void getVariables() {
  EEPROM.begin(32);

  int eeAddress = eeAddressStart;
  EEPROM.get(eeAddress, inputLevel);
  eeAddress += sizeof(int);

  EEPROM.get(eeAddress, threshold);
  eeAddress += sizeof(int);

  EEPROM.get(eeAddress, alwaysOn);
  eeAddress += sizeof(int);

  EEPROM.get(eeAddress, delayOff);
  eeAddress += sizeof(int);


  EEPROM.commit();
  EEPROM.end();
}

void putVariables() {
  EEPROM.begin(32);

  int eeAddress = eeAddressStart;
  EEPROM.put(eeAddress, inputLevel);
  eeAddress += sizeof(int);

  EEPROM.put(eeAddress, threshold);
  eeAddress += sizeof(int);

  EEPROM.put(eeAddress, alwaysOn);
  eeAddress += sizeof(int);

  EEPROM.put(eeAddress, delayOff);
  eeAddress += sizeof(int);


  EEPROM.end();
}

void printVariables() {
  Serial.print("InputLevel: ");
  Serial.print(inputLevel);
  Serial.print(", threshold: ");
  Serial.print(threshold);
  Serial.print(", alwaysOn: ");
  Serial.print(alwaysOn);
  Serial.print(", delayOff: ");
  Serial.println(delayOff);
}

//soizegenerate random mqtt clientID
char* generateClientID () {
  char* cID = new char[23]();
  for (int i = 0; i < 22; i += 1)
    cID[i] = (char)random(1, 256);
  return cID;
}

//callback to handle mqtt messages


//connects to websocket layer and mqtt layer
bool connectAws () {

  if (client == NULL) {
    client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
  } else {

    if (client->isConnected ()) {
      client->disconnect ();
    }
    delete client;
    client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
  }


  //delay is not necessary... it just help us to get a "trustful" heap space value
  delay (1000);
  Serial.print (millis ());
  Serial.print (" - conn: ");
  Serial.print (++connection);
  Serial.print (" - (");
  Serial.print (ESP.getFreeHeap ());
  Serial.println (")");


  int rc = ipstack.connect(aws_endpoint, port);
  if (rc != 1)
  {
    Serial.println("error connection to the websocket server");
    return false;
  } else {
    Serial.println("websocket layer connected");
  }


  Serial.println("MQTT connecting");
  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
  data.MQTTVersion = 3;
  char* clientID = generateClientID ();
  data.clientID.cstring = clientID;
  rc = client->connect(data);
  delete[] clientID;
  if (rc != 0)
  {
    Serial.print("error connection to MQTT server");
    Serial.println(rc);
    return false;
  }
  Serial.println("MQTT connected");
  return true;
}

bool messageGetArrived = false;
void getCurrentShadow() {
  subscribeGetAccepted();
  sendGetMessage();
  while (!messageGetArrived) {
    if (awsWSclient.connected ()) {
      client->yield();
    } else {
      if (connectAws ()) {
        subscribeGetAccepted();
      }
    }
  }
  Serial.println("messageGetArrived = true");
  unsubscribeGetAccepted();
}


// Searches for a nestedkey in a JsonObject:
bool containsNestedKey(const JsonObject& obj, const char* key) {
  for (const JsonPair& pair : obj) {
    if (!strcmp(pair.key, key))
      return true;
    if (containsNestedKey(pair.value.as<JsonObject>(), key))
      return true;
  }
  return false;
}

void resetWifi() {
  WiFi.disconnect(true);
  wifiManager.resetSettings();
  delay(500);
  ESP.reset();
}


void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("");

  //LED1 ausgeschaltet
  pinMode(LED1, OUTPUT);
  digitalWrite(LED1, 0);

  //LED2 ausgeschaltet
  pinMode(LED2, OUTPUT);
  digitalWrite(LED2, 0);

  //BTN1
  pinMode(BTN1, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BTN1), button1Pressed, CHANGE);

  //BTN2
  pinMode(BTN2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BTN2), button2Pressed, CHANGE);

  //WifiManager
  wifiManager.setCustomHeadElement("<style>html{filter: invert(100%); -webkit-filter: invert(100%);}</style>");
  if (wifiManager.autoConnect(configSsid, configPassword)) {
    digitalWrite(LED2, 1);
  }

  //LoadVariables
  getVariables();
  printVariables();

  //Serial.setDebugOutput(0);

  //fill AWS parameters
  awsWSclient.setAWSRegion(aws_region);
  awsWSclient.setAWSDomain(aws_endpoint);
  awsWSclient.setAWSKeyID(aws_key);
  awsWSclient.setAWSSecretKey(aws_secret);
  awsWSclient.setUseSSL(true);

  if (connectAws ()) {
    getCurrentShadow();
  }

  if (connectAws ()) {
    subscribeUpdateDelta();
  }





  //Light-Sleep for PowerSaving
  WiFi.mode(WIFI_STA);
  wifi_set_sleep_type(LIGHT_SLEEP_T);


}

void loop() {
  //keep the mqtt up and running
  if (awsWSclient.connected ()) {
    client->yield();
  } else {
    //handle reconnection
    if (connectAws ()) {
      subscribeUpdateDelta();

    }
  }

  //PowerSwitch
  if (alwaysOn || (inputLevel > threshold)) {
    digitalWrite(LED1, 1);
    isOn = true;
    
  } else if (isOn) {
    //digitalWrite(LED1, 0);
    underTreshold();
  }

  if (button1IsPressed) {
    button1IsPressed = false;
    alwaysOn = !alwaysOn;
    printVariables();
    putVariables();
    sendUpdateDesiredMessage();
  }

  if (button2IsPressed) {
    button2IsPressed = false;
    resetWifi();
  }

  inputLevel = getInputLevel();
  Serial.println(inputLevel);

  Serial.println("Loop done.");
  //delay(1000);
}


int getInputLevel() {
  int maxVal = 0;
  int tmpVal = 0;
  for (int i = 0; i < 100; i++) {
    tmpVal = analogRead(A0);
    if (tmpVal > maxVal) maxVal = tmpVal;
    delay(1);
  }

  return maxVal;
}


void underTreshold() {
  if (delayStartTime == 0) {
    delayStartTime= millis();
    Serial.println("starting Delay...");
  } else {
    if ((millis() - delayStartTime) > delayOff) {
      Serial.print("diffDelay: ");
      Serial.println(millis() - delayStartTime);
      digitalWrite(LED1, 0);
      delayStartTime = 0;
      isOn = false;
    } else {
      Serial.print("diffDelay: ");
      Serial.println(millis() - delayStartTime);
      //delayStartTime= millis();
    }
  }
}



void button1Pressed() {
  if ((millis() - alteZeit) > entprellZeit) {
    button1IsPressed = true;
    Serial.println("Button1Pressed");
    digitalWrite(LED1, 1);
    alteZeit = millis();
  }
}

void button2Pressed() {
  if ((millis() - alteZeit) > entprellZeit) {
    button2IsPressed = true;
    Serial.println("Button2Pressed");
    alteZeit = millis();
  }
}
