/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "fire.h"
#include "config.h"

// #include "AudioFileSourcePROGMEM.h"
// #include "AudioFileSourceID3.h"
// #include "AudioGeneratorMP3.h"
// #include "AudioOutputI2S.h"

// #include "pika.h"

// AudioGeneratorMP3 *mp3;
// AudioFileSourcePROGMEM *file;
// AudioOutputI2S *out;
// AudioFileSourceID3 *id3;

TTGOClass *ttgo;
TFT_eSPI *tft;
PCF8563_Class *rtc;

bool deviceConnected = false;
bool fire_alarm = false;
bool vibration = false;
uint32_t interval = 0;
int16_t x, y;
bool irq = false;
uint8_t test_brightness = 0;
bool init_done = false;

byte current_layout;
byte new_layout;

byte max_pages = 8;
byte current_page = 0;

BLECharacteristic *read_characteristic;

#define SERVICE_UUID "9ff3f90c-9a35-446e-9fd9-68a5a1d792ae"
#define WRITE_UUID "862c9860-0d89-4caf-8902-dc615e1181e9"
#define READ_UUID "862c9860-1d89-4caf-8902-dc615e1181e9"
#define TFT_GREY 0x5AEB
#define CRITICAL_INFO_LAYOUT 0
#define NON_CRITICAL_INFO_LAYOUT 1
#define ALARM_LAYOUT 8
#define TIME_LAYOUT 9

typedef enum
{
  VBUS_REMOVE,
  LV_ICON_BAT_1,
  LV_ICON_BAT_2,
  LV_ICON_BAT_3,
  LV_ICON_BAT_FULL,
  VBUS_PLUGIN,
  LV_ICON_CALCULATION
} lv_icon_battery_t;

void updateBatIcon(lv_icon_battery_t icon)
{
  int color;
  String str;
  byte x = 205;
  byte y = 20;
  byte w;
  byte h = 10;
  int16_t value_w;
  TTGOClass *ttgo = TTGOClass::getWatch();
  int level = ttgo->power->getBattPercentage();
  w = level / 10 * 2 + 10;
  // clear previous value
  tft->fillRoundRect(x - 40, y, w + 40, h + 10, 3, TFT_BLACK);
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);

  if (icon == LV_ICON_CALCULATION)
  {
    if (level < 20)
    {
      color = TFT_RED;
    }
    else
    {
      color = TFT_GREEN;
    }
  }

  str = String(level) + "%";
  value_w = tft->textWidth(str);
  tft->setTextSize(1);
  tft->drawString(str, x - value_w, y);
  tft->fillRoundRect(x, y + 5, w, h, 3, color);
}

// delete later, right now used for testing purposes
#define COMMAND_KEY 0

uint8_t response_array[131];

class ValueAttrs
{
private:
  int _value;
  String _desc;
  String _unit;
  int _value_digits;

public:
  // Constructor (runs automatically when object is created)
  ValueAttrs()
  {
    _value = 0;
    _desc = "";
    _unit = "";
    _value_digits = 3;
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
  void setValueDigits(byte new_value)
  {
    _value_digits = new_value;
  }

  int getValueDigits()
  {
    return _value_digits;
  }
  void setDesc(String new_desc)
  {
    _desc = new_desc;
  }

  String getDesc()
  {
    return _desc;
  }
  void setUnit(String new_unit)
  {
    _unit = new_unit;
  }

  String getUnit()
  {
    return _unit;
  }
};

class PageSetup
{
private:
  int _layout_type;
  byte _max_value_num;
  byte _max_digit_num[8];

  // what if value not printed / vidjeti s Kresom?

public:
  ValueAttrs values[8];

  // Constructor (runs automatically when object is created)
  PageSetup()
  {
    _layout_type = CRITICAL_INFO_LAYOUT;
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
    if (new_max_value_num > 1 && new_max_value_num < 8)
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

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *write_characteristic)
  {
    std::string write_value = write_characteristic->getValue();
    int response_array_size = 0;
    int value_for_page = 0;

    if (write_value.length() > 0)
    {
      Serial.println("*********");
      Serial.print("New value: ");

      write_value.c_str();
      // clear response
      memset(response_array, 0, sizeof(response_array));

      Serial.println(write_value.length());

      for (int i = 0; i < write_value.length(); i++)
      {
        Serial.print(write_value[i], HEX);
      }
      Serial.println();

      //  Identify capability
      if (write_value[COMMAND_KEY] == 0x01)
      {
        response_array[0] = 0x01;
        response_array[1] = 0x00;
        response_array[2] = 0x0E;
        response_array[3] = 0x00;
        response_array[4] = 0x01;
        response_array[5] = 0x00;
        response_array[6] = 0x02;
        response_array[7] = 0x00;
        response_array[8] = 0x03;
        response_array[9] = 0x00;
        response_array[10] = 0x04;
        response_array[11] = 0x00;
        response_array[12] = 0x18;
        response_array[13] = 0x00;
        response_array[14] = 0x19;
        response_array[15] = 0x00;
        response_array[16] = 0x1A;
        response_array[17] = 0x00;
        // response_array[18] = 0x1B;
        // response_array[19] = 0x00;
        // response_array[20] = 0x1C;
        // response_array[21] = 0x00;
        // response_array[22] = 0x1D;
        // response_array[23] = 0x00;
        // response_array[24] = 0x1E;
        // response_array[25] = 0x00;
        // response_array[26] = 0x20;
        // response_array[27] = 0x00;
        response_array[18] = 0x21;
        response_array[19] = 0x00;
        response_array[20] = 0x23;
        response_array[21] = 0x00;
        response_array_size = 22;
      }
      int level;
      // Battery level in %
      if (write_value[COMMAND_KEY] == 0x02)
      {
        level = ttgo->power->getBattPercentage();
        response_array[0] = 0x02;
        response_array[1] = 0x00;
        response_array[2] = level;
        response_array_size = 3;
      }
      // Battery level in mV
      if (write_value[COMMAND_KEY] == 0x03)
      {
        level = ttgo->power->getBattVoltage();
        response_array[0] = 0x03;
        response_array[1] = 0x00;
        response_array[2] = level >> 2;
        response_array[3] = level;
        response_array_size = 4;
      }
      // FW version
      if (write_value[COMMAND_KEY] == 0x04)
      {
        response_array[0] = 0x02;
        response_array[1] = 0x00;
        response_array[2] = 0x06;
        response_array[3] = 0x00;
        response_array[4] = 0x04;
        response_array[5] = 0x00;
        response_array[6] = 0x03;
        response_array[7] = 0x01;
        response_array[8] = 0x00;
        response_array[9] = 0x01;
        response_array_size = 10;
      }
      // Set number of watch pages
      if (write_value[COMMAND_KEY] == 0x17)
      {
        response_array[0] = 0x17;
        response_array[1] = 0x00;
        response_array[2] = 0x02;
        response_array[3] = 0x00;
        response_array[4] = 0x00;
        response_array_size = 5;

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
        response_array_size = 5;
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
        // accepted
        response_array[2] = 0x00;
        response_array[3] = 0x00;
        response_array_size = 4;
        // check if page exists
        if (write_value[2] > 0 && write_value[2] < max_pages)
        {
          // check if layout exists
          if (write_value[3] == CRITICAL_INFO_LAYOUT || write_value[3] == NON_CRITICAL_INFO_LAYOUT)
            pages[write_value[2]].setLayoutType(write_value[3]);
          else
          {
            // layout doesn't exist
            response_array[3] = 0x10;
          }
        }
        else
        {
          // page doesn't exist error code
          response_array[3] = 0x05;
        }
      }

      // set number of digits for page
      if (write_value[COMMAND_KEY] == 0x1A)
      {
        response_array[0] = 0x1A;
        response_array[1] = 0x00;
        // accepted
        response_array[2] = 0x00;
        response_array[3] = 0x00;
        response_array_size = 4;
        // check if page exists
        if (write_value[2] > 0 && write_value[2] < max_pages)
        {
          // check if value exists
          if (write_value[3] >= 0 && write_value[3] < pages[write_value[2]].getMaxPageNum())
          {
            // check digit number
            if (write_value[4] > 0 && write_value[3] < 4)
            {
              pages[write_value[2]]
                  .values[write_value[3]]
                  .setValueDigits(write_value[4]);
            }
            else if (write_value[4] < 0)
            {
              // number of digits too low
              response_array[2] = 0x12;
            }
            else
            {
              // number of digits too high
              response_array[2] = 0x13;
            }
          }
          else
          {
            // value doesn't exist
            response_array[2] = 0x13;
          }
        }
        else
        {
          // page doesn't exist error code
          response_array[2] = 0x05;
        }
      }

      // set value for page N, value M
      if (write_value[COMMAND_KEY] == 0x21)
      {
        response_array[0] = 0x21;
        response_array[1] = 0x00;
        // accepted
        response_array[2] = 0x00;
        response_array[3] = 0x00;
        response_array_size = 4;
        // check if page exists
        if (write_value[2] > 0 && write_value[2] < max_pages)
        {
          // check if value exists
          if (write_value[3] >= 0 && write_value[3] < pages[write_value[2]].getMaxPageNum())
          {
            value_for_page += write_value[4] << 0;
            value_for_page += write_value[5] << 1;
            value_for_page += write_value[6] << 2;
            value_for_page += write_value[7] << 3;
            Serial.println("value_for_page");
            Serial.println(value_for_page);
            pages[write_value[2]]
                .values[write_value[3]]
                .setValue(value_for_page);
          }
          else
          {
            // value doesn't exist error code
            response_array[2] = 0x11;
          }
        }
        else
        {
          // page doesn't exist error code
          response_array[2] = 0x05;
        }
      }

      // Init done
      if (write_value[COMMAND_KEY] == 0x23)
      {
        response_array[0] = 0x23;
        response_array[1] = 0x00;
        response_array[2] = 0x00;
        response_array[3] = 0x00;
        response_array[4] = 0x00;
        response_array_size = 5;
      }
      Serial.println();
      read_characteristic->setValue(response_array, response_array_size);
      read_characteristic->notify();

      for (int i = 0; i < write_value.length(); i++)
      {
        Serial.print(write_value[i]);
        Serial.print(" ");
      }
      Serial.println("*********");
    }

    if (write_value.length() <= 0)
    {
      return;
    }
  }
};

// critical info layout
void set_layout_0(void)
{
  int16_t value_w;
  if (current_layout != CRITICAL_INFO_LAYOUT)
  {
    current_layout = CRITICAL_INFO_LAYOUT;
    tft->fillScreen(TFT_BLACK);
  }
  updateBatIcon(LV_ICON_CALCULATION);
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);

  tft->setTextSize(2);
  tft->drawString(String(current_page), 210, 130);

  if (pages[current_page].getMaxPageNum() > 0)
  {
    value_w = tft->textWidth(String(pages[current_page].values[0].getValue()).substring(0, pages[current_page].values[0].getValueDigits()));
    tft->drawString(String(pages[current_page].values[0].getValue()).substring(0, pages[current_page].values[0].getValueDigits()), 0, 10);
    tft->drawString(pages[current_page].values[0].getUnit(), 0 + value_w, 10);
    tft->setTextSize(1);
    tft->drawString(pages[current_page].values[0].getDesc(), 0 + value_w + 50, 20);
  }

  if (pages[current_page].getMaxPageNum() > 1)
  {
    tft->setTextSize(3);
    value_w = tft->textWidth(String(pages[current_page].values[1].getValue()).substring(0, pages[current_page].values[1].getValueDigits()));
    tft->drawString(String(pages[current_page].values[1].getValue()).substring(0, pages[current_page].values[1].getValueDigits()), 85, 80);
    tft->drawString(pages[current_page].values[1].getUnit(), 85 + value_w, 80);
    tft->setTextSize(1);
    tft->drawString(pages[current_page].values[1].getDesc(), 85, 130);
  }

  if (pages[current_page].getMaxPageNum() > 2)
  {
    value_w = tft->textWidth(String(pages[current_page].values[2].getValue()).substring(0, pages[current_page].values[2].getValueDigits()));
    tft->drawString(String(pages[current_page].values[2].getValue()).substring(0, pages[current_page].values[2].getValueDigits()), 90, 200);
    tft->drawString(pages[current_page].values[2].getUnit(), 90 + value_w, 200);
    tft->setTextSize(1);
    tft->drawString(pages[current_page].values[2].getDesc(), 90, 220);
  }

  if (pages[current_page].getMaxPageNum() > 3)
  {
    value_w = tft->textWidth(String(pages[current_page].values[3].getValue()).substring(0, pages[current_page].values[3].getValueDigits()));
    tft->drawString(String(pages[current_page].values[3].getValue()).substring(0, pages[current_page].values[3].getValueDigits()), 170, 200);
    tft->drawString(pages[current_page].values[3].getUnit(), 170 + value_w, 200);
    tft->setTextSize(1);

    tft->drawString(pages[current_page].values[3].getDesc(), 170, 220);
  }

  if (pages[current_page].getMaxPageNum() > 4)
  {
    // value_w = tft->textWidth(String(pages[current_page].values[4].getValue()).substring(0, pages[current_page].values[3].getValueDigits()));
    // tft->drawString(String(pages[current_page].values[4].getValue()).substring(0, pages[current_page].values[3].getValueDigits()), 0, 200);
    // tft->drawString(pages[current_page].values[4].getUnit(), 0 + value_w, 200);
    tft->setTextSize(2);
    tft->drawString(pages[current_page].values[4].getDesc(), 0, 200);
  }

  drawSTATUS(deviceConnected);
}

// non critical info layout
void set_layout_1(void)
{
  int16_t value_w;
  if (current_layout != NON_CRITICAL_INFO_LAYOUT)
  {
    current_layout = NON_CRITICAL_INFO_LAYOUT;
    tft->fillScreen(TFT_BLACK);
  }
  updateBatIcon(LV_ICON_CALCULATION);
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);

  tft->setTextSize(2);
  tft->drawString(String(current_page), 210, 130);
  if (pages[current_page].getMaxPageNum() > 0)
  {
    tft->drawString(String(pages[current_page].values[0].getValue()).substring(0, pages[current_page].values[0].getValueDigits()), 123, 10);
    value_w = tft->textWidth(String(pages[current_page].values[0].getValue()).substring(0, pages[current_page].values[0].getValueDigits()));
    tft->drawString(pages[current_page].values[0].getUnit(), 123 + value_w, 10);
    tft->drawString(pages[current_page].values[0].getDesc(), 0, 10);
  }
  if (pages[current_page].getMaxPageNum() > 1)
  {
    tft->setTextSize(2);
    tft->drawString(String(pages[current_page].values[1].getValue()).substring(0, pages[current_page].values[1].getValueDigits()), 123, 50);
    value_w = tft->textWidth(String(pages[current_page].values[1].getValue()).substring(0, pages[current_page].values[1].getValueDigits()));
    tft->drawString(pages[current_page].values[1].getUnit(), 123 + value_w, 50);
    tft->drawString(pages[current_page].values[1].getDesc(), 0, 50);
  }
  if (pages[current_page].getMaxPageNum() > 2)
  {
    tft->setTextSize(2);
    tft->drawString(String(pages[current_page].values[2].getValue()).substring(0, pages[current_page].values[2].getValueDigits()), 123, 90);
    value_w = tft->textWidth(String(pages[current_page].values[2].getValue()).substring(0, pages[current_page].values[2].getValueDigits()));
    tft->drawString(pages[current_page].values[2].getUnit(), 123 + value_w, 90);
    tft->drawString(pages[current_page].values[2].getDesc(), 0, 90);
  }
  if (pages[current_page].getMaxPageNum() > 3)
  {
    tft->setTextSize(3);
    value_w = tft->textWidth(String(pages[current_page].values[3].getValue()).substring(0, pages[current_page].values[3].getValueDigits()));
    tft->drawString(String(pages[current_page].values[3].getValue()).substring(0, pages[current_page].values[3].getValueDigits()), 80, 160);
    tft->drawString(pages[current_page].values[3].getUnit(), 80 + value_w, 160);
    tft->setTextSize(2);
    tft->drawString(pages[current_page].values[3].getDesc(), 80, 200);
  }
  drawSTATUS(deviceConnected);
}

// set alarm layout
void set_alarm_layout(void)
{
  if (current_layout != ALARM_LAYOUT)
  {
    current_layout = ALARM_LAYOUT;
    tft->fillScreen(TFT_BLACK);
    tft->setSwapBytes(true);
    tft->pushImage(56, 56, 128, 128, fire);
  }
  // if (!(mp3->isRunning()))
  // {
  //   Serial.println("STARTING MP3");
  //   mp3->begin(id3, out);
  // }

  updateBatIcon(LV_ICON_CALCULATION);

  ttgo->setBrightness(test_brightness);
  test_brightness += 30;
  if (test_brightness > 240)
    test_brightness = 0;
  ttgo->motor->onec();
  delay(200);
}

// set time layout
void set_time_layout(void)
{

  if (current_layout != TIME_LAYOUT)
  {
    // current_layout = TIME_LAYOUT;
    tft->setTextSize(1);
    tft->drawString(rtc->formatDateTime(PCF_TIMEFORMAT_DD_MM_YYYY), 140, 200);
    tft->drawString(rtc->formatDateTime(PCF_TIMEFORMAT_HMS), 30, 200);
  }
}

class MyServerCallback : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    Serial.println("onConnect");
  }

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
    Serial.println("onDisconnect");
  }
};

void setupBLE(void)
{
  BLEDevice::init("MightySuit-Watch");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallback);
  BLEService *pService = pServer->createService(SERVICE_UUID);
  read_characteristic = pService->createCharacteristic(
      READ_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_NOTIFY);
  BLECharacteristic *write_characteristic = pService->createCharacteristic(
      WRITE_UUID,
      BLECharacteristic::PROPERTY_WRITE);

  write_characteristic->setCallbacks(new MyCallbacks());
  read_characteristic->setValue("read char");
  write_characteristic->setValue("write char");

  BLEDescriptor *desc = new BLEDescriptor(
      BLEUUID((uint16_t)0x2902)); // User Description Descriptor
  desc->setValue("Read Characteristic");
  read_characteristic->addDescriptor(desc);
  pService->start();

  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);

  // ---- Custom Advertising ----
  BLEAdvertisementData advData;

  uint8_t man_data[4] = {0xFF, 0xFF, 0x03, 0x00};

  // Add it as manufacturer-specific data (type 0xFF in BLE spec)
  advData.setManufacturerData((char *)man_data);

  // Add service UUID if you want clients to see it
  advData.setCompleteServices(BLEUUID(SERVICE_UUID));

  // Apply the custom advertising data
  pAdvertising->setAdvertisementData(advData);

  BLEDevice::startAdvertising();
}

void drawSTATUS(bool status)
{
  String str = status ? "cn" : "dc";
  tft->setTextSize(1);

  int16_t cW = tft->textWidth("cn");
  int16_t dW = tft->textWidth("dc");
  int16_t w = cW > dW ? cW : dW;
  int16_t x = 190;
  int16_t y = 0;
  int16_t h = tft->fontHeight() + 4;
  uint16_t col = status ? TFT_GREEN : TFT_GREY;
  tft->fillRoundRect(x, y, w, h, 3, col);
  tft->setTextColor(TFT_BLACK, col);
  tft->drawString(str, x + 2, y);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Start");

  // pages[0].values[0].setValue(123);
  // pages[0].values[0].setDesc("Gas level");
  // pages[0].values[0].setUnit("ppm");
  // pages[0].values[1].setValue(67);
  // pages[0].values[1].setDesc("Infr sensor");
  // pages[0].values[1].setUnit("%");
  // pages[0].values[2].setValue(89);
  // pages[0].values[2].setDesc("Left");
  // pages[0].values[2].setUnit("C");
  // pages[0].values[3].setValue(35);
  // pages[0].values[3].setDesc("Right");
  // pages[0].values[3].setUnit("C");
  // pages[0].values[4].setValue(0);
  pages[0].values[4].setDesc("Boots");
  // pages[0].values[4].setUnit("");
  // pages[1].values[0].setValue(31);
  // pages[1].values[0].setDesc("Temp Air");
  // pages[1].values[0].setUnit("C");
  // pages[1].values[1].setValue(45);
  // pages[1].values[1].setDesc("Temp In");
  // pages[1].values[1].setUnit("C");
  // pages[1].values[2].setValue(132);
  // pages[1].values[2].setDesc("Press");
  // pages[1].values[2].setUnit("hPa");
  // pages[1].values[3].setValue(67);
  // pages[1].values[3].setDesc("Heart rate");
  // pages[1].values[3].setUnit("BPM");
 

  pages[1].setLayoutType(NON_CRITICAL_INFO_LAYOUT);

  // Get watch instance
  ttgo = TTGOClass::getWatch();
  // Initialize the hardware
  ttgo->begin();
  // Turn on the backlight
  ttgo->openBL();
  //  Receive as a local variable for easy writing
  rtc = ttgo->rtc;
  tft = ttgo->tft;

  // Turn on the IRQ used

  pinMode(AXP202_INT, INPUT_PULLUP);
  attachInterrupt(AXP202_INT, []
                  { irq = true; }, FALLING);
  ttgo->power->adc1Enable(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1, AXP202_ON);
  ttgo->power->enableIRQ(AXP202_VBUS_REMOVED_IRQ | AXP202_PEK_SHORTPRESS_IRQ | AXP202_VBUS_CONNECT_IRQ | AXP202_CHARGING_FINISHED_IRQ, AXP202_ON);
  ttgo->power->clearIRQ();

  // attach touch screen interrupt pin
  pinMode(TP_INT, INPUT);
  ttgo->motor_begin();

  // Time check will be done, if the time is incorrect, it will be set to compile time
  rtc->check();
  setupBLE();
  // set font type
  tft->setTextFont(2);

  // Draw initial connection status
  drawSTATUS(false);
  updateBatIcon(LV_ICON_CALCULATION);

  //   // AUDIO
  //   ttgo->enableLDO3();

  //   file = new AudioFileSourcePROGMEM(pika, sizeof(pika));
  //   id3 = new AudioFileSourceID3(file);

  // #if defined(STANDARD_BACKPLANE)
  //   out = new AudioOutputI2S(0, 1);
  // #elif defined(EXTERNAL_DAC_BACKPLANE)
  //   out = new AudioOutputI2S();
  //   // External DAC decoding
  //   out->SetPinout(TWATCH_DAC_IIS_BCK, TWATCH_DAC_IIS_WS, TWATCH_DAC_IIS_DOUT);
  // #endif
  //   mp3 = new AudioGeneratorMP3();
}

void loop()
{
  // if (mp3->isRunning())
  // {
  //   if (!mp3->loop())
  //   {
  //     mp3->stop();
  //     file = new AudioFileSourcePROGMEM(pika, sizeof(pika));
  //     id3 = new AudioFileSourceID3(file);
  //     mp3 = new AudioGeneratorMP3();
  //   }
  // }
  if (millis() - interval > 1000)
  {

    ttgo->power->readIRQ();
    if (ttgo->power->isVbusPlugInIRQ())
    {
      updateBatIcon(VBUS_PLUGIN);
    }
    if (ttgo->power->isVbusRemoveIRQ())
    {
      updateBatIcon(VBUS_REMOVE);
    }
    if (ttgo->power->isChargingDoneIRQ())
    {
      updateBatIcon(LV_ICON_BAT_FULL);
    }
    if (ttgo->power->isPEKShortPressIRQ())
    {
      ttgo->power->clearIRQ();
    }
    ttgo->power->clearIRQ();

    interval = millis();
    if (fire_alarm)
    {
      set_time_layout();
      set_alarm_layout();
    }
    else if (pages[current_page].getLayout() == CRITICAL_INFO_LAYOUT)
    {
      set_layout_0();
    }
    else if (pages[current_page].getLayout() == NON_CRITICAL_INFO_LAYOUT)
    {
      set_layout_1();
    }

    if (vibration)
    {
      ttgo->motor->onec();
    }
  }

  if (digitalRead(TP_INT) == LOW)
  {
    if (ttgo->getTouch(x, y))
    {
      delay(100);
      current_page++;
      tft->fillScreen(TFT_BLACK);
      if (current_page > 7)
      {
        fire_alarm = true;
        current_page = 0;
      }
    }
  }

  // DISPLAY SLEEP
  if (irq)
  {
    irq = false;
    ttgo->power->readIRQ();
    if (ttgo->power->isPEKShortPressIRQ())
    {
      // Clean power chip irq status
      ttgo->power->clearIRQ();

      // Set  touchscreen sleep
      ttgo->displaySleep();

      ttgo->powerOff();

      // Set all channel power off
      ttgo->power->setPowerOutPut(AXP202_LDO3, false);
      ttgo->power->setPowerOutPut(AXP202_LDO4, false);
      ttgo->power->setPowerOutPut(AXP202_LDO2, false);
      ttgo->power->setPowerOutPut(AXP202_EXTEN, false);
      ttgo->power->setPowerOutPut(AXP202_DCDC2, false);

      // TOUCH SCREEN  Wakeup source
      // esp_sleep_enable_ext1_wakeup(GPIO_SEL_38, ESP_EXT1_WAKEUP_ALL_LOW);
      // PEK KEY  Wakeup source
      esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
      esp_deep_sleep_start();
    }
    ttgo->power->clearIRQ();
  }
}
