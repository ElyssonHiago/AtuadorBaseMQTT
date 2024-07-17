#ifndef CONNECT_DATA_H
#define CONNECT_DATA_H

const String topic_name = "/bomba1";


EspMQTTClient client(
  "SSID",
  "WIFI_PASSWORD",
  "IP_BROKER_ADDRES",  // MQTT Broker server ip
  "BROKER_LOGIN",   // Can be omitted if not needed
  "BROKER_PASSWORD",   // Can be omitted if not needed
  "TestClient",     // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);

#endif