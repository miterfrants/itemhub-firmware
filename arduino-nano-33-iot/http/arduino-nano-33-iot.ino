#include <ArduinoHttpClient.h>
#include <ArduinoBearSSL.h>
#include <WiFiNINA.h>
#include <MemoryFree.h>
#include <vector>
#include <string>

#include "tiny-json.h"
#include "ItemhubUtilities/ItemhubUtilities.h"
#include "ItemhubUtilities/Certs.h"

#define _DEBUG_ true
#define SWITCH "SWITCH"
#define SENSOR "SENSOR"
#define WIFI_SSID "{SSID}"
#define WIFI_PWD "{WIFI_PASSWORD}"
#define USER "{CLIENT_ID}"
#define PWD "{CLIENT_SECRET}"
#define DEVICE_ID "{DEVICE_ID}"
#define DOMAIN "{DOMAIN}"
#define PORT 443

WiFiClient wifiClient;
BearSSLClient client(wifiClient, TAs, TAs_NUM);

bool printWebData = true;
int wifiStatus = WL_IDLE_STATUS;

const int intervalSensor = 30 * 1000;
const int intervalSwitch = 2000;
const int intervalDeviceState = 100;

int timeoutCount = 0;
long loopCount = 0;

unsigned long currentSensorTimestamp;
unsigned long previousSensorTimestamp;
unsigned long currentDeviceStateTimestamp;
unsigned long previousDeviceStateTimestamp;
unsigned long currentSwitchTimestamp;
unsigned long previousSwitchTimestamp;

char token[200];
char remoteDeviceId[12];
std::string projectName;
std::string tokenHeader;
std::vector<ItemhubPin> pins;
bool isAuthing = false;
bool isBrSslIoError = false;
bool isRestart = false;
int reconnectCount = 0;

void setup()
{
    delay(5000);
    Serial.begin(115200);

    {PINS};

    isRestart = true;
}

void loop()
{
    if (reconnectCount >= 10)
    {
        // reboot;
        NVIC_SystemReset();
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        connectWifi();
    }

    if (isBrSslIoError)
    {
        isBrSslIoError = false;
        WiFi.disconnect();
        wifiClient.flush();
        wifiClient.stop();
    }

    // itemhub authorized
    while (strlen(remoteDeviceId) == 0)
    {
        Serial.println("auth");
        if (!isAuthing)
        {
            isAuthing = true;
            char postBody[256];
            strcpy(postBody, "{\"clientId\":\"");
            strcat(postBody, USER);
            strcat(postBody, "\",\"clientSecret\":\"");
            strcat(postBody, PWD);
            strcat(postBody, "\"}");
            AuthResponse authResponse = ItemhubUtilities::Auth(client, DOMAIN, PORT, postBody, isBrSslIoError, reconnectCount);
            strcpy(token, authResponse.token);
            strcpy(remoteDeviceId, authResponse.remoteDeviceId);
            isAuthing = false;
        }
    }

    if (isRestart)
    {
        Serial.println("log");
        isRestart = false;
        char logMessage[128];
        strcpy(logMessage, "{\"message\":\"");
        strcat(logMessage, "restart");
        strcat(logMessage, "\",\"deviceId\":\"");
        strcat(logMessage, DEVICE_ID);
        strcat(logMessage, "\"}");
        Serial.println(logMessage);
        ItemhubUtilities::Log(client, DOMAIN, PORT, token, logMessage, isBrSslIoError, reconnectCount);
    }

    // itemhub device state
    currentDeviceStateTimestamp = millis();
    if (currentDeviceStateTimestamp - previousDeviceStateTimestamp > intervalDeviceState)
    {
        previousDeviceStateTimestamp = currentDeviceStateTimestamp;
        ItemhubUtilities::Online(client, DOMAIN, PORT, remoteDeviceId, token, isBrSslIoError, reconnectCount);
    }

    // itemhub switch
    currentSwitchTimestamp = millis();
    if (currentSwitchTimestamp - previousSwitchTimestamp > intervalSwitch)
    {
        Serial.println(F("check switch state"));
        previousSwitchTimestamp = currentSwitchTimestamp;
        ItemhubUtilities::CheckSwitchState(client, DOMAIN, PORT, token, remoteDeviceId, pins, isBrSslIoError, reconnectCount);
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

                // int value = digitalRead(pins[i].pin);
                std::string valueString = std::to_string(1.00);
                char *value = (char *)valueString.c_str();
                pins[i].value = value;
            }
        }
        ItemhubUtilities::SendSensor(client, DOMAIN, PORT, token, remoteDeviceId, pins, isBrSslIoError, reconnectCount);
    }
}

void connectWifi()
{

    while (WiFi.begin(WIFI_SSID, WIFI_PWD) != WL_CONNECTED)
    {
        Serial.print(".");
        delay(5000);
    }
    Serial.println(".");
    IPAddress ip = WiFi.localIP();
    Serial.println(ip);
    ArduinoBearSSL.onGetTime(getTime);
}

unsigned long getTime()
{
    return WiFi.getTime();
}