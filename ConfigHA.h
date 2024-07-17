
#ifndef CONFIG_HA_H
#define CONFIG_HA_H

#include "EspMQTTClient.h"
#include "ConnectDataIFRN.h"
#include <ArduinoJson.h>


//String topic_config_name = "homeassistant/sensor"+topic_name+"/config";

class HomeAssistant{
  private:

  bool auto_discovery = true;      //default to false and provide end-user interface to allow toggling 

  //Variables for creating unique entity IDs and topics
  byte macAddr[6];                  //Device MAC address
  char uidPrefix[20] = "rctdev";      //Prefix for unique ID generation (limit to 20 chars)
  char devUniqueID[30];             //Generated Unique ID for this device (uidPrefix + last 6 MAC characters) 



  public:

  const String getUniqueID(){
    String hexstring="";

    for(int i = 0; i < 3; i++) {

      if(macAddr[i] < 0x10) {
        hexstring += '0';
      }
      hexstring += String(macAddr[i], HEX);
    
    }
    
    return "porta_"+hexstring;
  }
  

  void createDiscoveryUniqueID() {
    //Generate UniqueID from uidPrefix + last 6 chars of device MAC address
    //This should insure that even multiple devices installed in same HA instance are unique
    
    strcpy(devUniqueID, uidPrefix);
    int preSizeBytes = sizeof(uidPrefix);
    int preSizeElements = (sizeof(uidPrefix) / sizeof(uidPrefix[0]));
    //Now add last 6 chars from MAC address (these are 'reversed' in the array)
    int j = 0;
    for (int i = 2; i >= 0; i--) {
      sprintf(&devUniqueID[(preSizeBytes - 1) + (j)], "%02X", macAddr[i]);   //preSizeBytes indicates num of bytes in prefix - null terminator, plus 2 (j) bytes for each hex segment of MAC
      j = j + 2;
    }
    // End result is a unique ID for this device (e.g. rctdevE350CA) 
    Serial.print("Unique ID: ");
    Serial.println(devUniqueID);
  }

  void haDiscovery() {
    char topic[128];
    if (auto_discovery) {
      char buffer1[512];
      char uid[128];
      DynamicJsonDocument doc(512);
      doc.clear();
  
      Serial.println("Adicionando controle porta...");
      //Create unique topic based on devUniqueID
      strcpy(topic, "homeassistant/lock/");
      strcat(topic, client.getMqttClientName());
      strcat(topic, "/config");
      //Create unique_id based on devUniqueID
      strcpy(uid, getUniqueID().c_str());
      //Create JSON payload per HA documentation
      doc["name"] = String("Porta_") + client.getMqttClientName();
      doc["obj_id"] = "mqtt_porta";
      doc["uniq_id"] = getUniqueID();
      doc["stat_t"] = topic_name+"/"+client.getMqttClientName();
      doc["cmd_t"] = topic_name +"/"+ client.getMqttClientName()+"/cmd";
      doc["value_template"] = "{{ value_json.state }}";
      doc["availability"][0]["topic"] = topic_name +"/"+ client.getMqttClientName()+"/available";
      JsonObject device = doc.createNestedObject("device");
      device["ids"] = "Portas01";
      device["name"] = "Trancas das Portas";
//      device["mf"] = "lennedy";
      device["mdl"] = "ESP8266";
      device["sw"] = "0.01";
      device["hw"] = "0.01";
//      device["cu"] = "http://192.168.1.226/config";  //web interface for device, with discovery toggle
      serializeJson(doc, buffer1);
      //Publish discovery topic and payload (with retained flag)
      client.publish(topic, buffer1, true);
  
    } else {
  
      //Remove all entities by publishing empty payloads
      //Must use original topic, so recreate from original Unique ID
      //This will immediately remove/delete the device/entities from HA
      Serial.println("Removing discovered devices...");
  
      //Lux Sensor
      strcpy(topic, "homeassistant/porta/");
      strcat(topic, client.getMqttClientName());
      strcat(topic, "/config");
      client.publish(topic, "");
  
      Serial.println("Devices Removed...");
    }
  }

  void init(){
    WiFi.macAddress(macAddr);
    createDiscoveryUniqueID();
  }

  
  String getTopicName(){
      String topic_config_name;
      
      topic_config_name = "homeassistant/sensor";
//      topic_config_name += "MAC_";
//      topic_config_name += WiFi.macAddress();
      topic_config_name += "/teste";
      topic_config_name += "/config";
      
      return topic_config_name;
    }

    void getConfigMessage(String &message){
      /*
      StaticJsonDocument<300> jsonDoc;      

      jsonDoc["name"] =  "MQTT teste";
      jsonDoc["state_topic"] = topic_name;
      jsonDoc["manufacturer"] = "IFRN_Lennedy";
      jsonDoc["hw_version"] = "0.5";
      jsonDoc["sw_version"] = "0.2";

      serializeJson(jsonDoc, message);
      */
      DynamicJsonDocument doc(2048);
      doc.clear();
      doc["name"] =  "MQTT teste";
      doc["state_topic"] = topic_name;
      
      JsonObject device = doc.createNestedObject("device");
      device["name"] = "My MQTT Device";
      device["ids"] = "mymqttdevice01";
      device["mf"] = "Resinchem Tech";
      device["mdl"] = "ESP8266";
      device["sw"] = "1.24";
      device["hw"] = "0.45";
      device["cu"] = "http://192.168.1.226/config";
    }
};


#endif
