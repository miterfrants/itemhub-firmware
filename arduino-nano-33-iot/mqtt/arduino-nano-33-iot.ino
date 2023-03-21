#include <string>
#include <vector>
#include <WiFiNINA.h>
#include <ArduinoBearSSL.h>
#include <PubSubClient.h>

#include "ItemhubUtilities/ItemhubUtilities.h"
#include "ItemhubUtilities/Certs.h"

#define CLIENT_ID "{CLIENT_ID}"
#define CLIENT_SECRET "{CLIENT_SECRET}"
#define WIFI_SSID "{SSID}"
#define WIFI_PWD "{WIFI_PASSWORD}"
#define DEVICE_ID "{DEVICE_ID}"
#define DOMAIN "{DOMAIN}"

WiFiClient wifiClient;
BearSSLClient client(wifiClient, TAs, TAs_NUM);
PubSubClient mqttClient(client);
std::vector<ItemhubPin> pins;

const int intervalSensor = 1000 * 30;
const int intervalDeviceState = 1000 * 2;
unsigned long currentSensorTimestamp;
unsigned long previousSensorTimestamp;
unsigned long currentDeviceStateTimestamp;
unsigned long previousDeviceStateTimestamp;

void setup()
{
  Serial.begin(115200);
  {PINS};

  ArduinoBearSSL.onGetTime(getTime);
  client.setInsecure(BearSSLClient::SNI::Insecure);

  mqttClient.setServer(DOMAIN, 8883);
  mqttClient.setCallback(callback);
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    connectWiFi();
  }

  if (!mqttClient.connected())
  {
    connectMqtt();
  }

  mqttClient.loop();

  // itemhub device state
  currentDeviceStateTimestamp = millis();
  if (currentDeviceStateTimestamp - previousDeviceStateTimestamp > intervalDeviceState)
  {
    previousDeviceStateTimestamp = currentDeviceStateTimestamp;
    ItemhubUtilities::OnlineOverMQTT(mqttClient);
  }

  // itemhub sensor
  currentSensorTimestamp = millis();
  if (currentSensorTimestamp - previousSensorTimestamp > intervalSensor)
  {
    previousSensorTimestamp = currentSensorTimestamp;
    for (int i = 0; i < pins.size(); i++)
    {
      std::string mode = pins[i].mode;
      if (mode == SENSOR)
      {
        int sensorValue = 2;
        std::string valueString = std::to_string(sensorValue);
        pins[i].value = valueString;
        ItemhubUtilities::SendSensorDataOverMQTT(mqttClient, pins[i]);
      }
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println("");
  std::string result = "";
  std::string topicStringType(topic);
  for (int i = 0; i < length; i++)
  {
    result += (char)payload[i];
  }
  ItemhubUtilities::UpdateSwitchFromMQTT(topicStringType, result, pins);
}

void connectWiFi()
{
  Serial.print("Attempting to connect to SSID: ");
  Serial.print(WIFI_SSID);
  Serial.print(" ");

  while (WiFi.begin(WIFI_SSID, WIFI_PWD) != WL_CONNECTED)
  {
    // failed, retry
    Serial.print(".");
    delay(5000);
  }
  Serial.println();

  Serial.println("You're connected to the network");
  Serial.println(WiFi.localIP());
  Serial.println();
}

void connectMqtt()
{
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(CLIENT_ID, CLIENT_ID, CLIENT_SECRET))
    {
      Serial.println("MQTT connected");
      bindingSwitches();
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.println(mqttClient.state());
      Serial.print("TLS Status: ");
      Serial.println(client.connected());
      Serial.print("TLS Error: ");
      Serial.println(client.errorCode());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void bindingSwitches()
{
  for (int i = 0; i < pins.size(); i++)
  {
    std::string mode = pins[i].mode;
    if (mode == SWITCH)
    {
      std::string topic = DEVICE_ID;
      topic.append("/");
      topic.append(pins[i].pinString);
      topic.append("/switch");
      mqttClient.subscribe(topic.c_str());
    }
  }
}

unsigned long getTime()
{
  // Get the current time from the WiFi module
  return WiFi.getTime();
}
