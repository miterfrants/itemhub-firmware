#include <string>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include "ItemhubUtilities/ItemhubUtilities.h"
#include "ItemhubUtilities/Certs.h"

#define brokerClientId "{CLIENT_ID}"
#define brokerUser "{CLIENT_ID}"
#define brokerPwd "{CLIENT_SECRET}"
#define WIFI_SSID "{SSID}"
#define WIFI_PWD "{WIFI_PASSWORD}"
#define MSG_BUFFER_SIZE (50)
#define DEVICE_ID "{DEVICE_ID}"

WiFiClientSecure client;
PubSubClient mqttClient(client);
const char fingerPrint[] PROGMEM = FINGERPRINT;
X509List caCert(CA);
X509List cert(CERT);
PrivateKey clientPrivateKey(PRIVATE_KEY);
std::vector<ItemhubPin> pins;

char msg[MSG_BUFFER_SIZE];

const int intervalSensor = 1000 * 30;
const int intervalDeviceState = 1000 * 2;
unsigned long currentSensorTimestamp;
unsigned long previousSensorTimestamp;
unsigned long currentDeviceStateTimestamp;
unsigned long previousDeviceStateTimestamp;

void setup()
{
  Serial.begin(115200);
  pins.push_back(ItemhubPin(0, "IO0", SWITCH));

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

  // Set time via NTP, as required for x.509 validation
  configTime(3 * 60 * 60, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));

  client.setInsecure();
  client.setTrustAnchors(&caCert);
  client.setClientRSACert(&cert, &clientPrivateKey);
  client.setFingerprint(fingerPrint);

  mqttClient.setServer("itemhub.io", 8883);
  mqttClient.setCallback(callback);
}

void loop()
{
  if (!mqttClient.connected())
  {
    reconnect();
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

void reconnect()
{
  char err_buf[256];
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(brokerClientId, brokerUser, brokerPwd))
    {
      Serial.println("MQTT connected");
      bindingSwitches();
    }
    else
    {

      Serial.print("failed, rc=");
      Serial.println(mqttClient.state());
      client.getLastSSLError(err_buf, sizeof(err_buf));
      Serial.print("error: ");
      Serial.println(err_buf);
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