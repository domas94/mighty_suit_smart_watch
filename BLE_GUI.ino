/*
Copyright (c) 2019 lewis he
This is just a demonstration. Most of the functions are not implemented.
The main implementation is low-power standby.
The off-screen standby (not deep sleep) current is about 4mA.
Select standard motherboard and standard backplane for testing.
Created by Lewis he on October 10, 2019.
*/

// Please select the model you want to use in config.h
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "config.h"



#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include <soc/rtc.h>
#include "esp_wifi.h"
#include "esp_sleep.h"
#include <WiFi.h>
#include "gui.h"


#define G_EVENT_VBUS_PLUGIN         _BV(0)
#define G_EVENT_VBUS_REMOVE         _BV(1)
#define G_EVENT_CHARGE_DONE         _BV(2)

#define G_EVENT_WIFI_SCAN_START     _BV(3)
#define G_EVENT_WIFI_SCAN_DONE      _BV(4)
#define G_EVENT_WIFI_CONNECTED      _BV(5)
#define G_EVENT_WIFI_BEGIN          _BV(6)
#define G_EVENT_WIFI_OFF            _BV(7)

enum {
    Q_EVENT_WIFI_SCAN_DONE,
    Q_EVENT_WIFI_CONNECT,
    Q_EVENT_BMA_INT,
    Q_EVENT_AXP_INT,
} ;

#define DEFAULT_SCREEN_TIMEOUT  30*1000

#define WATCH_FLAG_SLEEP_MODE   _BV(1)
#define WATCH_FLAG_SLEEP_EXIT   _BV(2)
#define WATCH_FLAG_BMA_IRQ      _BV(3)
#define WATCH_FLAG_AXP_IRQ      _BV(4)

QueueHandle_t g_event_queue_handle = NULL;
EventGroupHandle_t g_event_group = NULL;
bool lenergy = false;
TTGOClass *ttgo;
TFT_eSPI *tft;



bool deviceConnected = false;
bool oldDeviceConnected = false;

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
        memset(response_array, 0, sizeof(response_array));
        // Identify capability
        if (write_value[1] == 0x01)
        {
          response_array[0] = 0x01;
          response_array[1] = 0x0E;
          response_array[2] = 0x01;
          response_array[3] = 0x02;
          response_array[4] = 0x03;
          response_array[5] = 0x04;
          response_array[6] = 0x18;
          response_array[7] = 0x19;
          response_array[8] = 0x1A;
          response_array[9] = 0x1B;
          response_array[10] = 0x1C;
          response_array[11] = 0x1D;
          response_array[12] = 0x1D;
          response_array[13] = 0x1E;
          response_array[14] = 0x20;
          response_array[15] = 0x21;
        }
        int level;
        // Battery level in %
        if (write_value[1] == 0x02)
        {
          level = ttgo->power->getBattPercentage();
          response_array[0] = 0x02;
          response_array[1] = 0x01;
          response_array[2] = level;
          Serial.println(level);

        }
        // Battery level in mV
        if (write_value[1] == 0x03)
        {
          level = ttgo->power->getBattVoltage();
          response_array[0] = 0x03;
          response_array[1] = 0x02;
          response_array[2] = level;
          response_array[3] = level>>2;
          Serial.println(level);

        }
        // 
        // FW version
        if (write_value[1] == 0x04)
        {
          response_array[0] = 0x02;
          response_array[1] = 0x06;
          response_array[2] = 0x04;
          response_array[3] = 0x00;
          response_array[4] = 0x03;
          response_array[5] = 0x01;
          response_array[5] = 0x00;
          response_array[5] = 0x01;
          int level = ttgo->power->getBattPercentage();
          Serial.println(level);

        }
        read_characteristic->setValue(response_array);

        for (int i = 0; i < write_value.length(); i++)
          Serial.print(write_value[i]);
       
      }

      if (write_value.length() <= 0) {
        return;
      }
    }
};

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

void setup()
{
    Serial.begin(115200);

    //Create a program that allows the required message objects and group flags
    g_event_queue_handle = xQueueCreate(20, sizeof(uint8_t));
    g_event_group = xEventGroupCreate();

    ttgo = TTGOClass::getWatch();

    //Initialize TWatch
    ttgo->begin();

    // Turn on the IRQ used
    ttgo->power->adc1Enable(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1, AXP202_ON);
    ttgo->power->enableIRQ(AXP202_VBUS_REMOVED_IRQ | AXP202_VBUS_CONNECT_IRQ | AXP202_CHARGING_FINISHED_IRQ, AXP202_ON);
    ttgo->power->clearIRQ();

    //Initialize lvgl
    ttgo->lvgl_begin();

    // Enable BMA423 interrupt ，
    // The default interrupt configuration,
    // you need to set the acceleration parameters, please refer to the BMA423_Accel example
    ttgo->bma->attachInterrupt();

    //Connection interrupted to the specified pin
    pinMode(BMA423_INT1, INPUT);
    attachInterrupt(BMA423_INT1, [] {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        uint8_t data = Q_EVENT_BMA_INT;
        xQueueSendFromISR(g_event_queue_handle, &data, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR ();
        }
    }, RISING);

    // Connection interrupted to the specified pin
    pinMode(AXP202_INT, INPUT);
    attachInterrupt(AXP202_INT, [] {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        uint8_t data = Q_EVENT_AXP_INT;
        xQueueSendFromISR(g_event_queue_handle, &data, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR ();
        }
    }, FALLING);

    //Check if the RTC clock matches, if not, use compile time
    ttgo->rtc->check();

    //Synchronize time to system time
    ttgo->rtc->syncToSystem();




    //Execute your own GUI interface
    setupGui();

    //Clear lvgl counter
    lv_disp_trig_activity(NULL);


    //When the initialization is complete, turn on the backlight
    ttgo->openBL();

    setupBLE();
}

void loop()
{
    bool  rlst;
    uint8_t data;
    if (xQueueReceive(g_event_queue_handle, &data, 5 / portTICK_RATE_MS) == pdPASS) {
        switch (data) {
        case Q_EVENT_BMA_INT:
            do {
                rlst =  ttgo->bma->readInterrupt();
            } while (!rlst);
            //! setp counter
            if (ttgo->bma->isStepCounter()) {
                updateStepCounter(ttgo->bma->getCounter());
            }
            break;
        case Q_EVENT_AXP_INT:
            ttgo->power->readIRQ();
            if (ttgo->power->isVbusPlugInIRQ()) {
                updateBatteryIcon(LV_ICON_CHARGE);
            }
            if (ttgo->power->isVbusRemoveIRQ()) {
                updateBatteryIcon(LV_ICON_CALCULATION);
            }
            if (ttgo->power->isChargingDoneIRQ()) {
                updateBatteryIcon(LV_ICON_CALCULATION);
            }
            if (ttgo->power->isPEKShortPressIRQ()) {
                ttgo->power->clearIRQ();
                return;
            }
            ttgo->power->clearIRQ();
            break;

        default:
            break;
        }
    }

        lv_task_handler();
    
}
