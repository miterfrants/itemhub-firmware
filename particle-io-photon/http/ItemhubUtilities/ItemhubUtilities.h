#include "../tiny-json/tiny-json.h"
#include "../TlsTcpClient/TlsTcpClient.h"
#include <vector>
#include <string>
#include <numeric>
#include <sstream>

enum
{
    MAX_FIELDS = 128
};
json_t pool[MAX_FIELDS];

class ItemhubPin
{
public:
    ItemhubPin(byte pin, char *pinString, char *mode)
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
            pinMode(pin, INPUT);
        }
    }
    byte pin;
    char *pinString;
    char *mode;
    char *value;
};

class AuthResponse
{
public:
    AuthResponse(char *token, char *remoteDeviceId)
    {
        strcpy(this->token, token);
        strcpy(this->remoteDeviceId, remoteDeviceId);
    }
    char token[200];
    char remoteDeviceId[12];
};

class ItemhubUtilities
{
public:
    static void Log(TlsTcpClient &client, char *host, int port, char *token, char *postBody, bool &isSslIoError)
    {
        char resp[256];
        ItemhubUtilities::Send(client, host, port, "POST", "/api/v1/log", postBody, token, resp, isSslIoError, "");
    }

    static AuthResponse Auth(TlsTcpClient &client, char *host, int port, char *postBody, bool &isSslIoError)
    {
        char resp[500];
        Send(client, host, port, "POST", "/api/v1/oauth/exchange-token-for-device", postBody, "", resp, isSslIoError, "");
        char token[200];
        char remoteDeviceId[12];
        if (strlen(resp) == 0)
        {
            Serial.println("no response");
            return AuthResponse("", "");
        }

        json_t const *jsonResp = ItemhubUtilities::Extract(resp);
        json_t const *tokenField = json_getProperty(jsonResp, "token");
        json_t const *deviceIdField = json_getProperty(jsonResp, "deviceId");
        strcpy(remoteDeviceId, json_getValue(deviceIdField));
        strcpy(token, json_getValue(tokenField));
        return AuthResponse(token, remoteDeviceId);
    }

    static void Online(TlsTcpClient &client, char *host, int port, char *remoteDeviceId, char *token, bool &isSslIoError)
    {
        char deviceOnlineEndpoint[64];
        strcpy(deviceOnlineEndpoint, "/api/v1/my/devices/");
        strcat(deviceOnlineEndpoint, remoteDeviceId);
        strcat(deviceOnlineEndpoint, "/online");
        char resp[100];
        ItemhubUtilities::Send(client, host, port, "POST", deviceOnlineEndpoint, "", token, resp, isSslIoError, remoteDeviceId);
    }

    static void CheckSwitchState(TlsTcpClient &client, char *host, int port, char *token, char *remoteDeviceId, std::vector<ItemhubPin> &pins, bool &isSslIoError)
    {
        char deviceStateEndpoint[128];
        strcpy(deviceStateEndpoint, "/api/v1/my/devices/");
        strcat(deviceStateEndpoint, remoteDeviceId);
        strcat(deviceStateEndpoint, "/switches");
        char resp[425];
        ItemhubUtilities::Send(client, host, port, "GET", deviceStateEndpoint, "", token, resp, isSslIoError, remoteDeviceId);

        char manuallyBody[500];
        strcpy(manuallyBody, "{\"data\":");
        strcat(manuallyBody, resp);
        strcat(manuallyBody, "}");
        json_t const *jsonData = json_create(manuallyBody, pool, MAX_FIELDS);
        if (jsonData == NULL)
        {
            Serial.println(F("early return check switch state"));
            return;
        }
        json_t const *data = json_getProperty(jsonData, "data");

        if (json_getType(data) == JSON_NULL)
        {
            Serial.println(F("Server Error"));
            return;
        }

        json_t const *item;
        for (int i = 0; i < pins.size(); i++)
        {
            if (pins[i].mode == SENSOR)
            {
                continue;
            }

            for (item = json_getChild(data); item != 0; item = json_getSibling(item))
            {
                if (JSON_OBJ != json_getType(item))
                {
                    continue;
                }

                char const *pin = json_getPropertyValue(item, "pin");
                char const *settingPin = pins[i].pinString;
                char const *value = json_getPropertyValue(item, "value");
                if (!pin)
                {
                    continue;
                }

                String stringValue = String(value);
                int intValue = stringValue.toInt();

                if (strcmp(pin, settingPin) == 0 && intValue == 0)
                {
                    digitalWrite(pins[i].pin, LOW);
                }
                else if (strcmp(pin, settingPin) == 0 && intValue == 1)
                {
                    digitalWrite(pins[i].pin, HIGH);
                }
            }
        }
    }

    static void SendSensor(TlsTcpClient &client, char *host, int port, char *token, char *remoteDeviceId, std::vector<ItemhubPin> &pins, bool &isSslIoError)
    {
        for (int i = 0; i < pins.size(); i++)
        {
            std::string mode = pins[i].mode;
            if (mode == SENSOR)
            {
                char devicePinDataEndpoint[128];
                strcpy(devicePinDataEndpoint, "/api/v1/my/devices/");
                strcat(devicePinDataEndpoint, remoteDeviceId);
                strcat(devicePinDataEndpoint, "/sensors/");
                strcat(devicePinDataEndpoint, pins[i].pinString);

                char postBody[64];
                strcpy(postBody, "{\"value\":");
                strcat(postBody, pins[i].value);
                strcat(postBody, "}");

                char resp[64];
                ItemhubUtilities::Send(client, host, port, "POST", devicePinDataEndpoint, postBody, token, resp, isSslIoError, remoteDeviceId);
            }
        }
    }

    static void Send(TlsTcpClient &client, char *host, int port, char *method, char *path, char *postBody, char *token, char *resp, bool &isSslIoError, char *remoteDeviceId)
    {
        while (client.isConnected() == false)
        {
            isSslIoError = true;
            return;
        }
        unsigned char buff[2048];
        bool respFlag = false;
        unsigned responseStart = false;
        char postBodyLength = strlen(postBody);
        std::string numStr = std::to_string(postBodyLength);
        // Send request to HTTPS web server.
        char requestBody[400];
        strcpy(requestBody, method);
        strcat(requestBody, " ");
        strcat(requestBody, path);
        strcat(requestBody, " HTTP/1.1\r\nHost: ");
        strcat(requestBody, host);
        strcat(requestBody, "\r\nContent-Type: application/json");
        if (strlen(token) > 0)
        {
            strcat(requestBody, "\r\nAuthorization: Bearer ");
            strcat(requestBody, token);
        }
        strcat(requestBody, "\r\nContent-Length: ");
        strcat(requestBody, numStr.c_str());
        strcat(requestBody, "\r\n\r\n");

        strcat(requestBody, postBody);
        client.write((unsigned char *)requestBody, strlen(requestBody));

        memset(resp, 0, sizeof(resp));
        unsigned int cursor = 0;
        while (1)
        {
            memset(buff, 0, sizeof(buff));
            int ret = client.read(buff, sizeof(buff) - 1);
            if (ret == MBEDTLS_ERR_SSL_WANT_READ)
            {
                if (respFlag == true)
                {
                    respFlag = false;
                    break;
                }
                delay(100);
                continue;
            }
            else if (ret <= 0)
            {
                isSslIoError = true;
                break;
            }
            else if (ret > 0)
            {
                respFlag = true;
                strcat(resp, (char *)buff);
            }
        }

        if (strlen(resp) == 0)
        {
            return;
        }

        // todo: refactor
        std::string test(resp);
        int startOfContentLength = test.find("\r\n\r\n");
        int endOfContentLength = test.find("\r\n", startOfContentLength + 4);
        std::string rawContentLength = test.substr(startOfContentLength + 4, endOfContentLength - startOfContentLength - 4);
        unsigned int contentLength = std::stoul(rawContentLength, nullptr, 16);
        std::string body = test.substr(endOfContentLength + 2, contentLength);
        memset(resp, 0, sizeof(resp));
        strcpy(resp, body.c_str());

        int httpStatus = GetHttpStatus(test);
        if (httpStatus == 401 || httpStatus == 403)
        {
            memset(remoteDeviceId, 0, sizeof(remoteDeviceId));
        }
    }

    static json_t const *Extract(char *resp)
    {
        json_t pool[512];
        json_t const *jsonData = json_create(resp, pool, 512);
        return jsonData;
    }

    static int
    GetHttpStatus(std::string &resp)
    {
        size_t startOfHttpMeta = resp.find("HTTP/1.1");
        if (startOfHttpMeta != std::string::npos)
        {
            std::string httpStatus = resp.substr(startOfHttpMeta + 9, 3);
            return std::stoi(httpStatus);
        }
        return 500;
    }
};
