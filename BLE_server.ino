/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "config.h"

//TTGOClass *ttgo;
//TFT_eSPI *tft;
//PCF8563_Class *rtc;

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t interval = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID "9ff3f90c-9a35-446e-9fd9-68a5a1d792ae"
#define READ_UUID "862c9860-0d89-4caf-8902-dc615e1181e9"
#define WRITE_UUID "862c9860-1d89-4caf-8902-dc615e1181e9"
#define TFT_GREY 0x5AEB

char response_array[131];

BLECharacteristic *read_characteristic;

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *write_characteristic)
  {
    std::string write_value = write_characteristic->getValue();

    if (write_value.length() > 0) {
    Serial.println("*********");
    Serial.print("New value: ");
    write_value.c_str();
    if(write_value[1] == 0x01)
      {
      
      response_array[0]=0x01;
      response_array[1]=0x01;
      response_array[2]=0x0E;
      response_array[3]=0x01;
      response_array[4]=0x02;
      response_array[5]=0x03;
      response_array[6]=0x04;
      response_array[7]=0x18;
      response_array[8]=0x19;
      response_array[9]=0x1A;
      response_array[10]=0x1B;
      response_array[11]=0x1C;
      response_array[12]=0x1D;
      response_array[13]=0x1E;
      response_array[14]=0x1F;
      response_array[15]=0x20;
      response_array[16]=0x21;
  
      read_characteristic->setValue(response_array);
      }
      
    for (int i = 0; i < write_value.length(); i++)
        Serial.print(write_value[i]);
    Serial.println();
    Serial.println("*********");
}

if (write_value.length() <= 0) {
    return;
}
  }
};

// bool setDateTimeFormBLE(const char *str)
// {
//   uint16_t year;
//   uint8_t month, day, hour, min, sec;
//   String temp, data;
//   int r1, r2;
//   if (str == NULL)
//     return false;

//   data = str;

//   r1 = data.indexOf(',');
//   if (r1 < 0)
//     return false;
//   temp = data.substring(0, r1);
//   year = (uint16_t)temp.toInt();

//   r1 += 1;
//   r2 = data.indexOf(',', r1);
//   if (r2 < 0)
//     return false;
//   temp = data.substring(r1, r2);
//   month = (uint16_t)temp.toInt();

//   r1 = r2 + 1;
//   r2 = data.indexOf(',', r1);
//   if (r2 < 0)
//     return false;
//   temp = data.substring(r1, r2);
//   day = (uint16_t)temp.toInt();

//   r1 = r2 + 1;
//   r2 = data.indexOf(',', r1);
//   if (r2 < 0)
//     return false;
//   temp = data.substring(r1, r2);
//   hour = (uint16_t)temp.toInt();

//   r1 = r2 + 1;
//   r2 = data.indexOf(',', r1);
//   if (r2 < 0)
//     return false;
//   temp = data.substring(r1, r2);
//   min = (uint16_t)temp.toInt();

//   r1 = r2 + 1;
//   temp = data.substring(r1);
//   sec = (uint16_t)temp.toInt();

//   // No parameter check, please set the correct time
//   Serial.printf("SET:%u/%u/%u %u:%u:%u\n", year, month, day, hour, min, sec);
//   rtc->setDateTime(year, month, day, hour, min, sec);

//   return true;
// }

void setupBLE(void)
{
  BLEDevice::init("MightySuit-Watch");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  read_characteristic = pService->createCharacteristic(
      WRITE_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY);
  BLECharacteristic *write_characteristic = pService->createCharacteristic(
      READ_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE);

  write_characteristic->setCallbacks(new MyCallbacks());
  read_characteristic->setValue("read char");
  write_characteristic->setValue("write char");

  BLEDescriptor *desc = new BLEDescriptor(
      BLEUUID((uint16_t)0x2901)); // User Description Descriptor
  desc->setValue("Read Characteristic");
  read_characteristic->addDescriptor(desc);
  pService->start();

  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

// void drawSTATUS(bool status)
// {
//     String str = status ? "Connection" : "Disconnect";
//     int16_t cW = tft->textWidth("Connection", 2);
//     int16_t dW = tft->textWidth("Disconnect", 2);
//     int16_t w = cW > dW ? cW : dW;
//     w += 6;
//     int16_t x = 160;
//     int16_t y = 20;
//     int16_t h = tft->fontHeight(2) + 4;
//     uint16_t col = status ? TFT_GREEN : TFT_GREY;
//     tft->fillRoundRect(x, y, w, h, 3, col);
//     tft->setTextColor(TFT_BLACK, col);
//     tft->setTextFont(2);
//     tft->drawString(str, x + 2, y);
// }

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

   // Get watch instance
    //ttgo = TTGOClass::getWatch();
    // Initialize the hardware
    //ttgo->begin();
    // Turn on the backlight
    //ttgo->openBL();
    //  Receive as a local variable for easy writing
    //rtc = ttgo->rtc;
    //tft = ttgo->tft;

    // Time check will be done, if the time is incorrect, it will be set to compile time
    //rtc->check();
    setupBLE();

  // Draw initial connection status
  //drawSTATUS(false);
}

void loop()
{
    // disconnected
    if (!deviceConnected && oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        Serial.println("Draw deviceDisconnected");
        //drawSTATUS(false);
    }

    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
        Serial.println("Draw deviceConnected");
        //drawSTATUS(true);
    }

    if (millis() - interval > 1000)
    {

        interval = millis();

        //tft->setTextColor(TFT_RED, TFT_BLACK);

        //tft->drawString(rtc->formatDateTime(PCF_TIMEFORMAT_DD_MM_YYYY), 50, 200, 4);

        //tft->drawString(rtc->formatDateTime(PCF_TIMEFORMAT_HMS), 5, 118, 7);
    }
}
