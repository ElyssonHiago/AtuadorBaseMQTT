#include "EspMQTTClient.h"
#include <ArduinoJson.h>
#include "EmonLib.h"
#include "ConnectDataIFRN.h"


#define DATA_INTERVAL 10000       // Intervalo para adquirir novos dados do sensor (milisegundos).
// Os dados serão publidados depois de serem adquiridos valores equivalentes a janela do filtro
#define AVAILABLE_INTERVAL 5000  // Intervalo para enviar sinais de available (milisegundos)
#define LED_INTERVAL_MQTT 1000        // Intervalo para piscar o LED quando conectado no broker
#define JANELA_FILTRO 1         // Número de amostras do filtro para realizar a média

byte ACIONAMENTO_PIN = 12;
byte CONTROLE_SISTEMA_PIN = 14;


unsigned long dataIntevalPrevTime = 0;      // will store last time data was send
unsigned long availableIntevalPrevTime = 0; // will store last time "available" was send

EnergyMonitor emon1;

void setup()
{
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ACIONAMENTO_PIN, OUTPUT); // Sets the trigPin as an Output
  pinMode(CONTROLE_SISTEMA_PIN, OUTPUT); // Sets the echoPin as an Input

  emon1.current(0, 30 / (3 * 0.75));

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

  if (payload == "ON") {
    digitalWrite(ACIONAMENTO_PIN, HIGH); //Liga o dispositivo
    client.executeDelayed(1 * 100, metodoPublisher);
  }
  else {
    digitalWrite(ACIONAMENTO_PIN, LOW); //Desliga o dispositivo
    client.executeDelayed(1 * 100, metodoPublisher);
  }
}

void controleLocal(const String payload) {
  if (payload == "ON") {
    digitalWrite(CONTROLE_SISTEMA_PIN, HIGH); //Controle pelo sistema
    client.publish(topic_name + "/controle_sistema", "ON");
  }
  else {
    digitalWrite(CONTROLE_SISTEMA_PIN, LOW); //Controle local
    client.publish(topic_name + "/controle_sistema", "OFF");
  }
}

void onConnectionEstablished()
{
  client.subscribe(topic_name + "/set", atuador);
  client.subscribe(topic_name + "/controle_sistema/set", controleLocal);

  availableSignal();
}

void availableSignal() {
  client.publish(topic_name + "/available", "online");
}

float readSensor() {
  double Irms = emon1.calcIrms(1480);

  return Irms;
}

void metodoPublisher() {
  static unsigned int amostras = 0;  //variável para realizar o filtro de média
  static float acumulador = 0;       //variável para acumular a média

  acumulador += readSensor();


  if (amostras >= JANELA_FILTRO) {
    StaticJsonDocument<300> jsonDoc;
    float corrente = acumulador / JANELA_FILTRO;

    jsonDoc["RSSI"] = WiFi.RSSI();
    jsonDoc["corrente"] = corrente;

    if (corrente >= 1 ) {
      jsonDoc["estado"] = "ON";
    }
    else {
      jsonDoc["estado"] = "OFF";
    }

    String payload = "";
    serializeJson(jsonDoc, payload);

    client.publish(topic_name, payload);
    amostras = 0;
    acumulador = 0;

    if (digitalRead(CONTROLE_SISTEMA_PIN) == HIGH)
      client.publish(topic_name + "/controle_sistema", "ON");
    else
      client.publish(topic_name + "/controle_sistema", "OFF");
  }
  amostras++;
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
