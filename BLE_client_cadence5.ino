#define USE_SERIAL_LOGGING
#define BLE_SENSOR_COUNT   2
#define DISPLAY_REFRESH_DELAY  100
#define BLE_RECONNECT_DELAY   3000
#define DATA_GRAPH_COUNT    200
#define WHEEL_SIZE    2105      // 700 25c 는 한바퀴에 2105mm 정도 가더라
#define RIDING_STATE_SETUP	0
#define RIDING_STATE_START	1
#define RIDING_STATE_STOP	2
#define UINT32_MAX				0xffffffff
#define ALTITUDE_SAMPLE_SEC	5
#define STOP_WAIT_TIME	10000


#include <SPI.h>
#include <TFT_eSPI.h>       // Hardware-specific library
#include "BLEDevice.h"
#include "Seeed_BMP280.h"
#include <Wire.h>

TFT_eSPI tft = TFT_eSPI();      // Invoke custom library
BMP280 bmp280;

#ifdef USE_SERIAL_LOGGING
class MyClientCallback : public BLEClientCallbacks {
	void onConnect(BLEClient* pclient)
	{
		Serial.println("onConnect");
	}
	void onDisconnect(BLEClient* pclient)
	{
		Serial.println("onDisconnect");
	}
};
static MyClientCallback cClientCallback;
#endif // USE_SERIAL_LOGGING


class RidingData {
public:
	RidingData(){ }
	~RidingData(){ }

	char totalTimeDisplay[9] = "00:00:00";
	char totalDistanceDisplay[7] = "  5.56";
	char temporatureDisplay[5] = " 0.0";
	char totalAltitudeDisplay[7] = "   0.0";

	// 필요한 것: 전체라이딩 시간, 전체 거리, 전체 케이던스 - 전체 평균속도, 전체 평균케이던스
	//        현재 속도, 현재 케이던스, 200초동안의 기록


	// total data : 여행시간, 라이딩시간, 라이딩거리 -> 전체평균거리

	// cadence data

	// revolution data

	// 센서에서 측정된 값
	uint16_t currentCadence=0;
	uint32_t currentRevolution=0;
	uint16_t initialCadence=0;
	uint32_t initialRevolution=0;
	float altitude=0.0;
	float pressure=0.0;
	float temporture=0.0;

	// 그래프 저장
	uint8_t graphIndex = DATA_GRAPH_COUNT-1;
	bool  graphUpdated = false;

	// 기준 시간들
	uint32_t startTime = 0;     // millis 기반의 속도계를 켠 시간
	uint32_t updateTime = 0;

	uint32_t totalTime = 0;     // 정지시간을 제외한 millis기반의 라이딩 시간
	uint32_t totalDistance = 0;
	uint32_t totalCadence = 0;
	float totalAltitude = 0;
	uint32_t stopTime = UINT32_MAX;
	uint32_t timeReduceRPM = UINT32_MAX;
	uint32_t timeReduceSpeed = UINT32_MAX;
	uint32_t lastCadenceCheckTime = 0;
	uint32_t lastRevolutionCheckTime = 0;

	uint8_t ridingState = RIDING_STATE_SETUP;

	float averageRPM = 0;
	float averageSpeed = 0;		

	float currentRPM = 0;
	float currentSpeed = 0;		

	float currentAltitude = 0;
	float previousAltitude = 0;

	uint32_t currentRevolutions = 0;
	uint32_t previousRevolutions = 0;

	uint16_t currentCadence = 0;
	uint16_t previousCadence = 0;
	
	void setup(){		
		updateTime = millis();

		// 초기값을 세팅한다. -> BLE에서 세팅해야 하지만 지금 연결이 안되니까 임시로여기 둔다.
		if( !initialCadence && !initialRevolution && currentCadence && currentRevolution) {
			initialCadence=currentCadence;
			initialRevolution=currentRevolution;
			startTime = updateTime;
		} 
		//
	}
	void calculateData( ) {   
		uint32_t preTime = updateTime;
		updateTime = millis();			
				
		float altitudeDelta = 0;		
		uint8_t idx = ( (updateTime - startTime) / 1000 ) % DATA_GRAPH_COUNT;
		if ( idx != graphIndex ) {	// 1초에 한번씩 처리할 것
			graphIndex = idx;     
			graphUpdated = true;

			// 온습도는 1초에 한번씩만 측정한다. 많이 측정했더니 멈추는듯?
			temporture = bmp280.getTemperature();
			pressure = bmp280.getPressure();
			altitude = bmp280.calcAltitude(pressure);
			Serial.printf("Temp: %fC, Pressure: %fPa, Altitude: %fm",temporture ,pressure ,altitude);

			uint8_t temp = (idx-1+DATA_GRAPH_COUNT) % ALTITUDE_SAMPLE_SEC;
			currectAltitude = (float)( currectAltitude * temp + altitude ) / (float)(temp+1);
			
			// 고도 계산 - 현재 고도는 계속 측정한다. 오차가 있기 때문에 5초간의 평균을 계산한다. 획득고도는 달릴때만 누적한다.
			if ( idx % ALTITUDE_SAMPLE_SEC == 0 ) {
				if( previousAltitude > 0 && (currectAltitude - previousAltitude) > 0 )
					altitudeDelta = currectAltitude - previousAltitude;
				previousAltitude = currectAltitude;
			}				
		}
		
		if ( ridingState == RIDING_STATE_START ) {			
			totalTime += updateTime - preTime;		// 전체시간 누적
			totalAltitude += altitudeDelta;			// 획득고도 누적
			
			averageSpeed = ( (float)totalDistance / (float)totalTime ) * 3.6;	// 평균속도 계산
			averageRPM = (float)totalCadence * 60000.0 / (float)totalTime;		// 평균 RPM 계산			
						
			// 속도 감쇄 계산
			if ( updateTime > timeReduceRPM ) 
				currentRPM = 60000.0 / (float)( updateTime - lastCadenceCheckTime );
			if ( updateTime > timeReduceSpeed ) 
				currentSpeed = (float)(WHEEL_SIZE) / (float)( updateTime - lastRevolutionCheckTime )  * 3.6;			

			// 정지 상태로 바꾼다.
			if ( updateTime > stopTime ) {
				ridingState = RIDING_STATE_STOP;	
				currentSpeed = 0;
				currentRPM = 0;
			}
				
		}
		else {
			if( startTime == 0 && initialRevolution != 0 && initialCadence != 0)
				startTime = updateTime;

			if( currentSpeed > 0 && currentRPM > 0 )
				ridingState = RIDING_STATE_START;
		}

		// 센서가 고장난것 같으니 여기서 가짜 데이터를 만들자. -> BLE에서 세팅해야 하지만 지금 연결이 안되니까 임시로여기 둔다.
		averageRPM = 10 + updateTime%70;
		averageSpeed = 10 + updateTime%30;

		// 거리계산 - 나중에는 revolution 데이터를 사용할 것이다.    -> BLE에서 세팅해야 하지만 지금 연결이 안되니까 임시로여기 둔다.
		uint32_t deltaTime = updateTime - preTime; // 밀리세크 단위
		totalTime += deltaTime;
		totalDistance += (float)currentSpeed * 1000.0f  * (float)totalTime / (1000.0f*60.0f*60.0f) ;
		totalCadence += currentRPM * 100.0f * (float)totalTime / (1000.0f*60.0f*60.0f);

		//


		// Display 정보 업데이트 - 달리든 안달리든 처리한다.
		uint32_t ss = totalTime/1000.0f, mm = ss / 60, hh = mm / 60;
		ss %= 60;
		mm %= 60;
		sprintf( totalTimeDisplay, "%02d:%02d:%02d",hh,mm,ss); 
		sprintf( temporatureDisplay, "%2.01f",temporture);
		sprintf( totalAltitudeDisplay, "%4.01f",totalAltitude);
		sprintf( totalDistanceDisplay, "%3.02f",totalDistance/1000000.0f);
	}
	void setCadence( uint16_t c, uint16_t time ) {
		// 시간과 함께 받는데 그게 무슨 의미가 있는가 싶다. 시간 버리고 millis를 사용하자.
		currentCadence = c;
		uint32_t now = millis();

		// 초기값을 세팅한다.
		if( initialCadence == 0 && currentCadence != 0 ) {
			initialCadence = currentCadence;			
			lastCadenceCheckTime = now;
			previousCadence = currentCadence;
			return;
		}
		
		totalCadence = currentCadence - initialCadence;
		float delta = currentCadence - previousCadence;
		float deltaTime = now-lastCadenceCheckTime;
		if ( delta > 0 && deltaTime > 0 ) {
			currentRPM = delta * 60000.0 / deltaTime;
			timeReduceRPM = deltaTime / delta + now;
			lastCadenceCheckTime = now;
			previousCadence = currentCadence;
		}		
	}
	void setRevolutions( uint32_t r, uint16_t time ) {
		currentRevolution = r;    
		uint32_t now = millis();

		// 초기값을 세팅한다.
		if( initialRevolution == 0 && currentRevolution != 0 ) {
			initialRevolution = currentRevolution;			
			lastRevolutionCheckTime = now;
			previousRevolutions = currentRevolution;			
			return;
		}

		totalDistance = ( currentRevolution - initialRevolution) * WHEEL_SIZE;
		float delta = currentRevolution - previousRevolutions;
		float deltaTime = now-lastRevolutionCheckTime;
		if ( delta > 0 && deltaTime > 0 ) {
			currentSpeed = delta * WHEEL_SIZE / deltaTime * 3.6;
			timeReduceSpeed = deltaTime / delta + now;
			lastRevolutionCheckTime = now;
			previousRevolutions = currentRevolution;
			stopTime = now + STOP_WAIT_TIME;
			
		}
	}
};
static RidingData gRideInfo;

class BluetoothManager {
public:
	BluetoothManager() : serviceUUID((uint16_t)0x1816 ), measurementcharUUID((uint16_t)0x2A5B ),featurecharUUID((uint16_t)0x2A5C ){}
	~BluetoothManager(){}

	const char* cadenceHardwareAddress = "D0:6E:67:E1:48:93"; // 49656-241 - cadence
	const char* revolutionHardwareAddress = "CC:FC:66:9E:70:8B";// 35483-193 - revolutions
	BLEUUID serviceUUID;     // Cycling Speed and Cadence  org.bluetooth.service.cycling_speed_and_cadence 0x1816  GSS
	BLEUUID measurementcharUUID; // CSC Measurement org.bluetooth.characteristic.csc_measurement  0x2A5B  GSS
	BLEUUID featurecharUUID;   // CSC Feature  org.bluetooth.characteristic.csc_feature  0x2A5C  GSS

	BLEClient* pSensorClient[BLE_SENSOR_COUNT] = { NULL, };
	bool isSpeedSensorConnected = false;
	bool isCadenceSensorConnected = false;
	int32_t reconnectTime = 0;

	static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
	{
		Serial.print("Notify callback for characteristic ");
		Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
		Serial.print(" of data length ");
		Serial.println(length);
		Serial.print("data: ");

		for (int8_t i = 0; i < length; i++)
			Serial.printf("%02x", (uint8_t)pData[i]);
		Serial.println(" ");

		// pBLERemoteCharacteristic 를 기준으로 속도, 케이던스만 처리하는게 정석
		if (pData[0] == 1 && isNotify == true)    
			gRideInfo.setRevolutions(  *(int32_t*)(pData + 1), *(int16_t*)(pData + 5) );  // 속도 데이터
		else if (pData[0] == 2 && isNotify == true)
			gRideInfo.setCadence(  *(int16_t*)(pData + 1), *(int16_t*)(pData + 3) );    // 케이던스 데이터
		else { }  // 그 외는 일단 처리하지 않는다.    
	}

	void setup( ) {
		BLEDevice::init("BikeComputer");
		for (int8_t j = 0; j < BLE_SENSOR_COUNT; j++)
		{
			pSensorClient[j] = BLEDevice::createClient();
#ifdef USE_SERIAL_LOGGING
			pSensorClient[j]->setClientCallbacks( &cClientCallback );
#endif // USE_SERIAL_LOGGING
		}   
	}
	void reconnectBLE( ) {
		if( reconnectTime > millis() ) return;

		reconnectTime = millis() + BLE_RECONNECT_DELAY;

		// 심플하게 하드웨어 주소를 사용하는 방식으로 한다.    
		if ( !isCadenceSensorConnected ) 
			connectSensor(pSensorClient[1], cadenceHardwareAddress);  

		if ( !isSpeedSensorConnected )
			connectSensor(pSensorClient[0], revolutionHardwareAddress);   
	}
	void connectSensor(BLEClient* pBLEClient, const char* pAddress)
	{ 
		Serial.println(pAddress);
		pBLEClient->connect(BLEAddress(pAddress));
		Serial.println(" - Sensor Connect");

		BLERemoteService* pRemoteService = pBLEClient->getService(serviceUUID);
		if (pRemoteService == nullptr)
		{
			Serial.print("Failed to find our service UUID: ");
			Serial.println(serviceUUID.toString().c_str());
			pBLEClient->disconnect();
			return;
		}
		Serial.println(" - Found our service");

		// Obtain a reference to the characteristic in the service of the remote BLE server.
		BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(featurecharUUID);
		if (pRemoteCharacteristic == nullptr)
		{
			Serial.print("Failed to find our characteristic UUID: ");
			Serial.println(featurecharUUID.toString().c_str());
			pBLEClient->disconnect();
			return;
		}
		Serial.println(" - Found our feature characteristic");

		// Read the value of the characteristic.
		if (pRemoteCharacteristic->canRead())
		{
			Serial.println(" - Found our feature characteristic2");
			int16_t value = pRemoteCharacteristic->readUInt16();
			Serial.print("The feature characteristic value was: ");
			Serial.printf("%d\r\n", value);
			if (value & 3)
			{ // Wheel Revolution Data Supported = 1, Crank Revolution Data Supported = 2
				pRemoteCharacteristic = pRemoteService->getCharacteristic(measurementcharUUID);
				if (pRemoteCharacteristic == nullptr)
				{
					Serial.print("Failed to find our characteristic UUID: ");
					Serial.println(measurementcharUUID.toString().c_str());
					pBLEClient->disconnect();
					return;
				}
				Serial.println(" - Found our measurement characteristic");

				if (pRemoteCharacteristic->canNotify())
					pRemoteCharacteristic->registerForNotify(BluetoothManager::notifyCallback);
				if (value & 3 == 1)
					isSpeedSensorConnected = true;
				else
					isCadenceSensorConnected = true;
			}
		} 
	}
};
static BluetoothManager gBLE;

class TftDisplay {
	enum {
		SCREEN_WIDTH  =        240,
		SCREEN_HEIGHT =       320,
		/*
		POS_HEADER_WIDTH  =      SCREEN_WIDTH,
		POS_HEADER_HEIGHT =     20,
		POS_HEADER_TIME_X =     0,
		POS_HEADER_TIME_Y   =    2,
		*/
		POS_CLOCK_RADIS =       100,
		POS_CLOCK_X     =      120,
		POS_CLOCK_Y     =      110,
		POS_CLOCK_CENTER_RADIS  =   3,
		POS_CLOCK_HANDS = 90,
		POS_CLOCK_SMALL_RADIS   =  35,
		POS_CLOCK_SMALL_X     =  POS_CLOCK_X,
		POS_CLOCK_SMALL_Y   =   POS_CLOCK_Y + 55,
		POS_CLOCK_SMALL_DELTA_X   = 30,
		POS_CLOCK_SMALL_DELTA_Y   = 30,
		POS_CLOCK_SMALL_HANDS = 20,
		POS_GRAPH_X       =   18,
		POS_GRAPH_HEIGHT      = 60,
		POS_GRAPH_Y       =    SCREEN_HEIGHT - POS_GRAPH_HEIGHT,
		POS_GRAPH_WIDTH   =     SCREEN_WIDTH - POS_GRAPH_X*2,
		POS_RIDING_TIME_X     =  POS_GRAPH_X,
		POS_RIDING_TIME_Y     = 234,
		POS_RIDING_SEPEED_X     =  SCREEN_WIDTH-POS_GRAPH_X,
		POS_RIDING_SEPEED_Y     =  POS_RIDING_TIME_Y,
		POS_RIDING_TEMP_X     =  POS_RIDING_TIME_X,
		POS_RIDING_TEMP_Y     = POS_RIDING_TIME_Y - 25,
		POS_RIDING_ALTITUDE_X     =  SCREEN_WIDTH-POS_GRAPH_X,
		POS_RIDING_ALTITUDE_Y     =  POS_RIDING_TIME_Y - 25
	};
public:
	int32_t refreshTime = 0;
	uint32_t preSpeed = 0;
	uint32_t preCadence = 0;

	void setup( ) {
		// LCD 초기화. 세워서 사용. 터치스크린 위치 세팅. 전기 절약을 위해 검정색 배경. 기본글자색 세팅
		tft.init();
		tft.setRotation(0);
		uint16_t calData[5] = { 308, 3487, 415, 3439, 6 };
		tft.setTouch(calData);
		tft.fillScreen(TFT_BLACK);
		tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Adding a background colour erases previous text automatically  

		drawFirstTime();    

		refreshTime = millis() + DISPLAY_REFRESH_DELAY;
	}
	void drawFirstTime()
	{
		tft.fillScreen(TFT_BLACK);

		Serial.printf("50: %d ", tft.textWidth("50", 2));
		Serial.printf("25: %d ", tft.textWidth("25", 2));
		Serial.printf("0: %d ", tft.textWidth("0", 2));
		Serial.printf("99: %d\n", tft.textWidth("99", 2));
		//50: 16 25: 16 0: 8 100: 24

		// 속도계 그리는 영역
		tft.drawCircle(POS_CLOCK_X, POS_CLOCK_Y, POS_CLOCK_RADIS, TFT_WHITE);   
		tft.drawCircle(POS_CLOCK_SMALL_X, POS_CLOCK_SMALL_Y, POS_CLOCK_SMALL_RADIS, TFT_WHITE);

		for(int i = 0; i<360; i+= 6) {
			if( i > 120 && i < 240 ) continue;
			float sx = cos((i-90)*0.0174532925);
			float sy = sin((i-90)*0.0174532925);
			uint8_t x0 = sx*(POS_CLOCK_RADIS-5)+POS_CLOCK_X;
			uint8_t y0 = sy*(POS_CLOCK_RADIS-5)+POS_CLOCK_Y;
			// Draw minute markers
			tft.drawPixel(x0, y0, TFT_WHITE);

			// Draw main quadrant dots
			if( i % 30 ==0 ) tft.fillCircle(x0, y0, 2, TFT_WHITE);      
		}

		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		tft.drawCentreString("*", POS_CLOCK_SMALL_X, POS_CLOCK_SMALL_Y - POS_CLOCK_SMALL_DELTA_Y, 1);
		tft.drawCentreString("3", POS_CLOCK_SMALL_X + POS_CLOCK_SMALL_DELTA_X, POS_CLOCK_SMALL_Y, 1);
		tft.drawCentreString("6", POS_CLOCK_SMALL_X, POS_CLOCK_SMALL_Y + POS_CLOCK_SMALL_DELTA_Y, 1);
		tft.drawCentreString("9", POS_CLOCK_SMALL_X - POS_CLOCK_SMALL_DELTA_X, POS_CLOCK_SMALL_Y, 1);

		// 하단 그래프 그리는 영역
		tft.drawRect(POS_GRAPH_X, POS_GRAPH_Y, POS_GRAPH_WIDTH-1, POS_GRAPH_HEIGHT, TFT_DARKGREY);
		tft.setTextColor(TFT_RED, TFT_BLACK);
		tft.drawString("40", 0, POS_GRAPH_Y, 2);
		tft.drawString("20", 0, POS_GRAPH_Y + 22, 2);
		tft.drawString("0", 0, SCREEN_HEIGHT - 16, 2);
		tft.setTextColor(TFT_CYAN, TFT_BLACK);
		tft.drawString("90", SCREEN_WIDTH - 17, POS_GRAPH_Y, 2);
		tft.drawString("45", SCREEN_WIDTH - 17, POS_GRAPH_Y + 22, 2);
		tft.drawString("0", SCREEN_WIDTH - 9, SCREEN_HEIGHT - 16, 2);

		/*
		// 임시로 해보자
		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		tft.drawString("SPEED:", POS_GRAPH_X + 3, POS_GRAPH_Y + 3, 2);
		tft.drawString("Time:", POS_GRAPH_X + 110, POS_GRAPH_Y + 3, 2);

		tft.drawString("CADENCE:", POS_GRAPH_X + 3, POS_GRAPH_Y + 33, 2);
		tft.drawString("Time:", POS_GRAPH_X + 110, POS_GRAPH_Y + 33, 2);    
		*/
	}

	void drawUpdateTime() {
		if( refreshTime > millis() ) return;

		refreshTime = millis() + DISPLAY_REFRESH_DELAY;
		/*
		// 헤더 그리는 영역
		tft.fillRect(0, 0, POS_HEADER_WIDTH, POS_HEADER_HEIGHT, TFT_LIGHTGREY);
		tft.setTextColor(TFT_BLACK, TFT_LIGHTGREY);
		tft.drawString("12:30", POS_HEADER_TIME_X, POS_HEADER_TIME_Y, 2);


		for (int8_t j = 0; j < BLE_SENSOR_COUNT; j++)
		{
		if (gBLE.pSensorClient[j]->isConnected())
		tft.setTextColor(TFT_BLUE, TFT_LIGHTGREY);
		else
		tft.setTextColor(TFT_BLACK, TFT_LIGHTGREY);
		tft.drawNumber(j, POS_HEADER_WIDTH - 10 - j * 15, POS_HEADER_TIME_Y, 2);
		}
		*/

		// 바늘을 그린다.   
		uint16_t i = (preSpeed*6) + 240;
		i%=360;
		float sx = cos((i-90)*0.0174532925);
		float sy = sin((i-90)*0.0174532925);
		uint8_t x0 = sx * POS_CLOCK_HANDS + POS_CLOCK_X;
		uint8_t y0 = sy * POS_CLOCK_HANDS + POS_CLOCK_Y;    
		tft.drawLine(POS_CLOCK_X, POS_CLOCK_Y, x0, y0, TFT_BLACK);

		i = (gRideInfo.currentSpeed*6) + 240;
		i%=360;
		sx = cos((i-90)*0.0174532925);
		sy = sin((i-90)*0.0174532925);
		x0 = sx * POS_CLOCK_HANDS + POS_CLOCK_X;
		y0 = sy * POS_CLOCK_HANDS + POS_CLOCK_Y;    
		tft.drawLine(POS_CLOCK_X, POS_CLOCK_Y, x0, y0, TFT_WHITE);

		preSpeed = gRideInfo.currentSpeed;

		tft.fillCircle(POS_CLOCK_X, POS_CLOCK_Y, POS_CLOCK_CENTER_RADIS, TFT_WHITE);

		i = (preCadence*3);
		i%=360;
		sx = cos((i-90)*0.0174532925);
		sy = sin((i-90)*0.0174532925);
		x0 = sx * POS_CLOCK_SMALL_HANDS + POS_CLOCK_SMALL_X;
		y0 = sy * POS_CLOCK_SMALL_HANDS + POS_CLOCK_SMALL_Y;    
		tft.drawLine(POS_CLOCK_SMALL_X, POS_CLOCK_SMALL_Y, x0, y0, TFT_BLACK);    

		i = (gRideInfo.currentRPM*3);
		i%=360;
		sx = cos((i-90)*0.0174532925);
		sy = sin((i-90)*0.0174532925);
		x0 = sx * POS_CLOCK_SMALL_HANDS + POS_CLOCK_SMALL_X;
		y0 = sy * POS_CLOCK_SMALL_HANDS + POS_CLOCK_SMALL_Y;    
		tft.drawLine(POS_CLOCK_SMALL_X, POS_CLOCK_SMALL_Y, x0, y0, TFT_WHITE);
		preCadence = gRideInfo.currentRPM;

		// 온도와 획득고도를 그리는 영역
		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		tft.drawString(gRideInfo.temporatureDisplay, POS_RIDING_TEMP_X, POS_RIDING_TEMP_Y, 4);
		tft.drawString("c", POS_RIDING_TEMP_X + 50, POS_RIDING_TEMP_Y, 1);
		tft.drawRightString(gRideInfo.totalAltitudeDisplay, POS_RIDING_ALTITUDE_X, POS_RIDING_ALTITUDE_Y, 4);
		tft.drawString("m", POS_RIDING_ALTITUDE_X+1, POS_RIDING_ALTITUDE_Y, 1);


		// 전체 거리와 시간 그리는 영역
		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		tft.drawString(gRideInfo.totalTimeDisplay, POS_RIDING_TIME_X, POS_RIDING_TIME_Y, 4);
		tft.drawRightString(gRideInfo.totalDistanceDisplay, POS_RIDING_SEPEED_X, POS_RIDING_SEPEED_Y, 4);
		tft.drawString("km", POS_RIDING_SEPEED_X+1, POS_RIDING_SEPEED_Y, 1);


		tft.drawNumber(gRideInfo.currentSpeed, 10, 30, 2);
		tft.drawNumber(gRideInfo.currentRPM, 240 - 40, 30, 2);
		/*
		tft.setTextColor(TFT_WHITE, TFT_BLACK);
		tft.drawNumber(gRideInfo.currentSpeed, POS_GRAPH_X + 70, POS_GRAPH_Y + 3, 2);
		tft.drawNumber(gRideInfo.totalDistance, POS_GRAPH_X + 150, POS_GRAPH_Y + 3, 2);

		tft.drawNumber(gRideInfo.currentRPM, POS_GRAPH_X + 70, POS_GRAPH_Y + 33, 2);
		tft.drawNumber(gRideInfo.totalCadence, POS_GRAPH_X + 150, POS_GRAPH_Y + 33, 2);   
		*/

		// 점찍자  1. 곧 그릴 라인을 지운다. 2. 현재라인에 케이던스 점 찍는다. 3. 현재 라인에 속도 점 찍는다.
		if ( gRideInfo.graphUpdated == true ) {
			gRideInfo.graphUpdated = false;
			tft.drawLine( 1 + POS_GRAPH_X + gRideInfo.graphIndex, POS_GRAPH_Y + 1, 1 + POS_GRAPH_X + gRideInfo.graphIndex, SCREEN_HEIGHT-2, TFT_BLACK );      
			tft.drawLine( 1 + POS_GRAPH_X + (gRideInfo.graphIndex+1)%DATA_GRAPH_COUNT, POS_GRAPH_Y + 1, 1 + POS_GRAPH_X + (gRideInfo.graphIndex+1)%DATA_GRAPH_COUNT, SCREEN_HEIGHT-2, TFT_WHITE );

			tft.drawLine( 1 + POS_GRAPH_X + gRideInfo.graphIndex, POS_GRAPH_Y + 60 - (gRideInfo.altitude)/20, 1 + POS_GRAPH_X + gRideInfo.graphIndex, SCREEN_HEIGHT-2, TFT_NAVY );

			uint8_t c = gRideInfo.currentRPM * 0.66;
			uint8_t s = gRideInfo.currentSpeed;
			if( c >= 60 ) c=60;
			if( s >= 60 ) s=60;
			if( c < 0 ) c=0;
			if( s < 0 ) s=0;
			tft.drawPixel(1 + POS_GRAPH_X + gRideInfo.graphIndex, ( POS_GRAPH_Y + POS_GRAPH_HEIGHT ) - c, TFT_CYAN);
			tft.drawPixel(1 + POS_GRAPH_X + gRideInfo.graphIndex, ( POS_GRAPH_Y + POS_GRAPH_HEIGHT ) - s, TFT_RED);           
		}
	}
};
static TftDisplay gDisplay;

void setup(void )
{
	Serial.begin(115200);
	Serial.println("Starting BLE Cycling Computer application...");

	// BLE 초기화  
	gBLE.setup(); 

	// LCD 초기화
	gDisplay.setup(); 

	// 데이터 초기화
	gRideInfo.setup();

	if(!bmp280.init()){
		Serial.println("I2C Sensor error!");
	}
}

void loop()
{ 
	//gBLE.reconnectBLE();

	gRideInfo.calculateData();

	gDisplay.drawUpdateTime();

	delay(100);
}