/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

// TFT_BLACK	0x0000	0	0	0	
// TFT_NAVY	0x000F	0	0	128	
// TFT_DARKGREEN	0x03E0	0	128	0	
// TFT_DARKCYAN	0x03EF	0	128	128	
// TFT_MAROON	0x7800	128	0	0	
// TFT_PURPLE	0x780F	128	0	128	
// TFT_OLIVE	0x7BE0	128	128	0	
// TFT_LIGHTGREY	0xD69A	211	211	211	
// TFT_DARKGREY	0x7BEF	128	128	128	
// TFT_BLUE	0x001F	0	0	255	
// TFT_GREEN	0x07E0	0	255	0	
// TFT_CYAN	0x07FF	0	255	255	
// TFT_RED	0xF800	255	0	0	
// TFT_MAGENTA	0xF81F	255	0	255	
// TFT_YELLOW	0xFFE0	255	255	0	
// TFT_WHITE	0xFFFF	255	255	255	
// TFT_ORANGE	0xFDA0	255	180	0	
// TFT_GREENYELLOW	0xB7E0	180	255	0	
// TFT_PINK	0xFE19	255	192	203	Lighter pink,
// was 0xFC9F
// TFT_BROWN	0x9A60	150	75	0	
// TFT_GOLD	0xFEA0	255	215	0	
// TFT_SILVER	0xC618	192	192	192	
// TFT_SKYBLUE	0x867D	135	206	235	
// TFT_VIOLET	0x915C	180	46	226	
// TFT_TRANSPARENT	0x0120		

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "head_fire.h"
#include "fire_alarm.h"
#include "config.h"

#include "AudioFileSourcePROGMEM.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

AudioGeneratorMP3 *mp3;
AudioFileSourcePROGMEM *file;
AudioOutputI2S *out;
AudioFileSourceID3 *id3;

TTGOClass *ttgo;
TFT_eSPI *tft;
PCF8563_Class *rtc;

bool deviceConnected = false;
bool fire_alarm_flag = false;
bool vibration = false;
uint32_t interval = 0;
int16_t x, y;
bool irq = false;
uint8_t test_brightness = 0;
uint16_t vibration_time = 0;
bool init_done = true;
bool refresh_screen = false;

byte new_layout;

byte max_pages = 8;
byte current_page = 0;

BLECharacteristic *read_characteristic;

uint8_t response_array[131];

#define SERVICE_UUID "9ff3f90c-9a35-446e-9fd9-68a5a1d792ae"
#define WRITE_UUID "862c9860-0d89-4caf-8902-dc615e1181e9"
#define READ_UUID "862c9860-1d89-4caf-8902-dc615e1181e9"
#define TFT_GREY 0x5AEB
#define COMMAND_KEY 0
#define CRITICAL_INFO_LAYOUT 0
#define NON_CRITICAL_INFO_LAYOUT 1
#define ALARM_LAYOUT 8
#define TIME_LAYOUT 9
#define MAX_STR_LEN 12

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

class ValueAttrs
{
private:
  int _value;
  String _desc;
  String _unit;
  int _value_digits;
  int _value_color;

public:
  // Constructor (runs automatically when object is created)
  ValueAttrs()
  {
    _value = 0;
    _desc = "";
    _unit = "";
    _value_digits = 3;
    _value_color = TFT_YELLOW;
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
    refresh_screen = true;
  }

  void setColor(int new_color)
  {
    _value_color = new_color;
    refresh_screen = true;
  }

  int getColor()
  {
    return _value_color;
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
  void setDesc(String new_desc, int len)
  {
    if (len < MAX_STR_LEN)
    {
      // for (int i = 0; i < len; i++)
      {
        _desc = new_desc;
        refresh_screen = true;
      }
      // _desc[MAX_STR_LEN] = '\0';
    }
  }

  String getDesc()
  {
    return _desc;
  }
  void setUnit(String new_unit, int len)
  {
    if (len < MAX_STR_LEN)
    {
      // for (int i = 0; i < len; i++)
      {
        _unit = new_unit;
        refresh_screen = true;
      }
      //_unit[MAX_STR_LEN] = '\0';
    }
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
  byte _max_value_cnt;
  byte _max_digit_num[8];

  // what if value not printed / vidjeti s Kresom?

public:
  ValueAttrs values[8];

  // Constructor (runs automatically when object is created)
  PageSetup()
  {
    _layout_type = CRITICAL_INFO_LAYOUT;
    _max_value_cnt = 8;
  }

  void setLayoutType(int new_layout)
  {
    _layout_type = new_layout;
  }

  int getLayout()
  {
    return _layout_type;
  }

  void setMaxPageValCnt(int new_max_value_cnt)
  {
    if (new_max_value_cnt > 1 && new_max_value_cnt < 8)
    {
      _max_value_cnt = new_max_value_cnt;
    }
  }

  void setMaxDigitNum(int new_max_digit_num, byte index)
  {
    if (new_max_digit_num > 1 && new_max_digit_num < 10)
    {
      _max_digit_num[index] = new_max_digit_num;
    }
  }

  byte getPageValueCnt()
  {
    return _max_value_cnt;
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
    String str_for_value;

    if (write_value.length() > 0)
    {
      Serial.println("START*********");
      Serial.print("Write value length: ");
      Serial.println(write_value.length() + 1);

      write_value.c_str();
      // clear response
      memset(response_array, 0, sizeof(response_array));

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
        response_array[12] = 0x17;
        response_array[13] = 0x00;
        response_array[14] = 0x18;
        response_array[15] = 0x00;
        response_array[16] = 0x19;
        response_array[17] = 0x00;
        response_array[18] = 0x1A;
        response_array[19] = 0x00;
        response_array[20] = 0x1B;
        response_array[21] = 0x00;
        response_array[22] = 0x1C;
        response_array[23] = 0x00;
        // response_array[22] = 0x1D;
        // response_array[23] = 0x00;
        // response_array[24] = 0x1E;
        // response_array[25] = 0x00;
        // response_array[26] = 0x20;
        // response_array[27] = 0x00;
        response_array[24] = 0x21;
        response_array[25] = 0x00;
        response_array[26] = 0x23;
        response_array[27] = 0x00;
        response_array_size = 28;
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
        response_array[2] = level & 0xFF;
        response_array[3] = (level >> 8) & 0xFF;
        response_array_size = 4;
      }
      // FW version
      if (write_value[COMMAND_KEY] == 0x04)
      {
        response_array[0] = 0x04;
        response_array[1] = 0x00;
        response_array[2] = 0x03;
        response_array[3] = 0x01;
        response_array[4] = 0x00;
        response_array[5] = 0x01;

        response_array_size = 6;
      }
      // Set number of watch pages
      if (write_value[COMMAND_KEY] == 0x17)
      {
        response_array[0] = 0x17;
        response_array[1] = 0x00;
        // accepted
        response_array[2] = 0x00;
        response_array[3] = 0x00;
        response_array_size = 4;

        // check if the desired watch page number is out of limits
        if (write_value[2] > 0 && write_value[2] < 9)
        {
          max_pages = write_value[2];
          Serial.println("Max page number set to");
          Serial.println(max_pages + 1);
        }
        // Number of pages too low
        if (write_value[2] < 1)
        {
          response_array[2] = 0x0C;
        }
        // Number of pages too high
        if (write_value[2] > 8)
        {
          response_array[2] = 0x0D;
        }
      }
      // Set number of values on a page
      if (write_value[COMMAND_KEY] == 0x18)
      {
        response_array[0] = 0x18;
        response_array[1] = 0x00;
        // accepted
        response_array[2] = 0x00;
        response_array[3] = 0x00;
        response_array_size = 4;
        // check if page exists
        if (write_value[2] > 0 && write_value[2] <= max_pages)
        {
          // if correct number of values
          if (write_value[3] > 1 && write_value[3] < 9)
          {
            pages[write_value[2]].setMaxPageValCnt(write_value[3]);
          }
          // number of values too low
          if (write_value[3] < 1)
          {
            response_array[2] = 0x0E;
          }
          // number of values too high
          if (write_value[3] > 8)
          {
            response_array[2] = 0x0F;
          }
        }
        // page doesn't exist
        if (write_value[2] < 1 || write_value[2] > max_pages)
        {
          response_array[2] = 0x05;
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
        if (write_value[2] > 0 && write_value[2] <= max_pages)
        {
          // check if layout exists
          if (write_value[3] == CRITICAL_INFO_LAYOUT || write_value[3] == NON_CRITICAL_INFO_LAYOUT)
          {
            Serial.println("Page layout set to:");
            pages[write_value[2]].setLayoutType(write_value[3]);
          }

          else
          {
            // layout doesn't exist
            response_array[2] = 0x10;
          }
        }
        else
        {
          // page doesn't exist error code
          response_array[2] = 0x05;
        }
      }

      // set number of digits for a page
      if (write_value[COMMAND_KEY] == 0x1A)
      {
        response_array[0] = 0x1A;
        response_array[1] = 0x00;
        // accepted
        response_array[2] = 0x00;
        response_array[3] = 0x00;
        response_array_size = 4;
        // check if page exists
        if (write_value[2] > 0 && write_value[2] <= max_pages)
        {
          // check if value exists
          if (write_value[3] > 0 && write_value[3] <= pages[write_value[2]].getPageValueCnt())
          {
            // check digit number
            if (write_value[4] > 0 && write_value[4] < 4)
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
            else if (write_value[4] > 3)
            {
              // number of digits too high
              response_array[2] = 0x13;
            }
          }
          else
          {
            // value doesn't exist
            response_array[2] = 0x11;
          }
        }
        else
        {
          // page doesn't exist error code
          response_array[2] = 0x05;
        }
      }

      // set unit string for page N, value M
      if (write_value[COMMAND_KEY] == 0x1B)
      {
        response_array[0] = 0x21;
        response_array[1] = 0x00;
        // accepted
        response_array[2] = 0x00;
        response_array[3] = 0x00;
        response_array_size = 4;
        // check if page exists
        if (write_value[2] >= 0 && write_value[2] <= max_pages)
        {
          // check if value exists
          if (write_value[3] > 0 && write_value[3] <= pages[write_value[2]].getPageValueCnt())
          {
            // check if string too long
            if (write_value[4] < MAX_STR_LEN - 1)
            {
              for (int i = 0; i < write_value[4]; i++)
              {
                str_for_value += write_value[i + 5];
              }
              pages[write_value[2]]
                  .values[write_value[3]]
                  .setUnit(str_for_value, write_value[4]);
            }
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

      // set description string for page N, value M
      if (write_value[COMMAND_KEY] == 0x1C)
      {
        response_array[0] = 0x21;
        response_array[1] = 0x00;
        // accepted
        response_array[2] = 0x00;
        response_array[3] = 0x00;
        response_array_size = 4;
        // check if page exists
        if (write_value[2] >= 0 && write_value[2] <= max_pages)
        {
          // check if value exists
          if (write_value[3] > 0 && write_value[3] <= pages[write_value[2]].getPageValueCnt())
          {
            // check if string too long
            if (write_value[4] < MAX_STR_LEN - 1)
            {
              for (int i = 0; i < write_value[4]; i++)
              {
                str_for_value += write_value[i + 5];
              }
              pages[write_value[2]]
                  .values[write_value[3]]
                  .setDesc(str_for_value, write_value[4]);
            }
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

      // set color for page N, value M
      if (write_value[COMMAND_KEY] == 0x1D)
      {
        response_array[0] = 0x21;
        response_array[1] = 0x00;
        // accepted
        response_array[2] = 0x00;
        response_array[3] = 0x00;
        response_array_size = 4;
        // check if page exists
        if (write_value[2] >= 0 && write_value[2] <= max_pages)
        {
          // check if value exists
          if (write_value[3] > 0 && write_value[3] <= pages[write_value[2]].getPageValueCnt())
          {
            if (write_value[4] == 0)
              if (write_value[5] == 255)
                if (write_value[6] == 0)
                {
                  pages[write_value[2]]
                      .values[write_value[3]]
                      .setColor(TFT_GREEN);
                }
            if (write_value[4] == 255)
              if (write_value[5] == 0)
                if (write_value[6] == 0)
                {
                  pages[write_value[2]]
                      .values[write_value[3]]
                      .setColor(TFT_RED);
                }
            if (write_value[4] == 0)
              if (write_value[5] == 0)
                if (write_value[6] == 255)
                {
                  pages[write_value[2]]
                      .values[write_value[3]]
                      .setColor(TFT_BLUE);
                }
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
        if (write_value[2] >= 0 && write_value[2] <= max_pages)
        {
          // check if value exists
          if (write_value[3] > 0 && write_value[3] <= pages[write_value[2]].getPageValueCnt())
          {
            value_for_page += write_value[4] << 0;
            value_for_page += write_value[5] << 8;
            value_for_page += write_value[6] << 16;
            value_for_page += write_value[7] << 24;
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
        response_array_size = 4;
        init_done = true;
      }
      Serial.println();
      read_characteristic->setValue(response_array, response_array_size);
      read_characteristic->notify();

      Serial.println("END*********");
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
  if (refresh_screen)
  {
    tft->fillScreen(TFT_BLACK);
    refresh_screen = false;
  }
  updateBatIcon(LV_ICON_CALCULATION);
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);

  tft->setTextSize(2);
  tft->drawString(String(current_page + 1), 210, 130);

  if (pages[current_page].getPageValueCnt() > 0)
  {
    tft->setTextColor(pages[current_page].values[0].getColor(), TFT_BLACK);
    value_w = tft->textWidth(String(pages[current_page].values[0].getValue()).substring(0, pages[current_page].values[0].getValueDigits()));
    tft->drawString(String(pages[current_page].values[0].getValue()).substring(0, pages[current_page].values[0].getValueDigits()), 0, 10);
    tft->drawString(pages[current_page].values[0].getUnit(), 0 + value_w, 10);
    tft->setTextSize(1);
    tft->drawString(pages[current_page].values[0].getDesc(), 0 + value_w + 50, 20);
  }

  if (pages[current_page].getPageValueCnt() > 1)
  {
    tft->setTextColor(pages[current_page].values[1].getColor(), TFT_BLACK);
    tft->setTextSize(3);
    value_w = tft->textWidth(String(pages[current_page].values[1].getValue()).substring(0, pages[current_page].values[1].getValueDigits()));
    tft->drawString(String(pages[current_page].values[1].getValue()).substring(0, pages[current_page].values[1].getValueDigits()), 85, 80);
    tft->drawString(pages[current_page].values[1].getUnit(), 85 + value_w, 80);
    tft->setTextSize(1);
    tft->drawString(pages[current_page].values[1].getDesc(), 85, 130);
  }

  if (pages[current_page].getPageValueCnt() > 2)
  {
    tft->setTextColor(pages[current_page].values[2].getColor(), TFT_BLACK);
    value_w = tft->textWidth(String(pages[current_page].values[2].getValue()).substring(0, pages[current_page].values[2].getValueDigits()));
    tft->drawString(String(pages[current_page].values[2].getValue()).substring(0, pages[current_page].values[2].getValueDigits()), 90, 200);
    tft->drawString(pages[current_page].values[2].getUnit(), 90 + value_w, 200);
    tft->setTextSize(1);
    tft->drawString(pages[current_page].values[2].getDesc(), 90, 220);
  }

  if (pages[current_page].getPageValueCnt() > 3)
  {
    tft->setTextColor(pages[current_page].values[3].getColor(), TFT_BLACK);
    value_w = tft->textWidth(String(pages[current_page].values[3].getValue()).substring(0, pages[current_page].values[3].getValueDigits()));
    tft->drawString(String(pages[current_page].values[3].getValue()).substring(0, pages[current_page].values[3].getValueDigits()), 170, 200);
    tft->drawString(pages[current_page].values[3].getUnit(), 170 + value_w, 200);
    tft->setTextSize(1);

    tft->drawString(pages[current_page].values[3].getDesc(), 170, 220);
  }

  if (pages[current_page].getPageValueCnt() > 4)
  {
    // value_w = tft->textWidth(String(pages[current_page].values[4].getValue()).substring(0, pages[current_page].values[3].getValueDigits()));
    // tft->drawString(String(pages[current_page].values[4].getValue()).substring(0, pages[current_page].values[3].getValueDigits()), 0, 200);
    // tft->drawString(pages[current_page].values[4].getUnit(), 0 + value_w, 200);
    tft->setTextColor(pages[current_page].values[4].getColor(), TFT_BLACK);
    tft->setTextSize(2);
    tft->drawString(pages[current_page].values[4].getDesc(), 0, 200);
  }
  tft->setTextSize(1);
  tft->drawString(rtc->formatDateTime(PCF_TIMEFORMAT_DD_MM_YYYY), 0, 150);
  tft->drawString(rtc->formatDateTime(PCF_TIMEFORMAT_HMS), 0, 130);
  drawSTATUS(deviceConnected);
}

// non critical info layout
void set_layout_1(void)
{
  int16_t value_w;
  if (refresh_screen)
  {
    tft->fillScreen(TFT_BLACK);
    refresh_screen = false;
  }
  updateBatIcon(LV_ICON_CALCULATION);
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);

  tft->setTextSize(2);
  tft->drawString(String(current_page + 1), 210, 130);
  if (pages[current_page].getPageValueCnt() > 0)
  {
    tft->setTextColor(pages[current_page].values[0].getColor(), TFT_BLACK);
    tft->drawString(String(pages[current_page].values[0].getValue()).substring(0, pages[current_page].values[0].getValueDigits()), 123, 10);
    value_w = tft->textWidth(String(pages[current_page].values[0].getValue()).substring(0, pages[current_page].values[0].getValueDigits()));
    tft->drawString(pages[current_page].values[0].getUnit(), 123 + value_w, 10);
    tft->drawString(pages[current_page].values[0].getDesc(), 0, 10);
  }
  if (pages[current_page].getPageValueCnt() > 1)
  {
    tft->setTextColor(pages[current_page].values[1].getColor(), TFT_BLACK);
    tft->setTextSize(2);
    tft->drawString(String(pages[current_page].values[1].getValue()).substring(0, pages[current_page].values[1].getValueDigits()), 123, 50);
    value_w = tft->textWidth(String(pages[current_page].values[1].getValue()).substring(0, pages[current_page].values[1].getValueDigits()));
    tft->drawString(pages[current_page].values[1].getUnit(), 123 + value_w, 50);
    tft->drawString(pages[current_page].values[1].getDesc(), 0, 50);
  }
  if (pages[current_page].getPageValueCnt() > 2)
  {
    tft->setTextColor(pages[current_page].values[2].getColor(), TFT_BLACK);
    tft->setTextSize(2);
    tft->drawString(String(pages[current_page].values[2].getValue()).substring(0, pages[current_page].values[2].getValueDigits()), 123, 90);
    value_w = tft->textWidth(String(pages[current_page].values[2].getValue()).substring(0, pages[current_page].values[2].getValueDigits()));
    tft->drawString(pages[current_page].values[2].getUnit(), 123 + value_w, 90);
    tft->drawString(pages[current_page].values[2].getDesc(), 0, 90);
  }
  if (pages[current_page].getPageValueCnt() > 3)
  {
    tft->setTextColor(pages[current_page].values[3].getColor(), TFT_BLACK);
    tft->setTextSize(3);
    value_w = tft->textWidth(String(pages[current_page].values[3].getValue()).substring(0, pages[current_page].values[3].getValueDigits()));
    tft->drawString(String(pages[current_page].values[3].getValue()).substring(0, pages[current_page].values[3].getValueDigits()), 80, 160);
    tft->drawString(pages[current_page].values[3].getUnit(), 80 + value_w, 160);
    tft->setTextSize(2);
    tft->drawString(pages[current_page].values[3].getDesc(), 80, 200);
  }
  tft->setTextSize(1);
  tft->drawString(rtc->formatDateTime(PCF_TIMEFORMAT_DD_MM_YYYY), 60, 140);
  tft->drawString(rtc->formatDateTime(PCF_TIMEFORMAT_HMS), 0, 140);
  drawSTATUS(deviceConnected);
}

// set alarm layout
void set_alarm_layout(void)
{
  if (refresh_screen)
  {
    tft->fillScreen(TFT_BLACK);
    tft->setSwapBytes(true);
    tft->pushImage(56, 56, 128, 128, head_fire);
    refresh_screen = false;
  }
  tft->setTextSize(1);
  tft->drawString(rtc->formatDateTime(PCF_TIMEFORMAT_DD_MM_YYYY), 140, 200);
  tft->drawString(rtc->formatDateTime(PCF_TIMEFORMAT_HMS), 30, 200);
  if (!(mp3->isRunning()))
  {
    mp3->begin(id3, out);
  }

  updateBatIcon(LV_ICON_CALCULATION);

  ttgo->setBrightness(test_brightness);
  test_brightness += 30;
  if (test_brightness > 240)
    test_brightness = 0;
  ttgo->motor->onec();
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

  pages[0].values[0].setValue(123);
  pages[0].values[0].setDesc("Gas level", 9);
  pages[0].values[0].setUnit("ppm", 3);
  pages[0].values[0].setColor(TFT_RED);
  pages[0].values[1].setValue(67);
  pages[0].values[1].setDesc("Infr sensor", 11);
  pages[0].values[1].setUnit("%", 1);
  pages[0].values[2].setValue(89);
  pages[0].values[2].setDesc("Left",4);
  pages[0].values[2].setUnit("C",1);
  pages[0].values[3].setValue(35);
  pages[0].values[3].setDesc("Right",5);
  pages[0].values[3].setUnit("C",1);
  pages[0].values[4].setValue(0);
  pages[0].values[4].setDesc("Boots",5);
  pages[0].values[4].setUnit("",0);
  pages[1].values[0].setValue(31);
  pages[1].values[0].setDesc("Temp Air",8);
  pages[1].values[0].setUnit("C",1);
  pages[1].values[1].setValue(45);
  pages[1].values[1].setDesc("Temp In",7);
  pages[1].values[1].setUnit("C",1);
  pages[1].values[2].setValue(132);
  pages[1].values[2].setDesc("Press",5);
  pages[1].values[2].setUnit("hPa",3);
  pages[1].values[3].setValue(67);
  pages[1].values[3].setDesc("Heart rate",10);
  pages[1].values[3].setUnit("BPM",3);

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

  // AUDIO
  ttgo->enableLDO3();

  file = new AudioFileSourcePROGMEM(fire_alarm, sizeof(fire_alarm));
  id3 = new AudioFileSourceID3(file);

#if defined(STANDARD_BACKPLANE)
  out = new AudioOutputI2S(0, 1);
#elif defined(EXTERNAL_DAC_BACKPLANE)
  out = new AudioOutputI2S();
  // External DAC decoding
  out->SetPinout(TWATCH_DAC_IIS_BCK, TWATCH_DAC_IIS_WS, TWATCH_DAC_IIS_DOUT);
#endif
  mp3 = new AudioGeneratorMP3();
}

void loop()
{
  if (mp3->isRunning())
  {
    if (!mp3->loop())
    {
      mp3->stop();
      file = new AudioFileSourcePROGMEM(fire_alarm, sizeof(fire_alarm));
      id3 = new AudioFileSourceID3(file);
      mp3 = new AudioGeneratorMP3();
    }
  }
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
    if (init_done)
    {
      if (fire_alarm_flag)
      {
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

      // if (vibration)
      // {
      //   ttgo->motor->onec();
      // }
    }
  }
  if (digitalRead(TP_INT) == LOW)
  {
    if (ttgo->getTouch(x, y))
    {
      delay(100);
      current_page++;
      refresh_screen = true;
      if (current_page > max_pages - 1)
      {
        fire_alarm_flag = true;
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
      esp_sleep_enable_ext1_wakeup(GPIO_SEL_38, ESP_EXT1_WAKEUP_ALL_LOW);
      // PEK KEY  Wakeup source
      // doesn't always work - maybe debouncing?
      // esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
      esp_deep_sleep_start();
    }
    ttgo->power->clearIRQ();
  }
}
