#include <string>
#include <vector>
#include "MQTT/MQTT.h"
#include "ItemhubUtilities/ItemhubUtilities.h"
#include "ItemhubUtilities/Certs.h"

#define CLIENT_ID "{CLIENT_ID}"
#define CLIENT_SECRET "{CLIENT_SECRET}"
#define DEVICE_ID "{DEVICE_ID}"
#define DOMAIN "{DOMAIN}"

MQTT client(CA, DOMAIN, 8883, callback);
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
}

void loop()
{
  if (!client.isConnected())
  {
    connectMqtt();
  }
  client.loop();

  // itemhub device state
  currentDeviceStateTimestamp = millis();
  if (currentDeviceStateTimestamp - previousDeviceStateTimestamp > intervalDeviceState)
  {
    previousDeviceStateTimestamp = currentDeviceStateTimestamp;
    ItemhubUtilities::OnlineOverMQTT(client);
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
        ItemhubUtilities::SendSensorDataOverMQTT(client, pins[i]);
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

void connectMqtt()
{
  char err_buf[256];
  while (!client.isConnected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(CLIENT_ID, CLIENT_ID, CLIENT_SECRET))
    {
      Serial.println("MQTT connected");
      bindingSwitches();
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.println(client.isConnected());
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
      client.subscribe(topic.c_str());
    }
  }
}