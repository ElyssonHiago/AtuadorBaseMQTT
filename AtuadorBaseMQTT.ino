#include "EspMQTTClient.h"
#include <ArduinoJson.h>
#include "EmonLib.h"
#include "ConnectDataIFRN.h"

#define DATA_INTERVAL 10000       // Intervalo para adquirir novos dados do sensor (milissegundos).
#define AVAILABLE_INTERVAL 5000  // Intervalo para enviar sinais de "disponível" (milissegundos)
#define LED_INTERVAL_MQTT 1000   // Intervalo para piscar o LED quando conectado no broker
#define JANELA_FILTRO 1          // Número de amostras do filtro para calcular a média

byte VALVULA_PIN = 7;
byte CONTROLE_SISTEMA_PIN = 14;

unsigned long dataIntevalPrevTime = 0;      // Armazena a última vez que os dados foram enviados
unsigned long availableIntevalPrevTime = 0; // Armazena a última vez que "disponível" foi enviado

EnergyMonitor emon1;

void setup()
{
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ACIONAMENTO_PIN, OUTPUT); // Define o pino de acionamento como uma saída
  pinMode(CONTROLE_SISTEMA_PIN, OUTPUT); // Define o pino de controle do sistema como uma saída

  // emon1.current(0, 30 / (3 * 0.75)); // Não iremos usar

  // Funcionalidades opcionais do EspMQTTClient
  // cliente.enableMQTTPersistence();
  client.enableDebuggingMessages(); // Habilita mensagens de depuração enviadas para a saída serial
  client.enableHTTPWebUpdater(); // Habilita o atualizador web. Usuário e senha padrão são valores de MQTTUsername e MQTTPassword. Estes podem ser substituídos com enableHTTPWebUpdater("user", "password").
  client.enableOTA(); // Habilita atualizações OTA (Over The Air). A senha padrão é MQTTPassword. A porta é a porta OTA padrão. Pode ser substituída com enableOTA("password", port).
  client.enableLastWillMessage("/bomba1/available", "offline"); // Você pode ativar a flag retain definindo o terceiro parâmetro como true
  // client.setKeepAlive(8);
  WiFi.mode(WIFI_STA);
}

void atuador(const String payload) {

  if (payload == "ON") {
    digitalWrite(VALVULA_PIN, HIGH); // Liga o dispositivo
    client.executeDelayed(1 * 100, metodoPublisher);
  }
  else {
    digitalWrite(VALVULA_PIN, LOW); // Desliga o dispositivo
    client.executeDelayed(1 * 100, metodoPublisher);
  }
}

void controleLocal(const String payload) {
  if (payload == "ON") {
    digitalWrite(CONTROLE_SISTEMA_PIN, HIGH); // Controle pelo sistema
    client.publish(topic_name + "/controle_sistema", "ON");
  }
  else {
    digitalWrite(CONTROLE_SISTEMA_PIN, LOW); // Controle local
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
  static unsigned int amostras = 0;  // Variável para realizar o filtro de média
  static float acumulador = 0;       // Variável para acumular a média

  acumulador += readSensor();

  if (amostras >= JANELA_FILTRO) {
    StaticJsonDocument<300> jsonDoc;
    float corrente = acumulador / JANELA_FILTRO;

    jsonDoc["RSSI"] = WiFi.RSSI();
    jsonDoc["corrente"] = corrente;

    if (corrente >= 1) {
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

  if (WiFi.status() == WL_CONNECTED) {
    if (client.isMqttConnected()) {
      if ((time_ms - ledMqttPrevTime) >= LED_INTERVAL_MQTT) {
        ledStatus = !digitalRead(LED_BUILTIN);
        digitalWrite(LED_BUILTIN, ledStatus);
        ledMqttPrevTime = time_ms;
      }
    }
    else {
      digitalWrite(LED_BUILTIN, LOW); // Liga o LED quando não está conectado ao MQTT
    }
  }
  else {
    digitalWrite(LED_BUILTIN, HIGH); // Desliga o LED quando não está conectado ao Wi-Fi
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
