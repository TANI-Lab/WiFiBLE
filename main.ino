#include <freertos/FreeRTOS.h>

#include "FS.h"
#include "SPIFFS.h"

#include <ESP32DMASPISlave.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

ESP32DMASPI::Slave slave;

#define DATA_RESET false
#define BLE_DATA_FILE "/BLEData"
#define BLE_DEVICE_NAME     "HAHA-NO-IKARI"
#define SERVICE_UUID        "55725ac1-066c-48b5-8700-2d9fb3603c5e"
#define CHARACTERISTIC_UUID "69ddb59c-d601-4ea4-ba83-44f679a670ba"

BLEServer *gpBLEServer = NULL;
BLECharacteristic *gpCharacteristic = NULL;
bool gbBLEReady = false;
bool deviceConnected = false;
bool oldDeviceConnected = false;
char gcBLEDeviceName[ 128 ];
char gcBLEPassword[ 32 ];
char gcWifiSSID[ 32 ];
char gcWifiPassword[ 32 ];
char gcSettingStatus[ 32 ];
char gcStartupSettingStatus;


byte gbRXData[ 128 ];
byte gbTXData[ 128 ];
bool bleOn = false;
bool gbWifiSetting = false;
bool gbWifiReady = false;
bool gbWifiFailure = false;
bool gbTimeReady = false;


static const uint32_t BUFFER_SIZE = 8192;
uint8_t* spi_slave_tx_buf;
uint8_t* spi_slave_rx_buf;

void setup() {
  // put your setup code here, to run once:
  Serial.begin( 115200 );
  memset( gbRXData, 0x00, sizeof( gbRXData ) );
  memset( gbTXData, 0x00, sizeof( gbTXData ) );
  memset( gcBLEDeviceName, 0x00, sizeof( gcBLEDeviceName ) );
  memset( gcBLEPassword, 0x00, sizeof( gcBLEPassword ) );
  memset( gcWifiSSID, 0x00, sizeof( gcWifiSSID ) );
  memset( gcWifiPassword, 0x00, sizeof( gcWifiPassword ) );
  memset( gcSettingStatus, 0x00, sizeof( gcSettingStatus ) );
  // check file system exists
  if (!SPIFFS.begin()) {
    Serial.println("Formating file system");
    SPIFFS.format();
    SPIFFS.begin();
  }
  LoadFromSettingData();

  xTaskCreatePinnedToCore( Task_Bluetooth,   /* タスクの入口となる関数名 */
                           "TASK1", /* タスクの名称 */
                           4096,   /* スタックサイズ */
                           NULL,    /* パラメータのポインタ */
                           1,       /* プライオリティ */
                           NULL,    /* ハンドル構造体のポインタ */
                           0 );     /* 割り当てるコア (0/1) */

  xTaskCreatePinnedToCore( Task_Comunicate,   /* タスクの入口となる関数名 */
                           "TASK2", /* タスクの名称 */
                           4096,   /* スタックサイズ */
                           NULL,    /* パラメータのポインタ */
                           1,       /* プライオリティ */
                           NULL,    /* ハンドル構造体のポインタ */
                           1 );     /* 割り当てるコア (0/1) */

  xTaskCreatePinnedToCore( Task_Wifi,   /* タスクの入口となる関数名 */
                           "TASK3", /* タスクの名称 */
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
    gcSettingStatus[ 0 ] = gcStartupSettingStatus;
  };
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic2) {
    std::string rxValues = pCharacteristic2->getValue();
    String rxValue = rxValues.c_str();
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

void Task_Wifi( void *param )
{

PROC_RETRY:
  
  gbWifiReady = false;
  
  while ( Wifi_Setup() == 1 )
  {
    vTaskDelay( 100 );
  }
  while( 1 )
  {
    if ( Wifi_Loop() == 1 )
    {
      goto PROC_RETRY;
    }
  }
  
}

void Task_Comunicate( void *param )
{
  ComInit();
  
  while( 1 )
  {
    Comunicate_Loop();
    vTaskDelay( 1 );
  }
}

void ComInit(){
  spi_slave_tx_buf = slave.allocDMABuffer(BUFFER_SIZE);
  spi_slave_rx_buf = slave.allocDMABuffer(BUFFER_SIZE);

  memset( spi_slave_tx_buf, 0x00, BUFFER_SIZE );
  memset( spi_slave_rx_buf, 0x00, BUFFER_SIZE );
  
  slave.setDataMode( SPI_MODE0 );
  slave.setMaxTransferSize( BUFFER_SIZE );
  slave.setDMAChannel(1); // 1 or 2 only
  slave.setQueueSize(1); // transaction queue size
  
  // HSPI CS: 15, CLK: 14, MOSI: 13, MISO: 12
  // VSPI CS:  5, CLK: 18, MOSI: 23, MISO: 19
  slave.begin( VSPI );
  
}
void Comunicate_Loop()
{
  int iCmd;
  time_t t;
  struct tm *tm;
  int i;
  uint16_t iYear;
  uint8_t iMonth;
  
  while (slave.available()) {
      iCmd = spi_slave_rx_buf[ 0 ];
      switch( iCmd ){
        case 1:   // Get Status Wifi & BLE 
          spi_slave_tx_buf[ 0 ] = gbWifiReady == true ? 1:0;
          spi_slave_tx_buf[ 1 ] = gbTimeReady == true ? 1:0;
        break;
        case 2:   // Get Local Time
          t = time( NULL );
          tm = localtime( &t );
          iYear = tm->tm_year + 1900;
          iMonth = (uint8_t)tm->tm_mon + 1;
          spi_slave_tx_buf[ 0 ] = (uint8_t)( iYear >> 8 );
          spi_slave_tx_buf[ 1 ] = (uint8_t)iYear;
          spi_slave_tx_buf[ 2 ] = iMonth;
          spi_slave_tx_buf[ 3 ] = (uint8_t)tm->tm_mday;
          spi_slave_tx_buf[ 4 ] = (uint8_t)tm->tm_hour;
          spi_slave_tx_buf[ 5 ] = (uint8_t)tm->tm_min;
          spi_slave_tx_buf[ 6 ] = (uint8_t)tm->tm_sec;
        break;
      }
      slave.pop();
  }
    
  if (slave.remained() == 0) {
      slave.queue(spi_slave_rx_buf, spi_slave_tx_buf, BUFFER_SIZE);
  }
  
}

void LoadFromSettingData(){
  if ( SPIFFS.exists( BLE_DATA_FILE ) ) {
      if ( DATA_RESET == true ) {
        Serial.println( "remove setting file." );
        SPIFFS.remove( BLE_DATA_FILE );
      }else{
        File f = SPIFFS.open(BLE_DATA_FILE, "r");
        if (f) {
          String s;
          s = f.readStringUntil('\n');
          s.trim();
          s.toCharArray( gcBLEDeviceName, sizeof( gcBLEDeviceName ) );
          s = f.readStringUntil('\n');
          s.trim();
          s.toCharArray( gcBLEPassword, sizeof( gcBLEPassword ) );
          s = f.readStringUntil('\n');
          s.trim();
          s.toCharArray( gcWifiSSID, sizeof( gcWifiSSID ) );
          s = f.readStringUntil('\n');
          s.trim();
          s.toCharArray( gcWifiPassword, sizeof( gcWifiPassword ) );
          s = f.readStringUntil('\n');
          s.trim();
          s.toCharArray( gcSettingStatus, sizeof( gcSettingStatus ) );
          
          
          f.close();
        }
      }
  }
  
  // if ( gcSettingStatus[ 0 ] == '1' )
  if ( strlen( gcBLEDeviceName ) == 0 ){
    sprintf( gcBLEDeviceName, "%s", BLE_DEVICE_NAME );    
  }

  if ( (uint8_t)gcSettingStatus[ 0 ] == 0 ) {
    gcSettingStatus[ 0 ] = '0';
  }

  gcStartupSettingStatus = gcSettingStatus[ 0 ];

}

void BluetoothSetup()
{
  
  BLEDevice::init( gcBLEDeviceName );
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

    memset( gbTXData, 0x00, sizeof( gbTXData ) );
    
    iStatus = atoi( &gcSettingStatus[ 0 ] );
    switch( iStatus ){
      case 0:
        sprintf( (char *)&gbTXData, "Please tell me the name of the device.\r\n" );
        gcSettingStatus[ 0 ] = '1';
      break;
      case 1:
      Serial.println( (char*)&gbRXData[ 0 ] );
        memmove( gcBLEDeviceName, gbRXData, sizeof( gcBLEDeviceName ) );
        sprintf( (char *)&gbTXData, "Please specify the Password of the device.\r\n" );
        gcSettingStatus[ 0 ] = '2';
      break;
      case 2:
        memmove( gcBLEPassword, gbRXData, sizeof( gcBLEPassword ) );
        sprintf( (char *)&gbTXData, "Please tell me the SSID of Wifi.\r\n" );
        gcSettingStatus[ 0 ] = '3';
      break;
      case 3:
        memmove( gcWifiSSID, gbRXData, sizeof( gcWifiSSID ) );
        sprintf( (char *)&gbTXData, "Please tell me the Password of Wifi.\r\n" );
        gcSettingStatus[ 0 ] = '4';
      break;
      case 4:
        memmove( gcWifiPassword, gbRXData, sizeof( gcWifiPassword ) );
        sprintf( (char *)&gbTXData, "Settings are complete. Restart your device. enjoy.\r\n" );
        gcSettingStatus[ 0 ] = '9';
      break;
      case 9:
        sprintf( (char *)&gbTXData, "Please enter your password.\r\n" );
        gcSettingStatus[ 0 ] = '5';
      break;
      case 5:
        if ( 0 == strcmp( gcBLEPassword, (char *)&gbRXData[0] ) ){
          sprintf( (char *)&gbTXData, "Please tell me the SSID of Wifi.\r\n" );
          gcSettingStatus[ 0 ] = '3';
        } else {
          sprintf( (char *)&gbTXData, "Please enter your password.\r\n" );
        }
      break;
      default:
        sprintf( (char *)&gbTXData, "enjoy.\r\n" );
      break;
    }
    
    gpCharacteristic->setValue( (uint8_t *)&gbTXData, strlen( (char *)&gbTXData ) );
    gpCharacteristic->notify();
    
    vTaskDelay( 50 );

    if ( gcSettingStatus[ 0 ] == '9' ){

      if ( SPIFFS.exists( BLE_DATA_FILE ) ) {
        SPIFFS.remove( BLE_DATA_FILE );
      }
      File f = SPIFFS.open(BLE_DATA_FILE, "w");
      if (f) {
        f.println( gcBLEDeviceName );
        f.println( gcBLEPassword );
        f.println( gcWifiSSID );
        f.println( gcWifiPassword );
        f.println( gcSettingStatus );
        f.close();
      }
      ESP.restart();
    }
  }
  if ( bleOn == true ) bleOn = false;

  if ( ( gcSettingStatus[ 0 ] == '9' ) || ( gcSettingStatus[ 0 ] == '5' ) ){
    gbWifiSetting = true;
  } else {
    gbWifiSetting = false;
  }

}

int Wifi_Setup()
{
  int c;
  while( gbWifiSetting == false )
  {
    vTaskDelay( 100 );
  }
  WiFi.disconnect();
  WiFi.mode( WIFI_STA );
  WiFi.begin( &gcWifiSSID[0], &gcWifiPassword[0] );
  c = 0;
  while ( WiFi.status() != WL_CONNECTED ){
    vTaskDelay( 500 );
    c++;
    if ( c > 10 ) break;
  }
  if ( c > 10 ) {
    gbWifiFailure = true;
    vTaskDelay( 300000 );
    return 1;
  }else {
    gbWifiFailure = false;
    gbWifiReady = true;
  }
  return 0;
}

int Wifi_Loop()
{
  long c;
  long lGMTOffset;
  
  lGMTOffset = 3600 * 9;
   configTime( lGMTOffset, 0, "pool.ntp.org" );
//  configTime( lGMTOffset, 0, "jp.pool.ntp.org" );
  vTaskDelay( 3000 );
  gbTimeReady = true;

  time_t t;
  struct tm *tm;

  t = time( NULL );
  tm = localtime( &t );
  Serial.print( tm->tm_hour );
  Serial.print( " " );
  Serial.print( tm->tm_min );
  Serial.print( " " );
  Serial.print( tm->tm_sec );
  Serial.println( " " );

  c = 0;
  while ( c < 1440 )        // 1day
  {
    //vTaskDelay( 60000 );    // 1min
    vTaskDelay( 10 );    // 1min
    c++;
  }
  
  return 0;
}
