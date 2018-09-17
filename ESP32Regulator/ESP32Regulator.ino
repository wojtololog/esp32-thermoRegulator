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
String mqtt_server = "";
String mqtt_port = ""; //uint16 but it could be 'int'
String mqtt_user = "";
String mqtt_pass = "";
String mqtt_topic = "";

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


bool connectToServer(BLEAddress pAddress) {
    Serial.print("Forming a connection to ");
    Serial.println(pAddress.toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

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
    //device pin value
    uint8_t pinbytes[6] = {0x00,0x00,0x00,0x00,0x00,0x00};
    uint8_t* pinValue = pinbytes;
    
    //access to characteristic require to write PIN
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
    Serial.println("value of characteristic:");
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
   pBLEScan->start(10); //scanning for 10 secounds   
}

//JSON handling functions prototypes
bool readFromJSON();
void writeToJSON();

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("ConfigCallback Activated");
  myWiFiManager->resetSettings();
  delay(500);
  btleSetup();
}

//MQTT subscriber
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  
   WiFiManager wifiManager;
   //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
   wifiManager.setAPCallback(configModeCallback);
   //wifiManager.resetSettings(); //reset saved settings
   wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
   
   //file system handling
    bool result = SPIFFS.begin();
    Serial.println("SPIFFS is opened:" + result);
   
    if(!readFromJSON()) {
     Serial.println("Failed to read config file, using default values");
   }
   btleSetup();
    
   //creatingWiFiManagerParams
   WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server.c_str(), 40); //m20.cloudmqtt.com
   WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port.c_str(), 6); // 10905 
   WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user.c_str(), 20); //udnnellq 
   WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass.c_str(), 20); //594Vf9OfDZ-6 
   WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", mqtt_topic.c_str(), 40);
   WiFiManagerParameter macDevice("macdevice1", "Device MAC address", macDeviceOne.c_str(), 40);
   WiFiManagerParameter hint("<small>* Tip: In order to repeat BT scan plug in ESP one more time</small>");
   WiFiManagerParameter scanningDevices("<h3> Wait bluetooth is scanning devices now...</h3>");
   WiFiManagerParameter searchedDevices("<h3> Device with MAC was found! </h3>");
   WiFiManagerParameter line("<br>");
   
    //adding custom html to config page
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);
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
    if(wifiManager.autoConnect("ESP 32 - test","wojtek2468")) { //After this we are connected to wifi network    
      mqtt_server = custom_mqtt_server.getValue();
      mqtt_port = custom_mqtt_port.getValue();
      mqtt_user = custom_mqtt_user.getValue();
      mqtt_pass = custom_mqtt_pass.getValue();
      mqtt_topic = custom_mqtt_topic.getValue();
      
      writeToJSON(); //to JSON writing  
      Serial.println("connected...yeey :)"); 
      delay(3000);
      
      if (doConnect == true && (macDevice.getValue() == macDeviceOne)) {
        BLEAddress addressToConnect = BLEAddress(macDevice.getValue()); //creating instance of BLEAddress -> contains searched MAC address
        if (connectToServer(addressToConnect)) { //connecting to BT remote server -> we have our MAC address in textbox
          Serial.println("We are now connected to the BLE Server.");
          connected = true;
          delay(3000);
        } else {
          Serial.println("We have failed to connect to the server; there is nothin more we will do.");
          wifiManager.resetSettings();
          delay(1000);
          ESP.restart();
        }
        doConnect = false;
      } else {
        wifiManager.resetSettings();
        delay(1000);
        ESP.restart();
      }
  
      if(connected) {  
        //Set CloudMQTT access
        client.setServer(custom_mqtt_server.getValue(), atoi(custom_mqtt_port.getValue()));
   
        //serwer mqqt
        while (!client.connected()) {
        Serial.println("Connecting to MQTT...");
        if (client.connect("ESP32Client", custom_mqtt_user.getValue(), custom_mqtt_pass.getValue() )) {
          Serial.println("connected");
          std::string characterstic = pRemoteCharacteristic->readValue();
          Serial.println("value of set temp: ");
          float temperature = (float)characterstic[1];
          float dividedTemperature = temperature / 2.00F;
          Serial.println(dividedTemperature);
          char temperatureToPublish[8];
          Serial.println(dtostrf(dividedTemperature,6,2,temperatureToPublish));
          client.publish(mqtt_topic.c_str(), dtostrf(dividedTemperature,6,2,temperatureToPublish));     
        } else {
          Serial.print("failed with state ");
          Serial.print(client.state());
          wifiManager.resetSettings();
          delay(2000);
          ESP.restart();
        }
      }
          
      } else {
        wifiManager.resetSettings();
        delay(2000);
        ESP.restart(); 
      }
    
   } else {
      writeToJSON();
      wifiManager.resetSettings();
      delay(2000);
      ESP.restart(); 
    }

  delay(10000); 
  ESP.restart();
}
  

void loop() {
        
}

void writeToJSON() {
  Serial.println("Saving to JSON");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
 
  //assigned parameters from program to JSON file
  json["mqtt_server"] = mqtt_server;
  json["mqtt_port"] = mqtt_port;
  json["mqtt_user"] = mqtt_user;
  json["mqtt_pass"] = mqtt_pass;
  json["mqtt_topic"] = mqtt_topic;
 
  //save JSON parameters to file
  File f = SPIFFS.open(CONFIG_FILE, "w");
  if(!f) {
    Serial.println("Can't open JSON file");
    return;
  }

  json.prettyPrintTo(Serial);
  json.printTo(f);
  f.close();

  Serial.println("All parameters saved succesfully");
}

bool readFromJSON() {

  File f = SPIFFS.open(CONFIG_FILE, "r");
  if(!f) {
    Serial.println("Can't open JSON file");
    return false;
  }
  else
  {
    size_t size = f.size();
    std::unique_ptr<char[]> buf(new char[size]); //buffor for our data
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
    if(json.containsKey("mqtt_server")) mqtt_server = json["mqtt_server"].as<String>();
    if(json.containsKey("mqtt_port")) mqtt_port = json["mqtt_port"].as<String>();
    if(json.containsKey("mqtt_user")) mqtt_user = json["mqtt_user"].as<String>();
    if(json.containsKey("mqtt_pass")) mqtt_pass = json["mqtt_pass"].as<String>();
    if(json.containsKey("mqtt_topic")) mqtt_topic = json["mqtt_topic"].as<String>();
  }

  Serial.println("Read from JSON succesfully");
  return true;
}



