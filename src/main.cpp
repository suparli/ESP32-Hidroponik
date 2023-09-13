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

#define TRIGGER_PIN 14
#define ECHO_PIN 12

#define DHTOUT_PIN 27
#define DHTTYPE DHT11

#define WATERTEMP_PIN 26

#define EC_PIN 35
#define EC_GROUND 33
#define EC_POWER 32

#define SD_PIN 5

#define RELAY1_PIN 16
#define RELAY2_PIN 17

DHT dht(DHTOUT_PIN, DHTTYPE);
OneWire oneWire(WATERTEMP_PIN);
DallasTemperature sensors(&oneWire);
Adafruit_ADS1115 ads;
HTTPClient http;
LiquidCrystal_I2C lcd(0x27, 16, 2);
File file;

//Variable Wifi Configuration
const char* ssid = "klimatwifi09";
const char* password = "11111111";
const char* apiKey = "aabbccdd"; // Gantilah dengan kunci API Anda

//Global Variable
float waterTemperature, distance, temperature, humidity, baPh, bbPh, baEc, bbEc, ph, ec, tds  = 0.0;
int pompaPh, pompaEc, stateLcd = 0;
int interval = 10;
time_t epochTime;
unsigned long previousMillis = 0;

//EC Variable
int R1= 1000; //Do not Replace R1 with a resistor lower than 300 ohms
int Ra=25; //Resistance of powering Pins

// Hana      [USA]        PPMconverion:  0.5
// Eutech    [EU]          PPMconversion:  0.64
//Tranchen  [Australia]  PPMconversion:  0.7
// Why didnt anyone standardise this?
float PPMconversion=0.7;

//The value below will change depending on what chemical solution we are measuring
//0.019 is generaly considered the standard for plant nutrients [google "Temperature compensation EC" for more info
float TemperatureCoef = 0.019; //this changes depending on what chemical we are measuring

//Mine was around 2.9 with plugs being a standard size they should all be around the same
//But If you get bad readings you can use the calibration script and fluid to get a better estimate for K
float K=2.88; 


float Temperature=10;
float EC=0;
float EC25 =0;
int ppm =0;
float raw= 0;
float Vin= 3.3;
float Vdrop= 0;
float Rc= 0;
float buffer=0;


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

  Serial.print("AIN0: ");
  Serial.print(adc0);
  Serial.print("\tVoltage: ");
  Serial.println(Voltage, 2);

  return Voltage;
}

float measureEc(){
  R1=(R1+Ra);
  sensors.requestTemperatures();
  Temperature=sensors.getTempCByIndex(0);
  // Temperature = 29.8;
  //************Estimates Resistance of Liquid ****************//
  digitalWrite(EC_POWER,HIGH);
  raw= analogRead(EC_PIN);
  raw= analogRead(EC_PIN);// This is not a mistake, First reading will be low beause if charged a capacitor
  digitalWrite(EC_POWER,LOW);

  //***************** Converts to EC **************************//
  Vdrop= (Vin*raw)/4096.0;
  Rc=(Vdrop*R1)/(Vin-Vdrop);
  Rc=Rc-Ra; //acounting for Digital Pin Resitance
  EC = 1000/(Rc*K);
  
  //*************Compensating For Temperaure********************//
  EC25  =  EC/ (1+ TemperatureCoef*(Temperature-25.0));
  ppm=(EC25)*(PPMconversion*1000);

  //************************** End OF EC Function ***************************//
  Serial.print("Rc: ");
  Serial.print(Rc);
  Serial.print(" EC: ");
  Serial.print(EC25);
  Serial.print(" Simens  ");
  Serial.print(ppm);
  Serial.print(" ppm  ");
  Serial.print(Temperature);
  Serial.println(" *C ");

  return EC25 ;
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
  const char* server = "http://192.168.10.209:80/api/kontrol";
  String dt = datetime();
  http.begin(server);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", apiKey);

  String jsonPayload = "{\"deviceId\":\"1\",\"deviceTime\":\"" + dt + "\"}";

  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    // Serial.print("Respon HTTP Code: ");
    // Serial.println(httpResponseCode);

    String payload = http.getString();
    http.end();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.println("Gagal membaca JSON dari respons.");
    } 
    else 
    {
        // Serial.println("Berhasil membaca JSON dari respons.");
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
  const char* server = "http://192.168.10.209:80/api/logging";
  String dt = datetime();
  http.begin(server);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", apiKey);

  String jsonPayload = "{\"deviceId\":\"1\",\"deviceTime\":\"" + dt + "\",\"waterLevel\":\"" + distance + "\",\"pompaPh\":\"" + pompaPh + "\", \"pompaEc\":\"" + pompaEc + "\",\"temperature\":\"" + temperature + "\",\"humidity\":\"" + humidity + "\",\"ph\":\"" + ph + "\",\"ec\":\"" + ec + "\",\"tds\":\"" + tds + "\",\"waterTemp\":\"" + waterTemperature + "\"}";
  Serial.println(jsonPayload);
  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    // Serial.print("Respon HTTP Code: ");
    // Serial.println(httpResponseCode);

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

String getFormattedDateTime() {
  tmElements_t tm;
  char buffer[20];
  // Mendapatkan tanggal dan waktu saat ini
  if (RTC.read(tm)) {
    time_t t = makeTime(tm);
    breakTime(t, tm);
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", tm.Year + 1970, tm.Month, tm.Day);
    return String(buffer);
  }
}

void sdcardLogging(){
  String fileName = "logging.csv";
  file = SD.open(fileName, FILE_WRITE);
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

void setup() {
  // Dijalankan saat pertama kali esp32 dinyalakan:
  Serial.begin(9600);

  lcd.init(); // Inisialisasi LCD
  lcd.backlight(); // Aktifkan pencahayaan LCD
 
  
  pinMode(RELAY1_PIN,OUTPUT);
  pinMode(RELAY2_PIN,OUTPUT);
  digitalWrite(RELAY1_PIN,LOW);
  digitalWrite(RELAY2_PIN,LOW);

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(EC_PIN,INPUT);
  pinMode(EC_POWER,OUTPUT);//Setting pin for sourcing current
  pinMode(EC_GROUND,OUTPUT);//setting pin for sinking current
  
  digitalWrite(EC_GROUND,LOW);
  dht.begin();
  sensors.begin();
  ads.begin();

  // WiFi.begin(ssid, password);
  WiFi.begin(ssid);
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
  waterTemperature = sensors.getTempCByIndex(0);
  distance         = measureDistance();
  temperature      = dht.readTemperature();
  humidity         = dht.readHumidity();
  ph               = measurePh();
  datetime();

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

  if(pompaPh == 1){
    digitalWrite(RELAY1_PIN, HIGH);
  } else {
    digitalWrite(RELAY1_PIN, LOW);
  }

  if(pompaEc == 1){
    digitalWrite(RELAY2_PIN, HIGH);
  } else {
    digitalWrite(RELAY2_PIN, LOW);
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 3000) {
    stateLcd ++;

    if(stateLcd > 4){
      stateLcd = 0;
    }

    if(stateLcd == 1){
      lcd.clear();
      lcd.setCursor(0, 0); // Set cursor ke baris 1, kolom 1
      lcd.print("WL     :"); // Menampilkan teks di LCD
      lcd.setCursor(9, 0);
      lcd.print(distance); // Menampilkan teks di LCD
      lcd.setCursor(14, 0);
      lcd.print("cm"); 

      lcd.setCursor(0, 1); // Set cursor ke baris 1, kolom 1
      lcd.print("TEMP   :"); // Menampilkan teks di LCD
      lcd.setCursor(9, 1);
      lcd.print(temperature); // Menampilkan teks di LCD
      lcd.setCursor(14, 1);
      lcd.print("c"); 
    }
    else if(stateLcd == 2){
      lcd.clear();
      lcd.setCursor(0, 0); // Set cursor ke baris 1, kolom 1
      lcd.print("HUM    :"); // Menampilkan teks di LCD
      lcd.setCursor(9, 0);
      lcd.print(humidity); // Menampilkan teks di LCD
      lcd.setCursor(15, 0);
      lcd.print("%"); 

      lcd.setCursor(0, 1); // Set cursor ke baris 1, kolom 1
      lcd.print("WTRTMP :"); // Menampilkan teks di LCD
      lcd.setCursor(9, 1);
      lcd.print(waterTemperature); // Menampilkan teks di LCD
      lcd.setCursor(15, 1);
      lcd.print("c"); 
    }
    else if(stateLcd == 3){
      lcd.clear();
      lcd.setCursor(0, 0); // Set cursor ke baris 1, kolom 1
      lcd.print("PH     :"); // Menampilkan teks di LCD
      lcd.setCursor(9, 0);
      lcd.print(ph); // Menampilkan teks di LCD
      lcd.setCursor(15, 0);
      lcd.print(""); 
    }
    else if(stateLcd == 4){
      lcd.clear();
      lcd.setCursor(0, 0); // Set cursor ke baris 1, kolom 1
      lcd.print("DATETIME :"); // Menampilkan teks di LCD
      lcd.setCursor(0, 1);
      lcd.print(datetime());
    }


    // Perbarui previousMillis
    previousMillis = currentMillis;
  }



  int loggingMinute = interval*60;  
  int logging = epochTime % loggingMinute;

  if(logging  == 0 || logging == 1){
    httpLogging();
    sdcardLogging();
  }  

  httpRealtime();

 

}