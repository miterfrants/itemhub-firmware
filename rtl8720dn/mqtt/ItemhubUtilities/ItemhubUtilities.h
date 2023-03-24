#include <string>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include "../tiny-json.h"

#define SWITCH "SWITCH"
#define SENSOR "SENSOR"

enum
{
    MAX_FIELDS = 512
};
json_t pool[MAX_FIELDS];

class ItemhubPin
{
public:
    ItemhubPin(byte pin, std::string pinString, std::string mode)
    {
        this->pin = pin;
        this->pinString = pinString;
        this->mode = mode;
        if (this->mode == "SWITCH")
        {
            pinMode(pin, OUTPUT);
        }
        if (this->mode == "SENSOR")
        {
            Serial.println("sensor");
            Serial.println(pinString.c_str());
            pinMode(pin, INPUT);
        }
    }
    byte pin;
    std::string pinString;
    std::string mode;
    std::string value;
};

class ItemhubUtilities
{
public:
    static void OnlineOverMQTT(PubSubClient &client)
    {
        client.publish("online", "");
    }

    static void SendSensorDataOverMQTT(PubSubClient &client, ItemhubPin &pin)
    {
        std::string endpoint = pin.pinString;
        std::string value = pin.value;
        endpoint.append("/sensor");
        std::string payload = "{\"value\":";
        payload.append(value);
        payload.append("}");
        client.publish(endpoint.c_str(), payload.c_str());
    }

    static void UpdateSwitchFromMQTT(std::string &topic, std::string &payload, std::vector<ItemhubPin> &pins)
    {
        json_t const *jsonData = json_create((char *)payload.c_str(), pool, MAX_FIELDS);
        if (jsonData == NULL)
        {
            Serial.println("json invalid format");
        }
        json_t const *jsonField = json_getProperty(jsonData, "Value");
        const char *value = json_getValue(jsonField);
        String stringValue = String(value);
        int intValue = stringValue.toInt();

        std::string delimiter("/");
        std::string pin(ItemhubUtilities::Split(topic, delimiter)[1].c_str());

        for (int i = 0; i < pins.size(); i++)
        {
            if (pins[i].mode == SENSOR)
            {
                continue;
            }

            if (pins[i].pinString == pin && intValue == 0)
            {
                digitalWrite(pins[i].pin, LOW);
            }
            else if (pins[i].pinString == pin && intValue == 1)
            {
                digitalWrite(pins[i].pin, HIGH);
            }
        }
    }

    static std::vector<std::string> Split(std::string s, std::string delimiter)
    {
        size_t pos_start = 0, pos_end, delim_len = delimiter.length();
        std::string token;
        std::vector<std::string> res;

        while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos)
        {
            token = s.substr(pos_start, pos_end - pos_start);
            pos_start = pos_end + delim_len;
            res.push_back(token);
        }

        res.push_back(s.substr(pos_start));
        return res;
    }
};