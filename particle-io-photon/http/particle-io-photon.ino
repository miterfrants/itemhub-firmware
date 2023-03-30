#define SWITCH "SWITCH"
#define SENSOR "SENSOR"

#include <string>
#include <vector>
#include "ItemhubUtilities/ItemhubUtilities.h"
#include "ItemhubUtilities/Certs.h"

#define USER "{CLIENT_ID}"
#define PWD "{CLIENT_SECRET}"
#define DEVICE_ID "{DEVICE_ID}"
#define DOMAIN "{DOMAIN}"
#define PORT 443

TlsTcpClient client;
unsigned long lastSync = millis();

char remoteDeviceId[12];
char token[200];
std::vector<ItemhubPin> pins;

const int intervalSensor = 30 * 1000;
const int intervalSwitch = 2 * 1000;
const int intervalDeviceState = 2 * 1000;

unsigned long currentSensorTimestamp;
unsigned long previousSensorTimestamp;
unsigned long currentSwitchTimestamp;
unsigned long previousSwitchTimestamp;
unsigned long currentDeviceStateTimestamp;
unsigned long previousDeviceStateTimestamp;

bool isAuthing = false;
bool isSslIoError = false;

void setup()
{
    delay(5000);
    Serial.begin(9600);
    {PINS};
    client.init(CA, strlen(CA) + 1);
    client.connect(DOMAIN, 443);
    if (!client.verify())
    {
        Serial.println("Server Certificates is in-valid.");
        delay(10 * 1000);
    }
}

void loop()
{
    if (isSslIoError)
    {
        client.close();
        client.init(CA, strlen(CA) + 1);
        client.connect(DOMAIN, 443);
        delay(300);
        isSslIoError = false;
    }

    // itemhub authorized
    while (strlen(remoteDeviceId) == 0)
    {
        if (!isAuthing)
        {
            isAuthing = true;
            char postBody[256];
            strcpy(postBody, "{\"clientId\":\"");
            strcat(postBody, USER);
            strcat(postBody, "\",\"clientSecret\":\"");
            strcat(postBody, PWD);
            strcat(postBody, "\"}");
            AuthResponse authResponse = ItemhubUtilities::Auth(client, DOMAIN, PORT, postBody, isSslIoError);
            strcpy(token, authResponse.token);
            strcpy(remoteDeviceId, authResponse.remoteDeviceId);
            isAuthing = false;
        }
    }

    // itemhub device state
    currentDeviceStateTimestamp = millis();
    if (currentDeviceStateTimestamp - previousDeviceStateTimestamp > intervalDeviceState)
    {
        previousDeviceStateTimestamp = currentDeviceStateTimestamp;
        ItemhubUtilities::Online(client, DOMAIN, PORT, remoteDeviceId, token, isSslIoError);
    }

    // // itemhub switch
    currentSwitchTimestamp = millis();
    if (currentSwitchTimestamp - previousSwitchTimestamp > intervalSwitch)
    {
        previousSwitchTimestamp = currentSwitchTimestamp;
        ItemhubUtilities::CheckSwitchState(client, DOMAIN, PORT, token, remoteDeviceId, pins, isSslIoError);
    }

    // // itemhub sensor
    currentSensorTimestamp = millis();
    if (currentSensorTimestamp - previousSensorTimestamp > intervalSensor)
    {
        previousSensorTimestamp = currentSensorTimestamp;
        ItemhubUtilities::SendSensor(client, DOMAIN, PORT, token, remoteDeviceId, pins, isSslIoError);
    }
}