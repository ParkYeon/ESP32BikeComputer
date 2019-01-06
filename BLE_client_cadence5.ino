#define USE_SERIAL_LOGGING

#define BLE_INDEX_SPEED    0
#define BLE_INDEX_CADENCE 1
#define BLE_INDEX_COUNT   2

#define SCREEN_WIDTH        240
#define SCREEN_HEIGHT       320
#define POS_HEADER_WIDTH      SCREEN_WIDTH
#define POS_HEADER_HEIGHT     20
#define POS_HEADER_TIME_X     0
#define POS_HEADER_TIME_Y     2
#define POS_CLOCK_RADIS       100
#define POS_CLOCK_X         120
#define POS_CLOCK_Y         130
#define POS_CLOCK_CENTER_RADIS    3
#define POS_CLOCK_SMALL_RADIS   35
#define POS_CLOCK_SMALL_X     POS_CLOCK_X
#define POS_CLOCK_SMALL_Y     185
#define POS_CLOCK_SMALL_DELTA_X   30
#define POS_CLOCK_SMALL_DELTA_Y   30
#define POS_GRAPH_X         18
#define POS_GRAPH_HEIGHT      60
#define POS_GRAPH_Y         SCREEN_HEIGHT - POS_GRAPH_HEIGHT
#define POS_GRAPH_WIDTH       SCREEN_WIDTH - POS_GRAPH_X*2
#define POS_RIDING_TIME_X     POS_GRAPH_X
#define POS_RIDING_TIME_Y     234
#define POS_RIDING_SEPEED_X     POS_HEADER_WIDTH-POS_GRAPH_X
#define POS_RIDING_SEPEED_Y     POS_RIDING_TIME_Y


#include <SPI.h>
#include <TFT_eSPI.h>       // Hardware-specific library
#include "BLEDevice.h"

class MyClientCallback : public BLEClientCallbacks {
  void onConnect( BLEClient* pclient ) {
#ifdef USE_SERIAL_LOGGING
    Serial.println( "onConnect" );
#endif
  }
  void onDisconnect( BLEClient* pclient ) {
#ifdef USE_SERIAL_LOGGING
    Serial.println( "onDisconnect" );
#endif
  }
};

static void       notifyCallback( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify );
static uint8_t      conv2d( const char* p );
int8_t          connectToServer( BLEClient* pBLEClient, const char *pAddress );
void          Scanning( );
void          DrawFirstTime( );
void          DrawUpdateTime( );
void          ClickScreen( );

static MyClientCallback cClientCallback;
TFT_eSPI        tft = TFT_eSPI( );      // Invoke custom library
uint32_t        targetTime = 0;     // for next 1 second timeout
uint8_t         hh = conv2d( __TIME__ );
uint8_t         mm = conv2d( __TIME__ + 3 );
uint8_t         ss = conv2d( __TIME__ + 6 );  // Get H, M, S from compile time
uint32_t        revolurtions = 0;
uint16_t        cadence = 0;
uint16_t        eventtime_s = 0;
uint16_t        eventtime_c = 0;

static BLEUUID      serviceUUID( (uint16_t)0x1816 );     // Cycling Speed and Cadence  org.bluetooth.service.cycling_speed_and_cadence 0x1816  GSS
static BLEUUID      measurementcharUUID( (uint16_t)0x2A5B ); // CSC Measurement org.bluetooth.characteristic.csc_measurement  0x2A5B  GSS
static BLEUUID      featurecharUUID( (uint16_t)0x2A5C );   // CSC Feature  org.bluetooth.characteristic.csc_feature  0x2A5C  GSS

static BLEClient*   pSensorClient[BLE_INDEX_COUNT] = { NULL, };
static char       pSensorAddress[BLE_INDEX_COUNT][18] = { "G0:00:00:00:00:00", "G0:00:00:00:00:00" };
static bool     reConnectTry[BLE_INDEX_COUNT] = { false, };

void setup( void ) {
#ifdef USE_SERIAL_LOGGING
  Serial.begin( 115200 );
  Serial.println( "Starting BLE Cycling Computer application..." );
#endif
  // BLE 초기화
  BLEDevice::init( "BikeComputer" );
  for ( int8_t j = 0; j < BLE_INDEX_COUNT; j++ ) {
    pSensorClient[j] = BLEDevice::createClient( );
    //pSensorClient[j]->setClientCallbacks( &cClientCallback );
  }

  // LCD 초기화. 세워서 사용. 터치스크린 위치 세팅. 전기 절약을 위해 검정색 배경. 기본글자색 세팅
  tft.init( );
  tft.setRotation( 0 );
  uint16_t calData[5] = { 308, 3487, 415, 3439, 6 };
  tft.setTouch( calData );
  tft.fillScreen( TFT_BLACK );
  tft.setTextColor( TFT_WHITE, TFT_BLACK );  // Adding a background colour erases previous text automatically  
  Scanning();
  DrawFirstTime( );  
  targetTime = millis( ) + 100;
}

void loop( ) {
  ClickScreen( );  
  DrawUpdateTime( );
  for ( int8_t j = 0; j < BLE_INDEX_COUNT; j++ ) {
    if ( pSensorClient[j]->isConnected( ) == false && reConnectTry[j] == true ) {
      Scanning( );
    }
  }
  delay( 10 );
}

void DrawFirstTime( ) {
  tft.fillScreen( TFT_BLACK );  
  
#ifdef USE_SERIAL_LOGGING
  Serial.printf( "50: %d ", tft.textWidth( "50", 2 ) );
  Serial.printf( "25: %d ", tft.textWidth( "25", 2 ) );
  Serial.printf( "0: %d ", tft.textWidth( "0", 2 ) );
  Serial.printf( "99: %d\n", tft.textWidth( "99", 2 ) );
  //50: 16 25: 16 0: 8 100: 24
#endif
  // 속도계 그리는 영역
  tft.drawCircle( POS_CLOCK_X, POS_CLOCK_Y, POS_CLOCK_RADIS, TFT_WHITE );
  tft.fillCircle( POS_CLOCK_X, POS_CLOCK_Y, POS_CLOCK_CENTER_RADIS, TFT_WHITE );
  tft.drawCircle( POS_CLOCK_SMALL_X, POS_CLOCK_SMALL_Y, POS_CLOCK_SMALL_RADIS, TFT_WHITE );
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.drawCentreString( "*", POS_CLOCK_SMALL_X, POS_CLOCK_SMALL_Y - POS_CLOCK_SMALL_DELTA_Y, 1 );
  tft.drawCentreString( "3", POS_CLOCK_SMALL_X + POS_CLOCK_SMALL_DELTA_X, POS_CLOCK_SMALL_Y, 1 );
  tft.drawCentreString( "6", POS_CLOCK_SMALL_X, POS_CLOCK_SMALL_Y + POS_CLOCK_SMALL_DELTA_Y, 1 );
  tft.drawCentreString( "9", POS_CLOCK_SMALL_X - POS_CLOCK_SMALL_DELTA_X, POS_CLOCK_SMALL_Y, 1 );
  
  // 전체 거리와 시간 그리는 영역
  tft.drawString( "00:24:54", POS_RIDING_TIME_X, POS_RIDING_TIME_Y, 4 );
  tft.drawRightString( "120.50", POS_RIDING_SEPEED_X, POS_RIDING_SEPEED_Y, 4 );
  tft.drawString( "km", POS_RIDING_SEPEED_X, POS_RIDING_SEPEED_Y, 1 );
  
  // 하단 그래프 그리는 영역
  tft.drawRect( POS_GRAPH_X, POS_GRAPH_Y, POS_GRAPH_WIDTH, POS_GRAPH_HEIGHT, TFT_DARKGREY );
  tft.setTextColor( TFT_RED, TFT_BLACK );
  tft.drawString( "40", 0, POS_GRAPH_Y, 2 );
  tft.drawString( "20", 0, POS_GRAPH_Y + 22, 2 );
  tft.drawString( "0", 0, SCREEN_HEIGHT - 16, 2 );
  tft.setTextColor( TFT_CYAN, TFT_BLACK );
  tft.drawString( "90", SCREEN_WIDTH - 17, POS_GRAPH_Y, 2 );
  tft.drawString( "45", SCREEN_WIDTH - 17, POS_GRAPH_Y + 22, 2 );
  tft.drawString( "0", SCREEN_WIDTH - 9, SCREEN_HEIGHT - 16, 2 );    

  // 임시로 해보자
  tft.setTextColor( TFT_WHITE, TFT_BLACK );   
  tft.drawString( "SPEED:", POS_GRAPH_X+3, POS_GRAPH_Y+3, 2 );
  tft.drawString( "Time:", POS_GRAPH_X+110, POS_GRAPH_Y+3, 2 );
  
  tft.drawString( "CADENCE:", POS_GRAPH_X+3, POS_GRAPH_Y+33, 2 );
  tft.drawString( "Time:", POS_GRAPH_X+110, POS_GRAPH_Y+33, 2 );  
}
void DrawUpdateTime( ) { 
  // 헤더 그리는 영역
  tft.fillRect( 0, 0, POS_HEADER_WIDTH, POS_HEADER_HEIGHT, TFT_LIGHTGREY );
  tft.setTextColor( TFT_BLACK, TFT_LIGHTGREY );
  tft.drawString( "12:30", POS_HEADER_TIME_X, POS_HEADER_TIME_Y, 2 );
  
  for ( int8_t j = 0; j < BLE_INDEX_COUNT; j++ ) {
    if ( pSensorClient[j]->isConnected( ) )
      tft.setTextColor( TFT_BLUE, TFT_LIGHTGREY );
    else
      tft.setTextColor( TFT_BLACK, TFT_LIGHTGREY );
    tft.drawNumber( j, POS_HEADER_WIDTH - 10 - j * 15, POS_HEADER_TIME_Y, 2 );
  }
  
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.drawNumber( millis( ), 60, 50, 4 );  

  // 임시로 해보자
  tft.setTextColor( TFT_WHITE, TFT_BLACK );     
  tft.drawNumber( revolurtions, POS_GRAPH_X+70, POS_GRAPH_Y+3, 2 );  
  tft.drawNumber( eventtime_s, POS_GRAPH_X+150, POS_GRAPH_Y+3, 2 );
   
  tft.drawNumber( cadence, POS_GRAPH_X+70, POS_GRAPH_Y+33, 2 );
  tft.drawNumber( eventtime_c, POS_GRAPH_X+150, POS_GRAPH_Y+33, 2 );
}
void ClickScreen( ) {  
  uint16_t x = 0, y = 0;                        // To store the touch coordinates  
  boolean pressed = tft.getTouch( &x, &y );       // Pressed will be set true is there is a valid touch on the screen 
                          // Draw a white spot at the detected coordinates
  if ( pressed ) {
    // 클릭은 0.1초 단위로입력 받자.
    if ( targetTime > millis( ) ) {
#ifdef USE_SERIAL_LOGGING    
      Serial.print( targetTime );
      Serial.print( ", " );
      Serial.println( millis( ) );    
#endif
      return;
    }
    targetTime = millis( ) + 100;

#ifdef USE_SERIAL_LOGGING
    tft.fillCircle( x, y, 2, TFT_WHITE );
    Serial.print( "x,y = " );
    Serial.print( x );
    Serial.print( "," );
    Serial.println( y );
#endif
  }
}

static uint8_t conv2d( const char* p ) {
  uint8_t v = 0;
  if ( '0' <= *p && *p <= '9' )
    v = *p - '0';
  return 10 * v + *++p - '0';
}

void Scanning( ) {
  BLEScan* pBLEScan = BLEDevice::getScan( );
  pBLEScan->setInterval( 1349 );
  pBLEScan->setWindow( 449 );
  pBLEScan->setActiveScan( true );
  BLEScanResults result = pBLEScan->start( 2, false );
  pBLEScan->stop();
  delay(100);

  for ( int16_t i = 0; i<result.getCount( ); i++ ) {
    //getAddress();
    //getName();    
#ifdef USE_SERIAL_LOGGING    
    Serial.println( result.getDevice( i ).toString( ).c_str( ) );
#endif
    if( result.getDevice( i ).getName( ).length() <=0 ) 
      continue;
    for ( int8_t j = 0; j < BLE_INDEX_COUNT; j++ ) {
      if ( pSensorClient[j]->isConnected( ) == false ) {
        int8_t ret = connectToServer( pSensorClient[j], result.getDevice( i ).getAddress( ).toString( ).c_str( ) );
        delay(500);
        if ( ret >= 0 ) {
          strcpy( pSensorAddress[j], result.getDevice( i ).getAddress( ).toString( ).c_str( ) );
          reConnectTry[j] = true;
          // 화면 출력도해보자.
          //tft.setTextColor( TFT_WHITE, TFT_BLACK );
          //tft.drawString( result.getDevice( i ).toString( ).c_str( ), 0, 30 + j * 20, 4 );
        }
        break;
      }
    }
  }  
}

int8_t connectToServer( BLEClient* pBLEClient, const char *pAddress ) {
  Serial.println( "connectToServer" );
  Serial.println( pAddress );
  pBLEClient->connect( BLEAddress( pAddress ) );

#ifdef USE_SERIAL_LOGGING
  Serial.println( " - Sensor Connect" );
#endif
  
  BLERemoteService* pRemoteService = pBLEClient->getService( serviceUUID );
  if ( pRemoteService == nullptr ) {
#ifdef USE_SERIAL_LOGGING      
    Serial.print( "Failed to find our service UUID: " );
    Serial.println( serviceUUID.toString( ).c_str( ) );
#endif      
    pBLEClient->disconnect( );
    return -1;
  }

#ifdef USE_SERIAL_LOGGING    
  Serial.println( " - Found our service" );
#endif 

  // Obtain a reference to the characteristic in the service of the remote BLE server.
  BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic( featurecharUUID );
  if ( pRemoteCharacteristic == nullptr ) {
#ifdef USE_SERIAL_LOGGING      
    Serial.print( "Failed to find our characteristic UUID: " );
    Serial.println( featurecharUUID.toString( ).c_str( ) );
#endif      
    pBLEClient->disconnect( );
    return -1;
  }

#ifdef USE_SERIAL_LOGGING    
  Serial.println( " - Found our feature characteristic" );
#endif    

  // Read the value of the characteristic.
  if ( pRemoteCharacteristic->canRead( ) ) {
    Serial.println( " - Found our feature characteristic2" );
    int16_t value = pRemoteCharacteristic->readUInt16( );    
#ifdef USE_SERIAL_LOGGING      
    Serial.print( "The feature characteristic value was: " );
    Serial.printf( "%d", value );
#endif
    if ( value & 3 ) { // Wheel Revolution Data Supported = 1, Crank Revolution Data Supported = 2
      pRemoteCharacteristic = pRemoteService->getCharacteristic( measurementcharUUID );
      if ( pRemoteCharacteristic == nullptr ) {
#ifdef USE_SERIAL_LOGGING      
        Serial.print( "Failed to find our characteristic UUID: " );
        Serial.println( measurementcharUUID.toString( ).c_str( ) );
#endif      
        pBLEClient->disconnect( );
        return -1;
      }
#ifdef USE_SERIAL_LOGGING    
      Serial.println( " - Found our measurement characteristic" );
#endif          

      if ( pRemoteCharacteristic->canNotify( ) )
        pRemoteCharacteristic->registerForNotify( notifyCallback );

      if ( value & 3 == 1 )
        return BLE_INDEX_SPEED;
      else
        return BLE_INDEX_CADENCE;
    }
  }
  return -1;
}

static void notifyCallback( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify ) {
#ifdef USE_SERIAL_LOGGING
  Serial.print( "Notify callback for characteristic " );
  Serial.print( pBLERemoteCharacteristic->getUUID( ).toString( ).c_str( ) );
  Serial.print( " of data length " );
  Serial.println( length );
  Serial.print( "data: " );
  for ( int8_t i = 0; i<length; i++ )
    Serial.printf( "%02x ", (uint8_t)pData[i] );
  Serial.println( " " );
#endif
  if ( pData[0] == 1 ) {
    // 속도 데이터
    //revolurtions = (int8_t)pData[1] + (int8_t)pData[2] * 256 + (int8_t)pData[3] * 65536 + (int8_t)pData[4] * 16777216;
    //eventtime_s = (int8_t)pData[5] + (int8_t)pData[6] * 256;
    revolurtions = *(int32_t*)(pData+1);
    eventtime_s = *(int16_t*)(pData+5);
    // 여기서 부터 처리를 해야한다.
  }
  else if ( pData[0] == 2 ) {
    // 케이던스 데이터
    //cadence = pData[1] + pData[2] * 256;
    //eventtime_c = pData[3] + pData[4] * 256;
    cadence = *(int16_t*)(pData+1);
    eventtime_c = *(int16_t*)(pData+3);
  }
  else {
    // 그 외는 일단 처리하지 않는다.
  }
}
