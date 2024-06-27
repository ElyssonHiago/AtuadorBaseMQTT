#include "EspMQTTClient.h"
#include <ArduinoJson.h>
#include "ConnectDataCell.h"

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

byte ACIONAMENTO_PIN = 13;
byte BUZZER_PIN = 12;


unsigned long dataIntevalPrevTime = 0;      // will store last time data was send
unsigned long availableIntevalPrevTime = 0; // will store last time "available" was send
unsigned long sensorReadTimePrev = 0;        // irá amazenar a última vez que o sensor for lido

String idChave;

void setup()
{
  Serial.begin(115200);

  nfc.begin(); //inicia o leitor de nfc/rfid
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ACIONAMENTO_PIN, OUTPUT); // Sets the trigPin as an Output
  pinMode(BUZZER_PIN, OUTPUT); // Sets the echoPin as an Input

  // Optional functionalities of EspMQTTClient
  //client.enableMQTTPersistence();
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  client.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
  client.enableLastWillMessage("/bomba1/available", "offline");  // You can activate the retain flag by setting the third parameter to true
  //client.setKeepAlive(8);
  WiFi.mode(WIFI_STA);
}

void atuador(const String payload) {

  if(payload == "authorized"){
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
  client.subscribe(topic_name +"/"+ client.getMqttClientName(), atuador);

  availableSignal();
}

void availableSignal() {
  client.publish(topic_name + "/available", "online");
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

  String payload = "";
  serializeJson(jsonDoc, payload);

  client.publish(topic_name, payload);
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
        ledStatus = !digitalRead(LED_BUILTIN);
        digitalWrite(LED_BUILTIN, ledStatus);
        ledMqttPrevTime = time_ms;
      }
    }
    else {
      digitalWrite(LED_BUILTIN, LOW); //liga led
    }
  }
  else {
    digitalWrite(LED_BUILTIN, HIGH); //desliga led
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
