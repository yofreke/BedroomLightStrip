#include <arduino.h>

#include "globals.h"
#include "pinMap.h"
#include "stateMachine.h"
#include "wifiClient.h"

unsigned long lastWifiUpdate = 0;

void wifiClient_setup()
{
  pinMode(PIN_WIFI_HAS_NEW_DATA, INPUT);
  pinMode(PIN_WIFI_RESET, OUTPUT);

  digitalWrite(PIN_WIFI_RESET, HIGH);

  Serial2.begin(9600);
}

void handleByte(byte dataIndex, byte data)
{
  Serial.println(String("handleByte: dataIndex= ") + dataIndex +
                 " data= " + data);
}

void handleInt(byte dataIndex, int data)
{
  Serial.println(String("handleInt: dataIndex= ") + dataIndex +
                 " data= " + data);

  // Map remote values to local values
  if (dataIndex == 0)
  {
    stateMachine_setMode(data);
  }
}

void handleFloat(byte dataIndex, float data)
{
  Serial.println(String("handleFloat: dataIndex= ") + dataIndex +
                 " data= " + data);

  // Map remote values to local values
  if (dataIndex == 0)
  {
    modeData_sunrise_brightnessWhite = data;
  }
  else if (dataIndex == 1)
  {
    modeData_sunrise_brightnessWarmWhite = data;
  }
}

void readBytes(byte *dataBuffer, int dataSize)
{
  int i = 0;
  int tryCount = 0;
  while (i < dataSize)
  {
    if (tryCount++ > 2000)
    {
      Serial.println("readBytes: timeout");
      return;
    }

    if (Serial2.available() == 0)
    {
      delay(1);
      continue;
    }

    // Serial.println(String("Reading byte ") + i + " of " + dataSize);
    dataBuffer[i++] = Serial2.read();
  }
}

void wifiClient_tick()
{
  if (digitalRead(PIN_WIFI_HAS_NEW_DATA) == HIGH &&
      g_rtcSeconds - lastWifiUpdate > 0)
  {
    lastWifiUpdate = g_rtcSeconds;
    Serial.println(
        "wifiClient: PIN_WIFI_HAS_NEW_DATA is high, sending command 1");
    Serial.println(g_rtcSeconds);
    // Command 1 sends back new data
    Serial2.write('1');
    Serial2.flush();
  }

  if (Serial2.available())
  {
    char dataType = Serial2.read();
    byte dataIndex = Serial2.read();
    // Serial.println(String("dataType=") + dataType + " dataIndex= " +
    // dataIndex);

    if (dataType == 'b')
    {
      byte data_byte = Serial2.read();
      handleByte(dataIndex, data_byte);
    }
    else if (dataType == 'i')
    {
      int data_int;
      readBytes((byte *)&data_int, 2);
      handleInt(dataIndex, data_int);
    }
    else if (dataType == 'f')
    {
      float data_float;
      readBytes((byte *)&data_float, 4);
      handleFloat(dataIndex, data_float);
    }
  }
}
