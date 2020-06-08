#include <arduino.h>

#include "globals.h"
#include "pinMap.h"
#include "wifiClient.h"

unsigned long lastWifiUpdate = 0;

void readBytes(byte *dataBuffer, int dataSize)
{
  int i = 0;
  while (i < dataSize)
  {
    if (Serial2.available() == 0)
    {
      delay(1);
      continue;
    }

    dataBuffer[i] = Serial2.read();
  }
}

void wifiClient_setup()
{
  pinMode(PIN_WIFI_TX, OUTPUT);
  pinMode(PIN_WIFI_RX, INPUT);
  pinMode(PIN_WIFI_HAS_NEW_DATA, INPUT);

  Serial2.begin(9600);
}

void handleByte(byte dataIndex, byte data)
{
  Serial.println(String("handleByte: dataIndex= ") + dataIndex +
                 " data= " + data);
}

void handleInt(byte dataIndex, int data)
{
  Serial.println(String("handleByte: dataIndex= ") + dataIndex +
                 " data= " + data);
}

void handleFloat(byte dataIndex, float data)
{
  Serial.println(String("handleByte: dataIndex= ") + dataIndex +
                 " data= " + data);
}

void wifiClient_tick()
{
  if (digitalRead(PIN_WIFI_HAS_NEW_DATA) && g_rtcSeconds - lastWifiUpdate > 0)
  {
    lastWifiUpdate = g_rtcSeconds;
    Serial.println(
        "wifiClient: PIN_WIFI_HAS_NEW_DATA is high, sending command 1");
    // Command 1 sends back new data
    Serial2.write('1');
    Serial2.flush();
  }

  if (Serial2.available())
  {
    char dataType = Serial.read();
    byte dataIndex = Serial.read();

    switch (dataType)
    {
    case 'b':
      byte data_byte = Serial2.read();
      handleByte(dataIndex, data_byte);
      break;
    case 'i':
      int data_int;
      readBytes((byte *)&data_int, 2);
      handleInt(dataIndex, data_int);
      break;
    case 'f':
      float data_float;
      readBytes((byte *)&data_float, 4);
      handleFloat(dataIndex, data_float);
      break;
    }
  }
}
