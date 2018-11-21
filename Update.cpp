#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h>


int FW_VERSION = 6; //версия приложения
const char* fwUrlBase = "http://company-ms.ru/update/"; // адрес до место прошивки
int newVersion;

String extern mqttClient;
String extern mqttTopic;
void helpPublic(String topicc, String myStringsgs);

void checkForUpdates() {
  String mac = "DOOR"; // проверочное значение для файла в котором написанна версия приложенияю пишем сюда ТИП устройства
  String fwURL = String( fwUrlBase );
  fwURL.concat( mac );
  String fwVersionURL = fwURL;
  fwVersionURL.concat( ".version" ); //окончиния имени файла с версией приложения
  Serial.println( "Checking for firmware updates." );
  Serial.print( "MAC address: " );
  Serial.println( mac );
  Serial.print( "Firmware version URL: " );
  Serial.println( fwVersionURL );
  HTTPClient httpClient;
  httpClient.begin( fwVersionURL );
  int httpCode = httpClient.GET();
  if ( httpCode == 200 ) {
    String  newFWVersion = httpClient.getString();
    Serial.print( "Current firmware version: " );
    Serial.println( FW_VERSION );
    Serial.print( "Available firmware version: " );
    Serial.println( newFWVersion );
    newVersion = newFWVersion.toInt();
    if ( newVersion > FW_VERSION ) {
      Serial.println( "Preparing to update" );

      String topic('/');
      topic += mqttClient;
      topic += mqttTopic;
      topic += '/';
      topic += "check";
      String message = "Preparing to update ";
      message += newVersion;
     
      helpPublic( topic,  message);
      
      String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" ); // если новая версия появилась, то апдейтим беря файл с .bin
      t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );
      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;
        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;
      }
    }
    else {
      Serial.println( "Already on latest version" );

      String topic('/');
      topic += mqttClient;
      topic += mqttTopic;
      topic += '/';
      topic += "check";
      String message = "latest version ";
      message += FW_VERSION;
      
     helpPublic( topic,  message);
     
    }
  }
  else {
    Serial.print( "Firmware version check failed, got HTTP response code " );
    Serial.println( httpCode );
  }
  httpClient.end();
}
