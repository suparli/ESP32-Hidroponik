#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <DS1307RTC.h>
#include <LiquidCrystal_I2C.h>
#include <SD.h>
#include <FS.h>
#include <SPI.h>

#define TRIGGER_PIN 	14
#define ECHO_PIN 		12
#define DHTOUT_PIN 		27
#define DHTTYPE 		DHT11
#define WATERTEMP_PIN 	26
#define SD_PIN 			5
#define RELAY1_PIN 		16
#define RELAY2_PIN 		17

DHT 				dht(DHTOUT_PIN, DHTTYPE);
OneWire 			oneWire(WATERTEMP_PIN);
DallasTemperature 	sensors(&oneWire);
Adafruit_ADS1115 	ads;
HTTPClient 			http;
LiquidCrystal_I2C 	lcd(0x27, 16, 2);
File 				file;

//Variable Wifi Configuration
const char* ssid 		= "Nokia 3310";
const char* password 	= "wifi19519";

//Variable API Configuration
const String host		= "http://192.168.10.14:80";
const String deviceId	= "1";
const String apiKey		= "aabbccdd"; 

//Global Variable
float waterTemperature, distance, temperature, humidity, baPh, bbPh, baEc, bbEc, ph, ec, tds  = 0.0;
int pompaPh, pompaEc, stateLcd = 0;
int interval = 10;
time_t epochTime;
unsigned long previousMillis = 0;

float measureDistance(){
	//Kirim sinyal ke trigger sensor
	digitalWrite(TRIGGER_PIN, HIGH);
	delayMicroseconds(10);
	digitalWrite(TRIGGER_PIN, LOW);

	//Baca Pulse dari echo pin
	unsigned long pulseDuration = pulseIn(ECHO_PIN, HIGH);

	//Hitung jarak berdasarkan waktu pulse
	float distance = pulseDuration * 0.0343 / 2; 

	return distance;
}

float measurePh()
{
	int16_t adc0; // 16 bits ADC read of input A0
	adc0 = ads.readADC_SingleEnded(0);
	float Voltage = (adc0 * 0.1875)/1000;

	return Voltage;
}

float measureEc(){
	int16_t adc1; // 16 bits ADC read of input A0
	adc1 = ads.readADC_SingleEnded(1);
	float Voltage = (adc1 * 0.1875)/1000;

	return Voltage;

}


String datetime(){
   tmElements_t tm;
   char buffer[20];
  
	if (RTC.read(tm)) {
		time_t t = makeTime(tm);
		epochTime = t;
		breakTime(t, tm);
		snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", tm.Year + 1970, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
		return String(buffer);

	} else {
		if (RTC.chipPresent()) {
			Serial.println("The DS1307 is stopped.  Please run the SetTime");
			Serial.println("example to initialize the time and begin running.");
			Serial.println();
		} else {
			Serial.println("DS1307 read error!  Please check the circuitry.");
			Serial.println();
		}
		delay(9000);
	}
  return "";
}

void httpRealtime(){
	const String server = host+"/api/kontrol";
	String dt 			= datetime();
	String jsonPayload 	= "{\"deviceId\":\""+deviceId+"\",\"deviceTime\":\"" + dt + "\"}";
	
	http.begin(server);
	http.addHeader("Content-Type", "application/json");
	http.addHeader("X-API-KEY", apiKey);
	int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
	String payload = http.getString();
	http.end();

	DynamicJsonDocument doc(1024);
	DeserializationError error = deserializeJson(doc, payload);

	if (error) {
		Serial.println("Gagal membaca JSON dari respons.");
	} 
	else 
	{
		const char* dataInterval= doc["data"]["interval"];
		int dataPompaPh = doc["data"]["pompa_ph"];
		int dataPompaEc = doc["data"]["pompa_ec"];
		const char* dataBaPh = doc["data"]["ba_ph"];
		const char* dataBbPh = doc["data"]["bb_ph"];
		const char* dataBaEc = doc["data"]["ba_ec"];
		const char* dataBbEc = doc["data"]["bb_ec"];

		interval = atoi(dataInterval);
		pompaPh = dataPompaPh;
		pompaEc = dataPompaEc;
		baEc = atof(dataBaEc);
		bbEc = atof(dataBbEc);
		baEc = atof(dataBaEc);
		bbEc = atof(dataBbEc);
	}
}
else
{
	Serial.print("Gagal mengirim request. Kode respons HTTP: ");
	Serial.println(httpResponseCode);
}
 
}

void httpLogging(){
	const String server 	= host+"/api/logging";
	String dt 			= datetime();
	String jsonPayload 	= "{\"deviceId\":\""+deviceId+"\",\"deviceTime\":\"" + dt + "\",\"waterLevel\":\"" + distance + "\",\"pompaPh\":\"" + pompaPh + "\", \"pompaEc\":\"" + pompaEc + "\",\"temperature\":\"" + temperature + "\",\"humidity\":\"" + humidity + "\",\"ph\":\"" + ph + "\",\"ec\":\"" + ec + "\",\"tds\":\"" + tds + "\",\"waterTemp\":\"" + waterTemperature + "\"}";
	http.begin(server);
	http.addHeader("Content-Type", "application/json");
	http.addHeader("X-API-KEY", apiKey);

	int httpResponseCode = http.POST(jsonPayload);

	if (httpResponseCode > 0) {
		String payload = http.getString();
		DynamicJsonDocument doc(1024);
		DeserializationError error = deserializeJson(doc, payload);

		if (error) {
			Serial.println("Gagal membaca JSON dari respons.");
		} 
		else 
		{
			Serial.println("Mengirim Logging");
			const char* message= doc["message"];
			Serial.println(message);
		}
	}
	else
	{
		Serial.print("Gagal mengirim request. Kode respons HTTP: ");
		Serial.println(httpResponseCode);
	}

	http.end();	
}

String getFormattedDateTime() 
{
	tmElements_t tm;
	char buffer[20];
	if (RTC.read(tm)) {
		time_t t = makeTime(tm);
		breakTime(t, tm);
		snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", tm.Year + 1970, tm.Month, tm.Day);
		return String(buffer);
	}

	return "";
}

void sdcardLogging(){
	String fileName = "logging.csv";
	file 			= SD.open(fileName, FILE_WRITE);
	String dateTime = getFormattedDateTime();
	// Menyimpan data ke file
	if (file) {
		file.print(dateTime+","+waterTemperature+","+distance+","+temperature+","+humidity+","+ph+","+ec+","+tds);
		file.close();
		Serial.println("Data telah disimpan di " + fileName);
	} else {
		Serial.println("Gagal membuka file " + fileName);
	}
}

void Lcd16x2(){
	stateLcd ++;

    if(stateLcd > 4){
		stateLcd = 0;
    }
    if(stateLcd == 1){
		lcd.clear();
		lcd.setCursor(0, 0); 
		lcd.print("WL     :"); 
		lcd.setCursor(9, 0);
		lcd.print(distance); 
		lcd.setCursor(14, 0);
		lcd.print("cm"); 

		lcd.setCursor(0, 1); 
		lcd.print("TEMP   :"); 
		lcd.setCursor(9, 1);
		lcd.print(temperature);
		lcd.setCursor(14, 1);
		lcd.print("c"); 
    }
    else if(stateLcd == 2){
		lcd.clear();
		lcd.setCursor(0, 0); 
		lcd.print("HUM    :"); 
		lcd.setCursor(9, 0);
		lcd.print(humidity); 
		lcd.setCursor(15, 0);
		lcd.print("%"); 

		lcd.setCursor(0, 1); 
		lcd.print("WTRTMP :"); 
		lcd.setCursor(9, 1);
		lcd.print(waterTemperature); 
		lcd.setCursor(15, 1);
		lcd.print("c"); 
    }
    else if(stateLcd == 3){
		lcd.clear();
		lcd.setCursor(0, 0); 
		lcd.print("PH :"); 
		lcd.setCursor(6, 0);
		lcd.print(ph); 
		lcd.setCursor(12, 0);
		lcd.print(""); 

		lcd.setCursor(0, 1); 
		lcd.print("EC :"); 
		lcd.setCursor(6, 1);
		lcd.print(ec); 
		lcd.setCursor(12, 1);
		lcd.print("ppm"); 
    }
    else if(stateLcd == 4){
		lcd.clear();
		lcd.setCursor(0, 0); 
		lcd.print("DATETIME"); 
		lcd.setCursor(0, 1);
		lcd.print(datetime());
    }


}

void debug()
{
	Serial.print("Jarak : ");
	Serial.print(distance);
	Serial.print(" cm  |  ");

	Serial.print("Suhu Udara : ");
	Serial.print(temperature);
	Serial.print(" c  |  ");

	Serial.print("Kelembaban Udara : ");
	Serial.print(humidity);
	Serial.print(" %  |  ");

	Serial.print("Suhu Air: ");
	Serial.print(waterTemperature);
	Serial.println(" c  |  ");

	Serial.print("PH: ");
	Serial.print(ph);
	Serial.println(" |  ");

	Serial.print("EC : ");
	Serial.print(ec);
	Serial.println(" ppm");
}

void setup() 
{
	Serial.begin(9600);

	lcd.init(); 
	lcd.backlight(); 

	pinMode(RELAY1_PIN,OUTPUT);
	pinMode(RELAY2_PIN,OUTPUT);
	pinMode(TRIGGER_PIN, OUTPUT);
	pinMode(ECHO_PIN, INPUT);

	digitalWrite(RELAY1_PIN,LOW);
	digitalWrite(RELAY2_PIN,LOW);


	dht.begin();
	sensors.begin();
	ads.begin();

	WiFi.begin(ssid, password);
	// WiFi.begin(ssid);

	while (WiFi.status() != WL_CONNECTED) {
		delay(1000);
		Serial.println("Menghubungkan ke WiFi...");
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Menghubungkan");
		lcd.setCursor(0, 1);
		lcd.print("WiFi");
		lcd.setCursor(5, 1);
		lcd.print(ssid);
	}
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Terhubung !!");
	Serial.println("Tersambung ke WiFi");

	if(!SD.begin(SD_PIN)){
		Serial.println("Card Mount Failed");
		return;
	}

	uint8_t cardType = SD.cardType();

	if(cardType == CARD_NONE){
		Serial.println("No SD card attached");
		return;
	}

}

void loop() {
	sensors.requestTemperatures();
	datetime();

	waterTemperature = sensors.getTempCByIndex(0);
	distance         = measureDistance();
	temperature      = dht.readTemperature();
	humidity         = dht.readHumidity();
	ph               = measurePh();
	ec               = measureEc();

	if(pompaPh == 1)
	{
		digitalWrite(RELAY1_PIN, HIGH);
	} else 
	{
		digitalWrite(RELAY1_PIN, LOW);
	}

	if(pompaEc == 1)
	{
		digitalWrite(RELAY2_PIN, HIGH);
	} 
	else 
	{
    	digitalWrite(RELAY2_PIN, LOW);
	}

	unsigned long currentMillis = millis();
	if (currentMillis - previousMillis >= 3000) 
	{
		Lcd16x2();
		previousMillis = currentMillis;
	}

	int loggingMinute	= interval*60;  
	int logging 		= epochTime % loggingMinute;

	if(logging  == 0 || logging == 1)
	{
	httpLogging();
	delay(1000);
	// sdcardLogging();
	}  

	httpRealtime();
	debug();

}