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

DHT dht(DHTOUT_PIN, DHTTYPE);
OneWire oneWire(WATERTEMP_PIN);
DallasTemperature sensors(&oneWire);
Adafruit_ADS1115 ads;



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

void setup() {
  // Dijalankan saat pertama kali esp32 dinyalakan:
  Serial.begin(115200);
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  dht.begin();
  sensors.begin();
  ads.begin();

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

  int16_t adc0; // 16 bits ADC read of input A0
  adc0 = ads.readADC_SingleEnded(0);
  float Voltage = (adc0 * 0.1875)/1000;

  Serial.print("AIN0: ");
  Serial.print(adc0);
  Serial.print("\tVoltage: ");
  Serial.println(Voltage, 2);



  delay(1000);


}