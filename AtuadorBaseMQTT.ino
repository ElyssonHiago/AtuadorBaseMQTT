#include "EspMQTTClient.h"
#include <ArduinoJson.h>
#include "ConnectDataIFRN.h"
#include "ConfigHA.h"

#define NDEF_DEBUG false
#include <Arduino.h>
#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>

#define DATA_INTERVAL 10000       // Intervalo para adquirir novos dados do sensor (milisegundos).
// Os dados serão publidados depois de serem adquiridos valores equivalentes a janela do filtro
#define AVAILABLE_INTERVAL 5000  // Intervalo para enviar sinais de available (milisegundos)
#define READ_SENSOR_INTERVAL 2000  // Intervalo para enviar sinais de available (milisegundos)
#define LED_INTERVAL_MQTT 1000        // Intervalo para piscar o LED quando conectado no broker
#define JANELA_FILTRO 1         // Número de amostras do filtro para realizar a média

//declaracao de objetos para uso na 
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);

byte RESET_PIN = 14;
byte ACIONAMENTO_PIN = 13;
byte BUZZER_PIN = 12;
byte BLINK_PIN = LED_BUILTIN;


unsigned long dataIntevalPrevTime = 0;      // will store last time data was send
unsigned long availableIntevalPrevTime = 0; // will store last time "available" was send
unsigned long sensorReadTimePrev = 0;        // irá amazenar a última vez que o sensor for lido

String idChave;
HomeAssistant HA;  //trata da configuuração dos dispositivos no home assistant

void setup()
{
  Serial.begin(115200);

  pinMode(BLINK_PIN, OUTPUT);
  pinMode(ACIONAMENTO_PIN, OUTPUT); // Sets the trigPin as an Output
  pinMode(BUZZER_PIN, OUTPUT); // Sets the echoPin as an Input  
  pinMode(RESET_PIN, OUTPUT);

  digitalWrite(RESET_PIN, LOW);
  delay(1000);
  digitalWrite(RESET_PIN, HIGH);
  delay(1000);
  

  nfc.begin(); //inicia o leitor de nfc/rfid

  // Optional functionalities of EspMQTTClient
  //client.enableMQTTPersistence();
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  client.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
  client.enableLastWillMessage("cm/porta/porta_BCDDC22B1561/available", "offline");  // You can activate the retain flag by setting the third parameter to true
  //client.setKeepAlive(8); 
  WiFi.mode(WIFI_STA);
}

void atuador(const String payload) {

  if(payload == "UNLOCK"){
    digitalWrite(ACIONAMENTO_PIN, HIGH);
    delay(500);
    digitalWrite(ACIONAMENTO_PIN, LOW);
    Serial.println("abertura realizada");
  }
  else{
    //Ativar buzzer
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("Sem autorização");
  }

}

void onConnectionEstablished()
{

  HA.init();
  HA.haDiscovery(); 
  client.subscribe(topic_name +"/"+ client.getMqttClientName()+"/cmd", atuador);

  availableSignal();

}

void availableSignal() {
  client.publish(topic_name +"/"+ client.getMqttClientName() + "/available", "online");
}

bool readSensor() {
  
  if (nfc.tagPresent(10)){
    NfcTag tag = nfc.read();
    idChave = tag.getUidString();
    return true;
  }
  return false;
}

void metodoPublisher() {

  StaticJsonDocument<300> jsonDoc;

  jsonDoc["RSSI"] = WiFi.RSSI();
  jsonDoc["state"] = "LOCKED";

  if( nfc.isInitiated())
    jsonDoc["erro"] = false;
  else
    jsonDoc["erro"] = true;

  String payload = "";
  serializeJson(jsonDoc, payload);

  client.publish(topic_name+"/"+client.getMqttClientName(), payload);
}

void sendChave(){

  StaticJsonDocument<300> jsonDoc;

  jsonDoc["RSSI"] = WiFi.RSSI();
  jsonDoc["idChave"] = idChave;
  jsonDoc["idFechadura"] = client.getMqttClientName();

  String payload = "";
  serializeJson(jsonDoc, payload);

  client.publish(topic_name+"/acesso", payload);

}

void blinkLed() {
  static unsigned long ledWifiPrevTime = 0;
  static unsigned long ledMqttPrevTime = 0;
  unsigned long time_ms = millis();
  bool ledStatus = false;

  if ( (WiFi.status() == WL_CONNECTED)) {
    if (client.isMqttConnected()) {
      if ( (time_ms - ledMqttPrevTime) >= LED_INTERVAL_MQTT) {
        ledStatus = !digitalRead(BLINK_PIN);
        digitalWrite(BLINK_PIN, ledStatus);
        ledMqttPrevTime = time_ms;
      }
    }
    else {
      digitalWrite(BLINK_PIN, LOW); //liga led
    }
  }
  else {
    digitalWrite(BLINK_PIN, HIGH); //desliga led
  }
}

void loop()
{
  unsigned long time_ms = millis();
  
  client.loop();

  if (time_ms - dataIntevalPrevTime >= DATA_INTERVAL) {
    client.executeDelayed(1 * 100, metodoPublisher);
    dataIntevalPrevTime = time_ms;
  }

  if (time_ms - availableIntevalPrevTime >= AVAILABLE_INTERVAL) {
    client.executeDelayed(1 * 500, availableSignal);
    availableIntevalPrevTime = time_ms;
  }

  if(readSensor() && (time_ms - sensorReadTimePrev >= READ_SENSOR_INTERVAL) ){
    client.executeDelayed(1 * 100, sendChave);
    sensorReadTimePrev = time_ms;
  }

  blinkLed();

}
