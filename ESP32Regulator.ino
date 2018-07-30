#include <Preferences.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include "BLEDevice.h"
#include <PubSubClient.h>




//pierwotna wersja
//#define mqtt_server       "mqtt_server"
//#define mqtt_port         "mqtt_port"
//#define mqtt_user         "mqtt_user"
//#define mqtt_pass         "mqtt_pass"
//#define mqtt_topic        "topic"

//parametry na sztywno
//const char* mqtt_server = "m20.cloudmqtt.com";
//int mqtt_port = 10905;
//const char* mqtt_user  = "udnnellq";
//const char* mqtt_pass  = "594Vf9OfDZ-6";

//const char* mqtt_server = NULL;
//const char* mqtt_port = NULL; //docelowo musi byc uint16 (moze byc int)
//const char* mqtt_user = NULL;
//const char* mqtt_pass = NULL;
//const char* mqtt_topic = NULL;

bool if_save = true; //parametr do checkboxa

//obsluga clienta MQTT
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
    String page;
    Serial.begin(115200);
   //tworzenie checkboxa w html
   char customhtml[24] = "type=\"checkbox\"";
   if(if_save) strcat(customhtml, " checked"); //jezeli checkbox true to dopisujemy stan "checked"

   //obsluga listy z numerami MAC termoregulatorow
   //funkcja zwraca wybrana wartosc pola listy select do pola tekstowego (funkcja1())
  
   // po wcisnieciu button przepisuje wartosc do pola tekstowego
   WiFiManagerParameter skrypt_java("<script language=\"JavaScript\"> function funkcja1(){var asd=document.getElementById('select1').options[document.getElementById('select1').selectedIndex].value; var macOutput=document.getElementById(\"macOutput\"); macOutput.value=asd;}</script>");
   WiFiManagerParameter mac_searcher("<select id=\"select1\"> <option value=\"0\">0</option> <option value=\"1\">1</option> <option value=\"2\">2</option></select><input type=\"text\" id=\"macOutput\"/><input type=\"button\" value=\"Pobierz\" onclick=\"funkcja1()\">");
   
    
   //Tworzenie parametrow na stronie konfiguracyjnej
   WiFiManagerParameter custom_mqtt_server("server", "mqtt server", NULL, 40);
   WiFiManagerParameter custom_mqtt_port("port", "mqtt port", NULL, 6);
   WiFiManagerParameter custom_mqtt_user("user", "mqtt user", NULL, 20);
   WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", NULL, 20);
   WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", NULL, 40);
   WiFiManagerParameter hint("<small>*Rada:jezeli chcesz zapisac parametry serwera MQTT zaznacz checkboxa</small>");
   WiFiManagerParameter save_checkbox("savebox","Zapisac?","T",2,customhtml);
  
    WiFiManager wifiManager;

    //wifiManager.setCustomHeadElement("<script language=\"JavaScript\"> function funkcja1(){var asd=document.getElementById('select1').options[document.getElementById('select1').selectedIndex].value var macOutput=document.getElementById(\"macOutput\"); macOutput.value=asd;}</script>");
    //reset saved settings
    //wifiManager.resetSettings(); 
    //set custom ip for portal
    wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

    //dodanie pol 
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);
    wifiManager.addParameter(&custom_mqtt_topic);
    wifiManager.addParameter(&save_checkbox);
    wifiManager.addParameter(&hint);
    wifiManager.addParameter(&mac_searcher);
    wifiManager.addParameter(&skrypt_java);
    //laczenie do AP 
    if(wifiManager.autoConnect("ESP 32 - test","wojtek2468")) {

    Serial.println("connected...yeey :)"); //po tej komendzie jestesmy polaczeni z wifi

//kopiowanie wartosci z pol w formularzu do zmiennych (wersja 1)    
//    strcpy(mqtt_server, custom_mqtt_server.getValue());
//    strcpy(mqtt_port, custom_mqtt_port.getValue());
//    strcpy(mqtt_user, custom_mqtt_user.getValue());
//    strcpy(mqtt_pass, custom_mqtt_pass.getValue());
//    strcpy(mqtt_topic, custom_mqtt_topic.getValue());

    if_save = (strncmp(save_checkbox.getValue(),"T",1) == 0); //0==0, if_save = true -> odczytana wczensiej wartosc (z JSONA) jest taka sama, checkbox zaznaczony;

    //ustawienie serwera cloudmqtt
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
 
    }
  }

  client.publish(custom_mqtt_topic.getValue(), "wersja 2");
  
  } //zamkniecie if(wifiManager.autoConnect("ESP 32 - test","wojtek2468"))
}

void loop() {
//podtrzymanie polaczenia z mqtt  
client.loop();
}
