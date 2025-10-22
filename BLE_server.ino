/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "config.h"

TTGOClass *ttgo;
TFT_eSPI *tft;
PCF8563_Class *rtc;

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t interval = 0;
byte page = 0;
byte max_pages = 1;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID "9ff3f90c-9a35-446e-9fd9-68a5a1d792ae"
#define READ_UUID "862c9860-0d89-4caf-8902-dc615e1181e9"
#define WRITE_UUID "862c9860-1d89-4caf-8902-dc615e1181e9"
#define TFT_GREY 0x5AEB
#define LAYOUT_0 0
#define LAYOUT_1 1
#define LAYOUT_2 2
#define LAYOUT_3 3
#define LAYOUT_4 4

// delete later, right now used for testing purposes
#define COMMAND_KEY 1

char response_array[131];

class ValueAttrs
{
private:
  int _value;
  String _desc;
  String _unit;

public:
  // Constructor (runs automatically when object is created)
  ValueAttrs()
  {
    _value = 0;
    _desc = "";
    _unit = "";
  }

  void init(int value, String desc, String unit)
  {
    _value = value;
    _desc = desc;
    _unit = unit;
  }

  void setValue(int new_value)
  {
    _value = new_value;
  }

  int getValue()
  {
    return _value;
  }
};

class PageSetup
{
private:
  int _layout_type;
  byte _max_value_num;
  byte _max_digit_num[8];
  ValueAttrs _values[8];

public:
  // Constructor (runs automatically when object is created)
  PageSetup()
  {
    _layout_type = 0;
    _max_value_num = 8;
  }

  void setLayoutType(int new_layout)
  {
    _layout_type = new_layout;
  }

  int getLayout()
  {
    return _layout_type;
  }

  void setMaxPageNum(int new_max_value_num)
  {
    if (new_max_value_num > 1 &&  new_max_value_num < 8)
    {
      _max_value_num = new_max_value_num;
    }
  }

  void setMaxDigitNum(int new_max_digit_num, byte index)
  {
    if (new_max_digit_num > 1 && new_max_digit_num < 10)
    {
      _max_digit_num[index] = new_max_digit_num;
    }
  }

  byte getMaxPageNum()
  {
    return _max_value_num;
  }
};

PageSetup pages[8];

BLECharacteristic *read_characteristic;

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *write_characteristic)
  {
    std::string write_value = write_characteristic->getValue();

    if (write_value.length() > 0)
    {
      Serial.println("*********");
      Serial.print("New value: ");
      write_value.c_str();
      // clear response before setting new values
      memset(response_array, 0, sizeof(response_array));

      // TODO: promijeniti u write_value[0], za sada je ovako lakše testirati preko mobitela
      //  Identify capability
      if (write_value[COMMAND_KEY] == 0x01)
      {
        response_array[0] = 0x01;
        response_array[1] = 0x00;
        response_array[2] = 0x0E;
        response_array[3] = 0x01;
        response_array[4] = 0x02;
        response_array[5] = 0x03;
        response_array[6] = 0x04;
        response_array[7] = 0x18;
        response_array[8] = 0x19;
        response_array[9] = 0x1A;
        response_array[10] = 0x1B;
        response_array[11] = 0x1C;
        response_array[12] = 0x1D;
        response_array[13] = 0x1D;
        response_array[14] = 0x1E;
        response_array[15] = 0x20;
        response_array[16] = 0x21;
      }
      int level;
      // Battery level in %
      if (write_value[COMMAND_KEY] == 0x02)
      {
        level = ttgo->power->getBattPercentage();
        response_array[0] = 0x02;
        response_array[1] = 0x00;
        response_array[2] = 0x01;
        response_array[3] = level;
        Serial.println(level);
      }
      // Battery level in mV
      if (write_value[COMMAND_KEY] == 0x03)
      {
        level = ttgo->power->getBattVoltage();
        response_array[0] = 0x03;
        response_array[1] = 0x00;
        response_array[2] = 0x02;
        response_array[3] = level;
        response_array[4] = level >> 2;
      }
      // FW version
      if (write_value[COMMAND_KEY] == 0x04)
      {
        response_array[0] = 0x02;
        response_array[1] = 0x00;
        response_array[2] = 0x06;
        response_array[3] = 0x04;
        response_array[4] = 0x00;
        response_array[5] = 0x03;
        response_array[6] = 0x01;
        response_array[7] = 0x00;
        response_array[8] = 0x01;
      }
      // Set number of watch pages
      if (write_value[COMMAND_KEY] == 0x17)
      {
        response_array[0] = 0x17;
        response_array[1] = 0x00;
        response_array[2] = 0x02;
        response_array[3] = 0x00;
        response_array[4] = 0x00;

        // check if the desired watch page number is out of limits
        if (write_value[3] > 1 && write_value[3] < 8)
        {
          max_pages = write_value[2];
        }
        // Number of pages too low
        if (write_value[3] < 1)
        {
          response_array[2] = 0x0C;
        }
        // Number of pages too high
        if (write_value[3] > 8)
        {
          response_array[2] = 0x0D;
        }
      }
      // Set number of values on a page
      if (write_value[COMMAND_KEY] == 0x18)
      {
        response_array[0] = 0x18;
        response_array[1] = 0x00;
        response_array[2] = 0x02;
        // accepted
        response_array[3] = 0x00;
        response_array[4] = 0x00;
        // if page exists
        if (write_value[3] > 1 && write_value[3] < max_pages)
        {
          // if correct number of values
          if (write_value[4] > 1 && write_value[4] < 8)
          {
            pages[write_value[3] - 1].setMaxPageNum(write_value[4]);
          }
          // number of values too low
          if (write_value[4] > 8)
          {
            response_array[4] = 0x0E;
          }
          // number of values too high
          if (write_value[4] < 1)
          {
            response_array[4] = 0x0F;
          }
        }
        // page doesn't exist
        if (write_value[3] < 1 || write_value[3] > max_pages)
        {
          response_array[3] = 0x05;
        }
      }
      // set page layout type
      if (write_value[COMMAND_KEY] == 0x19)
      {
        response_array[0] = 0x19;
        response_array[1] = 0x00;
        response_array[2] = 0x02;
        // accepted
        response_array[3] = 0x00;
        response_array[4] = 0x00;
        if (write_value[3] > 1 && write_value[3] < 8)
        {
          pages[write_value[3] - 1].setLayoutType(write_value[4]);
        }
        else
        {
          // page doesn't exist error code
          response_array[3] = 0x05;
        }
        // TODO: Add layout doesn't exist, first need to implement layouts
        //  response_array[3] = 0x10;
      }

      // set number of digits for page
      if (write_value[COMMAND_KEY] == 0x1A)
      {
        response_array[0] = 0x1A;
        response_array[1] = 0x00;
        response_array[2] = 0x02;
        // accepted
        response_array[3] = 0x00;
        response_array[4] = 0x00;
        // check if page exists
        if (write_value[3] > 1 && write_value[3] < 8)
        {
          // check if value exists
          if (write_value[4] > 1 && write_value[4] < (pages[write_value[3] - 1].getMaxPageNum()))
            pages[write_value[3] - 1].setMaxDigitNum(write_value[5], write_value[4] - 1);
        }
        else
        {
          // page doesn't exist error code
          response_array[3] = 0x05;
        }
        // implement other error codes
      }

      read_characteristic->setValue(response_array);

      for (int i = 0; i < write_value.length(); i++)
        Serial.print(write_value[i]);
      Serial.println();
      Serial.println("*********");
    }

    if (write_value.length() <= 0)
    {
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

void set_layout_0(void)
{
  // clear screen
  // tft->fillScreen(TFT_BLACK);
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  tft->drawString("Value 1", 0, 0, 4);
  tft->drawString("Value 2", 0, 50, 4);
  tft->drawString(rtc->formatDateTime(PCF_TIMEFORMAT_DD_MM_YYYY), 50, 200, 4);
  tft->drawString(rtc->formatDateTime(PCF_TIMEFORMAT_HMS), 5, 118, 7);
}

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

void drawSTATUS(bool status)
{
  String str = status ? "Connection" : "Disconnect";
  int16_t cW = tft->textWidth("Connection", 2);
  int16_t dW = tft->textWidth("Disconnect", 2);
  int16_t w = cW > dW ? cW : dW;
  w += 6;
  int16_t x = 160;
  int16_t y = 20;
  int16_t h = tft->fontHeight(2) + 4;
  uint16_t col = status ? TFT_GREEN : TFT_GREY;
  tft->fillRoundRect(x, y, w, h, 3, col);
  tft->setTextColor(TFT_BLACK, col);
  tft->setTextFont(2);
  tft->drawString(str, x + 2, y);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  // Get watch instance
  ttgo = TTGOClass::getWatch();
  // Initialize the hardware
  ttgo->begin();
  // Turn on the backlight
  ttgo->openBL();
  //  Receive as a local variable for easy writing
  rtc = ttgo->rtc;
  tft = ttgo->tft;

  // Time check will be done, if the time is incorrect, it will be set to compile time
  rtc->check();
  setupBLE();

  // Draw initial connection status
  drawSTATUS(false);
}

void loop()
{
  // disconnected
  if (!deviceConnected && oldDeviceConnected)
  {
    oldDeviceConnected = deviceConnected;
    Serial.println("Draw deviceDisconnected");
    drawSTATUS(false);
  }

  // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
    Serial.println("Draw deviceConnected");
    drawSTATUS(true);
  }

  if (millis() - interval > 1000)
  {

    interval = millis();
    set_layout_0();
  }
}
