/* Clock BIM v1.0
 * © Alexandru Piteli himikat123@gmail.com, Nürnberg, Deutschland, 2019 
 * http://esp8266.atwebpages.com/?p=clock
 */
                               // Board: Generic ESP8266 Module 
                               // 1MB (256kB SPIFFS)
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "TM1637.h"
#include <Time.h>
#include <NtpClientLib.h>
#include <FS.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>
#include <ESP8266TrueRandom.h>
#include <Wire.h>
#include "BlueDot_BME280.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "SHT21.h"

#define site "http://esp8266.atwebpages.com/api/"
#define CLK         14
#define DIO         12
#define ONE_WIRE_BUS 2
#define BUTTON       0

extern "C"{
  #include "clock.h"
  #include "languages.h"
}

ESP8266WebServer webServer(80);
TM1637 tm1637(CLK,DIO);
Ticker half_sec;
ntpClient *ntp;
BlueDot_BME280 bme1;
BlueDot_BME280 bme2;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress thermometer;
SHT21 SHT21;

int8_t TimeDisp[]={0,0,0,0};
bool clockPoint=true;
bool upd=true;

void half(){
  upd=true;
  clockPoint=!clockPoint;
  sec--;
}

void setup(){
    //Display
  tm1637.init();
  tm1637.set(5);
  tm1637.display(TimeDisp);
  tm1637.set(5);
    //Serial port
  Serial.begin(74880);
  while(!Serial);
    //SPIFS
  if(!SPIFFS.begin()) while(1) yield();
    //PINs
  pinMode(BUTTON,INPUT);
    //WIFI MODE
  WiFi.mode(WIFI_STA);
    //EEPROM 
  read_eeprom();
    //Ticker
  half_sec.attach_ms(500,half);
    //Settings
  is_settings();
  web_settings();
    //sensors
  sensors_init();
    //NTP
  ntp=ntpClient::getInstance("time.windows.com",1);
  ntp->setInterval(15,1800);
  ntp->setTimeZone(html.zone);
  ntp->setDayLight(html.adj);
  ntp->begin();
  siteTime();
    //MDNS
  html.mdns.toCharArray(text_buf,(html.mdns.length())+1);
  MDNS.begin(text_buf);
  MDNS.addService("http","tcp",80);
}
  
void loop(){
  if(upd){
    if(WiFi.status()!=WL_CONNECTED) connectToWiFi();
    if(sec-html.duration*2<=0){
      if(sec>html.duration){
        bool t=get_temp();
        d_temp(round(temp),t);
      }
      else{
        bool h=get_humidity();
        d_hum(round(hum),h);
      }
      if(sec<=0) sec=html.duration*2+html.every*2;
    }
    else{
      d_time();
    }
    upd=false;
    tm1637.display(TimeDisp);
    tmElements_t from={0,html.fm,html.fh,weekday(),day(),month(),year()-1970};
    uint32_t fromT=makeTime(from);
    tmElements_t to={0,html.tm,html.th,weekday(),day(),month(),year()-1970};
    uint32_t toT=makeTime(to);
    if(fromT<=now() and now()<=toT) tm1637.set(html.bright);
    else tm1637.set(html.bright_n);
    is_settings();
  }
  else webServer.handleClient();
}

void d_temp(uint8_t t,bool e){
  if(e) d_time();
  else{
    tm1637.point(POINT_OFF);
    if(html.ti_units) t=t*1.8+32;
    TimeDisp[0]=html.ti_units?t>99?t/100%10:t/10%10:t/10%10;
    TimeDisp[1]=html.ti_units?t>99?t/10%10:t%10:t%10;
    TimeDisp[2]=html.ti_units?t>99?t%10:0x0A:0x0A;
    TimeDisp[3]=html.ti_units?0x0F:0x0C;
  }
}

void d_hum(uint8_t h,bool e){
  if(e) d_time();
  else{
    tm1637.point(POINT_OFF);
    TimeDisp[0]=h/10%10;
    TimeDisp[1]=h%10;
    TimeDisp[2]=0x0E;
    TimeDisp[3]=0x0D;
  }
}

void d_time(){
  if(clockPoint) tm1637.point(POINT_ON);
  else tm1637.point(POINT_OFF);
  TimeDisp[0]=html.timef?hour()<10?0x0E:hour()/10:hourFormat12()<10?0x0E:hourFormat12()/10;
  TimeDisp[1]=html.timef?hour()%10:hourFormat12()%10;
  TimeDisp[2]=minute()/10;
  TimeDisp[3]=minute()%10;  
}

void read_eeprom(){
  String fileData;
  File file=SPIFFS.open("/save/save.json","r");
  if(file){
    fileData=file.readString();
    file.close();
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root=jsonBuffer.parseObject(fileData);
    if(root.success()){
      String ap_ssid=root["APSSID"];  
      String ap_pass=root["APPASS"];
      String ap_ip  =root["APIP"];
      String ap_mask=root["APMASK"];
      html.chnl     =root["CHNL"];
      html.hide     =root["HIDE"];
      html.zone     =root["ZONE"];
      html.bright   =root["BRIGHT"];
      html.bright_n =root["BRIGHT_N"];
      html.adj      =root["DAYLIGHT"];
      html.timef    =root["TIME"];
      html.lang     =root["LANG"];
      html.typ      =root["TYPE"];
      html.temp     =root["TEMP"];
      html.hum      =root["HUM"];
      html.t_cor    =root["T_COR"];
      html.h_cor    =root["H_COR"];
      html.ti_units =root["TI_UNITS"];
      String ip     =root["IP"];
      String mask   =root["MASK"];
      String gw     =root["GATEWAY"];
      String dns1   =root["DNS1"];
      String dns2   =root["DNS2"];
      String mdns   =root["MDNS"];
      html.fh       =root["FH"];
      html.fm       =root["FM"];
      html.th       =root["TH"];
      html.tm       =root["TM"];
      html.every    =root["EVERY"];
      html.duration =root["DURAT"];
      html.ip       =ip;
      html.mask     =mask;
      html.gateway  =gw;
      html.dns1     =dns1;
      html.dns2     =dns2;
      sec           =html.every;
      if(ap_ssid!="") ap_ssid.toCharArray(html.ap_ssid,(ap_ssid.length())+1);
      if(ap_pass!="") ap_pass.toCharArray(html.ap_pass,(ap_pass.length())+1);
      if(ap_ip!="") html.ap_ip=ap_ip;
      if(ap_mask!="") html.ap_mask=ap_mask;
      if(mdns!="") html.mdns=mdns;
    }
  }
  
  File f=SPIFFS.open("/save/ssids.json","r");
  if(f){
    String fData=f.readString();
    f.close();
    DynamicJsonBuffer jsonBuf;
    JsonObject& json=jsonBuf.parseObject(fData);
    if(json.success()){
      ssids.num=json["num"];
      for(uint8_t i=0;i<ssids.num;i++){
        ssids.ssid[i]=json["nets"][i*2].as<String>();
        ssids.pass[i]=json["nets"][i*2+1].as<String>();
      }
    }
  }
}

void is_settings(void){
  if(!digitalRead(BUTTON)){
    Serial.println("*********************************");
    Serial.println("entering settings mode");
    TimeDisp[0]=0x05;
    TimeDisp[1]=0x0B;
    TimeDisp[2]=0x0F;
    TimeDisp[3]=0x0F;
    tm1637.display(TimeDisp);
    IPAddress ip;
    IPAddress subnet;
    IPAddress gateway;
    if(ip.fromString(html.ap_ip) and gateway.fromString(html.ap_ip) and subnet.fromString(html.ap_mask)){
      WiFi.softAPConfig(ip,gateway,subnet);
    }
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(html.ap_ssid,html.ap_pass,html.chnl,html.hide);
    Serial.printf("connect to %s, password is %s\r\n",html.ap_ssid,html.ap_pass);
    Serial.println("type "+html.ap_ip+" in address bar of your browser");
    String IP=WiFi.localIP().toString();
    Serial.print("alt IP is "); Serial.println(IP);
    if(IP=="0.0.0.0") WiFi.disconnect();
    web_settings();
    while(1){
      webServer.handleClient();
      yield();
    }
  }
}

void connectToWiFi(void){
  is_settings();
  if(!ssids.num){
    TimeDisp[0]=0x05;
    TimeDisp[1]=0x0B;
    TimeDisp[2]=0x0F;
    TimeDisp[3]=0x0F;
    tm1637.display(TimeDisp);
    Serial.print("Creating network \""); Serial.print(html.ap_ssid);
    Serial.print("\" with password \""); Serial.print(html.ap_pass); Serial.println("\"");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(html.ap_ssid,html.ap_pass,html.chnl,html.hide);
    String IP=WiFi.localIP().toString();
    if(IP=="0.0.0.0") WiFi.disconnect();
    web_settings();
    html.mdns.toCharArray(text_buf,(html.mdns.length())+1);
    MDNS.begin(text_buf);
    MDNS.addService("http","tcp",80);
    while(1){
      webServer.handleClient();
      yield();
    }
  }
  else{
    if(WiFi.status()!=WL_CONNECTED){
      uint8_t n=WiFi.scanNetworks();
      if(n!=0){
        for(uint8_t i=0;i<n;i++){
          for(uint8_t k=0;k<ssids.num;k++){
            delay(1);
            if(WiFi.SSID(i)==ssids.ssid[k]){
              rssi=WiFi.RSSI(i);
              ssids.ssid[k].toCharArray(ssid,(ssids.ssid[k].length())+1);
              ssids.pass[k].toCharArray(password,(ssids.pass[k].length())+1);
              WiFi.begin(ssid,password);
              Serial.print("Connecting to "); Serial.println(ssid);
              break;
            }
          }
        }
      }
      uint8_t e=0;
      while(WiFi.status()!=WL_CONNECTED){
        if((e++)>20){
          for(uint8_t k=0;k<ssids.num;k++){
            delay(1);
            ssids.ssid[k].toCharArray(ssid,(ssids.ssid[k].length())+1);
            ssids.pass[k].toCharArray(password,(ssids.pass[k].length())+1);
            WiFi.begin(ssid,password);
            Serial.print("Connecting to "); Serial.println(ssid);
            if(WiFi.status()==WL_CONNECTED) goto connectedd;
          }
          Serial.print("Unable to connect to "); Serial.println(ssid);
          is_settings();
          delay(20000);
        }
        delay(500);
        is_settings();
      }
    }
connectedd:
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    if(html.typ==1){
      IPAddress ip;
      IPAddress subnet;
      IPAddress gateway;
      IPAddress dns1;
      IPAddress dns2;
      if(ip.fromString(html.ip) and
         gateway.fromString(html.gateway) and
         subnet.fromString(html.mask) and
         dns1.fromString(html.dns1) and
         dns2.fromString(html.dns2)){
        WiFi.config(ip,gateway,subnet,dns1,dns2);
      }
    }
    rssi=viewRSSI(String(WiFi.SSID()));
  }
  WiFi.SSID().toCharArray(ssid,(WiFi.SSID().length())+1);
  Serial.print("\r\nConnected to \""); Serial.print(ssid); Serial.println("\"");
  html.mdns.toCharArray(text_buf,(html.mdns.length())+1);
  Serial.print("mDNS name is "); Serial.println(text_buf);
  MDNS.begin(text_buf);
  MDNS.addService("http","tcp",80);
  Serial.println(WiFi.localIP());
}

int viewRSSI(String s){
  uint8_t n=WiFi.scanNetworks();
  int rssi=0;
  if(n!=0){
    for(uint8_t i=0;i<n;i++){
      if(WiFi.SSID(i)==s) rssi=WiFi.RSSI(i);
    }  
  }
  return rssi;
}

boolean summertime(){
  if(month()<3||month()>10) return false;
  if(month()>3&&month()<10) return true;
  if(month()==3&&(hour()+24*day())>=(1+24*(31-(5*year()/4+4)%7))||month()==10&&(hour()+24*day())<(1+24*(31-(5*year()/4+1)%7))) return true;
  else return false;
}

void siteTime(){
  String url=site; 
  url+="get_time.php?d=clock&m=";
  url+=WiFi.macAddress();
  url+="&f="; url+=fw;
  url+="&l="; url+=html.lang;
  HTTPClient client;
  client.begin(url);
  int httpCode=client.GET();
  if(httpCode==HTTP_CODE_OK){
    httpData=client.getString();
    char stamp[12];
    httpData.toCharArray(stamp,12);
    int dayLight=0;
    if(summertime() and html.adj) dayLight=3600;
    setTime(atol(stamp)+(html.zone*3600)+dayLight);
  }
  httpData="";
}

void sensors_init(void){
    //BME280
  bme1.parameter.communication=0;
  bme2.parameter.communication=0;
  bme1.parameter.I2CAddress=0x77;
  bme2.parameter.I2CAddress=0x76;
  bme1.parameter.sensorMode=0b11;
  bme2.parameter.sensorMode=0b11;
  bme1.parameter.IIRfilter=0b100;
  bme2.parameter.IIRfilter=0b100;
  bme1.parameter.humidOversampling=0b101;
  bme2.parameter.humidOversampling=0b101;
  bme1.parameter.tempOversampling=0b101;
  bme2.parameter.tempOversampling=0b101;
  bme1.parameter.pressOversampling=0b101;
  bme2.parameter.pressOversampling=0b101;
  bme1.parameter.pressureSeaLevel=1013.25;
  bme2.parameter.pressureSeaLevel=1013.25;
  bme1.parameter.tempOutsideCelsius=15;
  bme2.parameter.tempOutsideCelsius=15;
  bme1.parameter.tempOutsideFahrenheit=59;
  bme2.parameter.tempOutsideFahrenheit=59;
  if(bme1.init()==0x60) bme1Detected=true;
  if(bme2.init()==0x60) bme2Detected=true;
    //DS18B20
  sensors.begin();
  dsDetected=sensors.getDeviceCount();
  if(dsDetected){
    sensors.getAddress(thermometer,0);
    sensors.setResolution(thermometer,10);
    sensors.requestTemperatures();
  }
    //SHT21;
  SHT21.begin();
  Wire.beginTransmission(SHT21_ADDRESS);
  Wire.write(0xE7);
  Wire.endTransmission();
  delay(100);
  Wire.requestFrom(SHT21_ADDRESS,1);
  if(Wire.available()==1){
    Wire.read();
    shtDetected=true;
  }
  if(bme1Detected) Serial.println("BME1 detected");
  else Serial.println("BME1 not detected");
  if(bme2Detected) Serial.println("BME2 detected");
  else Serial.println("BME2 not detected");
  if(dsDetected) Serial.println("DS18 detected");
  else Serial.println("DS18 not detected");
  if(shtDetected) Serial.println("SHT detected");
  else Serial.println("SHT not detected");
}

bool get_temp(void){
  bool err=true; 
  if(html.temp==1){
    if(bme1Detected){
      temp=bme1.readTempC(); 
      err=false;
    }
    if(bme2Detected){
      temp=bme2.readTempC(); 
      err=false;
    }
  }
  if(html.temp==2){
    if(shtDetected){
      temp=SHT21.getTemperature();
      err=false; 
    }
  }
  if(html.temp==3){
    if(dsDetected){
      temp=sensors.getTempC(thermometer);
      err=false;
    }
    sensors.requestTemperatures();
  }
  temp+=html.t_cor;
  return err; 
}

bool get_humidity(void){
  bool err=true;
  if(html.hum==1){
    if(bme1Detected){
      hum=bme1.readHumidity();
      err=false;
    }
    if(bme2Detected){
      hum=bme2.readHumidity();
      err=false;
    }
  }
  if(html.hum==2){
    if(shtDetected){
      hum=SHT21.getHumidity();
      err=false;
    }
  }
  hum+=html.h_cor;
  return err;
}
