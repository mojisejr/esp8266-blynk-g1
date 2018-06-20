//LAST UPDATE : 13/4/2561
//EDIT TOPIC: ADD TMD data
//CODER: CBNON

#include <SHT21.h>
#include <DHT.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <SimpleTimer.h>
#include <ArduinoJson.h>
#include <WidgetRTC.h>
#include <TimeLib.h>
#include <Servo.h>

#define DHTPIN D3
#define DHTTYPE DHT22
#define FANPIN D8
#define HDPIN D7.
#define FANSHUTTER D5
#define INSHUTTER D6

SHT21 SHT21;
DHT dht(DHTPIN, DHTTYPE);
WidgetTerminal terminal(V51);
WidgetRTC rtc;
SimpleTimer timer;

Servo fanshutter;
Servo inshutter;
 
float C_Temp = 0.0;
float C_RH = 0.0;
float Air_Temp = 0.0;
float Air_RH = 0.0;

float VPD;
float temp_diff;
int vpd_mode;

char auth[] = "";

//TMD API
const char* host = "data.tmd.go.th";
const char* uid = "";
const char* token1 = "";
//TMD vars
const char* StationName;
const char* _Lat;
const char* _Long;
const char* _Time;
const char* _Temp;
const char* _Pressure;
const char* _DewPoint;
const char* _RH;
const char* _VP;

BLYNK_CONNECTED(){
  Blynk.virtualWrite(V50, 1); //initializing the fanmode to be 1 : Automode and let the button light up
  TMD_API();
  Blynk.syncAll();
}
BLYNK_WRITE(V14){ // manual Humidifier on and off
  int pinValue = param.asInt();
  if(vpd_mode == 0){
    digitalWrite(HDPIN, pinValue);
    Blynk.virtualWrite(V14, pinValue);
  }
}
BLYNK_WRITE(V13){ // manual fan turn on and off
  int pinValue = param.asInt();
  if(vpd_mode == 0){
    digitalWrite(FANPIN, pinValue);
    Blynk.virtualWrite(V13, pinValue);
  }
}
BLYNK_WRITE(V50){
  vpd_mode = param.asInt();
  if(vpd_mode == 1){
    VPD_Function();
  }else{
  }
}
void _Servo(){
  
}
void TMD_API(){
  WiFiClient client;
  if(!client.connect(host, 80)){
    Serial.println("connection failed");
    return;
  }
  String url = "/api/Weather3Hours/V1/?tpye=json&province=จันทบุรี";

  Serial.print("Requseting URL ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
  "accept: application/json\r\n" +
  "host: " + host + "\r\n" +
  "connection: keep-alive\r\n\r\n");

  int timeout = millis() + 5000;
  while(client.available() == 0){
    if(timeout - millis() < 0){
      Serial.println(">>> Client Timeout!");
      client.stop();
      return;
    }
  }
  //check HTTP Status
  char _status[32] = {0};
  client.readBytesUntil('\r', _status, sizeof(_status));
  if(strcmp(_status, "HTTP/1.1 200 OK") != 0){
    Serial.print("Unexpected response: ");
    Serial.println(_status);
    return;
  }
  //Skip Headers
  char endOfHeaders[] = "\r\n\r\n";
  if(!client.find(endOfHeaders)){
    Serial.println("Invalid response");
    return;
  }

  //Allocate JsonBuffer
  const size_t capacity = JSON_ARRAY_SIZE(2) + 25*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(6) + 2*JSON_OBJECT_SIZE(7) + 2*JSON_OBJECT_SIZE(12) + 1550;
  DynamicJsonBuffer jsonBuffer(capacity);

  while(client.available()){
    String line = client.readStringUntil('\r');
    JsonObject& root = jsonBuffer.parseObject(line);
    if(!root.success()){
      Serial.println("Parsing failed!");
      return;
    }

    JsonObject& PhliuAgromet = root["Stations"][1];
    StationName = PhliuAgromet["StationNameEng"];
    JsonObject& PA_Data = PhliuAgromet["Observe"];
    _Time = PA_Data["Time"];
    _Temp = PA_Data["Temperature"]["Value"];
    _RH = PA_Data["RelativeHumidity"]["Value"];
    _Pressure = PA_Data["StationPressure"]["Value"];
    _DewPoint = PA_Data["DewPoint"]["Value"];
    _VP = PA_Data["VaporPressure"]["Value"];
    
    terminal.println(String(StationName));
    terminal.println(String(_Time));
    terminal.println("Temp: " + String(_Temp) +" *C");
    terminal.println("RH: " + String(_RH) + " %");
    terminal.println("Pressure: " + String(_Pressure) + " hPa");
    terminal.println("DewPoint: " + String(_DewPoint) + " *C");
    terminal.println("VaporPressure: " + String(_VP) + " hPa");
    terminal.flush();
  }
}
void readBattLv(){
  int batt_value = analogRead(A0);
  float volt = 3.637*(float)batt_value/1023.00;
  float volt_pct = (100*volt)/3.637;
  Blynk.virtualWrite(V127, volt);
}
void readParameters(){
  Air_Temp = dht.readTemperature();
  Air_RH = dht.readHumidity();
  C_Temp = SHT21.getTemperature();
  C_RH = SHT21.getHumidity();

  if((!isnan(Air_Temp)) || (!isnan(Air_RH))){ // prevent from nan value
    Blynk.virtualWrite(V8, Air_Temp);
    Blynk.virtualWrite(V9, Air_RH);
  }
  Blynk.virtualWrite(V10,C_Temp);
  Blynk.virtualWrite(V11,C_RH);
}
void FAN(int pinValue){
  digitalWrite(FANPIN, pinValue);
  Blynk.virtualWrite(V13, pinValue);
}
void Humidifier(int pinValue){
   digitalWrite(HDPIN, pinValue);
   Blynk.virtualWrite(V14, pinValue);
}
void VPD_Function(){
  float VP_sat = (610.7 * pow(10,(7.5*C_Temp)/(237.3+C_Temp)))/1000;
  float VP_air = ((610.7 * pow(10,(7.5*Air_Temp)/(237.3+Air_Temp)))/1000)*(Air_RH/100);
  VPD = VP_sat - VP_air;
  temp_diff = C_Temp - Air_Temp;
  Blynk.virtualWrite(V1, VPD);
  Blynk.virtualWrite(V4, temp_diff);
  if((vpd_mode == 1) && (hour() < 18)){
    if(VPD < 0.4){
      FAN(HIGH);
      Humidifier(LOW);
    }else if(VPD > 0.4 && VPD < 0.8){
      FAN(LOW);
      Humidifier(LOW);
    }else if(VPD > 0.8){
      FAN(LOW);
      Humidifier(HIGH);
    }
  }
}
void setup() {
  SHT21.begin();
  Serial.begin(115200);
  pinMode(FANPIN, OUTPUT);
  pinMode(HDPIN, OUTPUT);
  fanshutter.attach(FANSHUTTER);
  inshutter.attach(INSHUTTER);
  Blynk.begin(auth, "", "");
  timer.setInterval(2000, VPD_Function);
  timer.setInterval(1000, readParameters);
  //timer.setInterval(2000, TMD_API);
  //timer.setInterval(5000, readBattLv);
}

void loop() {
  Blynk.run();
  timer.run();
}

