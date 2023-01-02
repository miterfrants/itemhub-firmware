#include <vector>
#include <numeric>
#include "../tiny-json.h"
#define SWITCH "SWITCH"
#define SENSOR "SENSOR"

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
    static AuthResponse Log(BearSSLClient &client, char *host, int port, char *token, char *postBody, bool &isBrSslIoError, int &reconnectCount)
    {
        Serial.println("log");
        char resp[256];
        ItemhubUtilities::Send(client, host, port, "POST", "/api/v1/log", postBody, token, resp, isBrSslIoError, reconnectCount);
    }

    static AuthResponse Auth(BearSSLClient &client, char *host, int port, char *postBody, bool &isBrSslIoError, int &reconnectCount)
    {
        char resp[512];
        Send(client, host, port, "POST", "/api/v1/oauth/exchange-token-for-device", postBody, "", resp, isBrSslIoError, reconnectCount);
        char token[200];
        char remoteDeviceId[12];

        if (strlen(resp) == 0)
        {
            return AuthResponse("", "");
        }
        json_t const *jsonResp = ItemhubUtilities::Extract(resp);
        json_t const *tokenField = json_getProperty(jsonResp, "token");
        json_t const *deviceIdField = json_getProperty(jsonResp, "deviceId");
        strcpy(remoteDeviceId, json_getValue(deviceIdField));
        strcpy(token, json_getValue(tokenField));
        return AuthResponse(token, remoteDeviceId);
    }

    static void Online(BearSSLClient &client, char *host, int port, char *remoteDeviceId, char *token, bool &isBrSslIoError, int &reconnectCount)
    {
        Serial.println(F("Online"));
        char deviceOnlineEndpoint[64];
        strcpy(deviceOnlineEndpoint, "/api/v1/my/devices/");
        strcat(deviceOnlineEndpoint, remoteDeviceId);
        strcat(deviceOnlineEndpoint, "/online");
        char resp[100];
        ItemhubUtilities::Send(client, host, port, "POST", deviceOnlineEndpoint, "", token, resp, isBrSslIoError, reconnectCount);
    }

    static void CheckSwitchState(BearSSLClient &client, char *host, int port, char *token, char *remoteDeviceId, std::vector<ItemhubPin> &pins, bool &isBrSslIoError, int &reconnectCount)
    {
        Serial.println(F("inner check switch state"));
        char deviceStateEndpoint[128];
        strcpy(deviceStateEndpoint, "/api/v1/my/devices/");
        strcat(deviceStateEndpoint, remoteDeviceId);
        strcat(deviceStateEndpoint, "/switches");
        char resp[425];
        ItemhubUtilities::Send(client, host, port, "GET", deviceStateEndpoint, "", token, resp, isBrSslIoError, reconnectCount);

        char manuallyBody[500];
        strcpy(manuallyBody, "{\"data\":");
        strcat(manuallyBody, resp);
        strcat(manuallyBody, "}");
        json_t pool[64];
        json_t const *jsonData = json_create(manuallyBody, pool, 64);
        if (jsonData == NULL)
        {
            Serial.println(F("early return check switch state"));
            return;
        }
        json_t const *data = json_getProperty(jsonData, "data");

        if (data == NULL)
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
                    Serial.println(F("PIN LOW"));
                    digitalWrite(pins[i].pin, LOW);
                }
                else if (strcmp(pin, settingPin) == 0 && intValue == 1)
                {
                    Serial.println(F("PIN HIGH"));
                    digitalWrite(pins[i].pin, HIGH);
                }
            }
        }
    }

    static void SendSensor(BearSSLClient &client, char *host, int port, char *token, char *remoteDeviceId, std::vector<ItemhubPin> &pins, bool &isBrSslIoError, int &reconnectCount)
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
                ItemhubUtilities::Send(client, host, port, "POST", devicePinDataEndpoint, postBody, token, resp, isBrSslIoError, reconnectCount);
            }
        }
    }

    static void Send(BearSSLClient &client, char *host, int port, char *method, char *path, char *postBody, char *token, char *resp, bool &isBrSslIoError, int &reconnectCount)
    {
        resp[0] = '\0';
        int timeoutCount = 0;
        while (!client.connected())
        {
            client.connect(host, port);
            if (!client.connected())
            {
                Serial.println(F("http client not connected"));
                delay(5000);
            }
            timeoutCount += 1;
            if (timeoutCount >= 3)
            {
                Serial.print(F("timeoutConnect:"));
                Serial.println(client.errorCode());
                long rssi = WiFi.RSSI();
                Serial.print("RSSI: ");
                Serial.println(rssi);
                int errorCode = client.errorCode();
                if (errorCode == 31 || errorCode == 1 || errorCode == 34)
                {
                    WiFi.end();
                    client.reInit(host);
                    isBrSslIoError = true;
                    reconnectCount += 1;
                }
                return;
            }
        }

        Serial.println("Client connected");
        reconnectCount = 0;
        unsigned long start = millis();
        unsigned long current = millis();
        char header[300];
        strcpy(header, method);
        strcat(header, " ");
        strcat(header, path);
        strcat(header, " HTTP/1.1\nHost: ");
        strcat(header, host);
        strcat(header, "\nContent-Type: application/json\nAuthorization: Bearer ");
        strcat(header, token);
        strcat(header, "\n");
        client.print(header);

        if (method == "POST")
        {
            client.print("Content-Length: ");
            client.println(strlen(postBody));
            client.println();
            client.println(postBody);
        }
        if (method == "GET")
        {
            client.print("Connection: close\r\n\r\n");
        }

        unsigned int cursor = 0;
        bool isHeadEnd = false;
        bool isFirstLine = false;
        unsigned int contentLength = 0;
        unsigned int startPosition = 0;
        unsigned int endPosition = 0;
        char headerOneLine[128];
        client.flush();
        delay(500);
        while (client.connected())
        {

            current = millis();
            if (current - start > 10 * 1000)
            {
                Serial.println(F("send timeout"));
                resp[0] = '\0';
                client.flush();
                client.stop();
                return;
            }
            char c = client.read();
            if (c == NULL) // client be terminat
            {
                Serial.println(F("client be terminat"));
                resp[0] = '\0';
                client.flush();
                client.stop();
                return;
            }

            if (!isHeadEnd) // response head
            {
                headerOneLine[cursor] = c;
                cursor++;

                bool isNewLine = c == '\n';
                if (isNewLine)
                {
                    // truncate http client cache memory leak
                    headerOneLine[cursor] = '\0';
                }

                bool isEndOfHeaderSign = ((strlen(headerOneLine) == 2 && strncmp(headerOneLine, "\r\n", 2) == 0) ||
                                          (strlen(headerOneLine) == 1 && strncmp(headerOneLine, "\n", 1) == 0) ||
                                          (strlen(headerOneLine) == 2 && strncmp(headerOneLine, "\n\r", 2) == 0));

                if (isEndOfHeaderSign)
                {
                    isHeadEnd = true;
                    headerOneLine[0] = '\0'; // clear headerOneLine
                    cursor = 0;
                }
                else if (isNewLine)
                {
                    headerOneLine[0] = '\0';
                    cursor = 0;
                }
                continue;
            }
            // response content length
            if (c == '\n' && !isFirstLine)
            {
                resp[cursor] = '\0';
                isFirstLine = true;
                if (strlen(resp) > 20)
                {
                    Serial.println(F("resp bigger than 20"));
                    Serial.println(strlen(resp));
                    Serial.println(resp);
                    resp[0] = '\0';
                    client.flush();
                    client.stop();
                    return;
                }

                std::string test(resp);
                if (test.find("<html>", 0) == 0)
                {
                    Serial.println(F("server error"));
                    resp[0] = '\0';
                    client.flush();
                    client.stop();
                    return;
                }
                contentLength = std::stoi(resp, 0, 16);
                startPosition = cursor + 1;
                endPosition = startPosition + contentLength - 1;
                memset(resp, 0, sizeof(resp));
            }
            if (startPosition == 0 || endPosition == 0)
            {
                // get response content length
                resp[cursor] = c;
            }
            else if (cursor >= startPosition && cursor <= endPosition)
            {
                resp[cursor - startPosition] = c;
            }
            else
            {
            }

            cursor += 1;
        }
        if (method == "POST")
        {
            client.println("Connection: close\r\n\r\n");
        }
        resp[contentLength] = '\0';

        Serial.println(resp);
        Serial.println("end of response");
        client.flush();
        client.stop();
    }

    static json_t const *Extract(char *resp)
    {
        json_t pool[512];
        json_t const *jsonData = json_create(resp, pool, 512);
        return jsonData;
    }

    static char *ExtractBody(char *resp, bool isArray)
    {
        std::string prefix = "{";
        if (isArray)
        {
            prefix = "[";
        }
        std::string response(resp);
        int startOfJsonObject = response.find(prefix);

        std::string failed = "FAILED";
        std::string rawContentLength = response.substr(0, startOfJsonObject);

        unsigned int contentLength = std::stoul(rawContentLength, nullptr, 16);

        if (startOfJsonObject != -1)
        {
            std::string body = response.substr(startOfJsonObject, contentLength);
            return (char *)body.c_str();
        }
        return "failed";
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
