#include <RTClock.h>
#include <RTClib.h>
#include <vector>

#define DEBUG 1
#define SWITCH "SWITCH"
#define SENSOR "SENSOR"
#include "ItemhubUtilities/ItemhubUtilities.h"
#include "ItemhubUtilities/Certs.h"

#define HOST "{DOMAIN}"
#define PORT 443
#define USER "{CLIENT_ID}"
#define PWD "{CLIENT_SECRET}"

bool BC26Init = false;
bool isAuth = false;
int BC26ResponseTimeoutCount = 0;

RTClock rt(RTCSEL_LSI);
RTC_DS1307 rtc;
std::vector<ItemhubPin> pins;

String remoteDeviceId = "";
String token = "";
String user = USER;
String pwd = PWD;

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
  Serial.begin(115200);
  Serial2.begin(115200);
  {PINS};
}

void loop()
{
  if (BC26ResponseTimeoutCount > 5)
  {
    Serial2.println("reset bc26");
    digitalWrite(PC13, LOW);
    delay(3000);
    digitalWrite(PC13, HIGH);
    delay(10000);

    ItemhubUtilities::SendATCommand("AT+QPOWD=2");
    delay(5000);
    BC26ResponseTimeoutCount = 0;
    BC26Init = false;
  }

  while (!BC26Init)
  {
    Serial2.println("BC26 init");
    Serial2.print(".");
    BC26Init = ItemhubUtilities::BC26init(CA_PEM, BC26ResponseTimeoutCount);
    if (BC26ResponseTimeoutCount > 10)
    {
      break;
    }
    delay(500);
    Serial2.println("");
    Serial2.println("initialization OK");
  }

  if (!BC26Init)
  {
    Serial2.print("timeout count:");
    Serial2.println(BC26ResponseTimeoutCount);
    return;
  }

  if (remoteDeviceId == "")
  {
    String postBody = "{\"clientId\":\"" + user + "\",\"clientSecret\":\"" + pwd + "\"}";
    AuthResponse authResponse = ItemhubUtilities::Auth(HOST, PORT, postBody, BC26ResponseTimeoutCount);
    if (authResponse.token == "")
    {
      return;
    }
    token = authResponse.token;
    remoteDeviceId = authResponse.remoteDeviceId;
  }

  // itemhub device state
  currentDeviceStateTimestamp = millis();
  if (currentDeviceStateTimestamp - previousDeviceStateTimestamp > intervalDeviceState && remoteDeviceId != "")
  {
    previousDeviceStateTimestamp = currentDeviceStateTimestamp;
    ItemhubUtilities::Online(HOST, PORT, remoteDeviceId, token, BC26ResponseTimeoutCount);
  }

  // itemhub switch
  currentSwitchTimestamp = millis();
  if (currentSwitchTimestamp - previousSwitchTimestamp > intervalSwitch && remoteDeviceId != "")
  {
    previousSwitchTimestamp = currentSwitchTimestamp;
    ItemhubUtilities::CheckSwitchState(HOST, PORT, token, remoteDeviceId, pins, BC26ResponseTimeoutCount);
  }

  // itemhub sensor
  currentSensorTimestamp = millis();
  if (currentSensorTimestamp - previousSensorTimestamp > intervalSensor && remoteDeviceId != "")
  {
    previousSensorTimestamp = currentSensorTimestamp;
    for (int i = 0; i < pins.size(); i++)
    {
      String mode = pins[i].mode;
      if (strcmp(mode.c_str(), SENSOR) == 0)
      {
        int value = digitalRead(pins[i].pin);
        String valueString = String(value);
        pins[i].value = valueString;
      }
    }
    ItemhubUtilities::SendSensor(HOST, PORT, token, remoteDeviceId, pins, BC26ResponseTimeoutCount);
  }
}