#include <string>
#include <vector>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "ItemhubUtilities/ItemhubUtilities.h"
#include "ItemhubUtilities/Certs.h"

#define SWITCH "SWITCH"
#define SENSOR "SENSOR"
#define WIFI_SSID "{SSID}"
#define WIFI_PWD "{WIFI_PASSWORD}"
#define USER "{CLIENT_ID}"
#define PWD "{CLIENT_SECRET}"

std::string host = "itemhub.io";
std::string remoteDeviceId;
std::string token;
WiFiClientSecure client;
char *ca(CA_PEM);
std::vector<ItemhubPin> pins;
const int intervalSensor = 30 * 1000;
const int intervalSwitch = 2000;
const int intervalDeviceState = 2000;

unsigned long currentSensorTimestamp;
unsigned long previousSensorTimestamp;
unsigned long currentSwitchTimestamp;
unsigned long previousSwitchTimestamp;
unsigned long currentDeviceStateTimestamp;
unsigned long previousDeviceStateTimestamp;

void setup()
{
  delay(5000);
  Serial.begin(115200);
  {PINS};

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  while (remoteDeviceId.length() == 0)
  {
    std::string postBody = "{\"clientId\":\"";
    postBody.append(USER);
    postBody.append("\",\"clientSecret\":\"");
    postBody.append(PWD);
    postBody.append("\"}");
    AuthResponse authResponse = ItemhubUtilities::Auth(client, ca, host, postBody);
    token = authResponse.token;
    remoteDeviceId = authResponse.remoteDeviceId;
  }
}

void loop()
{
  // itemhub device state
  currentDeviceStateTimestamp = millis();
  if (currentDeviceStateTimestamp - previousDeviceStateTimestamp > intervalDeviceState)
  {
    previousDeviceStateTimestamp = currentDeviceStateTimestamp;
    ItemhubUtilities::Online(client, ca, host, remoteDeviceId, token);
  }

  // itemhub switch
  currentSwitchTimestamp = millis();
  if (currentSwitchTimestamp - previousSwitchTimestamp > intervalSwitch)
  {
    previousSwitchTimestamp = currentSwitchTimestamp;
    ItemhubUtilities::CheckSwitchState(client, ca, host, token, remoteDeviceId, pins);
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
        int value = digitalRead(pins[i].pin);
        std::string valueString = std::to_string(value);
        pins[i].value = valueString;
      }
    }
    ItemhubUtilities::SendSensor(client, ca, host, token, remoteDeviceId, pins);
  }
}
