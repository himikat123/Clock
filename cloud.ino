void getWeatherNow(void){
  Serial.println("weather update");
  String url;
  if(config.provider == 0){    
    url = "http://api.openweathermap.org/data/2.5/weather";
    if(config.citysearch == 0) url += "?q=" + String(config.city);
    if(config.citysearch == 1) url += "?id=" + String(config.cityid);
    if(config.citysearch == 2) url += "?lat=" + String(config.lat) + "&lon=" + String(config.lon);
    url += "&units=metric&appid=" + String(config.appid);
  }
  if(config.provider == 1){
    url = "http://api.weatherbit.io/v2.0/current?key=";
    url += String(config.appkey);
    if(config.citysearch == 0) url += "&city=" + String(config.city);
    if(config.citysearch == 1) url += "&city_id=" + String(config.cityid);
    if(config.citysearch == 2) url += "&lat=" + String(config.lat) + "&lon=" + String(config.lon);
  }
  parseWeatherNow(weatherRequest(url));
}

String weatherRequest(String url){
  Serial.println(url);
  String httpData = "";
  HTTPClient client;
  client.begin(url);
  int httpCode = client.GET();
  if(httpCode > 0){
    if(httpCode == HTTP_CODE_OK){
      httpData = client.getString();
    }
  }
  client.end();
  return httpData;
}

void parseWeatherNow(String httpData){
  Serial.println(httpData);
  StaticJsonDocument<2048> root;
  DeserializationError error = deserializeJson(root, httpData);
  if(error) return;
  if(config.provider == 0){
    datas.temp_web       = root["main"]["temp"];
    datas.hum_web        = root["main"]["humidity"];
    datas.pres_web       = root["main"]["pressure"];
    datas.pres_web      *= 0.75;
  }
  if(config.provider == 1){
    datas.temp_web       = root["data"][0]["temp"];
    datas.hum_web        = root["data"][0]["rh"];
    datas.pres_web       = root["data"][0]["pres"];
    datas.pres_web      *= 0.75;
  }
  httpData = "";
}

void thingspk_recv(void){
  String url;
  url = "http://api.thingspeak.com/channels/";
  url += String(config.chid); 
  url += "/feeds.json?api_key=";
  url += String(config.rdkey);
  url += "&results=1";
  parseThing(thingRequest(url));
}

String thingRequest(String url){
  String httpData = "";
  HTTPClient client;
  client.begin(url);
  int httpCode = client.GET();
  if(httpCode > 0){
    if(httpCode == HTTP_CODE_OK){
      httpData = client.getString();
    }
  }
  client.end();
  return httpData;
}

void parseThing(String httpData){
  StaticJsonDocument<2048> root;
  DeserializationError error = deserializeJson(root, httpData);
  if(error) return;
  datas.thing[1] = root["feeds"][0]["field1"];
  datas.thing[2] = root["feeds"][0]["field2"];
  datas.thing[3] = root["feeds"][0]["field3"];
  datas.thing[4] = root["feeds"][0]["field4"];
  datas.thing[5] = root["feeds"][0]["field5"];
  datas.thing[6] = root["feeds"][0]["field6"];
  datas.thing[7] = root["feeds"][0]["field7"];
  datas.thing[8] = root["feeds"][0]["field8"];
  httpData = "";
}

void thingspk_send(void){
  String url;
  url = "http://api.thingspeak.com/update?api_key=";
  url += String(config.wrkey);
  if(config.tf1 > 0){
    url += "&field1=";
    switch(config.tf1){
      case 1: url += String(sensors.bme280_temp); break;
      case 2: url += String(sensors.bme280_hum); break;
      case 3: url += String(sensors.bme280_pres); break;
      case 4: url += String(sensors.bmp180_temp); break;
      case 5: url += String(sensors.bmp180_pres); break;
      case 6: url += String(sensors.sht21_temp); break;
      case 7: url += String(sensors.sht21_hum); break;
      case 8: url += String(sensors.dht22_temp); break;
      case 9: url += String(sensors.dht22_hum); break;
      case 10: url += String(sensors.ds18_temp); break;
      case 11: url += String(sensors.ds32_temp); break;
      default: url += "0"; break;
    }
  }
  if(config.tf2 > 0){
    url += "&field2=";
    switch(config.tf2){
      case 1: url += String(sensors.bme280_temp); break;
      case 2: url += String(sensors.bme280_hum); break;
      case 3: url += String(sensors.bme280_pres); break;
      case 4: url += String(sensors.bmp180_temp); break;
      case 5: url += String(sensors.bmp180_pres); break;
      case 6: url += String(sensors.sht21_temp); break;
      case 7: url += String(sensors.sht21_hum); break;
      case 8: url += String(sensors.dht22_temp); break;
      case 9: url += String(sensors.dht22_hum); break;
      case 10: url += String(sensors.ds18_temp); break;
      case 11: url += String(sensors.ds32_temp); break;
      default: url += "0"; break;
    }
  }
  if(config.tf3 > 0){
    url += "&field3=";
    switch(config.tf3){
      case 1: url += String(sensors.bme280_temp); break;
      case 2: url += String(sensors.bme280_hum); break;
      case 3: url += String(sensors.bme280_pres); break;
      case 4: url += String(sensors.bmp180_temp); break;
      case 5: url += String(sensors.bmp180_pres); break;
      case 6: url += String(sensors.sht21_temp); break;
      case 7: url += String(sensors.sht21_hum); break;
      case 8: url += String(sensors.dht22_temp); break;
      case 9: url += String(sensors.dht22_hum); break;
      case 10: url += String(sensors.ds18_temp); break;
      case 11: url += String(sensors.ds32_temp); break;
      default: url += "0"; break;
    }
  }
  if(config.tf4 > 0){
    url += "&field4=";
    switch(config.tf4){
      case 1: url += String(sensors.bme280_temp); break;
      case 2: url += String(sensors.bme280_hum); break;
      case 3: url += String(sensors.bme280_pres); break;
      case 4: url += String(sensors.bmp180_temp); break;
      case 5: url += String(sensors.bmp180_pres); break;
      case 6: url += String(sensors.sht21_temp); break;
      case 7: url += String(sensors.sht21_hum); break;
      case 8: url += String(sensors.dht22_temp); break;
      case 9: url += String(sensors.dht22_hum); break;
      case 10: url += String(sensors.ds18_temp); break;
      case 11: url += String(sensors.ds32_temp); break;
      default: url += "0"; break;
    }
  }
  if(config.tf5 > 0){
    url += "&field5=";
    switch(config.tf5){
      case 1: url += String(sensors.bme280_temp); break;
      case 2: url += String(sensors.bme280_hum); break;
      case 3: url += String(sensors.bme280_pres); break;
      case 4: url += String(sensors.bmp180_temp); break;
      case 5: url += String(sensors.bmp180_pres); break;
      case 6: url += String(sensors.sht21_temp); break;
      case 7: url += String(sensors.sht21_hum); break;
      case 8: url += String(sensors.dht22_temp); break;
      case 9: url += String(sensors.dht22_hum); break;
      case 10: url += String(sensors.ds18_temp); break;
      case 11: url += String(sensors.ds32_temp); break;
      default: url += "0"; break;
    }
  }
  if(config.tf6 > 0){
    url += "&field6=";
    switch(config.tf6){
      case 1: url += String(sensors.bme280_temp); break;
      case 2: url += String(sensors.bme280_hum); break;
      case 3: url += String(sensors.bme280_pres); break;
      case 4: url += String(sensors.bmp180_temp); break;
      case 5: url += String(sensors.bmp180_pres); break;
      case 6: url += String(sensors.sht21_temp); break;
      case 7: url += String(sensors.sht21_hum); break;
      case 8: url += String(sensors.dht22_temp); break;
      case 9: url += String(sensors.dht22_hum); break;
      case 10: url += String(sensors.ds18_temp); break;
      case 11: url += String(sensors.ds32_temp); break;
      default: url += "0"; break;
    }
  }
  if(config.tf7 > 0){
    url += "&field7=";
    switch(config.tf7){
      case 1: url += String(sensors.bme280_temp); break;
      case 2: url += String(sensors.bme280_hum); break;
      case 3: url += String(sensors.bme280_pres); break;
      case 4: url += String(sensors.bmp180_temp); break;
      case 5: url += String(sensors.bmp180_pres); break;
      case 6: url += String(sensors.sht21_temp); break;
      case 7: url += String(sensors.sht21_hum); break;
      case 8: url += String(sensors.dht22_temp); break;
      case 9: url += String(sensors.dht22_hum); break;
      case 10: url += String(sensors.ds18_temp); break;
      case 11: url += String(sensors.ds32_temp); break;
      default: url += "0"; break;
    }
  }
  if(config.tf8 > 0){
    url += "&field8=";
    switch(config.tf8){
      case 1: url += String(sensors.bme280_temp); break;
      case 2: url += String(sensors.bme280_hum); break;
      case 3: url += String(sensors.bme280_pres); break;
      case 4: url += String(sensors.bmp180_temp); break;
      case 5: url += String(sensors.bmp180_pres); break;
      case 6: url += String(sensors.sht21_temp); break;
      case 7: url += String(sensors.sht21_hum); break;
      case 8: url += String(sensors.dht22_temp); break;
      case 9: url += String(sensors.dht22_hum); break;
      case 10: url += String(sensors.ds18_temp); break;
      case 11: url += String(sensors.ds32_temp); break;
      default: url += "0"; break;
    }
  }
  String httpData = "";
  HTTPClient client;
  client.begin(url);
  int httpCode = client.GET();
  if(httpCode > 0){
    if(httpCode == HTTP_CODE_OK){
      httpData = client.getString();
    }
  }
  client.end();
  httpData = "";
}
