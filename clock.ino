/* Clock BIM v2.1
 * © himikat123@gmail.com, Nürnberg, Deutschland, 2019 
 * https://github.com/himikat123/Clock
 */
                               // Board: Generic ESP8266 Module 
                               // 1MB (256kB SPIFFS)
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "TM1637_6D.h"
#include "LedControl.h"
#include <Time.h>
#include <NtpClientLib.h>
#include <DS3231.h>
#include <FS.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>
#include <ESP8266TrueRandom.h>
#include <Wire.h>
#include "BlueDot_BME280.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "SHT21.h"

#define CLK             14
#define DIO             12
#define DAT             13
#define CLOCK           15
#define LOAD             4
#define ONE_WIRE_BUS     5
#define BUTTON           0
#define SDA              2
#define SCL              0
#define CLOCK_ADDRESS 0x68

extern "C"{
  #include "clock.h"
  #include "languages.h"
}

ESP8266WebServer  webServer(80);
TM1637_6D         tm1637_6D(CLK,DIO);
Ticker            half_sec;
WiFiUDP           UDPNTPClient;
DS3231            Clock;
BlueDot_BME280    bme1;
BlueDot_BME280    bme2;
OneWire           oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress     thermometer;
SHT21             SHT21;
LedControl        lc=LedControl(DAT,CLOCK,LOAD,1);

void half(){
  upd=true;
  clockPoint=!clockPoint;
  bool a,b;
  uint8_t hr=(ds3231Detected)?Clock.getHour(a,b):hour();
  uint8_t mn=(ds3231Detected)?Clock.getMinute():minute();
  uint8_t sc=(ds3231Detected)?Clock.getSecond():second();
  if(--sec<=config.duration*2){
    if(config.temp>0){
      if(config.hum>0){
        if(config.pres>0) display_pres(round(pres));
        else display_time(hr,mn,sc);
      }
      else display_time(hr,mn,sc);
    }
    else display_time(hr,mn,sc);
  }
  else if(sec<=config.duration*4){
    if(config.temp>0){
      if(config.hum>0) display_hum(hum);
      else if(config.pres>0) display_pres(round(pres));
      else display_time(hr,mn,sc);
    }
    else if(config.hum>0){
      if(config.pres>0) display_pres(round(pres));
      else display_time(hr,mn,sc);
    }
    else display_time(hr,mn,sc); 
  }
  else if(sec<=config.duration*6){
    if(config.temp>0) display_temp(temp);
    else if(config.hum>0) display_hum(hum);
    else if(config.pres>0) display_pres(round(pres));
    else display_time(hr,mn,sc); 
  }
  else{
    display_time(hr,mn,sc);
  }
  if(config.every==60){
    if(sc==10){
      sec=120; 
      sens_upd=true;
    }
  }
  else{
    if(sec<=0){
      sec=config.every*2;
      sens_upd=true;
    }
  }
  if(config.disp==0 or config.disp==1) tm1637_6D.display(TimeDisp,DispPoint);
  if((now()-lastsync)<config.ntp_period and net_connected) ntpsync=true;
  else ntpsync=false;
}

void setup(){
    //Serial port
  Serial.begin(74880);
  while(!Serial);
    //PINs
  pinMode(BUTTON,INPUT);
  pinMode(ONE_WIRE_BUS,INPUT);
  pinMode(SDA,INPUT);
    //i2c
  Wire.pins(SDA,SCL);
    //SPIFS
  if(!SPIFFS.begin()) while(1) yield();
    //EEPROM 
  read_eeprom();
    //sensors
  sensors_init();
    //Get sensors
  out();
  tempS=get_temp();
  humS=get_humidity();
  presS=get_pres();
    //Ticker
  half_sec.attach_ms(500,half);
    //Display
  TimeDisp[0]=0x10;
  TimeDisp[1]=0x10;
  TimeDisp[2]=0x10;
  TimeDisp[3]=0x10;
  TimeDisp[4]=0x10;
  TimeDisp[5]=0x10;
  DispPoint[0]=0;
  DispPoint[1]=0;
  DispPoint[2]=0;
  DispPoint[3]=0;
  DispPoint[4]=0;
  DispPoint[5]=0;
  if(config.disp==0 or config.disp==1){
    tm1637_6D.init();
    tm1637_6D.set(4);
    tm1637_6D.display(TimeDisp,DispPoint);
  }
  if(config.disp==2 or config.disp==3){
    lc.shutdown(0,false);
    lc.setIntensity(0,8);
    lc.clearDisplay(0);
    lc.setChar(0,7,'-',false);
    lc.setChar(0,6,'-',false);
    lc.setChar(0,5,'-',false);
    lc.setChar(0,4,'-',false);
    lc.setChar(0,3,'-',false);
    lc.setChar(0,2,'-',false);
    lc.setChar(0,1,'-',false);
    lc.setChar(0,0,'-',false);
  }
    //Time init
  if(ds3231Detected){
    bool a,b,c;
    setTime(
      Clock.getHour(a,b),
      Clock.getMinute(),
      Clock.getSecond(),
      Clock.getDate(), 
      Clock.getMonth(c),
      Clock.getYear()
    );
  }
    //WIFI MODE
  WiFi.mode(WIFI_STA);
  if(WiFi.status()!=WL_CONNECTED) connectToWiFi();
    //Settings
  is_settings();
  web_settings();
    //NTP
  siteTime();
  getNTPtime();
    //MDNS
  config.mdns.toCharArray(text_buf,(config.mdns.length())+1);
  MDNS.begin(text_buf);
  MDNS.addService("http","tcp",80);
}

  
void loop(){
  if(sens_upd){
    tempS=get_temp();
    humS=get_humidity();
    presS=get_pres();
    sens_upd=false;
  } 
  if(upd){
    if(WiFi.status()!=WL_CONNECTED){
      net_connected=false;
      connectToWiFi();
      showTime=true;
    }
    else{
      net_connected=true;
      if(ds3231Detected and ntpsync){
        bool a,b,c;
        if(
          Clock.getYear()!=year() or
          Clock.getMonth(c)!=month() or
          Clock.getDate()!=day() or
          Clock.getHour(a,b)!=hour() or
          Clock.getMinute()!=minute() or
          Clock.getSecond()!=second()
          ){
          Clock.setYear(year());
          Clock.setMonth(month());
          Clock.setDate(day());
          Clock.setHour(hour());
          Clock.setMinute(minute());
          Clock.setSecond(second());
        }
      }
      if(!ntpsync and ntt>16) ntt=16;
      if(ntt--<=0){
        getNTPtime();
        ntt=config.ntp_period;
      }
    }
    upd=false;
    tmElements_t from={0,config.fm,config.fh,weekday(),day(),month(),year()-1970};
    uint32_t fromT=makeTime(from);
    tmElements_t to={0,config.tm,config.th,weekday(),day(),month(),year()-1970};
    uint32_t toT=makeTime(to);
    if(fromT<=now() and now()<=toT){
      if(config.disp==0 or config.disp==1) tm1637_6D.set(round(config.bright/2));
      if(config.disp==2 or config.disp==3) lc.setIntensity(0,config.bright);
    }
    else{
      if(config.disp==0 or config.disp==1) tm1637_6D.set(round(config.bright_n/2));
      if(config.disp==2 or config.disp==3) lc.setIntensity(0,config.bright_n);
    }
    is_settings();
    if(oc++>=300){
      out();
      oc=0;
    }
  }
  else webServer.handleClient();
}

void getNTPtime(){
  unsigned long _unixTime=0;
  if(WiFi.status()==WL_CONNECTED){
    UDPNTPClient.begin(2390);
    IPAddress timeServerIP;
    WiFi.hostByName(config.ntp.c_str(),timeServerIP);
    memset(packetBuffer,0,48);
    packetBuffer[0]=0b11100011;
    packetBuffer[1]=0;
    packetBuffer[2]=6;
    packetBuffer[3]=0xEC;
    packetBuffer[12]=49;
    packetBuffer[13]=0x4E;
    packetBuffer[14]=49;
    packetBuffer[15]=52;
    UDPNTPClient.beginPacket(timeServerIP,123);
    UDPNTPClient.write(packetBuffer,48);
    UDPNTPClient.endPacket();
    delay(100);
    int cb=UDPNTPClient.parsePacket();
    if(cb==0) Serial.println("No NTP packet yet");
    else{
      Serial.print("NTP packet received, length=");
      Serial.println(cb);
      UDPNTPClient.read(packetBuffer,48);
      unsigned long highWord=word(packetBuffer[40],packetBuffer[41]);
      unsigned long lowWord=word(packetBuffer[42],packetBuffer[43]);
      unsigned long secsSince1900=highWord<<16|lowWord;
      const unsigned long seventyYears=2208988800UL;
      _unixTime=secsSince1900-seventyYears;
    }
  } 
  else delay(500);
  yield();
  if(_unixTime>0 and _unixTime<2000000000){
    UnixTimestamp=_unixTime;
    setTime(UnixTimestamp);
    int dayLight=0;
    if(summertime() and config.adj) dayLight=3600;
    setTime(UnixTimestamp+(config.zone*3600)+dayLight);
    lastsync=now();
  }
  Serial.println(UnixTimestamp); 
}

void display_temp(float t){
  String ts=String(t);
  char buf[8];
  ts.toCharArray(buf,8);
  String integ=strtok(buf,".");
  String fract=strtok(NULL,".");
  uint8_t frct=int(fract[0])-0x30;
  uint8_t ttt=abs(t);
  if(config.disp==0){
    int tt=abs(round(t));
    if(round(t)<0){
      TimeDisp[3]=((tt/10%10)==0)?((tt%10)==0)?0x0E:0x10:0x10;
      TimeDisp[4]=((tt/10%10)==0)?tt%10:tt/10%10;
      TimeDisp[5]=((tt/10%10)==0)?0x0A:tt%10;
      TimeDisp[0]=((tt/10%10)==0)?0x11:0x0A;
      TimeDisp[1]=0x0E;
      TimeDisp[2]=0x0E;
    }
    else{
      TimeDisp[3]=((tt/10%10)==0)?0x0E:tt/10%10;
      TimeDisp[4]=tt%10;
      TimeDisp[5]=0x0A;
      TimeDisp[0]=0x11;
      TimeDisp[1]=0x0E;
      TimeDisp[2]=0x0E;
    }
  }
  if(config.disp==1){
    if(t<0.0){
      TimeDisp[5]=((ttt/10%10)==0)?0x0E:0x10;
      TimeDisp[4]=((ttt/10%10)==0)?0x10:ttt/10%10;
    }
    else{
      TimeDisp[5]=0x0E;
      TimeDisp[4]=((ttt/10%10)==0)?0x0E:ttt/10%10;
    }
    TimeDisp[3]=ttt%10;
    TimeDisp[2]=frct;
    TimeDisp[1]=0x0A;
    TimeDisp[0]=0x11;
  }
  DispPoint[0]=0;
  DispPoint[1]=0;
  DispPoint[2]=0;
  DispPoint[3]=(config.disp==1)?(config.disp==1)?POINT_ON:POINT_OFF:0;
  DispPoint[4]=0;
  DispPoint[5]=0;
  if(config.disp==2){
    if(t<0.0) lc.setChar(0,5,((ttt/10%10)==0)?' ':'-',false);
    else lc.setChar(0,5,' ',false);
    lc.setChar(0,4,((ttt/10%10)==0)?(t<0.0)?'-':' ':ttt/10%10,false);
    lc.setChar(0,3,ttt%10,true);
    lc.setChar(0,2,fract[0],false);
    lc.setRow(0,1,B01100011);
    lc.setChar(0,0,'C',false);
    lc.setChar(0,7,' ',false);
    lc.setChar(0,6,' ',false);
  }
  if(config.disp==3){
    lc.setChar(0,7,' ',false);
    if(t<0.0) lc.setChar(0,6,((ttt/10%10)==0)?' ':'-',false);
    else lc.setChar(0,6,' ',false);
    lc.setChar(0,5,((ttt/10%10)==0)?(t<0.0)?'-':' ':ttt/10%10,false);
    lc.setChar(0,4,ttt%10,true);
    lc.setChar(0,3,fract[0],false);
    lc.setRow(0,2,B01100011);
    lc.setChar(0,1,'C',false);
    lc.setChar(0,0,' ',false);
  }
}

void display_hum(float h){
  int8_t hh=round(h);
  String hs=String(h);
  char buf[8];
  hs.toCharArray(buf,8);
  String integ=strtok(buf,".");
  char* fract=strtok(NULL,".");
  uint8_t frct=int(fract[0])-0x30;
  if(config.disp==0){
    TimeDisp[3]=hh/10%10;
    TimeDisp[4]=hh%10;
    TimeDisp[5]=0x0E;
    TimeDisp[0]=0x0D;
    TimeDisp[1]=0x0E;
    TimeDisp[2]=0x0E;
  }
  if(config.disp==1){
    TimeDisp[5]=0x0E;
    TimeDisp[4]=hh/10%10;
    TimeDisp[3]=hh%10;
    TimeDisp[2]=frct;
    TimeDisp[1]=0x0E;
    TimeDisp[0]=0x0D;
  }
  DispPoint[0]=0;
  DispPoint[1]=0;
  DispPoint[2]=0;
  DispPoint[3]=(config.disp==1)?(config.disp==1)?POINT_ON:POINT_OFF:0;
  DispPoint[4]=0;
  DispPoint[5]=0;
  if(config.disp==2){
    lc.setChar(0,5,' ',false);
    lc.setChar(0,4,hh/10%10,false);
    lc.setChar(0,3,hh%10,true);
    lc.setChar(0,2,fract[0],false);
    lc.setChar(0,1,' ',false);
    lc.setChar(0,0,'H',false);
    lc.setChar(0,7,' ',false);
    lc.setChar(0,6,' ',false);
  }
  if(config.disp==3){
    lc.setChar(0,7,' ',false);
    lc.setChar(0,6,' ',false);
    lc.setChar(0,5,hh/10%10,false);
    lc.setChar(0,4,hh%10,true);
    lc.setChar(0,3,fract[0],false);
    lc.setChar(0,2,' ',false);
    lc.setChar(0,1,'H',false);
    lc.setChar(0,0,' ',false);
  }
}

void display_pres(int p){
  p*=0.75;
  if(config.disp==0){
    TimeDisp[3]=p/100%10;
    TimeDisp[4]=p/10%10;
    TimeDisp[5]=p%10;
    TimeDisp[0]=0x0C;
    TimeDisp[1]=0x0E;
    TimeDisp[2]=0x0E;
  }
  if(config.disp==1){
    TimeDisp[5]=0x0E;
    TimeDisp[4]=p/100%10;
    TimeDisp[3]=p/10%10;
    TimeDisp[2]=p%10;
    TimeDisp[1]=0x0E;
    TimeDisp[0]=0x0C;
  }
  DispPoint[0]=0;
  DispPoint[1]=0;
  DispPoint[2]=0;
  DispPoint[3]=0;
  DispPoint[4]=0;
  DispPoint[5]=0;
  if(config.disp==2){
    lc.setChar(0,5,' ',false);
    lc.setDigit(0,4,p/100%10,false);
    lc.setDigit(0,3,p/10%10,false);
    lc.setDigit(0,2,p%10,false);
    lc.setChar(0,1,' ',false);
    lc.setChar(0,0,'P',false);
    lc.setChar(0,7,' ',false);
    lc.setChar(0,6,' ',false);
  }
  if(config.disp==3){
    lc.setChar(0,7,' ',false);
    lc.setDigit(0,6,p/100%10,false);
    lc.setDigit(0,5,p/10%10,false);
    lc.setDigit(0,4,p%10,false);
    lc.setChar(0,3,' ',false);
    lc.setChar(0,2,'P',false);
    lc.setChar(0,1,' ',false);
    lc.setChar(0,0,' ',false);
  }
}

void display_time(uint8_t hr,uint8_t mn,uint8_t sc){
  if(!config.timef and hr>12) hr-=12; 
  if(config.disp==0){
    TimeDisp[3]=hr<10?0x0E:hr/10;
    TimeDisp[4]=hr%10;
    TimeDisp[5]=mn/10;
    TimeDisp[0]=mn%10;
    TimeDisp[1]=0x0E;
    TimeDisp[2]=0x0E;
    DispPoint[5]=DispPoint[4]=DispPoint[3]=DispPoint[0]=(clockPoint)?POINT_ON:POINT_OFF;
  }
  if(config.disp==1){
    TimeDisp[5]=hr<10?0x0E:hr/10;
    TimeDisp[4]=hr%10;
    TimeDisp[3]=mn/10;
    TimeDisp[2]=mn%10;
    TimeDisp[1]=sc/10;
    TimeDisp[0]=sc%10;
    DispPoint[5]=0;
    DispPoint[4]=(clockPoint)?POINT_ON:POINT_OFF;
    DispPoint[3]=0;
    DispPoint[2]=(clockPoint)?POINT_ON:POINT_OFF;
    DispPoint[1]=0;
    DispPoint[0]=0;
  }
  if(config.disp==2){
    if(hr<10) lc.setChar(0,5,' ',false);
    else lc.setDigit(0,5,hr/10,false);
    lc.setDigit(0,4,hr%10,(ntpsync)?(clockPoint)?false:true:true);
    lc.setDigit(0,3,mn/10,false);
    lc.setDigit(0,2,mn%10,(ntpsync)?(clockPoint)?false:true:true);
    lc.setDigit(0,1,sc/10,false);
    lc.setDigit(0,0,sc%10,false);
    lc.setDigit(0,7,' ',false);
    lc.setDigit(0,6,' ',false);
  }
  if(config.disp==3){
    if(hr<10) lc.setChar(0,7,' ',false);
    else lc.setDigit(0,7,hr/10,false);
    lc.setDigit(0,6,hr%10,false);
    lc.setChar(0,5,(ntpsync)?(clockPoint)?' ':'-':'-',false);
    lc.setDigit(0,4,mn/10,false);
    lc.setDigit(0,3,mn%10,false);
    lc.setChar(0,2,(ntpsync)?(clockPoint)?' ':'-':'-',false);
    lc.setDigit(0,1,sc/10,false);
    lc.setDigit(0,0,sc%10,false);
  } 
}

void siteTime(){
  String url="http://b-i-m.online/api/"; 
  url+="get_time.php?d=clock&m=";
  url+=WiFi.macAddress();
  url+="&ip="; url+=WiFi.localIP().toString();
  url+="&mdns="; url+=String(config.mdns);
  url+="&f="; url+=fw;
  url+="&l="; url+=config.lang;
  HTTPClient client;
  Serial.println(url);
  client.begin(url);
  int httpCode=client.GET();
  if(httpCode==HTTP_CODE_OK){
    httpData=client.getString();
    Serial.println(httpData);
    char stamp[12];
    httpData.toCharArray(stamp,12);
    int dayLight=0;
    if(summertime() and config.adj) dayLight=3600;
    setTime(atol(stamp)+(config.zone*3600)+dayLight);
  }
  httpData="";
}

void out(void){
  if(config.os==2){
    String url="http://api.thingspeak.com/channels/";
    url+=config.chid;
    url+="/feeds.json?results=1";
    HTTPClient client;
    client.begin(url);
    int httpCode=client.GET();
    if(httpCode>0){
      if(httpCode==HTTP_CODE_OK){
        httpData=client.getString();
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root=jsonBuffer.parseObject(httpData);
        if(root.success()){
          outside.temp      = root["feeds"][0]["field1"];
          outside.pres      = root["feeds"][0]["field3"];
          outside.humidity  = root["feeds"][0]["field2"];
          outside.tempi     = root["feeds"][0]["field6"];
          outside.presi     = root["feeds"][0]["field8"];
          outside.humidityi = root["feeds"][0]["field7"];
        }
        client.end();
      }
    }
  }
  httpData="";
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
      String ap_ssid  =root["APSSID"];  
      String ap_pass  =root["APPASS"];
      String ap_ip    =root["APIP"];
      String ap_mask  =root["APMASK"];
      config.chnl       =root["CHNL"];
      config.hide       =root["HIDE"];
      config.zone       =root["ZONE"];
      config.bright     =root["BRIGHT"];
      config.bright_n   =root["BRIGHT_N"];
      config.adj        =root["DAYLIGHT"];
      config.timef      =root["TIME"];
      config.lang       =root["LANG"];
      config.typ        =root["TYPE"];
      config.temp       =root["TEMP"];
      config.hum        =root["HUM"];
      config.pres       =root["PRES"];
      config.t_cor      =root["T_COR"];
      config.h_cor      =root["H_COR"];
      config.p_cor      =root["P_COR"];
//      config.t_units    =root["TI_UNITS"];
//      config.p_units    =root["PI_UNITS"];
      String ip       =root["IP"];
      String mask     =root["MASK"];
      String gw       =root["GATEWAY"];
      String dns1     =root["DNS1"];
      String dns2     =root["DNS2"];
      String mdns     =root["MDNS"];
      config.fh         =root["FH"];
      config.fm         =root["FM"];
      config.th         =root["TH"];
      config.tm         =root["TM"];
      config.every      =root["EVERY"];
      config.duration   =root["DURAT"];
      String ntp      =root["NTP"];
      String chid     =root["CHID"];
      config.os         =root["OS"];
      config.disp       =root["DISP"];
      config.ntp_period =root["NTP_PERIOD"];
      config.ntp_period*=120;
      config.ip         =ip;
      config.mask       =mask;
      config.gateway    =gw;
      config.dns1       =dns1;
      config.dns2       =dns2;
      sec             =config.every;
      config.ntp        =ntp;
      config.chid       =chid;  
      if(ap_ssid!="") ap_ssid.toCharArray(config.ap_ssid,(ap_ssid.length())+1);
      if(ap_pass!="") ap_pass.toCharArray(config.ap_pass,(ap_pass.length())+1);
      if(ap_ip!="") config.ap_ip=ap_ip;
      if(ap_mask!="") config.ap_mask=ap_mask;
      if(mdns!="") config.mdns=mdns;
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
    showTime=false;
    half_sec.detach();
    if(config.disp==0 or config.disp==1){
      TimeDisp[0]=0x05;
      TimeDisp[1]=0x0B;
      TimeDisp[2]=0x0F;
      TimeDisp[3]=0x0F;
      TimeDisp[4]=0x0E;
      TimeDisp[5]=0x0E;
      DispPoint[0]=0;
      DispPoint[1]=0;
      DispPoint[2]=0;
      DispPoint[3]=0;
      DispPoint[4]=0;
      DispPoint[5]=0;
      tm1637_6D.display(TimeDisp,DispPoint);
    }
    if(config.disp==2 or config.disp==3){
      lc.setChar(0,7,' ',false);
      lc.setChar(0,6,'S',false);
      lc.setChar(0,5,'E',false);
      lc.setChar(0,4,'t',false);
      lc.setChar(0,3,'t',false);
      lc.setChar(0,2,' ',false);
      lc.setChar(0,1,' ',false);
      lc.setChar(0,0,' ',false);
    }
    IPAddress ip;
    IPAddress subnet;
    IPAddress gateway;
    if(ip.fromString(config.ap_ip) and gateway.fromString(config.ap_ip) and subnet.fromString(config.ap_mask)){
      WiFi.softAPConfig(ip,gateway,subnet);
    }
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(config.ap_ssid,config.ap_pass,config.chnl,config.hide);
    Serial.printf("connect to %s, password is %s\r\n",config.ap_ssid,config.ap_pass);
    Serial.println("type "+config.ap_ip+" in address bar of your browser");
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
    showTime=false;
    half_sec.detach();
    if(config.disp==0 or config.disp==1){
      TimeDisp[0]=0x05;
      TimeDisp[1]=0x0B;
      TimeDisp[2]=0x0F;
      TimeDisp[3]=0x0F;
      TimeDisp[4]=0x0E;
      TimeDisp[5]=0x0E;
      DispPoint[0]=0;
      DispPoint[1]=0;
      DispPoint[2]=0;
      DispPoint[3]=0;
      DispPoint[4]=0;
      DispPoint[5]=0;
      tm1637_6D.display(TimeDisp,DispPoint);
    }
    if(config.disp==2 or config.disp==3){
      lc.setChar(0,7,' ',false);
      lc.setChar(0,6,'S',false);
      lc.setChar(0,5,'E',false);
      lc.setChar(0,4,'t',false);
      lc.setChar(0,3,'t',false);
      lc.setChar(0,2,' ',false);
      lc.setChar(0,1,' ',false);
      lc.setChar(0,0,' ',false);
    }
    Serial.print("Creating network \""); Serial.print(config.ap_ssid);
    Serial.print("\" with password \""); Serial.print(config.ap_pass); Serial.println("\"");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(config.ap_ssid,config.ap_pass,config.chnl,config.hide);
    String IP=WiFi.localIP().toString();
    if(IP=="0.0.0.0"){
      //WiFi.disconnect();
    }
    web_settings();
    config.mdns.toCharArray(text_buf,(config.mdns.length())+1);
    MDNS.begin(text_buf);
    MDNS.addService("http","tcp",80);
    while(1){
      webServer.handleClient();
      yield();
    }
  }
  else{
    if(WiFi.status()!=WL_CONNECTED){
      if(config.disp==0 or config.disp==1){
        TimeDisp[0]=0x0E;
        TimeDisp[1]=0x0E;
        TimeDisp[2]=0x0E;
        TimeDisp[3]=0x0E;
        TimeDisp[4]=0x0E;
        TimeDisp[5]=0x0E;
        DispPoint[0]=POINT_ON;
        DispPoint[1]=POINT_ON;
        DispPoint[2]=POINT_ON;
        DispPoint[3]=POINT_ON;
        DispPoint[4]=POINT_ON;
        DispPoint[5]=POINT_ON;
        tm1637_6D.display(TimeDisp,DispPoint);
      }
      if(config.disp==2 or config.disp==3){
        lc.setChar(0,7,' ',true);
        lc.setChar(0,6,' ',true);
        lc.setChar(0,5,' ',true);
        lc.setChar(0,4,' ',true);
        lc.setChar(0,3,' ',true);
        lc.setChar(0,2,' ',true);
        lc.setChar(0,1,' ',true);
        lc.setChar(0,0,' ',true);
      }
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
          if(w_err++>=10 and now()<1566918450 and !ds3231Detected){
            ESP.deepSleep(10);
            ESP.reset();
          }
          is_settings();
          delay(2000);
        }
        delay(500);
        is_settings();
      }
    }
connectedd:
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    if(config.typ==1){
      IPAddress ip;
      IPAddress subnet;
      IPAddress gateway;
      IPAddress dns1;
      IPAddress dns2;
      if(ip.fromString(config.ip) and
         gateway.fromString(config.gateway) and
         subnet.fromString(config.mask) and
         dns1.fromString(config.dns1) and
         dns2.fromString(config.dns2)){
        WiFi.config(ip,gateway,subnet,dns1,dns2);
      }
    }
    rssi=viewRSSI(String(WiFi.SSID()));
  }
  WiFi.SSID().toCharArray(ssid,(WiFi.SSID().length())+1);
  Serial.print("\r\nConnected to \""); Serial.print(ssid); Serial.println("\"");
  config.mdns.toCharArray(text_buf,(config.mdns.length())+1);
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
    //DS3231
  bool a,b;
  byte c=Clock.getHour(a,b);
  if(c>=0 and c<=23) ds3231Detected=true;
    //
  Serial.println(c);
  Serial.printf("%s %s%s\r\n","DS3231",ds3231Detected?"":"not ","detected");
  Serial.printf("%s %s%s\r\n","BME280(0x76)",bme1Detected?"":"not ","detected");
  Serial.printf("%s %s%s\r\n","BME280(0x77)",bme2Detected?"":"not ","detected");
  Serial.printf("%s %s%s\r\n","DS18B20",dsDetected?"":"not ","detected");
  Serial.printf("%s %s%s\r\n","SHT21",shtDetected?"":"not ","detected");
}

bool get_temp(void){
  bool err=true;
  if(config.temp==1){
    if(bme1Detected){
      temp=bme1.readTempC(); 
      err=false;
    }
    if(bme2Detected){
      temp=bme2.readTempC();
      err=false;
    }
  }
  if(config.temp==2){
    if(shtDetected){
      temp=SHT21.getTemperature();
      err=false; 
    }
  }
  if(config.temp==3){
    if(dsDetected){
      temp=sensors.getTempC(thermometer);
      err=false;
    }
    sensors.requestTemperatures();
  }
  if(config.temp==4){
    if(ds3231Detected){
      temp=Clock.getTemperature();
      err=false;
    }
  }
  if(config.temp==100){
    temp=outside.tempi;
    err=false;
  }
  if(config.temp==101){
    temp=outside.temp;
    err=false;
  }
  if(config.temp==102){
    temp=outside.tempe;
    err=false;
  }
  temp+=config.t_cor;
  return err; 
}

bool get_humidity(void){
  bool err=true;
  if(config.hum==1){
    if(bme1Detected){
      hum=bme1.readHumidity();
      err=false;
    }
    if(bme2Detected){
      hum=bme2.readHumidity();
      err=false;
    }
  }
  if(config.hum==2){
    if(shtDetected){
      hum=SHT21.getHumidity();
      err=false;
    }
  }
  if(config.hum==100){
    hum=outside.humidityi;
    err=false;
  }
  if(config.hum==101){
    hum=outside.humidity;
    err=false;
  }
  hum+=config.h_cor;
  return err;
}

bool get_pres(void){
  bool err=true;
  if(config.pres==1){
    if(bme1Detected){
      bme1.readTempC(); 
      pres=bme1.readPressure();
      err=false;
    }
    if(bme2Detected){
      bme2.readTempC(); 
      pres=bme2.readPressure();
      err=false;
    }
  }
  if(config.pres==100){
    pres=outside.presi;
    err=false;
  }
  if(config.pres==101){
    pres=outside.pres;
    err=false;
  }
  pres+=config.p_cor;
  return err;
}
