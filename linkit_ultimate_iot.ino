/*
This is the final code. Just change the Device ID and Device Key, to the one which you found 
and then change the MAC address and phone number to the one you want to associate your project with. 
*/

//#include <b64.h>
#include <HttpClient.h>
#include <LDHT.h>
#include <LTask.h>
#include <LWiFi.h>
#include <LWiFiClient.h>
#include <LDateTime.h>
#include <LBT.h>
#include <LFlash.h>
#include <LSD.h>
#include <LBTServer.h>
#include <LGSM.h>
#include <LStorage.h>

#define DEVICEID "DhjiujqO" // Change this to your device ID
#define DEVICEKEY "DKiuhihiuoihfa6" // Change this to your Device Key
#define WIFI_AUTH LWIFI_WPA  // choose from LWIFI_OPEN, LWIFI_WPA, or LWIFI_WEP.
#define per 50
#define per1 3
#define SITE_URL "api.mediatek.com"
#define DHTPIN 8          // what pin we're connected to
#define DHTTYPE DHT11     // using DHT11 sensor
#define Drv LFlash          // use Internal 10M Flash

LWiFiClient c;
unsigned int rtc;
unsigned int lrtc;
unsigned int rtc1;
unsigned int lrtc1;
const char* MACAddr = "12:34:56:ab:cd:ef";    //MAC address of the Bluetooth device you want to associate with your project
char phoneNum[20] = "+123456789";    //The number from which SMS commands work
char port[4] = {0};
char connection_info[21] = {0};
char ip[21] = {0};
char ssid[50];
char pass[50];
char smsCommand[500];
int portnum;
int val = 0;
int relayPin = 9;     //Set this as the pin the relay is connected to
String tempSt;
String humiSt;
String mq2St;
String relayState;
String btSendCommand;
String smsFormattedValues;
String tcpdata = String(DEVICEID) + "," + String(DEVICEKEY) + ",0";

LWiFiClient c2;
HttpClient http(c2);

LDHT dht(DHTPIN, DHTTYPE);

void setup()
{
  Serial.begin(115200);
  while (!Serial) delay(1000);
  pinMode(10, OUTPUT);
  Drv.begin();
  dht.begin();
  LTask.begin();
  LWiFi.begin();
  LBTServer.begin((uint8_t*)"My_BTServer");

  readWifiSetting();

  Serial.println("Connecting to AP");
  while (0 == LWiFi.connect(ssid, LWiFiLoginInfo(WIFI_AUTH, pass)))
  {
    delay(1000);
  }

  Serial.println("calling connection");

  while (!c2.connect(SITE_URL, 80))
  {
    Serial.println("Re-Connecting to WebSite");
    delay(1000);
  }
  delay(100);

  while (!LSMS.ready())
  {
    delay(1000);
  }

  Serial.println("GSM initialized!");

  LSMS.beginSMS(phoneNum);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  getconnectInfo();
  connectTCP();
}

void loop()
{

  uploadWifiData();     //Uploads data to MCS and checks for bluetooth clients after uploading each datapoint
  sendBtSensorData();     //Checks for bluetooth clients
  smsControl();       //Checks for any SMS commands and if available, acts accordingly

}

void getconnectInfo() {
  //calling RESTful API to get TCP socket connection
  c2.print("GET /mcs/v2/devices/");
  c2.print(DEVICEID);
  c2.println("/connections.csv HTTP/1.1");
  c2.print("Host: ");
  c2.println(SITE_URL);
  c2.print("deviceKey: ");
  c2.println(DEVICEKEY);
  c2.println("Connection: close");
  c2.println();

  delay(500);

  int errorcount = 0;
  while (!c2.available())
  {
    Serial.println("waiting HTTP response: ");
    Serial.println(errorcount);
    errorcount += 1;
    if (errorcount > 10) {
      c2.stop();
      return;
    }
    delay(100);
  }
  int err = http.skipResponseHeaders();

  int bodyLen = http.contentLength();
  Serial.print("Content length is: ");
  Serial.println(bodyLen);
  Serial.println();
  char c;
  int ipcount = 0;
  int count = 0;
  int separater = 0;
  while (c2)
  {
    int v = c2.read();
    if (v != -1)
    {
      c = v;
      Serial.print(c);
      connection_info[ipcount] = c;
      if (c == ',')
        separater = ipcount;
      ipcount++;
    }
    else
    {
      Serial.println("no more content, disconnect");
      c2.stop();
    }
  }
  Serial.print("The connection info: ");
  Serial.println(connection_info);
  int i;
  for (i = 0; i < separater; i++)
  { ip[i] = connection_info[i];
  }
  int j = 0;
  separater++;
  for (i = separater; i < 21 && j < 5; i++)
  { port[j] = connection_info[i];
    j++;
  }
  Serial.println("The TCP Socket connection instructions:");
  Serial.print("IP: ");
  Serial.println(ip);
  Serial.print("Port: ");
  Serial.println(port);
  portnum = atoi (port);
  Serial.println(portnum);

}     //Get connection info

void connectTCP() {
  //establish TCP connection with TCP Server with designate IP and Port
  c.stop();
  Serial.println("Connecting to TCP");
  Serial.println(ip);
  Serial.println(portnum);
  while (0 == c.connect(ip, portnum))
  {
    Serial.println("Re-Connecting to TCP");
    delay(1000);
  }
  Serial.println("send TCP connect");
  c.println(tcpdata);
  c.println();
  Serial.println("waiting TCP response:");
}     //Connect TCP

void heartBeat() {
  Serial.println("send TCP heartBeat");     //heartBeat
  c.println(tcpdata);
  c.println();
}     //TCP Heartbeat

void uploadtemperature() {
  //calling RESTful API to upload temperature to MCS

  String upload_temp;
  String temperature = "dht11_temp,,";
  String tempData;
  float rawTemp;

  Serial.println("calling connection");
  LWiFiClient c2;

  while (!c2.connect(SITE_URL, 80))
  {
    Serial.println("Re-Connecting to WebSite");
    delay(1000);
  }
  delay(100);

  dht.read();     //Setting up DHT11 sensor

  rawTemp = dht.readTemperature();
  tempData = String(rawTemp);
  upload_temp = temperature + tempData;
  Serial.println(upload_temp);

  int tempLength = upload_temp.length();

  HttpClient http(c2);

  c2.print("POST /mcs/v2/devices/");
  c2.print(DEVICEID);
  c2.println("/datapoints.csv HTTP/1.1");
  c2.print("Host: ");
  c2.println(SITE_URL);
  c2.print("deviceKey: ");
  c2.println(DEVICEKEY);
  c2.print("Content-Length: ");
  c2.println(tempLength);
  c2.println("Content-Type: text/csv");
  c2.println("Connection: close");
  c2.println();
  c2.println(upload_temp);

  delay(1000);

  int errorcount = 0;
  while (!c2.available())
  {
    Serial.print("waiting HTTP response: ");
    Serial.println(errorcount);
    errorcount += 1;
    if (errorcount > 10) {
      c2.stop();
      return;
    }
    delay(100);
  }
  int err = http.skipResponseHeaders();

  int bodyLen = http.contentLength();
  Serial.print("Content length is: ");
  Serial.println(bodyLen);
  Serial.println();
  while (c2)
  {
    int v = c2.read();
    if (v != -1)
    {
      Serial.print(char(v));
    }
    else
    {
      Serial.println("no more content, disconnect");
      c2.stop();
    }
  }
}

void uploadhumidity() {
  //calling RESTful API to upload humidity to MCS

  String upload_humi;
  String humidity = "dht11_humi,,";
  String humiData;
  float rawHumi;

  Serial.println("calling connection");
  LWiFiClient c2;

  while (!c2.connect(SITE_URL, 80))
  {
    Serial.println("Re-Connecting to WebSite");
    delay(1000);
  }
  delay(100);

  dht.read();     //Setting up DHT11 sensor

  rawHumi = dht.readHumidity();
  humiData = String(rawHumi);
  upload_humi = humidity + humiData;
  Serial.println(upload_humi);

  int humiLength = upload_humi.length();

  HttpClient http(c2);

  c2.print("POST /mcs/v2/devices/");
  c2.print(DEVICEID);
  c2.println("/datapoints.csv HTTP/1.1");
  c2.print("Host: ");
  c2.println(SITE_URL);
  c2.print("deviceKey: ");
  c2.println(DEVICEKEY);
  c2.print("Content-Length: ");
  c2.println(humiLength);
  c2.println("Content-Type: text/csv");
  c2.println("Connection: close");
  c2.println();
  c2.println(upload_humi);

  delay(1000);

  int errorcount = 0;
  while (!c2.available())
  {
    Serial.print("waiting HTTP response: ");
    Serial.println(errorcount);
    errorcount += 1;
    if (errorcount > 10) {
      c2.stop();
      return;
    }
    delay(100);
  }
  int err = http.skipResponseHeaders();

  int bodyLen = http.contentLength();
  Serial.print("Content length is: ");
  Serial.println(bodyLen);
  Serial.println();
  while (c2)
  {
    int v = c2.read();
    if (v != -1)
    {
      Serial.print(char(v));
    }
    else
    {
      Serial.println("no more content, disconnect");
      c2.stop();
    }
  }
}

void uploadMQ2Data() {
  //calling RESTful API to upload MQ2 voltage to MCS

  String upload_mq2;
  String mq2 = "mq2_sens,,";
  String mq2Data;
  int mq2Percent;

  Serial.println("calling connection");
  LWiFiClient c2;

  while (!c2.connect(SITE_URL, 80))
  {
    Serial.println("Re-Connecting to WebSite");
    delay(1000);
  }
  delay(100);

  float sensorValue;

  sensorValue = analogRead(A0);
  mq2Percent = sensorValue / 1023 * 100;

  Serial.print("Sensor percent = ");
  Serial.println(mq2Percent);
  delay(1000);

  mq2Data = String(mq2Percent);
  upload_mq2 = mq2 + mq2Data;
  Serial.println(upload_mq2);

  int mq2Length = upload_mq2.length();

  HttpClient http(c2);

  c2.print("POST /mcs/v2/devices/");
  c2.print(DEVICEID);
  c2.println("/datapoints.csv HTTP/1.1");
  c2.print("Host: ");
  c2.println(SITE_URL);
  c2.print("deviceKey: ");
  c2.println(DEVICEKEY);
  c2.print("Content-Length: ");
  c2.println(mq2Length);
  c2.println("Content-Type: text/csv");
  c2.println("Connection: close");
  c2.println();
  c2.println(upload_mq2);

  delay(1000);

  int errorcount = 0;
  while (!c2.available())
  {
    Serial.print("waiting HTTP response: ");
    Serial.println(errorcount);
    errorcount += 1;
    if (errorcount > 10) {
      c2.stop();
      return;
    }
    delay(100);
  }
  int err = http.skipResponseHeaders();

  int bodyLen = http.contentLength();
  Serial.print("Content length is: ");
  Serial.println(bodyLen);
  Serial.println();
  while (c2)
  {
    int v = c2.read();
    if (v != -1)
    {
      Serial.print(char(v));
    }
    else
    {
      Serial.println("no more content, disconnect");
      c2.stop();
    }
  }
}

void relayStatusUpload() {
  //calling RESTful API to upload relay status to MCS

  String upload_relay;

  Serial.println("calling connection");
  LWiFiClient c2;

  while (!c2.connect(SITE_URL, 80))
  {
    Serial.println("Re-Connecting to WebSite");
    delay(1000);
  }
  delay(100);
  if (digitalRead(relayPin) == 1)
    upload_relay = "relay_state,,1";
  else
    upload_relay = "relay_state,,0";
  int thislength = upload_relay.length();
  HttpClient http(c2);
  c2.print("POST /mcs/v2/devices/");
  c2.print(DEVICEID);
  c2.println("/datapoints.csv HTTP/1.1");
  c2.print("Host: ");
  c2.println(SITE_URL);
  c2.print("deviceKey: ");
  c2.println(DEVICEKEY);
  c2.print("Content-Length: ");
  c2.println(thislength);
  c2.println("Content-Type: text/csv");
  c2.println("Connection: close");
  c2.println();
  c2.println(upload_relay);

  delay(500);

  int errorcount = 0;
  while (!c2.available())
  {
    Serial.print("waiting HTTP response: ");
    Serial.println(errorcount);
    errorcount += 1;
    if (errorcount > 10) {
      c2.stop();
      return;
    }
    delay(100);
  }
  int err = http.skipResponseHeaders();

  int bodyLen = http.contentLength();
  Serial.print("Content length is: ");
  Serial.println(bodyLen);
  Serial.println();
  while (c2)
  {
    int v = c2.read();
    if (v != -1)
    {
      Serial.print(char(v));
    }
    else
    {
      Serial.println("no more content, disconnect");
      c2.stop();
    }
  }
}

void relaySwitch() {

  String relay_on = "relay_switch,1";
  String relay_off = "relay_switch,0";
  String relayCommand = "";

  while (c.available())
  {
    int v = c.read();
    if (v != -1)
    {
      Serial.print((char)v);
      relayCommand += (char)v;
      if (relayCommand.substring(40).equals(relay_on)) {
        digitalWrite(relayPin, HIGH);
        Serial.println("Switch relay on ");
        relayCommand = "";
      }
      else if (relayCommand.substring(40).equals(relay_off)) {
        digitalWrite(relayPin, LOW);
        Serial.println("Switch relay off");
        relayCommand = "";
      }
    }
  }
}         //Check for the relay switch on MCS and turn relay on or off accordingly

void uploadWifiData() {

  relayStatusUpload();
  sendBtSensorData();
  relaySwitch();
  sendBtSensorData();
  uploadtemperature();
  sendBtSensorData();
  uploadhumidity();
  sendBtSensorData();
  uploadMQ2Data();

  LDateTime.getRtc(&rtc);
  if ((rtc - lrtc) >= per) {
    heartBeat();
    lrtc = rtc;
  }
  //Check for report datapoint status interval
  LDateTime.getRtc(&rtc1);
  if ((rtc1 - lrtc1) >= per1) {
    relayStatusUpload();
    sendBtSensorData();
    relaySwitch();
    sendBtSensorData();
    uploadtemperature();
    sendBtSensorData();
    uploadhumidity();
    sendBtSensorData();
    uploadMQ2Data();
    lrtc1 = rtc1;
  }
}        //Command to upload data to MCS. After uploading or receiving each datapoint,it checks if bluetooth is available

void btUploadTemp() {

  dht.read();
  float tempC = dht.readTemperature();

  tempSt = String(tempC);
}       //Get temperature to upload via Bluetooth

void btUploadHumi() {

  dht.read();
  float humi = dht.readHumidity();

  humiSt = String(humi);
}       //Get humidity to upload via Bluetooth

void btUploadMq2() {

  int mq2Percent;
  float sensorValue;

  sensorValue = analogRead(A0);
  mq2Percent = sensorValue / 1023 * 100;

  mq2St = String(mq2Percent);
}       //Get MQ2 sensor value to upload via Bluetooth

void btUploadRelaySwitch() {

  String relayCommand = LBTServer.readString();

  if (relayCommand == "On") {
    digitalWrite(relayPin, HIGH);
    Serial.println("Relay pin is on");
  }
  else if (relayCommand == "Off") {
    digitalWrite(relayPin, LOW);
    Serial.println("Relay pin is off");
  }
}       //Check for bluetooth command to on or off relay

void readRelayState() {

  if (digitalRead(relayPin) == HIGH) {
    relayState = "On";
  }
  else if (digitalRead(relayPin) == LOW) {
    relayState = "Off";
  }
}       //Check if the relay pin is on or off to upload status via bluetooth

void sendBtSensorData() {

  if (LBTServer.connected()) {

    btUploadTemp();
    btUploadHumi();
    btUploadMq2();
    btUploadRelaySwitch();
    readRelayState();

    btSendCommand = tempSt + "," + humiSt + "," + mq2St + "," + relayState;

    String btSendCommandSt = String(btSendCommand);
    int btSendCommandSize = btSendCommandSt.length();
    char btSendCommandBuffer[100];
    for (int a = 0 ; a <= btSendCommandSize ; a++)
    {
      btSendCommandBuffer[a] = btSendCommandSt[a];
    }

    int write_size = LBTServer.write(btSendCommandBuffer, btSendCommandSize);
    Serial.println(btSendCommandBuffer);
    LBTServer.flush();
  }
  else
  {
    Serial.println("Retrying bluetooth connection");   //Else retry
    LBTServer.accept(5, MACAddr);
  }
}       //Send all bluetooth variables to pre-specified MAC Address as a formatted string

void checkForSMS() {

  char p_num[20];   //Number from which the SMS is received
  //char dtaget[500];
  int len = 0;

  if (LSMS.available()) // Check if there is new SMS
  {
    LSMS.remoteNumber(p_num, 20);
    Serial.print("SMS received from ");
    Serial.println(p_num);

    if (strcmp(p_num, phoneNum) == 0)  {

      while (true)
      {
        int v = LSMS.read();
        if (v < 0)
          break;

        smsCommand[len++] = (char)v;
      }
      Serial.println(smsCommand);
    }

    else {
      Serial.println("Not authorized to receive command from this number");
      LSMS.flush(); // delete message
    }
  }
  else (Serial.println("No SMS command received!"));
}       //Uses GSM. Check for any available SMS from pre-specified phone number. Rejects commands from unauthorized phone numbers

void sendSMS() {
  if (LSMS.endSMS())
  {
    Serial.println("SMS sent");
  }
  else
  {
    Serial.println("SMS is not sent");
  }
}       //Used for sending SMS to pre-specified phone number

void smsSaveVariables() {
  float temp = 0.0;
  float humi = 0.0;
  float sensorValue;
  int mq2Percent;
  String RelayState;

  sensorValue = analogRead(A0);
  mq2Percent = sensorValue / 1023 * 100;

  dht.read();
  temp = dht.readTemperature();
  humi = dht.readHumidity();
  delay(500);

  if (digitalRead(relayPin) == HIGH) {
    RelayState = "on";
  }
  else if (digitalRead(relayPin) == LOW) {
    RelayState = "off";
  }

  String TempString = String(temp);
  String HumiString = String(humi);
  String Mq2String = String(mq2Percent);

  String smsTempSt = "Temperature: " + TempString + " degrees C" + '\n';
  String smsHumiSt = "Humidity: " + HumiString + "%" + '\n';
  String smsMq2St = "MQ2 sensor value: " + Mq2String + "%" + '\n';
  String RelayStateSt = "The relay is " + RelayState;

  smsFormattedValues = smsTempSt + smsHumiSt + smsMq2St + RelayStateSt;
}       //Get variables to send via sms, format them properly, and save them in a string

void smsSendSwitchedOnRelay() {
  LSMS.beginSMS(phoneNum);
  LSMS.print("Switched on the relay");
  sendSMS();
}       //Send sms to confirm switching on relay

void smsSendSwitchedOffRelay() {
  LSMS.beginSMS(phoneNum);
  LSMS.print("Switched off the relay");
  sendSMS();
}       //Send sms to confirm switching off relay

void smsCommandRelay() {
  if (strcmp(smsCommand, "Switch on the relay") == 0) {
    digitalWrite(relayPin, HIGH);
    Serial.println("Switched on relay by SMS");
    smsSendSwitchedOnRelay();
    LSMS.flush(); // delete message
  }
  else if (strcmp(smsCommand, "Switch off the relay") == 0) {
    digitalWrite(relayPin, LOW);
    Serial.println("Switched off relay by SMS");
    smsSendSwitchedOffRelay();
    LSMS.flush(); // delete message
  }
  else {
    Serial.println("Command not recognized (Relay Control)!");
    LSMS.flush(); // delete message
  }
}       //Check for sms command to switch on or off the relay. If available, do as commanded, and send confirmation sms

void smsCheckVariablesRequest() {
  if (strcmp(smsCommand, "Send variables") == 0) {
    LSMS.beginSMS(phoneNum);
    smsSaveVariables();
    LSMS.print(smsFormattedValues);
    Serial.println(" ");
    Serial.println(smsFormattedValues);
    Serial.println(" ");
    sendSMS();
  }
  else {
    Serial.println("Command not recognized (Variables)!");
  }
}       //Check for sms command to send variables. If available, send the variables by sms

void smsSendVariables() {
  checkForSMS();
  smsCheckVariablesRequest();
}    //First check for sms and save it in buffer. If command recognized, send variables

void smsControlRelay() {
  checkForSMS();
  smsCommandRelay();
}    //First check for sms and save it in buffer. If command recognized, control relay accordingly

void smsControl() {
  memset(smsCommand, '\0', sizeof(smsCommand));
  smsSendVariables();
  memset(smsCommand, '\0', sizeof(smsCommand));
  smsControlRelay();
}    //First clear sms buffer, then check for send variables command, then clear buffer and check for relay control command, and act accordingly

void readWifiSetting() {
  Drv.begin();

  LFile f = Drv.open("wifi.txt", FILE_READ);
  if (!f)
    Serial.println("Fail to open wifi.txt");

  char buf[100];
  int len = f.size();

  f.read(buf, len);
  f.close();
  buf[len] = '\0';

  int i = 0, j = 0;

  while (buf[i] && buf[i] != ',')
  {
    ssid[j++] = buf[i++];
  }
  ssid[j] = 0;
  if (buf[i] != ',')
    Serial.println("Fail to parse wifi.txt");
  i++;
  j = 0;
  while (buf[i])
  {
    pass[j++] = buf[i++];
  }
  pass[j] = 0;
}
