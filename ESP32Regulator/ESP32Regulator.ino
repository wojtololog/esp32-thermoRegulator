#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include "BLEDevice.h"

const char* CONFIG_FILE = "/config.json";
boolean checkbut_status = false;
String mqtt_server = "";
String mqtt_port = ""; //uint16 but it could be 'int'
String mqtt_user = "";
String mqtt_pass = "";
String mqtt_topic = "";
String tempMacAddress;
BLEAddress addr = BLEAddress("");

//Bluetooth section
// The remote service we wish to connect to.
static BLEUUID serviceUUID("47e9ee00-47e9-11e4-8939-164230d1df67");
//("91bad492-b950-4226-aa2b-4ede9fa42f59");
// The characteristic of the remote service we are interested in - settings.
static BLEUUID    charUUID("47e9ee2b-47e9-11e4-8939-164230d1df67");
//("0d563a58-196a-48ce-ace2-dfec78acc814");
// The characteristic of the PIN.
static BLEUUID    pinUUID("47e9ee30-47e9-11e4-8939-164230d1df67");
static BLEAddress *pServerAddress;
std::string macDeviceOne = "";
static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  Serial.println("Notify callback for characteristic ");
  Serial.println(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.println(" of data length ");
  Serial.println(length);
}



bool connectToServer(BLEAddress pAddress,BLEClient* pClient) {
    Serial.print("Forming a connection to ");
    Serial.println(pAddress.toString().c_str());
     
    // Connect to the remove BLE Server.
    pClient->connect(pAddress);
    if(pClient->isConnected()==true){
    Serial.println(" - Connected to server");
    } else {
       ESP.restart();
    }

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the PIN characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(pinUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find PIN characteristic UUID: ");
      Serial.println(pinUUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found PIN characteristic");

    //DOMYSLnie ustawiony na 6 bitow = regulator
    //uint8_t pinbytes[4] = {0x57,0x04,0x00,0x00};
    uint8_t pinbytes[6] = {0x00,0x00,0x00,0x00,0x00,0x00};
    uint8_t* pinValue = pinbytes;
    //pinValue = pinbytes*;
    //Serial.println("Setting PIN characteristic value to \"" + pinValue + "\"");

    //tutaj dopisałem true.
    pRemoteCharacteristic->writeValue(pinValue, 4, true);

    delay(1000);
    
    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found our characteristic");
    //pRemoteCharacteristic->registerForNotify(notifyCallback);
}

void connectToBLE(String mac) {
  Serial.println("Starting Arduino BLE CometBlue application...");
  BLEDevice::init("");
  addr = BLEAddress(mac.c_str());  
  doConnect = true;
  
  BLEClient*  pClient  = BLEDevice::createClient();
  Serial.println(" - Created client");
  Serial.println(doConnect);
  
  if (doConnect == true) {
    if (connectToServer(addr, pClient)) {
      Serial.println("We are now connected to the BLE Server.");
      connected = true;
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }
}

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    //Serial.println(advertisedDevice.toString().c_str());
    Serial.println(advertisedDevice.getAddress().toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(serviceUUID)) {

      // W tej funkcji trzeba by nie przerywac skanowania i wyszukiwac urzadzenia - zapisywac ich adresy do tablicy
      Serial.print("Found our device!  address: "); 
      advertisedDevice.getScan()->stop(); //znalezlismy nasze uzyteczne urzadzen -> stopujemy scan 
      pServerAddress = new BLEAddress(advertisedDevice.getAddress()); //pobranie adresu urzadzenia
      macDeviceOne = advertisedDevice.getAddress().toString(); //adres mac urzadzenia w tablicy
      doConnect = true; 

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

void btleSetup() {
   BLEDevice::init("");
   BLEScan* pBLEScan = BLEDevice::getScan(); //begin to scan devices
   pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); //handling for each found device
   pBLEScan->setActiveScan(true);
   pBLEScan->start(30); //scanning for 30 secounds   
}

//JSON handling functions
bool ReadFile();
void WriteFile();

//MQTT subscriber
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
   Serial.begin(115200);
   //file system handling
    bool result = SPIFFS.begin();
    Serial.println("SPIFFS is opened:" + result);
   
    if(!ReadFile()) {
     Serial.println("Failed to read config file, using default values");
   }
   btleSetup();
 
   //html checkbox
   char customhtml[24] = "type=\"checkbox\"";

   if(checkbut_status) {
     strcat(customhtml, " checked"); //only way to handle checkbox state is to add "checked" string
   }

      Serial.println("Value of mqtt server: ");
      Serial.println(mqtt_server.c_str());
      Serial.println("Value of macDevice: ");
      Serial.println(macDeviceOne.c_str());
   //creatingWiFiManagerParams
   WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server.c_str(), 40); //m20.cloudmqtt.com
   WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port.c_str(), 6); // 10905 
   WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user.c_str(), 20); //udnnellq 
   WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass.c_str(), 20); //594Vf9OfDZ-6 
   WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", NULL, 40);
   WiFiManagerParameter macDevice("macdevice1", "Device MAC address", macDeviceOne.c_str(), 40);
   WiFiManagerParameter hint("<small>* Tip: If you want to save your MQTT params check it</small>");
   WiFiManagerParameter save_checkbox("savebox","Zapisac?","T",2,customhtml);
   WiFiManagerParameter scanningDevices("<h3> Wait bluetooth is scanning devices now...</h3>");
   WiFiManagerParameter searchedDevices("<h3> Device with MAC was found! </h3>");
   WiFiManagerParameter line("<br>");
   
   WiFiManager wifiManager;
   wifiManager.resetSettings(); //reset saved settings
   wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    //adding custom html to config page
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);
    wifiManager.addParameter(&line);
    wifiManager.addParameter(&save_checkbox);
    wifiManager.addParameter(&line);
    wifiManager.addParameter(&hint);
    wifiManager.addParameter(&line);
    wifiManager.addParameter(&scanningDevices);
    wifiManager.addParameter(&line);
 
    if(macDeviceOne != "") {
      wifiManager.addParameter(&searchedDevices);
      wifiManager.addParameter(&macDevice);
      wifiManager.addParameter(&custom_mqtt_topic);
      wifiManager.addParameter(&line);
    }
    //Connecting to AP
    if(wifiManager.autoConnect("ESP 32 - test","wojtek2468")) {
      mqtt_topic = custom_mqtt_topic.getValue();
      checkbut_status = (strncmp(save_checkbox.getValue(),"T",1) == 0); //0==0, checkbut_status = true -> odczytana wczensiej wartosc (z JSONA) jest taka sama, checkbox zaznaczony;
      if(checkbut_status) {
       mqtt_server = custom_mqtt_server.getValue();
       mqtt_port = custom_mqtt_port.getValue();
       mqtt_user = custom_mqtt_user.getValue();
       mqtt_pass = custom_mqtt_pass.getValue();
      }
      else {
       mqtt_server = "";
       mqtt_port = "";
       mqtt_user = "";
       mqtt_pass = "";
      }
      
      WriteFile(); //to JSON writing  
      Serial.println("connected...yeey :)"); //After this we are connected to wifi network 
      tempMacAddress = macDevice.getValue();
      mqtt_server = custom_mqtt_server.getValue();
      mqtt_port = custom_mqtt_port.getValue();
      mqtt_user = custom_mqtt_user.getValue();
      mqtt_pass = custom_mqtt_pass.getValue();
      delay(3000);
                                
 }  else {
    delay(3000);
    ESP.restart();
  }
}

void loop() {
      
      connectToBLE(tempMacAddress);
      
       if(connected) {
        //Set CloudMQTT access
        client.setServer(mqtt_server.c_str(), atoi(mqtt_port.c_str()));
        //serwer mqqt
        while (!client.connected()) {
          Serial.println("Connecting to MQTT...");
          if (client.connect("ESP32Client", mqtt_user.c_str(), mqtt_pass.c_str() )) {  
            Serial.println("connected");
            std::string value = pRemoteCharacteristic->readValue();
            Serial.println("value of set temp: ");
            Serial.println((float)value[1]);
            float castedTemperature = value[1]/2;
            char temperatureToPublish[8];
            client.publish(mqtt_topic.c_str(), dtostrf(castedTemperature,6,2,temperatureToPublish)); 
            client.loop();    
        } else {
          Serial.print("failed with state ");
          Serial.print(client.state());
          delay(2000);
          ESP.restart();
          delay(5000);
        }
      }
     }               
delay(10000);
}
//  uint8_t readTemperature = pRemoteCharacteristic->readUInt8();
//  float castedTemperature = (float) (readTemperature/2);
//  char temperatureToPublish[8];
  //Serial.println(dtostrf(castedTemperature,6,2,temperatureToPublish));
//  client.publish(mqtt_topic.c_str(), dtostrf(castedTemperature,6,2,temperatureToPublish));
  //Holding connection with MQTT 
       


void WriteFile() {
  Serial.println("zapisywanie JSON");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
 
  //przypisanie parametrow z programu do jsona
  json["checkbut_status"] = checkbut_status;
  json["mqtt_server"] = mqtt_server;
  json["mqtt_port"] = mqtt_port;
  json["mqtt_user"] = mqtt_user;
  json["mqtt_pass"] = mqtt_pass;
 
  //zapis parametrow do pliku
  File f = SPIFFS.open(CONFIG_FILE, "w");
  if(!f) {
    Serial.println("Nie udalo sie otworzyc pliku do zapisu parametrow");
  }

  json.prettyPrintTo(Serial);
  json.printTo(f);
  f.close();

  Serial.println("Pomyslnie zapisano parametry");
}

bool ReadFile() {

  File f = SPIFFS.open(CONFIG_FILE, "r");
  if(!f) {
    Serial.println("Nie udalo sie otworzyc pliku do zapisu parametrow");
    return false;
  }
  else
  {
    size_t size = f.size();
    std::unique_ptr<char[]> buf(new char[size]); //bufor na dane
    //odczyt
    f.readBytes(buf.get(),size);
    f.close();

    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(buf.get());

    if(!json.success()) {
      Serial.println("JSON PARSE FAILED");
    }

    json.printTo(Serial);

    //zapisywanie z jsona do zmiennych
    if(json.containsKey("checkbut_status")) checkbut_status = json["checkbut_status"]; 
    if(json.containsKey("mqtt_server")) mqtt_server = json["mqtt_server"].as<String>();;
    if(json.containsKey("mqtt_port")) mqtt_port = json["mqtt_port"].as<String>();;
    if(json.containsKey("mqtt_user")) mqtt_user = json["mqtt_user"].as<String>();;
    if(json.containsKey("mqtt_pass")) mqtt_pass = json["mqtt_pass"].as<String>();;
    
  }

  Serial.println("Read from JSON succesfully");
  return true;
}

