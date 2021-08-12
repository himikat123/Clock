String getContentType(String filename){
  if(webServer.hasArg("download"))    return "application/octet-stream";
  else if(filename.endsWith(".htm"))  return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css"))  return "text/css";
  else if(filename.endsWith(".js"))   return "application/javascript";
  else if(filename.endsWith(".json")) return "text/json";
  else if(filename.endsWith(".png"))  return "image/png";
  else if(filename.endsWith(".gif"))  return "image/gif";
  else if(filename.endsWith(".jpg"))  return "image/jpeg";
  else if(filename.endsWith(".ico"))  return "image/x-icon";
  else if(filename.endsWith(".xml"))  return "text/xml";
  else if(filename.endsWith(".pdf"))  return "application/x-pdf";
  else if(filename.endsWith(".zip"))  return "application/x-zip";
  else if(filename.endsWith(".gz"))   return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  //Serial.println(path);
  if(webServer.hasHeader("Cookie")){
    String cookie = webServer.header("Cookie");
    int8_t au = cookie.indexOf("auth");
    uint8_t cook[10]; 
    uint8_t coincid = 0;
    uint8_t code_auth[10];
    ESP.rtcUserMemoryRead(0, (uint32_t*)&code_auth, 10);
    for(uint8_t i=0; i<10; i++){ 
      cook[i] = (uint8_t)cookie[au + 5 + i] - 48;
    }
    for(uint8_t i=0; i<10; i++) if(code_auth[i] == cook[i]) coincid++;
    if(au != -1 and coincid == 10) return FileRead(path); 
    else return FileRead("/login.htm"); 
  }
  else return FileRead("/login.htm");
}

bool FileRead(String path){
  //Serial.println(path);
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz)) path += ".gz";
    //Serial.println(path);
    File file = SPIFFS.open(path, "r");
    size_t sent = webServer.streamFile(file, contentType);
    file.close();
    return true;
  }
  //Serial.println("No file found");
  return false;
}

void handleFileUpload(void){
  if(webServer.uri() != "/edit") return;
  HTTPUpload& upload = webServer.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/" + filename;
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } 
  else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
  } 
  else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) fsUploadFile.close();
  }
}

String notFound = "HTTP 404 - File Not Found";

void web_settings(void){
  webServer.on("/esp/save.php", HTTP_POST, [](){
    String daten = webServer.arg("CONFIG");
    File file = SPIFFS.open("/config.json", "w");
    if(file){
      file.print(daten);
      file.close();
      webServer.send(200, "text/plain", "OK");
    }
    else webServer.send(200, "text/plain", "ERROR");  
  });
////////////////////////////////////////////////////////////////////////
  webServer.on("/esp/ssids.php", HTTP_GET, [](){
    String json = "{";
    uint8_t n = WiFi.scanNetworks();
    for(uint8_t i=0; i<n; i++){
      json += "\""; 
      json += WiFi.SSID(i);
      json += "\":\"";
      json += abs(WiFi.RSSI(i));
      if(i == n - 1) json += "\"}";
      else json += "\",";
    }
    webServer.send(200, "text/json", json);
  });
///////////////////////////////////////////////////////////////////////
  webServer.on("/esp/ip_gw.php", HTTP_GET, [](){
    String json = "{\"ip_s\":\""; json += WiFi.localIP().toString();    json += "\",";
    json += "\"gw_s\":\"";        json += WiFi.gatewayIP().toString();  json += "\",";
    json += "\"mask_s\":\"";      json += WiFi.subnetMask().toString(); json += "\"}";
    webServer.send(200, "text/plain", json);
  });
//////////////////////////////////////////////////////////////////////////
  webServer.on("/esp/sens.php", HTTP_GET, [](){
    String json = "{\"fw\":\"v"; json += fw; json += "\",";
    json += "\"ssid\":\""; json += WiFi.SSID(); json += "\",";
    json += "\"ch\":"; json += WiFi.channel(); json += ",";
    json += "\"sig\":\""; json += WiFi.RSSI(); json += "dB\",";
    json += "\"mac\":\""; json += WiFi.macAddress(); json += "\",";
    json += "\"ip\":\""; json += WiFi.localIP().toString(); json += "\",";
    json += "\"bme280_temp\":\""; json += sensors.bme280_det ? String(sensors.bme280_temp) : "40400"; json += "\",";
    json += "\"bme280_hum\":\""; json += sensors.bme280_det ? String(sensors.bme280_hum) : "40400"; json += "\",";
    json += "\"bme280_pres\":\""; json += sensors.bme280_det ? String(sensors.bme280_pres) : "40400"; json += "\",";
    json += "\"bmp180_temp\":\""; json += sensors.bmp180_det ? String(sensors.bmp180_temp) : "40400"; json += "\",";
    json += "\"bmp180_pres\":\""; json += sensors.bmp180_det ? String(sensors.bmp180_pres) : "40400"; json += "\",";
    json += "\"sht21_temp\":\""; json += sensors.sht21_det ? String(sensors.sht21_temp) : "40400"; json += "\",";
    json += "\"sht21_hum\":\""; json += sensors.sht21_det ? String(sensors.sht21_hum) : "40400"; json += "\",";
    json += "\"dht22_temp\":\""; json += sensors.dht22_det ? String(sensors.dht22_temp) : "40400"; json += "\",";
    json += "\"dht22_hum\":\""; json += sensors.dht22_det ? String(sensors.dht22_hum) : "40400"; json += "\",";
    json += "\"ds18_temp\":\""; json += sensors.ds18_det ? String(sensors.ds18_temp) : "40400"; json += "\",";
    json += "\"ds32_temp\":\""; json += sensors.ds32_det ? String(sensors.ds32_temp) : "40400"; json += "\",";
    json += "\"snum\":"; json += datas.snum; json += ",";
    json += "\"snum2\":"; json += datas.snum2; json += "}";
    webServer.send(200, "text/json", json);
  });
//////////////////////////////////////////////////////////////////////////
  webServer.on("/esp/sensors.php", HTTP_GET, [](){
    String json = "{\"dbmet\":\""; json += sensors.bme280_det ? String(sensors.bme280_temp - sensors.bme280_temp_corr) : "40400"; json += "\",";
    json += "\"dbmeh\":\""; json += sensors.bme280_det ? String(sensors.bme280_hum - sensors.bme280_hum_corr) : "40400"; json += "\",";
    json += "\"dbmep\":\""; json += sensors.bme280_det ? String(sensors.bme280_pres - sensors.bme280_pres_corr) : "40400"; json += "\",";
    json += "\"dbmpt\":\""; json += sensors.bmp180_det ? String(sensors.bmp180_temp - sensors.bmp180_temp_corr) : "40400"; json += "\",";
    json += "\"dbmpp\":\""; json += sensors.bmp180_det ? String(sensors.bmp180_pres - sensors.bmp180_pres_corr) : "40400"; json += "\",";
    json += "\"dshtt\":\""; json += sensors.sht21_det ? String(sensors.sht21_temp - sensors.sht21_temp_corr) : "40400"; json += "\",";
    json += "\"dshth\":\""; json += sensors.sht21_det ? String(sensors.sht21_hum - sensors.sht21_hum_corr) : "40400"; json += "\",";
    json += "\"ddhtt\":\""; json += sensors.dht22_det ? String(sensors.dht22_temp - sensors.dht22_temp_corr) : "40400"; json += "\",";
    json += "\"ddhth\":\""; json += sensors.dht22_det ? String(sensors.dht22_hum - sensors.dht22_hum_corr) : "40400"; json += "\",";
    json += "\"dds18t\":\""; json += sensors.ds18_det ? String(sensors.ds18_temp - sensors.ds18_temp_corr) : "40400"; json += "\",";
    json += "\"dds32t\":\""; json += sensors.ds32_det ? String(sensors.ds32_temp - sensors.ds32_temp_corr) : "40400"; json += "\"}";
    webServer.send(200, "text/json", json);
  });
///////////////////////////////////////////////////////////////////
  webServer.on("/esp/bright.php", HTTP_GET, [](){
    int bright = webServer.arg("br").toInt();
    if(datas.is_day) config.day_bright = bright;
    else config.night_bright = bright;
    webServer.send(200, "text/plain", "OK");
  });
/////////////////////////////////////////////////////////////////////////
  webServer.on("/esp/bright2.php", HTTP_GET, [](){
    int bright = webServer.arg("br").toInt();
    if(datas.is_day2) config.day_bright2 = bright;
    else config.night_bright2 = bright;
    webServer.send(200, "text/plain", "OK");
  });
////////////////////////////////////////////////////////////////////////
  webServer.on("/esp/reboot.php", HTTP_GET, [](){
    webServer.send(200, "text/plain", "OK");
    ESP.deepSleep(10);
    ESP.reset();
  });
////////////////////////////////////////////////////////////////////////
  webServer.on("/esp/list.php", HTTP_GET, [](){
    Dir dir = SPIFFS.openDir("/");
    String output = "{\"fl\":[";
    while(dir.next()){
      if(output != "{\"fl\":[") output += ',';
      output += "{\"name\":\""; output += dir.fileName();
      output += "\",\"type\":\"file\",\"size\":\""; output += dir.fileSize();
      output += "\"}"; 
    }
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    output += "],\"fs\":{\"total\":";
    output += round(fs_info.totalBytes / 1024);
    output += ",\"used\":";
    output += round(fs_info.usedBytes / 1024);
    output += "}}";
    webServer.send(200, "text/json", output);
  });
////////////////////////////////////////////////////////////////////////
  webServer.on("/esp/del.php", HTTP_GET, [](){
    String path = "/" + webServer.arg("d");
    if(!SPIFFS.exists(path)) return webServer.send(404, "text/plain", "FileNotFound");
    SPIFFS.remove(path);
    webServer.send(200, "text/plain", "OK");
    path = String();
  });
////////////////////////////////////////////////////////////////////////
  webServer.on("/esp/rename.php", HTTP_GET, [](){
    String alt = "/" + webServer.arg("old");
    String neu = "/" + webServer.arg("new");
    if(!SPIFFS.exists(alt)) return webServer.send(404, "text/plain", "FileNotFound");
    SPIFFS.rename(alt, neu);
    webServer.send(200, "text/plain", "OK");
    alt = String();
    neu = String();
  });
//////////////////////////////////////////////////////////////////////
  webServer.on("/esp/login.php", HTTP_POST, [](){
    String login = webServer.arg("LOGIN");
    String passw = webServer.arg("PASSW");
    if(login == config.username and passw == config.password){
      String code = "";
      uint8_t code_auth[10];
      for(uint8_t i=0; i<10; i++){
        code_auth[i] = ESP8266TrueRandom.random(0, 10);
        code += String(code_auth[i]); 
      }
      webServer.sendHeader("Location", "/");
      webServer.sendHeader("Cache-Control", "no-cache");
      webServer.sendHeader("Set-Cookie", "auth=" + code + "; Max-Age=3600; path=/");
      webServer.send(301);
      ESP.rtcUserMemoryWrite(0, (uint32_t*) & code_auth, 10);
    }
    else{
      webServer.sendHeader("Location", "/login.htm?error");
      webServer.sendHeader("Cache-Control", "no-cache");
      webServer.send(301);
    }
  });
///////////////////////////////////////////////////////////////////
  webServer.on("/esp/user.php", HTTP_POST, [](){
    String user = webServer.arg("name");
    String old_pass = webServer.arg("oldpass");
    String new_pass = webServer.arg("newpass");
    if(String(config.password) == old_pass){
      File filew = SPIFFS.open("/user.us", "w");
      if(filew){
        filew.print("{\"user\":\"" + user + "\",\"pass\":\"" + new_pass + "\"}");
        filew.close();
        webServer.send(200, "text/plain", "OK");
      }
      else webServer.send(200, "text/plain", "ERROR Write file");
    }
    else webServer.send(200, "text/plain", "error");
  });
///////////////////////////////////////////////////////////////
  webServer.on("/esp/username.php", HTTP_GET, [](){
    String username = "{\"user\":\"";
    username += config.username;
    username += "\"}";
    webServer.send(200, "text/plain", username);
  });
////////////////////////////////////////////////////////////////
  webServer.on("/edit", HTTP_POST, [](){
    webServer.send(200, "text/plain", "");
  }, handleFileUpload);
////////////////////////////////////////////////////////////////
  webServer.on("/esp/update.php", HTTP_POST, [](){
    if(Update.hasError()) handleFileRead("/fail.htm");
    else handleFileRead("/ok.htm");
    delay(1000);
    ESP.deepSleep(10);
    ESP.reset();
  }, [](){
    HTTPUpload& upload = webServer.upload();
    if(upload.status == UPLOAD_FILE_START){
      Serial.setDebugOutput(true);
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if(!Update.begin(maxSketchSpace)){
        Update.printError(Serial);
      }
    }
    else if(upload.status == UPLOAD_FILE_WRITE){
      if(Update.write(upload.buf,upload.currentSize) != upload.currentSize){
        Update.printError(Serial);
      }
    } 
    else if(upload.status == UPLOAD_FILE_END){
      if(Update.end(true)){
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } 
      else{
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });
//////////////////////////////////////////////////////////////////
  webServer.on("/esp/update_f.php", HTTP_POST, [](){
    if(Update.hasError()) handleFileRead("/fail.htm");
    else handleFileRead("/ok.htm");
    delay(1000);
    ESP.deepSleep(10);
    ESP.reset();
  }, [](){
    HTTPUpload& upload = webServer.upload();
    if(upload.status == UPLOAD_FILE_START){
      Serial.setDebugOutput(true);
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
//      if(!Update.begin(maxSketchSpace, U_SPIFFS)){
//        Update.printError(Serial);
//      }
    }
    else if(upload.status == UPLOAD_FILE_WRITE){
      if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
        Update.printError(Serial);
      }
    } 
    else if(upload.status == UPLOAD_FILE_END){
      if(Update.end(true)){
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } 
      else{
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });
//////////////////////////////////////////////////////////////////
  webServer.onNotFound([](){
    if(!handleFileRead(webServer.uri())) webServer.send(404, "text/plain", notFound);
  });
  const char * headerkeys[] = {"User-Agent", "Cookie"};
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
  webServer.collectHeaders(headerkeys, headerkeyssize);
  webServer.begin();
}
