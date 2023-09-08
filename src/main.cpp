#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

#define TRIGGER_PIN 14
#define ECHO_PIN 12

#define DHTOUT_PIN 27
#define DHTTYPE DHT11

#define WATERTEMP_PIN 26

#define EC_PIN 32
#define EC_GROUND 33
#define EC_POWER 35

DHT dht(DHTOUT_PIN, DHTTYPE);
OneWire oneWire(WATERTEMP_PIN);
DallasTemperature sensors(&oneWire);
Adafruit_ADS1115 ads;


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

const int TempProbePossitive =8;  //Temp Probe power connected to pin 9
const int TempProbeNegative=9;    //Temp Probe Negative connected to pin 8
float Temperature=10;
float EC=0;
float EC25 =0;
int ppm =0;
float raw= 0;
float Vin= 5;
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

void setup() {
  // Dijalankan saat pertama kali esp32 dinyalakan:
  Serial.begin(115200);
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  dht.begin();
  sensors.begin();
  ads.begin();
  pinMode(EC_PIN,INPUT);
  pinMode(EC_POWER,OUTPUT);//Setting pin for sourcing current
  pinMode(EC_GROUND,OUTPUT);//setting pin for sinking current
  digitalWrite(EC_GROUND,LOW);
  

}

void loop() {
  // Dijalankan secara berulang / terus menerus
  Serial.print("Jarak : ");
  Serial.print(measureDistance());
  Serial.println(" cm");

  Serial.print("Suhu Udara : ");
  Serial.print(dht.readTemperature());
  Serial.println(" c");

  Serial.print("Kelembaban Udara : ");
  Serial.print(dht.readHumidity());
  Serial.println(" %");

  sensors.requestTemperatures();
  Serial.print("Suhu Air: ");
  Serial.print(sensors.getTempCByIndex(0));
  Serial.println(" C");

  


  delay(1000);


}