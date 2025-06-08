#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <freertos/FreeRTOS.h>

#include <ESP32DMASPISlave.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <Adafruit_PWMServoDriver.h>
#include <algorithm> // std::max() を使うために必要

ESP32DMASPI::Slave slave;
#define LED_PIN  13
#define BLE_DEVICE_NAME     "HAHA-NO-ECHO"
#define SERVICE_UUID        "3f9b3b8e-0c3d-4a72-85a6-b267e9b3b52e"
#define CHARACTERISTIC_UUID "c67427d5-e705-4b37-8e58-68384ae1eacf"

#define C_WIRE_SDA 21     // I2C
#define C_WIRE_SCL 22     // I2C

#define C_CMD_TEMP    ("Temp")
#define C_CMD_SERVO   ("Sv")


// PCA9685のI2Cアドレス
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
// サーボモーターのPWMパルス範囲（500us～2500us）
#define SERVO_MIN  108  // 500us
#define SERVO_MAX  487  // 2500us




BLEServer *gpBLEServer = NULL;
BLECharacteristic *gpCharacteristic = NULL;
bool gbBLEReady = false;
bool deviceConnected = false;
bool oldDeviceConnected = false;

byte gbRXData[ 128 ];
byte gbTXData[ 128 ];
bool bleOn = false;
bool ledSW = false;
int gbRXDataPos = 0;

static const uint32_t BUFFER_SIZE = 8192;
uint8_t* spi_slave_tx_buf;
uint8_t* spi_slave_rx_buf;

bool parse_command(const char *input, char *cmd_buf, size_t cmd_buf_size, int *channel, int *value) {
    char temp[64];
    strncpy(temp, input, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0'; // 念のためnull終端保証

    char *token = strtok(temp, ",");
    if (!token) return false;

    // コマンド名
    if (strlen(token) >= cmd_buf_size) return false;
    strcpy(cmd_buf, token);

    // チャンネル
    token = strtok(NULL, ",");
    if (!token) {
        *channel = 0;
        *value = 0;
        return true;  // チャンネル・値なしでもOK扱い
    }
    char *endptr;
    *channel = strtol(token, &endptr, 10);
    if (*endptr != '\0') return false;

    // 値
    token = strtok(NULL, ",");
    if (!token) {
        *value = 0;
        return true;  // 値なしなら0に
    }
    *value = strtol(token, &endptr, 10);
    if (*endptr != '\0') return false;

    return true;
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin( 115200 );
  memset( gbRXData, 0x00, sizeof( gbRXData ) );
  memset( gbTXData, 0x00, sizeof( gbTXData ) );
  // check file system exists
  pinMode(LED_PIN, OUTPUT);
  Wire.begin( C_WIRE_SDA, C_WIRE_SCL );

  xTaskCreatePinnedToCore( Task_Bluetooth,   /* タスクの入口となる関数名 */
                           "TASK1", /* タスクの名称 */
                           4096,   /* スタックサイズ */
                           NULL,    /* パラメータのポインタ */
                           1,       /* プライオリティ */
                           NULL,    /* ハンドル構造体のポインタ */
                           0 );     /* 割り当てるコア (0/1) */

  xTaskCreatePinnedToCore( Task_Servo,   
                           "TASK2", 
                           2048,   
                           NULL,    
                           1,       
                           NULL,    
                           0 );     
  
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
    memset( gbRXData, 0x00, sizeof( gbRXData ) );
    if( rxValue.length() > 0 ){
      int iOffset = gbRXDataPos;
      for(int i=0; i<rxValue.length(); i++ ){
        if ( byte( rxValue[ i ] ) == 0 ) continue;
        gbRXData[ iOffset + i ] = rxValue[ i ];
        gbRXDataPos++;
      }
      if ( gbRXData[ gbRXDataPos -1 ] == 0x0A ){
        gbRXData[ gbRXDataPos -1 ] = 0x00;
        bleOn = true;
        gbRXDataPos = 0;
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

  pwm.begin();
  pwm.setPWMFreq(50); // 50Hzサーボ駆動

}
void moveServo(int servoChannel, int angle) {
  angle = constrain(angle, 0, 180);
  int pulse = map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
  pwm.setPWM(servoChannel, 0, pulse);
}

void Bluetooth_Loop()
{
  int iStatus;
  char cmd[16];
  int ch, val;

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

    if ( ledSW == true ) {
        ledSW = false;
        digitalWrite(LED_PIN, LOW);
    }else{
        ledSW = true;
        digitalWrite(LED_PIN, HIGH);
    }

//    if (memcmp(gbRXData, C_CMD_SERVO, sizeof(C_CMD_SERVO) ) == 0) {
    if (parse_command((char*)gbRXData, cmd, sizeof(cmd), &ch, &val)) {
      memset( gbTXData, 0x00, sizeof( gbTXData ) );
      sprintf( (char *)gbTXData, "unknown command.\n"
      );
      if (memcmp(cmd, C_CMD_SERVO, sizeof(C_CMD_SERVO) ) == 0) {
        memset( gbTXData, 0x00, sizeof( gbTXData ) );
        sprintf( (char *)gbTXData, "NG\n" );

        if ( ( ch >= 0 ) && ( ch <= 16 ) ) {
          moveServo( ch, val );
          sprintf( (char *)gbTXData, "OK\n" );
        }
      }
    }else{
      memset( gbTXData, 0x00, sizeof( gbTXData ) );
      sprintf( (char *)gbTXData, "unknown command.\n"
      );

    }

    gpCharacteristic->setValue( (uint8_t *)&gbTXData, strlen( (char *)&gbTXData ) );
    gpCharacteristic->notify();

    vTaskDelay( 50 );
  }

  if ( bleOn == true ) bleOn = false;

}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void Task_Servo( void *param )
{
  int iMin;
  
  Task_Servo_Setup();
  while ( 1 ){
    Task_Servo_Loop();
    iMin = 0;
    while ( iMin < 1 ){
      vTaskDelay( 100 );
      iMin++;
    }
  }
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void Task_Servo_Setup()
{
//  while ( !bme.begin() ){
//    vTaskDelay( 100 );
//  }
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void Task_Servo_Loop()
{
//  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
//  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
//
//  bme.read( gfPres, gfTemp, gfHum, tempUnit, presUnit );
//  gfPres /= 100.0;     // Bug?

}
