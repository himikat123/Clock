/* Clock BIM v3.0
 * © himikat123@gmail.com, Nürnberg, Deutschland, 2019-2021 
 * https://github.com/himikat123/Clock
 */
                               // Board: Generic ESP8266 Module 
                               // 1MB (256kB SPIFFS)
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "TM1637_6D.h"
#include "LedControl.h"
#include <TimeLib.h>
#include <NtpClientLib.h>
#include <DS3231.h>
#include <FS.h>
#include <Ticker.h>
#include <ESP8266TrueRandom.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP085.h>
#include "SHT21.h"
#include "DHTesp.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "clock.h"

#define CLK             14
#define DIO             12
#define DAT             13
#define CLOCK           15
#define LOAD             4
#define ONE_WIRE_BUS     0
#define BUTTON           0
#define SDA              2
#define SCL              0
#define CLOCK_ADDRESS 0x68

int dhtPin = 5;

ESP8266WebServer  webServer(80);
TM1637_6D         tm1637_6D(CLK, DIO);
Ticker            half_sec;
Ticker            weather_upd;
Ticker            thing_upd;
Ticker            thing_send;
ntpClient         *ntp;
DS3231            Clock;
SHT21             SHT21;
DHTesp            dht;
Adafruit_BMP085   bmp;
Adafruit_BME280   bme;
OneWire           oneWire(ONE_WIRE_BUS);
DallasTemperature term(&oneWire);
DeviceAddress     thermometer;
LedControl        lc = LedControl(DAT, CLOCK, LOAD, 1);
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();

void half(void){
  datas.halb = true;
}

void w_upd(void){
  datas.weather_upd = true;
}

void t_upd(void){
  datas.thing_upd = true;
}

void t_snd(void){
  datas.thing_snd = true;
}

void setup(void){
    //Serial port
  Serial.begin(74880);
  while(!Serial);
    //PINs
  pinMode(BUTTON, INPUT);
  pinMode(ONE_WIRE_BUS, INPUT);
  pinMode(SDA, INPUT);
    //i2c
  Wire.pins(SDA, SCL);
    //SPIFS
  if(!SPIFFS.begin()) while(1) yield();
    //EEPROM 
  read_eeprom();
  ESP.rtcUserMemoryWrite(0, (uint32_t*) ESP8266TrueRandom.random(0, 1000), 10);
    //sensors
  sensors_init();
    //Display
  tm1637_6D.init();
  tm1637_6D.set(3);
  tm1637_6D.display(datas.clock_dig[0], datas.clock_points[0]);
  lc.shutdown(0, false);
  lc.shutdown(1, false);
  lc.setIntensity(0, 7);
  lc.setIntensity(1, 7);
  lc.clearDisplay(0);
  lc.clearDisplay(1);
  lc.setChar(0, 7, '-', false);
  lc.setChar(0, 6, '-', false);
  lc.setChar(0, 5, '-', false);
  lc.setChar(0, 4, '-', false);
  lc.setChar(0, 3, '-', false);
  lc.setChar(0, 2, '-', false);
  lc.setChar(0, 1, '-', false);
  lc.setChar(0, 0, '-', false);
  lc.setChar(1, 7, '-', false);
  lc.setChar(1, 6, '-', false);
  lc.setChar(1, 5, '-', false);
  lc.setChar(1, 4, '-', false);
  lc.setChar(1, 3, '-', false);
  lc.setChar(1, 2, '-', false);
  lc.setChar(1, 1, '-', false);
  lc.setChar(1, 0, '-', false);
    //WIFI MODE
  WiFi.mode(WIFI_STA);
  connectToWiFi();
    //Settings
  web_settings();
    //NTP
  ntp = ntpClient::getInstance(config.ntp, 1);
  ntp -> setInterval(15, config.ntp_period * 60);
  ntp -> setTimeZone(config.utc);
  ntp -> setDayLight(config.daylight);
  ntp -> begin();
    //Ticker
  weather_upd.attach_ms(1200000, w_upd);
  thing_upd.attach_ms(30000, t_upd);
  thing_send.attach_ms(300000, t_snd);
}

void loop(void){
  if(WiFi.status() == WL_CONNECTED){
    if(datas.ap_mode) WiFi.mode(WIFI_STA);
    datas.ap_mode = false;
  }
  else apMode(WIFI_AP);
  if(datas.prev_sec != second()){
    sensors_read();
    datas.halb = false;
    half_sec.attach_ms(500, half);
    data_prep();
    display_fill();
    datas.prev_sec = second();
  }
  if(datas.halb){
    data_prep();
    display_fill();
    datas.halb = false;
    half_sec.detach();
  }
  if(datas.weather_upd){
    if((String(config.appid) != "" or String(config.appkey) != "") and !datas.ap_mode) getWeatherNow();
    datas.weather_upd = false;
  }
  if(datas.thing_upd){
    if(config.thngrcv and !datas.ap_mode) thingspk_recv();
    datas.thing_upd = false;
  }
  if(datas.thing_snd){
    if(config.thngsend and !datas.ap_mode) thingspk_send();
    datas.thing_snd = false;
  }
  if(now() - ntp -> getLastNTPSync() < 2){
    if(now() > 1617569756 and sensors.ds32_det){
      bool a, b, c;
      if(
        Clock.getYear() + 2000 != year() or
        Clock.getMonth(c) != month() or
        Clock.getDate() != day() or
        Clock.getHour(a, b) != hour() or
        Clock.getMinute() != minute() or
        Clock.getSecond() != second()
      ){
        Serial.println("sync RTC");
        Clock.setYear(year() - 2000);
        Clock.setMonth(month());
        Clock.setDate(day());
        Clock.setHour(hour());
        Clock.setMinute(minute());
        Clock.setSecond(second());
      }
    }
  }
  webServer.handleClient();
}

void data_prep(void){
  if((millis() - datas.prev_millis >= config.dp[datas.snum] * 1000) or (config.dp[datas.snum] == 0)){
    datas.snum++;
    for(uint8_t i=datas.snum; i<6; i++){
      if(config.dp[datas.snum] == 0) datas.snum++;
      else break;
    }
    if(datas.snum > 5) datas.snum = 0;
    datas.prev_millis = millis();
  }
  if((millis() - datas.prev_millis2 >= config.d2p[datas.snum2] * 1000) or (config.d2p[datas.snum2] == 0)){
    datas.snum2++;
    for(uint8_t i=datas.snum2; i<6; i++){
      if(config.d2p[datas.snum2] == 0) datas.snum2++;
      else break;
    }
    if(datas.snum2 > 5) datas.snum2 = 0;
    datas.prev_millis2 = millis();
  }
  if(config.dp[datas.snum] > 0){
    switch(config.dt[datas.snum]){
      case 1: 
        if(String(config.ds[datas.snum]) == "C") datas.dots = get_clock(datas.dots, 0);
        if(String(config.ds[datas.snum]) == "D") datas.dots = get_date(0);
        break;
      case 2:
        if(String(config.ds[datas.snum]) == "T") datas.dots = get_temp(sensors.bme280_temp, 0);
        if(String(config.ds[datas.snum]) == "H") datas.dots = get_hum((float)sensors.bme280_hum, 0);
        if(String(config.ds[datas.snum]) == "P") datas.dots = get_pres((float)sensors.bme280_pres, 0);
        break;
      case 3:
        if(String(config.ds[datas.snum]) == "T") datas.dots = get_temp(sensors.bmp180_temp, 0);
        if(String(config.ds[datas.snum]) == "P") datas.dots = get_pres((float)sensors.bmp180_pres, 0);
        break;
      case 4:
        if(String(config.ds[datas.snum]) == "T") datas.dots = get_temp(sensors.sht21_temp, 0);
        if(String(config.ds[datas.snum]) == "H") datas.dots = get_hum((float)sensors.sht21_hum, 0);
        break;
      case 5:
        if(String(config.ds[datas.snum]) == "T") datas.dots = get_temp(sensors.dht22_temp, 0);
        if(String(config.ds[datas.snum]) == "H") datas.dots = get_hum((float)sensors.dht22_hum, 0);
        break;
      case 6:
        datas.dots = get_temp(sensors.ds18_temp, 0);
        break;
      case 7:
        datas.dots = get_temp(sensors.ds32_temp, 0);
        break;
      case 8://thingspeak
        if(config.ds[datas.snum][0] == 'T') datas.dots = get_temp(datas.thing[String(config.ds[datas.snum][2]).toInt()], 0);
        if(config.ds[datas.snum][0] == 'H') datas.dots = get_hum(datas.thing[String(config.ds[datas.snum][2]).toInt()], 0);
        if(config.ds[datas.snum][0] == 'P') datas.dots = get_pres(datas.thing[String(config.ds[datas.snum][2]).toInt()], 0);
        break;
      case 9://weather
        if(String(config.ds[datas.snum]) == "T") datas.dots = get_temp(datas.temp_web, 0);
        if(String(config.ds[datas.snum]) == "H") datas.dots = get_hum(datas.hum_web, 0);
        if(String(config.ds[datas.snum]) == "P") datas.dots = get_pres(datas.pres_web, 0);
        break;
      default:;; 
    }
  }
  if(config.d2p[datas.snum2] > 0){
    switch(config.d2t[datas.snum2]){
      case 1: 
        if(String(config.d2s[datas.snum2]) == "C") datas.dots2 = get_clock(datas.dots2, 1);
        if(String(config.d2s[datas.snum2]) == "D") datas.dots2 = get_date(1);
        break;
      case 2:
        if(String(config.d2s[datas.snum2]) == "T") datas.dots2 = get_temp(sensors.bme280_temp, 1);
        if(String(config.d2s[datas.snum2]) == "H") datas.dots2 = get_hum((float)sensors.bme280_hum, 1);
        if(String(config.d2s[datas.snum2]) == "P") datas.dots2 = get_pres((float)sensors.bme280_pres, 1);
        break;
      case 3:
        if(String(config.d2s[datas.snum2]) == "T") datas.dots2 = get_temp(sensors.bmp180_temp, 1);
        if(String(config.d2s[datas.snum2]) == "P") datas.dots2 = get_pres((float)sensors.bmp180_pres, 1);
        break;
      case 4:
        if(String(config.d2s[datas.snum2]) == "T") datas.dots2 = get_temp(sensors.sht21_temp, 1);
        if(String(config.d2s[datas.snum2]) == "H") datas.dots2 = get_hum((float)sensors.sht21_hum, 1);
        break;
      case 5:
        if(String(config.d2s[datas.snum2]) == "T") datas.dots2 = get_temp(sensors.dht22_temp, 1);
        if(String(config.d2s[datas.snum2]) == "H") datas.dots2 = get_hum((float)sensors.dht22_hum, 1);
        break;
      case 6:
        datas.dots2 = get_temp(sensors.ds18_temp, 1);
        break;
      case 7:
        datas.dots2 = get_temp(sensors.ds32_temp, 1);
        break;
      case 8://thingspeak
        if(config.d2s[datas.snum2][0] == 'T') datas.dots2 = get_temp(datas.thing[String(config.d2s[datas.snum2][2]).toInt()], 1);
        if(config.d2s[datas.snum2][0] == 'H') datas.dots2 = get_hum(datas.thing[String(config.d2s[datas.snum2][2]).toInt()], 1);
        if(config.d2s[datas.snum2][0] == 'P') datas.dots2 = get_pres(datas.thing[String(config.d2s[datas.snum2][2]).toInt()], 1);
        break;
      case 9://weather
        if(String(config.d2s[datas.snum2]) == "T") datas.dots2 = get_temp(datas.temp_web, 1);
        if(String(config.d2s[datas.snum2]) == "H") datas.dots2 = get_hum(datas.hum_web, 1);
        if(String(config.d2s[datas.snum2]) == "P") datas.dots2 = get_pres(datas.pres_web, 1);
        break;
      default:;; 
    }
  }
}

void tm1637_fill(uint8_t type, uint8_t disp){
  if(type == 0){
    if(disp == 0) datas.clock_points[0][5] = datas.clock_points[0][4] = datas.clock_points[0][3] = datas.clock_points[0][0] = datas.dots ? POINT_ON : POINT_OFF;
    if(disp == 1) datas.clock_points[1][5] = datas.clock_points[1][4] = datas.clock_points[1][3] = datas.clock_points[1][0] = datas.dots2 ? POINT_ON : POINT_OFF;
    datas.clock_dig[disp][0] = datas.clock_dig[disp][5];
    datas.clock_dig[disp][5] = datas.clock_dig[disp][4];
    datas.clock_dig[disp][4] = datas.clock_dig[disp][3];
    datas.clock_dig[disp][3] = datas.clock_dig[disp][2];
    datas.clock_dig[disp][1] = datas.clock_dig[disp][2] = 0x0E;
    uint8_t bright = 10;
    if(disp == 0){
      if(datas.is_day) bright = round(config.day_bright / 12);
      else bright = round(config.night_bright / 12);
    }
    if(disp == 1){
      if(datas.is_day2) bright = round(config.day_bright2 / 12);
      else bright = round(config.night_bright2 / 12);
    }
    tm1637_6D.set(bright > 7 ? 7 : bright);
    tm1637_6D.display(datas.clock_dig[disp], datas.clock_points[disp]);
  }
  
  if(type == 1){
    datas.clock_points[disp][5] = datas.clock_points[disp][3] = datas.clock_points[disp][1] = datas.clock_points[disp][0] = POINT_OFF;
    if(disp == 0) datas.clock_points[0][4] = datas.clock_points[0][2] = datas.dots ? POINT_ON : POINT_OFF;
    if(disp == 1) datas.clock_points[1][4] = datas.clock_points[1][2] = datas.dots2 ? POINT_ON : POINT_OFF;
    int8_t dig[6];
    for(uint8_t i=0; i<6; i++) dig[i] = datas.clock_dig[disp][6 - i];
    uint8_t bright = 10;
    if(disp == 0){
      if(datas.is_day) bright = round(config.day_bright / 12);
      else bright = round(config.night_bright / 12);
    }
    if(disp == 1){
      if(datas.is_day2) bright = round(config.day_bright2 / 12);
      else bright = round(config.night_bright2 / 12);
    }
    tm1637_6D.set(bright > 7 ? 7 : bright);
    tm1637_6D.display(dig, datas.clock_points[disp]);
  }
}

void max7219_fill(uint8_t type1, uint8_t type2){
  uint8_t bright = 15;
  uint8_t bright2 = 15;
  if(datas.is_day) bright = round(config.day_bright / 6);
  else bright = round(config.night_bright / 6);
  if(datas.is_day2) bright2 = round(config.day_bright2 / 6);
  else bright2 = round(config.night_bright2 / 6);
  uint8_t msymb[2][20] = {
    {
      0x7E, 0x30, 0x6D, 0x79, //0123
      0x33, 0x5B, 0x5F, 0x70, //4567
      0x7F, 0x7B, 0x63, 0x4F, //89°E
      0x67, 0x37, 0x00, 0x0F, //PH t
      0x01, 0x4E, 0x00, 0x00  //-C
    },
    {
      0xFE, 0xB0, 0xED, 0xF9, //0.1.2.3.
      0xB3, 0xDB, 0xDF, 0xF0, //4.5.6.7.
      0xFF, 0xFB, 0xE3, 0xCF, //8.9.°.E.
      0xE7, 0xB7, 0x80, 0x8F, //P.H. .t.
      0x81, 0xCE, 0x80, 0x80  //-.C. . .
    },
  };

  if(((type1 == 2 || type1 == 3) && !(type2 == 2 || type2 == 3)) || (!(type1 == 2 || type1 == 3) && (type2 == 2 || type2 == 3))){
    if(type1 == 2 || type1 == 3){
      uint8_t dot = (type1 == 2) ? (datas.dots == true) ? 1 : 0 : 0;
      uint8_t typ = (type1 == 2) ? 1 : 0;
      lc.setIntensity(0, bright);
      lc.setRow(0, 0, msymb[0][datas.clock_dig[0][7 - typ]]);
      lc.setRow(0, 1, msymb[0][datas.clock_dig[0][6 - typ]]);
      lc.setRow(0, 2, msymb[dot][datas.clock_dig[0][5 - typ]]);
      lc.setRow(0, 3, msymb[0][datas.clock_dig[0][4 - typ]]);
      lc.setRow(0, 4, msymb[dot][datas.clock_dig[0][3 - typ]]);
      lc.setRow(0, 5, msymb[0][datas.clock_dig[0][2 - typ]]);
      lc.setRow(0, 6, (type1 == 2) ? 0 : msymb[0][datas.clock_dig[0][1]]);
      lc.setRow(0, 7, (type1 == 2) ? 0 : msymb[0][datas.clock_dig[0][0]]);
    }
    if(type2 == 2 || type2 == 3){
      uint8_t dot = (type2 == 2) ? (datas.dots2 == true) ? 1 : 0 : 0;
      uint8_t typ = (type2 == 2) ? 1 : 0;
      lc.setIntensity(0, bright2);
      lc.setRow(0, 0, msymb[0][datas.clock_dig[1][7 - typ]]);
      lc.setRow(0, 1, msymb[0][datas.clock_dig[1][6 - typ]]);
      lc.setRow(0, 2, msymb[dot][datas.clock_dig[1][5 - typ]]);
      lc.setRow(0, 3, msymb[0][datas.clock_dig[1][4 - typ]]);
      lc.setRow(0, 4, msymb[dot][datas.clock_dig[1][3 - typ]]);
      lc.setRow(0, 5, msymb[0][datas.clock_dig[1][2 - typ]]);
      lc.setRow(0, 6, (type2 == 2) ? 0 : msymb[0][datas.clock_dig[1][1]]);
      lc.setRow(0, 7, (type2 == 2) ? 0 : msymb[0][datas.clock_dig[1][0]]);
    }
  }
  if((type1 == 2 || type1 == 3) && (type2 == 2 || type2 == 3)){
    uint8_t dot = (type1 == 2) ? (datas.dots == true) ? 1 : 0 : 0;
    uint8_t typ = (type1 == 2) ? 1 : 0;
    lc.setIntensity(0, bright);
    lc.setRow(0, 0, msymb[0][datas.clock_dig[0][7 - typ]]);
    lc.setRow(0, 1, msymb[0][datas.clock_dig[0][6 - typ]]);
    lc.setRow(0, 2, msymb[dot][datas.clock_dig[0][5 - typ]]);
    lc.setRow(0, 3, msymb[0][datas.clock_dig[0][4 - typ]]);
    lc.setRow(0, 4, msymb[dot][datas.clock_dig[0][3 - typ]]);
    lc.setRow(0, 5, msymb[0][datas.clock_dig[0][2 - typ]]);
    lc.setRow(0, 6, (type1 == 2) ? 0 : msymb[0][datas.clock_dig[0][1]]);
    lc.setRow(0, 7, (type1 == 2) ? 0 : msymb[0][datas.clock_dig[0][0]]);
    dot = (type2 == 2) ? (datas.dots2 == true) ? 1 : 0 : 0;
    typ = (type2 == 2) ? 1 : 0;
    lc.setIntensity(1, bright2);
    lc.setRow(1, 0, msymb[0][datas.clock_dig[1][7 - typ]]);
    lc.setRow(1, 1, msymb[0][datas.clock_dig[1][6 - typ]]);
    lc.setRow(1, 2, msymb[dot][datas.clock_dig[1][5 - typ]]);
    lc.setRow(1, 3, msymb[0][datas.clock_dig[1][4 - typ]]);
    lc.setRow(1, 4, msymb[dot][datas.clock_dig[1][3 - typ]]);
    lc.setRow(1, 5, msymb[0][datas.clock_dig[1][2 - typ]]);
    lc.setRow(1, 6, (type2 == 2) ? 0 : msymb[0][datas.clock_dig[1][1]]);
    lc.setRow(1, 7, (type2 == 2) ? 0 : msymb[0][datas.clock_dig[1][0]]);
  }
}

void ws2812b_fill(uint8_t type1, uint8_t type2){
  uint8_t bright = 25;
  if(datas.is_day) bright = config.day_bright;
  else bright = config.night_bright;
  uint8_t bright2 = 25;
  if(datas.is_day2) bright2 = config.day_bright2;
  else bright2 = config.night_bright2;
  if(bright < 1) bright = 1;
  if(bright > 100) bright = 100;
  if(bright2 < 1) bright2 = 1;
  if(bright2 > 100) bright2 = 100;
  if((type1 == 4 and type2 != 4) or (type1 != 4 and type2 == 4)){
    uint8_t disp = (type2 == 4) ? 1 : 0;
    uint8_t dots = (disp == 0) ? (datas.dots) ? 1 : 0 : (datas.dots2) ? 1 : 0;
    StaticJsonDocument<400> doc;
    JsonArray data = doc.createNestedArray("ws2812");
    data.add(datas.clock_dig[disp][2]);
    data.add(datas.clock_dig[disp][3]);
    data.add(datas.clock_dig[disp][4]);
    data.add(datas.clock_dig[disp][5]);
    data.add(dots);
    data.add(disp == 0 ? config.dc[datas.snum] : config.d2c[datas.snum2]);
    data.add(bright);
    data.add(0);
    data.add(0);
    data.add(0);
    data.add(0);
    data.add(0);
    data.add(0);
    data.add(0);
    serializeJson(doc, Serial);
    Serial.println();
  }
  if(type1 == 4 and type2 == 4){
    StaticJsonDocument<400> doc;
    JsonArray data = doc.createNestedArray("ws2812");
    data.add(datas.clock_dig[0][2]);
    data.add(datas.clock_dig[0][3]);
    data.add(datas.clock_dig[0][4]);
    data.add(datas.clock_dig[0][5]);
    data.add((datas.dots) ? 1 : 0);
    data.add(config.dc[datas.snum]);
    data.add(bright);
    data.add(datas.clock_dig[1][2]);
    data.add(datas.clock_dig[1][3]);
    data.add(datas.clock_dig[1][4]);
    data.add(datas.clock_dig[1][5]);
    data.add((datas.dots2) ? 1 : 0);
    data.add(config.d2c[datas.snum2]);
    data.add(bright2);
    serializeJson(doc, Serial);
    Serial.println();
  }
}

void display_fill(void){
  tmElements_t daymode = {0, config.md, config.hd, weekday(), day(), month(), year() - 1970};
  uint32_t fromT = makeTime(daymode);
  tmElements_t nightmode = {0, config.mn, config.hn, weekday(), day(), month(), year() - 1970};
  uint32_t toT = makeTime(nightmode);
  if(fromT <= now() and now() <= toT) datas.is_day = true;
  else datas.is_day = false;
  tmElements_t daymode2 = {0, config.md2, config.hd2, weekday(), day(), month(), year() - 1970};
  uint32_t fromT2 = makeTime(daymode2);
  tmElements_t nightmode2 = {0, config.mn2, config.hn2, weekday(), day(), month(), year() - 1970};
  uint32_t toT2 = makeTime(nightmode2);
  if(fromT2 <= now() and now() <= toT2) datas.is_day2 = true;
  else datas.is_day2 = false;
  if(config.disp == 0) tm1637_fill(0, 0);
  if(config.disp2 == 0) tm1637_fill(0, 1);
  if(config.disp == 1) tm1637_fill(1, 0);
  if(config.disp2 == 1) tm1637_fill(1, 1);
  if(config.disp == 2 or config.disp == 3 or config.disp2 == 2 or config.disp2 == 3) max7219_fill(config.disp, config.disp2);
  if(config.disp == 4 or config.disp2 == 4) ws2812b_fill(config.disp, config.disp2);
}

bool get_clock(bool dots, uint8_t displ){
  uint8_t hr = config.time_format ? hour() : hourFormat12();
  uint8_t hh = hr < 10 ? 0x0E : hr / 10;
  uint8_t hl = hr % 10;
  uint8_t mh = minute() / 10;
  uint8_t ml = minute() % 10;
  uint8_t sh = second() / 10;
  uint8_t sl = second() % 10;
  uint8_t disp_tp = (displ == 0) ? config.disp : config.disp2;
  datas.clock_dig[displ][0] = (disp_tp == 3) ? hh : 0x0E;
  datas.clock_dig[displ][1] = (disp_tp == 0 || disp_tp == 4) ? 0x0E : (disp_tp == 3) ? hl : hh;
  datas.clock_dig[displ][2] = (disp_tp == 0 || disp_tp == 4) ? hh : (disp_tp == 3) ? dots ? 0x10 : 0x0E : hl;
  datas.clock_dig[displ][3] = (disp_tp == 0 || disp_tp == 4) ? hl : mh;
  datas.clock_dig[displ][4] = (disp_tp == 0 || disp_tp == 4) ? mh : ml;
  datas.clock_dig[displ][5] = (disp_tp == 0 || disp_tp == 4) ? ml : (disp_tp == 3) ? dots ? 0x10 : 0x0E : sh;
  datas.clock_dig[displ][6] = (disp_tp == 0 || disp_tp == 4) ? 0x0E : (disp_tp == 3) ? sh : sl;
  datas.clock_dig[displ][7] = (disp_tp == 3) ? sl : 0x0E;
  return !dots;
}

bool get_date(uint8_t displ){
  uint8_t dh = day() / 10;
  uint8_t dl = day() % 10;
  uint8_t mh = month() / 10;
  uint8_t ml = month() % 10;
  uint8_t yh = (year() - 2000) / 10;
  uint8_t yl = (year() - 2000) % 10;
  uint8_t disp_tp = (displ == 0) ? config.disp : config.disp2;
  datas.clock_dig[displ][0] = (disp_tp == 3) ? dh : 0x0E;
  datas.clock_dig[displ][1] = (disp_tp == 0 || disp_tp == 4) ? 0x0E : (disp_tp == 3) ? dl : 0x0E;
  datas.clock_dig[displ][2] = (disp_tp == 0 || disp_tp == 4) ? dh : (disp_tp == 3) ? 0x10 : dh;
  datas.clock_dig[displ][3] = (disp_tp == 0 || disp_tp == 4) ? dl : (disp_tp == 3) ? mh : dl;
  datas.clock_dig[displ][4] = (disp_tp == 0 || disp_tp == 4) ? mh : (disp_tp == 3) ? ml : 0x10;
  datas.clock_dig[displ][5] = (disp_tp == 0 || disp_tp == 4) ? ml : (disp_tp == 3) ? 0x10 : mh;
  datas.clock_dig[displ][6] = (disp_tp == 0 || disp_tp == 4) ? 0x0E : (disp_tp == 3) ? yh : ml;
  datas.clock_dig[displ][7] = (disp_tp == 3) ? yl : 0x0E;
  return 0;
}

bool get_temp(float temperature, uint8_t displ){
  int t = round(temperature);
  datas.clock_dig[displ][0] = datas.clock_dig[displ][6] = datas.clock_dig[displ][7] = 0x0E;
  if(t > 99 or t < -50){
    datas.clock_dig[displ][1] = 0x0E;
    datas.clock_dig[displ][2] = datas.clock_dig[displ][3] = 0x10;
    datas.clock_dig[displ][4] = 0x0A;
    datas.clock_dig[displ][5] = 0x11;
  }
  else{
    uint8_t th = floor(abs(t) / 10);
    uint8_t tl = abs(t) % 10;
    if(th == 0) th = 0x0E;
    uint8_t disp_tp = (displ == 0) ? config.disp : config.disp2;
    datas.clock_dig[displ][1] = (disp_tp == 0 || disp_tp == 4) ? 0x0E : (t < 0) ? (t < -9) ? 0x10 : 0x0E : 0x0E;
    datas.clock_dig[displ][2] = (disp_tp == 0 || disp_tp == 4) ? (t < 0) ? 0x10 : th : (t < 0) ? (t < -9) ? th : 0x10 : th;
    datas.clock_dig[displ][3] = (disp_tp == 0 || disp_tp == 4) ? (t < 0) ? (t < -9) ? th : tl : tl : tl;
    datas.clock_dig[displ][4] = (disp_tp == 0 || disp_tp == 4) ? (t < 0) ? (t < -9) ? tl : 0x0A : 0x0A : 0x0A;
    datas.clock_dig[displ][5] = (disp_tp == 0 || disp_tp == 4) ? (t < 0) ? (t < -9) ? 0x0A : 0x11 : 0x11 : 0x11;
  }
  return 0;
}

bool get_hum(float humidity, uint8_t displ){
  uint8_t h = round(humidity);
  datas.clock_dig[displ][5] = 0x0D;
  datas.clock_dig[displ][0] = datas.clock_dig[displ][6] = datas.clock_dig[displ][7] = 0x0E;
  if(h > 100){
    datas.clock_dig[displ][1] = datas.clock_dig[displ][4] = 0x0E;
    datas.clock_dig[displ][2] = datas.clock_dig[displ][3] = 0x10;
  }
  else{
    uint8_t hh = h / 10;
    uint8_t hl = h % 10;
    if(hh == 0) hh = 0x0E;
    uint8_t disp_tp = (displ == 0) ? config.disp : config.disp2;
    datas.clock_dig[displ][1] = (disp_tp == 0 || disp_tp == 4) ? 0x0E : (h > 99) ? 1 : 0x0E;
    datas.clock_dig[displ][2] = (disp_tp == 0 || disp_tp == 4) ? (h > 99) ? 1 : hh : (h > 99) ? 0 : hh;
    datas.clock_dig[displ][3] = (disp_tp == 0 || disp_tp == 4) ? (h > 99) ? 0 : hl : (h > 99) ? 0 : hl;
    datas.clock_dig[displ][4] = (disp_tp == 0 || disp_tp == 4) ? (h > 99) ? 0 : 0x0E : 0x0E;
  }
  return 0;
}

bool get_pres(float pressure, uint8_t displ){
  uint16_t p = round(pressure);
  datas.clock_dig[displ][5] = 0x0C;
  datas.clock_dig[displ][0] = datas.clock_dig[displ][1] = datas.clock_dig[displ][6] = datas.clock_dig[displ][7] = 0x0E;
  if(p > 999){
    datas.clock_dig[displ][2] = datas.clock_dig[displ][3] = datas.clock_dig[displ][4] = 0x10;
  }
  else{
    datas.clock_dig[displ][2] = p / 100;
    datas.clock_dig[displ][3] = p % 100 / 10;
    datas.clock_dig[displ][4] = p % 10;
  }
  return 0;
}

void read_eeprom(void){
  File file = SPIFFS.open("/config.json", "r");
  while(file.available()){
    String json = file.readString();
    //Serial.println(json);
    DynamicJsonDocument conf(8192);
    DeserializationError error = deserializeJson(conf, json);
    if(!error){
      strlcpy(config.ssid, conf["ssid"] | config.ssid, sizeof(config.ssid));
      strlcpy(config.pass, conf["pass"] | config.pass, sizeof(config.pass));
      strlcpy(config.mask, conf["mask"] | config.mask, sizeof(config.mask));
      strlcpy(config.dns1, conf["dns1"] | config.dns1, sizeof(config.dns1));
      strlcpy(config.dns2, conf["dns2"] | config.dns2, sizeof(config.dns2));
      strlcpy(config.ip, conf["ip"] | config.ip, sizeof(config.ip));
      strlcpy(config.gw, conf["gw"] | config.gw, sizeof(config.gw));
      config.type = conf["type"].as<bool>() | config.type;
      strlcpy(config.apssid, conf["apssid"] | config.apssid, sizeof(config.apssid));
      strlcpy(config.appass, conf["appass"] | config.appass, sizeof(config.appass));
      strlcpy(config.apmask, conf["apmask"] | config.apmask, sizeof(config.apmask));
      strlcpy(config.apip, conf["apip"] | config.apip, sizeof(config.apip));
      config.chnl = conf["chnl"] | config.chnl;

      config.day_bright = conf["day_bright"] | config.day_bright;
      config.day_bright2 = conf["day_bright2"] | config.day_bright2;
      config.night_bright = conf["night_bright"] | config.night_bright;
      config.night_bright2 = conf["night_bright2"] | config.night_bright2;
      config.hd = conf["hd"] | config.hd;
      config.hd2 = conf["hd2"] | config.hd2;
      config.md = conf["md"] | config.md;
      config.md2 = conf["md2"] | config.md2;
      config.hn = conf["hn"] | config.hn;
      config.hn2 = conf["hn2"] | config.hn2;
      config.mn = conf["mn"] | config.mn;
      config.mn2 = conf["mn2"] | config.mn2;
      config.disp = conf["disp"] | config.disp;
      config.disp2 = conf["disp2"] | config.disp2;
      
      config.time_format = conf["time"].as<bool>() | config.time_format;
      config.utc = conf["utc"] | config.utc;
      config.daylight = conf["dlst"].as<bool>() | config.daylight;
      strlcpy(config.ntp, conf["ntp"] | config.ntp, sizeof(config.ntp));
      config.ntp_period = conf["ntp_period"] | config.ntp_period;

      sensors.bme280_temp_corr = conf["bmet"] | sensors.bme280_temp_corr;
      sensors.bmp180_temp_corr = conf["bmpt"] | sensors.bmp180_temp_corr;
      sensors.sht21_temp_corr = conf["shtt"] | sensors.sht21_temp_corr;
      sensors.dht22_temp_corr = conf["dhtt"] | sensors.dht22_temp_corr;
      sensors.bme280_hum_corr = conf["bmeh"] | sensors.bme280_hum_corr;
      sensors.sht21_hum_corr = conf["shth"] | sensors.sht21_hum_corr;
      sensors.dht22_hum_corr = conf["dhth"] | sensors.dht22_hum_corr;
      sensors.bme280_pres_corr = conf["bmep"] | sensors.bme280_pres_corr;
      sensors.bmp180_pres_corr = conf["bmpp"] | sensors.bmp180_pres_corr;
      sensors.ds18_temp_corr = conf["ds18t"] | sensors.ds18_temp_corr;
      sensors.ds32_temp_corr = conf["ds32t"] | sensors.ds32_temp_corr;
      
      config.tupd = conf["tupd"] | config.tupd;
      config.thngsend = conf["thngsnd"] | config.thngsend;
      config.thngrcv = conf["thngrcv"] | config.thngrcv;

      config.tf1 = conf["tf1"] | config.tf1;
      config.tf2 = conf["tf2"] | config.tf2;
      config.tf3 = conf["tf3"] | config.tf3;
      config.tf4 = conf["tf4"] | config.tf4;
      config.tf5 = conf["tf5"] | config.tf5;
      config.tf6 = conf["tf6"] | config.tf6;
      config.tf7 = conf["tf7"] | config.tf7;
      config.tf8 = conf["tf8"] | config.tf8;
      strlcpy(config.rdkey, conf["rdkey"] | config.rdkey, sizeof(config.rdkey));
      strlcpy(config.wrkey, conf["wrkey"] | config.wrkey, sizeof(config.wrkey));
      config.chid = conf["chid"] | config.chid;

      config.dp[0] = conf["dp0"] | config.dp[0];
      config.dp[1] = conf["dp1"] | config.dp[1];
      config.dp[2] = conf["dp2"] | config.dp[2];
      config.dp[3] = conf["dp3"] | config.dp[3];
      config.dp[4] = conf["dp4"] | config.dp[4];
      config.dp[5] = conf["dp5"] | config.dp[5];
      config.dt[0] = conf["dt0"] | config.dt[0];
      config.dt[1] = conf["dt1"] | config.dt[1];
      config.dt[2] = conf["dt2"] | config.dt[2];
      config.dt[3] = conf["dt3"] | config.dt[3];
      config.dt[4] = conf["dt4"] | config.dt[4];
      config.dt[5] = conf["dt5"] | config.dt[5];
      strlcpy(config.dc[0], conf["dc0"] | config.dc[0], sizeof(config.dc[0]));
      strlcpy(config.dc[1], conf["dc1"] | config.dc[1], sizeof(config.dc[1]));
      strlcpy(config.dc[2], conf["dc2"] | config.dc[2], sizeof(config.dc[2]));
      strlcpy(config.dc[3], conf["dc3"] | config.dc[3], sizeof(config.dc[3]));
      strlcpy(config.dc[4], conf["dc4"] | config.dc[4], sizeof(config.dc[4]));
      strlcpy(config.dc[5], conf["dc5"] | config.dc[5], sizeof(config.dc[5]));
      strlcpy(config.ds[0], conf["ds0"] | config.ds[0], sizeof(config.ds[0]));
      strlcpy(config.ds[1], conf["ds1"] | config.ds[1], sizeof(config.ds[1]));
      strlcpy(config.ds[2], conf["ds2"] | config.ds[2], sizeof(config.ds[2]));
      strlcpy(config.ds[3], conf["ds3"] | config.ds[3], sizeof(config.ds[3]));
      strlcpy(config.ds[4], conf["ds4"] | config.ds[4], sizeof(config.ds[4]));
      strlcpy(config.ds[5], conf["ds5"] | config.ds[5], sizeof(config.ds[5]));
      config.d2p[0] = conf["d2p0"] | config.d2p[0];
      config.d2p[1] = conf["d2p1"] | config.d2p[1];
      config.d2p[2] = conf["d2p2"] | config.d2p[2];
      config.d2p[3] = conf["d2p3"] | config.d2p[3];
      config.d2p[4] = conf["d2p4"] | config.d2p[4];
      config.d2p[5] = conf["d2p5"] | config.d2p[5];
      config.d2t[0] = conf["d2t0"] | config.d2t[0];
      config.d2t[1] = conf["d2t1"] | config.d2t[1];
      config.d2t[2] = conf["d2t2"] | config.d2t[2];
      config.d2t[3] = conf["d2t3"] | config.d2t[3];
      config.d2t[4] = conf["d2t4"] | config.d2t[4];
      config.d2t[5] = conf["d2t5"] | config.d2t[5];
      strlcpy(config.d2c[0], conf["d2c0"] | config.d2c[0], sizeof(config.d2c[0]));
      strlcpy(config.d2c[1], conf["d2c1"] | config.d2c[1], sizeof(config.d2c[1]));
      strlcpy(config.d2c[2], conf["d2c2"] | config.d2c[2], sizeof(config.d2c[2]));
      strlcpy(config.d2c[3], conf["d2c3"] | config.d2c[3], sizeof(config.d2c[3]));
      strlcpy(config.d2c[4], conf["d2c4"] | config.d2c[4], sizeof(config.d2c[4]));
      strlcpy(config.d2c[5], conf["d2c5"] | config.d2c[5], sizeof(config.d2c[5]));
      strlcpy(config.d2s[0], conf["d2s0"] | config.d2s[0], sizeof(config.d2s[0]));
      strlcpy(config.d2s[1], conf["d2s1"] | config.d2s[1], sizeof(config.d2s[1]));
      strlcpy(config.d2s[2], conf["d2s2"] | config.d2s[2], sizeof(config.d2s[2]));
      strlcpy(config.d2s[3], conf["d2s3"] | config.d2s[3], sizeof(config.d2s[3]));
      strlcpy(config.d2s[4], conf["d2s4"] | config.d2s[4], sizeof(config.d2s[4]));
      strlcpy(config.d2s[5], conf["d2s5"] | config.d2s[5], sizeof(config.d2s[5]));

      config.provider = conf["provider"] | config.provider;
      config.citysearch = conf["citysearch"] | config.citysearch;
      strlcpy(config.city, conf["city"] | config.city, sizeof(config.city));
      strlcpy(config.lat, conf["lat"] | config.lat, sizeof(config.lat));
      strlcpy(config.lon, conf["lon"] | config.lon, sizeof(config.lon));
      strlcpy(config.cityid, conf["cityid"] | config.cityid, sizeof(config.cityid));
      strlcpy(config.appid, conf["appid"] | config.appid, sizeof(config.appid));
      strlcpy(config.appkey, conf["appkey"] | config.appkey, sizeof(config.appkey));
    }
  }
  
  file = SPIFFS.open("/user.us", "r");
  while(file.available()){
    String json = file.readString();
    DynamicJsonDocument conf(128);
    DeserializationError error = deserializeJson(conf, json);
    if(!error){
      strlcpy(config.username, conf["user"] | config.username, sizeof(config.username));
      strlcpy(config.password, conf["pass"] | config.password, sizeof(config.password));
    }
  }
}

void apMode(WiFiMode_t amode){
  if(datas.ap_mode == false){
    WiFi.mode(amode);
    WiFi.softAP(config.apssid, config.appass, config.chnl);
    datas.ap_mode = true;
  }
}

void connectToWiFi(void){
  if(config.ssid != ""){
    uint8_t i = 0;
    WiFi.begin(config.ssid, config.pass);
    while(WiFi.status() != WL_CONNECTED){
      delay(500);
      if(i++ > 20) break;
    }
    if(WiFi.status() == WL_CONNECTED){
      datas.ap_mode = false;
      WiFi.setAutoConnect(true);
      WiFi.setAutoReconnect(true);
      if(config.type){
        IPAddress ip;
        IPAddress subnet;
        IPAddress gateway;
        IPAddress dns1;
        IPAddress dns2;
        if(ip.fromString(config.ip) and
           gateway.fromString(config.gw) and
           subnet.fromString(config.mask) and
           dns1.fromString(config.dns1) and
           dns2.fromString(config.dns2)
        ) WiFi.config(ip, gateway, subnet, dns1, dns2);
      }
      Serial.print("Connected to: "); Serial.println(WiFi.SSID());
      Serial.print("IP address: "); Serial.println(WiFi.localIP());
    }
  }
  else apMode(WIFI_AP);
}

void sensors_init(void){
    //BME280
  sensors.bme280_det = bme.begin(0x76, &Wire);
  if(!sensors.bme280_det) sensors.bme280_det = bme.begin(0x77, &Wire);
    //BMP180
  if(bmp.begin()) sensors.bmp180_det = true;
    //SHT21;
  SHT21.begin();
  Wire.beginTransmission(SHT21_ADDRESS);
  Wire.write(0xE7);
  Wire.endTransmission();
  delay(100);
  Wire.requestFrom(SHT21_ADDRESS, 1);
  if(Wire.available() == 1){
    Wire.read();
    sensors.sht21_det = true;
  }
    //DHT22
  dht.setup(dhtPin, DHTesp::DHT22);
  dht.getTempAndHumidity();
  dht.getStatus();
  if(dht.getStatus() == 0) sensors.dht22_det = true;
    //DS18B20
  term.begin();
  sensors.ds18_det = term.getDeviceCount();
  if(sensors.ds18_det > 0){
    term.getAddress(thermometer, 0);
    term.setResolution(thermometer, 10);
    term.requestTemperatures();
  }
    //DS3231
  bool a, b;
  byte c = Clock.getHour(a, b);
  if(c >= 0 and c <= 23){
    sensors.ds32_det = true;
    bool a, b, c;
    setTime(
      Clock.getHour(a, b), 
      Clock.getMinute(),
      Clock.getSecond(), 
      Clock.getDate(), 
      Clock.getMonth(c), 
      Clock.getYear() + 2000
    );
  }
   //
  Serial.printf("%s %s%s\r\n", "DS3231", sensors.ds32_det ? "" : "not ", "detected");
  Serial.printf("%s %s%s\r\n", "BME280", sensors.bme280_det ? "" : "not ", "detected");
  Serial.printf("%s %s%s\r\n", "BMP180", sensors.bmp180_det ? "" : "not ", "detected");
  Serial.printf("%s %s%s\r\n", "DS18B20", sensors.ds18_det ? "" : "not ", "detected");
  Serial.printf("%s %s%s\r\n", "SHT21", sensors.sht21_det ? "" : "not ", "detected");
  Serial.printf("%s %s%s\r\n", "DHT22", sensors.dht22_det ? "" : "not ", "detected");
}

void sensors_read(void){
/////////// Temperature //////////////////////////////////
  if(sensors.bme280_det){
    sensors_event_t temp_event;
    bme_temp -> getEvent(&temp_event);
    sensors.bme280_temp = temp_event.temperature + sensors.bme280_temp_corr;
  }
  if(sensors.bmp180_det) sensors.bmp180_temp = bmp.readTemperature() + sensors.bmp180_temp_corr;
  if(sensors.sht21_det) sensors.sht21_temp = SHT21.getTemperature() + sensors.sht21_temp_corr;
  if(sensors.dht22_det) sensors.dht22_temp = dht.getTemperature() + sensors.dht22_temp_corr;
  if(sensors.ds18_det){ sensors.ds18_temp = term.getTempC(thermometer) + sensors.ds18_temp_corr; term.requestTemperatures();}
  if(sensors.ds32_det) sensors.ds32_temp = Clock.getTemperature() + sensors.ds32_temp_corr;
/////////// Humidity //////////////////////////////////
  if(sensors.bme280_det){
    sensors_event_t humidity_event;
    bme_humidity -> getEvent(&humidity_event);
    sensors.bme280_hum = humidity_event.relative_humidity + sensors.bme280_hum_corr;
  }
  if(sensors.sht21_det) sensors.sht21_hum = SHT21.getHumidity() + sensors.sht21_hum_corr;
  if(sensors.dht22_det) sensors.dht22_hum = dht.getHumidity() + sensors.dht22_hum_corr;
////////// Pressure /////////////////////////////////////
  if(sensors.bme280_det){
    sensors_event_t pressure_event;
    bme_pressure -> getEvent(&pressure_event);
    sensors.bme280_pres = pressure_event.pressure + sensors.bme280_pres_corr;
    sensors.bme280_pres *= 0.75;
  }
  if(sensors.bmp180_det){
    sensors.bmp180_pres = bmp.readPressure() / 100 + sensors.bmp180_pres_corr;
    sensors.bmp180_pres *= 0.75;
  }
}
