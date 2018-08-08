#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include "BLEDevice.h"
#include <PubSubClient.h>

const char* CONFIG_FILE = "/config.json";
bool checkbut_status = false; 
const char* mqtt_server;
const char* mqtt_port; //uint16 but it could be 'int'
const char* mqtt_user;
const char* mqtt_pass;

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
  
}

void loop() {
   //html checkbox
   char customhtml[24] = "type=\"checkbox\"";
   
   if(checkbut_status) {
     strcat(customhtml, " checked"); //only way to handle checkbox state is to add "checked" string
   }

   
   //JavaScript functions
   WiFiManagerParameter skrypt_java("<script language=\"JavaScript\"> function funkcja1(){var asd=document.getElementById('select1').options[document.getElementById('select1').selectedIndex].value; var macOutput=document.getElementById(\"macOutput\"); macOutput.value=asd;}</script>");
   WiFiManagerParameter mac_searcher("<select id=\"select1\"> <option value=\"0\">0</option> <option value=\"1\">1</option> <option value=\"2\">2</option></select><input type=\"text\" id=\"macOutput\"/><input type=\"button\" value=\"Pobierz\" onclick=\"funkcja1()\">");
     
   //Creating custom HTML
   WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
   WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
   WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 20);
   WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass, 20);
   WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", NULL, 40);
   WiFiManagerParameter hint("<small>*Rada:jezeli chcesz zapisac parametry serwera MQTT zaznacz checkboxa</small>");
   WiFiManagerParameter save_checkbox("savebox","Zapisac?","T",2,customhtml);
   WiFiManagerParameter line("<br>");
  
   WiFiManager wifiManager;
   
    //reset saved settings
    wifiManager.resetSettings(); 
    
    wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

    //adding custom html to config page
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);
    wifiManager.addParameter(&custom_mqtt_topic);
    wifiManager.addParameter(&line);
    wifiManager.addParameter(&save_checkbox);
    wifiManager.addParameter(&line);
    wifiManager.addParameter(&hint);
    wifiManager.addParameter(&line);
    wifiManager.addParameter(&mac_searcher);
    wifiManager.addParameter(&line);
    wifiManager.addParameter(&skrypt_java);
    
    //Connecting to AP
    if(wifiManager.autoConnect("ESP 32 - test","wojtek2468")) {

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

    WriteFile();   
    Serial.println("connected...yeey :)"); //After this we are connected to wifi network 
    
    //Set CloudMQTT access
    client.setServer(custom_mqtt_server.getValue(), atoi(custom_mqtt_port.getValue()));
   
    //serwer mqqt
    while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32Client", custom_mqtt_user.getValue(), custom_mqtt_pass.getValue() )) {

      Serial.println("connected");
 
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
      ESP.restart();
      delay(5000);
    }
  }

  client.publish(custom_mqtt_topic.getValue(), "wersja 2");
  
  } else {  //zamkniecie if(wifiManager.autoConnect("ESP 32 - test","wojtek2468"))
    ESP.restart();
    delay(5000);
  }
  
//Holding connection with MQTT 
client.loop();
}

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
    if(json.containsKey("mqtt_server")) mqtt_server = json["mqtt_server"];
    if(json.containsKey("mqtt_port")) mqtt_port = json["mqtt_port"];
    if(json.containsKey("mqtt_user")) mqtt_user = json["mqtt_user"];
    if(json.containsKey("mqtt_pass")) mqtt_pass = json["mqtt_pass"];
    
  }

  Serial.println("Write to JSON succesful");
  return true;
}

