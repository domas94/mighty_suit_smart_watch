/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID "9ff3f90c-9a35-446e-9fd9-68a5a1d792ae"
#define READ_UUID "862c9860-0d89-4caf-8902-dc615e1181e9"
#define WRITE_UUID "862c9860-1d89-4caf-8902-dc615e1181e9"

BLECharacteristic *rCharacteristic;

class MyCallbacks: public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *characteristic)
    {
        rCharacteristic->setValue("notf promjena");
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  BLEDevice::init("Long name works now");
BLEServer *pServer = BLEDevice::createServer();
BLEService *pService = pServer->createService(SERVICE_UUID);
rCharacteristic = pService->createCharacteristic(
                                         WRITE_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE |
                                         BLECharacteristic::PROPERTY_NOTIFY
                                       );
BLECharacteristic *wCharacteristic = pService->createCharacteristic(
                                         READ_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  wCharacteristic->setCallbacks(new MyCallbacks());
  rCharacteristic->setValue("read char");
  wCharacteristic->setValue("write char");

  BLEDescriptor *desc = new BLEDescriptor(
    BLEUUID((uint16_t)0x2901));   // User Description Descriptor
  desc->setValue("Read Characteristic");
  rCharacteristic->addDescriptor(desc);
  pService->start();

  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(2000);
}
