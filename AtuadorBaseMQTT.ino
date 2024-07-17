#include "EspMQTTClient.h"
#include <ArduinoJson.h>

#include "ConnectDataIFRN.h"


#define DATA_INTERVAL 10000       // Intervalo para adquirir novos dados do sensor (milisegundos).
// Os dados serão publidados depois de serem adquiridos valores equivalentes a janela do filtro
#define AVAILABLE_INTERVAL 5000  // Intervalo para enviar sinais de available (milisegundos)
#define LED_INTERVAL_MQTT 1000        // Intervalo para piscar o LED quando conectado no broker
#define JANELA_FILTRO 1         // Número de amostras do filtro para realizar a média

byte VALVULA1_PIN = 12;
byte VALVULA2_PIN = 14;


unsigned long dataIntevalPrevTime = 0;      // will store last time data was send
unsigned long availableIntevalPrevTime = 0; // will store last time "available" was send

void setup()
{
 
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VALVULA1_PIN, OUTPUT); // Sets the trigPin as an Output
  pinMode(VALVULA2_PIN, OUTPUT); // Sets the echoPin as an Input

  // Optional functionalities of EspMQTTClient
  //client.enableMQTTPersistence();
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  client.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
  client.enableLastWillMessage(TOPIC_AVAILABLE , "offline");  // You can activate the retain flag by setting the third parameter to true
  //client.setKeepAlive(8);
  WiFi.mode(WIFI_STA);
}

void valvula1(const String payload) {

  if (payload == "ON") {
    digitalWrite(VALVULA1_PIN, HIGH); //Liga o dispositivo
  }
  else {
    digitalWrite(VALVULA1_PIN, LOW); //Desliga o dispositivo
  }
  
  client.executeDelayed(1 * 100, metodoPublisher);
}

void valvula2(const String payload) {
  if (payload == "ON") {
    digitalWrite(VALVULA2_PIN, HIGH); //Controle pelo sistema
  }
  else {
    digitalWrite(VALVULA2_PIN, LOW); //Controle local
  }

  client.executeDelayed(1 * 100, metodoPublisher);
}

void onConnectionEstablished()
{
  client.subscribe(topic_name + "/valv1/set", valvula1);
  client.subscribe(topic_name + "/valv2/set", valvula2);

  availableSignal();
}

void availableSignal() {
  client.publish(TOPIC_AVAILABLE, "online");
}

void metodoPublisher() {

  StaticJsonDocument<300> jsonDoc;
  
  jsonDoc["RSSI"]     = WiFi.RSSI();
  jsonDoc["valvula1"] = digitalRead(VALVULA1_PIN)==1 ? "ON" : "OFF";
  jsonDoc["valvula2"] = digitalRead(VALVULA2_PIN)==1 ? "ON" : "OFF";
  jsonDoc["erro"]     = false;
  jsonDoc["invalido"] = false;
  jsonDoc["heap"]     = ESP.getFreeHeap();
  jsonDoc["stack"]    = ESP.getFreeContStack();

  String payload = "";
  serializeJson(jsonDoc, payload);

  client.publish(topic_name, payload);
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

  blinkLed();

}
