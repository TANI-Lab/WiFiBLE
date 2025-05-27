#include <freertos/FreeRTOS.h>

#include <ESP32DMASPISlave.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

ESP32DMASPI::Slave slave;
#define LED_PIN  13
#define BLE_DEVICE_NAME     "HAHA-NO-ECHO"
#define SERVICE_UUID        "3f9b3b8e-0c3d-4a72-85a6-b267e9b3b52e"
#define CHARACTERISTIC_UUID "c67427d5-e705-4b37-8e58-68384ae1eacf"

BLEServer *gpBLEServer = NULL;
BLECharacteristic *gpCharacteristic = NULL;
bool gbBLEReady = false;
bool deviceConnected = false;
bool oldDeviceConnected = false;


byte gbRXData[ 128 ];
byte gbTXData[ 128 ];
bool bleOn = false;
bool ledSW = false;

static const uint32_t BUFFER_SIZE = 8192;
uint8_t* spi_slave_tx_buf;
uint8_t* spi_slave_rx_buf;

void setup() {
  // put your setup code here, to run once:
  Serial.begin( 115200 );
  memset( gbRXData, 0x00, sizeof( gbRXData ) );
  memset( gbTXData, 0x00, sizeof( gbTXData ) );
  // check file system exists
  pinMode(LED_PIN, OUTPUT);

  xTaskCreatePinnedToCore( Task_Bluetooth,   /* タスクの入口となる関数名 */
                           "TASK1", /* タスクの名称 */
                           4096,   /* スタックサイズ */
                           NULL,    /* パラメータのポインタ */
                           1,       /* プライオリティ */
                           NULL,    /* ハンドル構造体のポインタ */
                           0 );     /* 割り当てるコア (0/1) */


  
}
//---------------------------------------------------------
// Callbacks
//---------------------------------------------------------
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  };
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic2) {
    String rxValue = pCharacteristic2->getValue();
    //String rxValue = rxValues.c_str();
    rxValue.trim();
    memset( gbRXData, 0x00, sizeof( gbRXData ) );
    if( rxValue.length() > 0 ){
      bleOn = rxValue[0]!=0;
      for(int i=0; i<rxValue.length(); i++ ){
        gbRXData[ i ] = rxValue[ i ];
      }
    }
  }
};

void loop() {
  // put your main code here, to run repeatedly:
  
}

void Task_Bluetooth( void *param )
{
  BluetoothSetup();
  while( 1 )
  {
    Bluetooth_Loop();
    vTaskDelay( 100 );
  }
}

void BluetoothSetup()
{
  
  BLEDevice::init( BLE_DEVICE_NAME );
  // Server
  gpBLEServer = BLEDevice::createServer();
  gpBLEServer->setCallbacks( new MyServerCallbacks() );
  // Service
  BLEService *pService = gpBLEServer->createService( SERVICE_UUID );
  // Characteristic
  gpCharacteristic = pService->createCharacteristic(
                                CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ   |
                                BLECharacteristic::PROPERTY_WRITE  |
                                BLECharacteristic::PROPERTY_NOTIFY |
                                BLECharacteristic::PROPERTY_INDICATE
  );
  gpCharacteristic->addDescriptor( new BLE2902() );
  gpCharacteristic->setCallbacks( new MyCharacteristicCallbacks() );
  //
  pService->start();
  // Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID( SERVICE_UUID );
  pAdvertising->setScanResponse( true );
  pAdvertising->setMinPreferred( 0x0 );
  pAdvertising->setMinPreferred( 0x06 );  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred( 0x12 );
  BLEDevice::startAdvertising();
  
}


void Bluetooth_Loop()
{
  int iStatus;
  
  // disconnecting
  if(!deviceConnected && oldDeviceConnected){
    vTaskDelay(500); // give the bluetooth stack the chance to get things ready
    gpBLEServer->startAdvertising();
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if(deviceConnected && !oldDeviceConnected){
    oldDeviceConnected = deviceConnected;
    
  }

  if ( ( deviceConnected == true ) && ( bleOn == true ) ){

    //memset( gbTXData, 0x00, sizeof( gbTXData ) );
    
    if ( ledSW == true ) {
        ledSW = false;
        digitalWrite(LED_PIN, LOW);
    }else{
        ledSW = true;
        digitalWrite(LED_PIN, HIGH);
    }
    
    memcpy( gbTXData, gbRXData, sizeof( gbRXData ) );

    gpCharacteristic->setValue( (uint8_t *)&gbTXData, strlen( (char *)&gbTXData ) );
    gpCharacteristic->notify();

    vTaskDelay( 50 );
  }

  if ( bleOn == true ) bleOn = false;

}

