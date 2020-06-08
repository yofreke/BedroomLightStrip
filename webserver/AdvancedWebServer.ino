/*
   Copyright (c) 2015, Majenko Technologies
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 * * Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

 * * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 * * Neither the name of Majenko Technologies nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
   Tasks this arduino is responsible for:
   - Track basic values in RAM similar to I2C device registers
   - Expose HTTP API to change state values
   - Expose I2C (FIXME: Is this best?) API for getting state values
   - Expose hasData boolean as digital pin state, automatically reset on data
   read
*/

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>

#include <SPI.h>

#ifndef STASSID
#define STASSID "ASUS"
#define STAPSK "tatmk78106"
#endif

const char *ssid = STASSID;
const char *password = STAPSK;

ESP8266WebServer server(80);

#define PIN_HAS_NEW_DATA 1
#define PIN_BUILTIN_LED 13

bool stateData_bool[8];
int stateData_int[8];
float stateData_float[8];

struct NewData
{
  char dataType;
  int dataIndex;
};

#define NEW_DATA_LIST_SIZE 8

int newDataListIndex = 0;
struct NewData newDataList[NEW_DATA_LIST_SIZE];

void handleRoot()
{
  digitalWrite(PIN_BUILTIN_LED, 1);

  String message = "Number of args received: ";
  message += server.args();
  message += "\n";

  int argCount = server.args();
  if (argCount > 0)
  {
    String argName;
    int argValueIndex;
    String argValue;

    for (int i = 0; i < server.args(); i++)
    {
      argName = server.argName(i);
      argValue = server.arg(i);

      argValueIndex = argName.substring(1).toInt();

      // Handle arg
      char dataTypeIn = argName.charAt(0);
      switch (dataTypeIn)
      {
      case 'b':
        stateData_bool[argValueIndex] = argValue.toInt() == 1;
        Serial.println(String("State data: bool #") + argValueIndex + " = " +
                       stateData_bool[argValueIndex]);
        break;

      case 'i':
        stateData_int[argValueIndex] = argValue.toInt();
        Serial.println(String("State data: int #") + argValueIndex + " = " +
                       stateData_int[argValueIndex]);
        break;

      case 'f':
        stateData_float[argValueIndex] = argValue.toFloat();
        Serial.println(String("State data: float #") + argValueIndex + " = " +
                       stateData_float[argValueIndex]);
        break;
      }

      if (newDataListIndex < NEW_DATA_LIST_SIZE)
      {
        newDataList[newDataListIndex++] = {dataTypeIn, argValueIndex};
      }

      // Add to response
      message += "Arg number" + (String)i + " -> ";
      message += server.argName(i) + ": " + argValueIndex + ": ";
      message += server.arg(i) + "\n";
    }

    // Signal there is new data to GPIO
    digitalWrite(PIN_HAS_NEW_DATA, 1);
  }
  else
  {
    message +=
        "No arguments in URL. Example: 192.168.0.0?b0=1&i2=42&f7=1234.5678";
  }

  server.send(200, "text/plain", message);

  digitalWrite(PIN_BUILTIN_LED, 0);
}

void setup(void)
{
  pinMode(PIN_BUILTIN_LED, OUTPUT);
  digitalWrite(PIN_BUILTIN_LED, 0);

  pinMode(PIN_HAS_NEW_DATA, OUTPUT);
  digitalWrite(PIN_HAS_NEW_DATA, 0);

  Serial.begin(57600);
  Serial.print("Starting wifi for SSID:");
  Serial.print(ssid);
  Serial.println("");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.print("\nConnected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting mdns responder");
  if (MDNS.begin("bedroomlights"))
  {
    Serial.println("> Success");
  }
  else
  {
    Serial.println("> Failure");
  }

  // Add routes
  server.on("/", handleRoot);

  // Start listening for connections
  server.begin();

  Serial.println("HTTP server ready");
}

void serialWriteInt(int i)
{
  byte *data[2] = (byte *)&i;
  Serial.write(data, sizeof(i));
}

void serialWriteFloat(float f)
{
  byte *data[4] = (byte *)&f;
  Serial.write(data, sizeof(f));
}

int serialReadBlocking()
{
  while (Serial.available() == 0)
  {
    delay(1);
  }
  return Serial.read();
}

void loop()
{
  // Update http server
  server.handleClient();
  MDNS.update();

  // Update serial communication
  if (Serial.available())
  {
    int command = serialReadBlocking();
    Serial.println(String("Got serial command:") + command);

    if (command == '0')
    {
      // Send a single requested value
      int inputDataType = serialReadBlocking();
      int inputDataIndex = serialReadBlocking();

      Serial.println(String("> inputDataType= ") + inputDataType +
                     " inputDataIndex= " + inputDataIndex);

      switch (inputDataType)
      {
      case 'b':
        Serial.println(stateData_bool[inputDataIndex]);
        break;
      case 'i':
        Serial.println(stateData_int[inputDataIndex]);
        break;
      case 'f':
        Serial.println(stateData_float[inputDataIndex]);
        break;
      }
    }
    else if (command == '1')
    {
      Serial.println(String("newDataListIndex= ") + newDataListIndex);

      // Send any new values
      for (int i = 0; i < newDataListIndex; i++)
      {
        Serial.write(newDataList[i].dataType);
        Serial.write(newDataList[i].dataIndex);

        switch (newDataList[i].dataType)
        {
        case 'b':
          Serial.write(stateData_bool[newDataList[i].dataIndex]);
          break;
        case 'i':
          serialWriteInt(stateData_int[newDataList[i].dataIndex]);
          break;
        case 'f':
          serialWriteFloat(stateData_float[newDataList[i].dataIndex]);
          break;
        }
      }

      // Reset new value index
      newDataListIndex = 0;
      digitalWrite(PIN_HAS_NEW_DATA, 0);
    }
  }
}
