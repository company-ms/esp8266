#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>                       // Импортируем бмблиотеку
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <NTPClient.h>          //для времени
#include <TimeLib.h>           //для времени
#include <WiFiUdp.h>

String ssid, password;   //хранение логина/пароля
String mqttServer, mqttUser, mqttPassword, mqttClient = "ESP_Relay", mqttTopic = "/Relay";
uint16_t mqttPort = 1883;//порт по умолчанию
String hhmmss;
long unixt;
uint32_t timings = 0;

int8_t GMT = 3; //часовой пояс

const byte pinBuiltinLed = 2; // контакт для мигания светодиодом
//int16_t Door_Led_Pin = 13; // выбрать контакт для светодиода
int16_t Door_Sensor_Pin = 14; // контакт для датчика
bool valTec; // переменная для хранения состояния датчика
bool Flag = 0;

extern int FW_VERSION; //тянем переменную из update.cpp
extern int newVersion;//тянем переменную из update.cpp

time_t hourr60; //время в минутах * 60+3часа
int dat; //хранится день недели


const char* ssidArg = "ssid";
const char* passwordArg = "password";
const char* serverArg = "server";
const char* portArg = "port";
const char* userArg = "user";
const char* mqttpswdArg = "mqttpswd";
const char* clientArg = "client";
const char* topicArg = "topic";
const char* gpioArg = "gpio";
const char* levelArg = "level";
const char* onbootArg = "onboot";
const char* rebootArg = "reboot";
const byte maxStrParamLength = 32;                  //максимальная длинна поля
const char configSign[4] = { '#', 'R', 'E', 'L' }; //проверка на корректность eeprom
const char* aHostname = "A.Andreev5_DoorEsp";//имя esp какием его видит роутер

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
WiFiClient espClient;
PubSubClient pubsubClient(espClient);
ESP8266WebServer httpServer(80);
ADC_MODE(ADC_VCC); // настраиваем считывание напряжения

void DeepSleep(); //функция для проверки нужно спать или нет
void checkForUpdates(); //обновление прошивки функуия в .cpp

void setup() {
  Serial.begin(115200);            //инициализация сериал 
  EEPROM.begin(1024);             //инициализация ЕЕПРОМ
  
//pinMode(Door_Led_Pin, OUTPUT); // установить Door_Led_Pin как выход
pinMode(Door_Sensor_Pin, INPUT); // установить Door_Sensor_Pin как вход


  WiFi.setAutoConnect(false);   // автоматически подключался к последней использованной точке доступа.
  WiFi.setAutoReconnect(false);// повторного подключения к точке доступа.
WiFi.hostname(aHostname);//имя esp какием его видит роутер
  httpServer.on("/mqtt", handleMQTTConfig);   //страница с mqtt настройками
  httpServer.on("/store", handleStoreConfig);//страница с записью в eeprom
  httpServer.on("/upd", updatFun);   // страница для апдейта
  timeClient.begin();      //инициируем время
pinMode(pinBuiltinLed, OUTPUT); //встроенный светодиод
  
  delay(10);
  WiFi.mode(WIFI_STA);    //
  delay(100);            //задержка для профиликики глючков
  SmartConnect();       //запускаем подключение
  httpServer.begin();  //запускаем http сервер

  if (mqttServer.length()) {
    pubsubClient.setServer(mqttServer.c_str(), mqttPort);
    pubsubClient.setCallback(mqttCallback);
  }

  //обработка 404 страницы 
  httpServer.onNotFound([]() {
  httpServer.send(404, "text/plain", "FileNotFound");
  });
}


//подключаемся к вай фай сети ,которая осталась в памяти eeprom
void SmartConnect(){
  const uint32_t timeout = 60000; // 30 sec.
  uint32_t maxTime = millis() + timeout;

   readConfig(); //читаем значение из EEPROM
   Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
   Serial.printf("Password: %s\n", WiFi.psk().c_str());
   
  if (!WiFi.isConnected() && WiFi.SSID() != ""){ 
      Serial.print(F("Connect last WiFi"));
      WiFi.mode(WIFI_STA);
      delay(300);
        WiFi.begin(ssid.c_str(), password.c_str());
        while (WiFi.status() != WL_CONNECTED) {
              digitalWrite(pinBuiltinLed, LOW);
              delay(500);
              
              Serial.print(".");
              digitalWrite(pinBuiltinLed, HIGH);
              delay(500);
      
  if ((int32_t)(millis() - maxTime) >= 0) {
        Serial.println(F(" fail!"));
        SmartConnectWiFi();
        break;
        } 
      }
    Serial.println(WiFi.localIP());  
      }  else {
         Serial.println(F("None SSID/ Go SmartConnect"));
         SmartConnectWiFi();
              }   
              
}


//если в памяти ничего, то подключаемся по СмартКоннекту
void SmartConnectWiFi(){
  const uint32_t timeoutSmart = 30000; // 1 min
  uint32_t maxTimeSmart = millis() + timeoutSmart;
     WiFi.beginSmartConfig();
     delay(200);
     Serial.println("Runing SmartConnect");
      while(WiFi.status() != WL_CONNECTED)
      {
      digitalWrite(pinBuiltinLed, LOW);
      delay(200);
      digitalWrite(pinBuiltinLed, HIGH);
      delay(200);
      Serial.print(".");
         if(WiFi.smartConfigDone()){
         Serial.println(F("WiFi Smart Config Done."));
        
         break;
         }
         if ((int32_t)(millis() - maxTimeSmart) >= 0) {
         Serial.println(F(" fail connect SmartConfig!"));
         WiFi.stopSmartConfig();
         SmartConnect();
         break;
         }
      }
      delay(1000);
      Serial.println("");
      Serial.println("");
      WiFi.printDiag(Serial);
      Serial.println(WiFi.localIP());
   Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
   Serial.printf("Password: %s\n", WiFi.psk().c_str());
   password = WiFi.psk();
   ssid = WiFi.SSID();
   WiFi.stopSmartConfig();
  // WiFi.setAutoReconnect(true);
   writeConfig();
}


//--//--//--//-- Работа с EEPROM
//читаем из памяти
bool readConfig() {
  uint16_t offset = 0;

  Serial.println("Reading config from EEPROM");
  for (byte i = 0; i < sizeof(configSign); i++) {
    if (EEPROM.read(offset + i) != configSign[i])
      return false;
  }
  //пропускаем проверочные символы
  offset += sizeof(configSign);
  //переменные для подключения WiFi
  offset = readEEPROMString(offset, ssid);
  offset = readEEPROMString(offset, password);
  //mqtt переменные
  offset = readEEPROMString(offset, mqttServer);
  EEPROM.get(offset, mqttPort);
  offset += sizeof(mqttPort);
  offset = readEEPROMString(offset, mqttUser);
  offset = readEEPROMString(offset, mqttPassword);
  offset = readEEPROMString(offset, mqttClient);
  offset = readEEPROMString(offset, mqttTopic);
  Serial.println(ssid);
  Serial.println(password);
  return true;
}

//пишем в память
void writeConfig() {
  uint16_t offset = 0;

  Serial.println("Writing config to EEPROM");
  //читаем  проверочные символы
  EEPROM.put(offset, configSign);
  offset += sizeof(configSign);
  //переменные для подключения WiFi
  offset = writeEEPROMString(offset, ssid);
  offset = writeEEPROMString(offset, password);
  //mqtt переменные
  offset = writeEEPROMString(offset, mqttServer);
  EEPROM.put(offset, mqttPort);
  offset += sizeof(mqttPort);
  offset = writeEEPROMString(offset, mqttUser);
  offset = writeEEPROMString(offset, mqttPassword);
  offset = writeEEPROMString(offset, mqttClient);
  offset = writeEEPROMString(offset, mqttTopic);
  //ждем на всякий случай, т.к для записи нужно время
  delay(60);
  EEPROM.commit();
  delay(60);
}
//функция для работы с EEPROM
uint16_t readEEPROMString(uint16_t offset, String& str) {
  char buffer[maxStrParamLength + 1];

  buffer[maxStrParamLength] = 0;
  for (byte i = 0; i < maxStrParamLength; i++) {
    if (! (buffer[i] = EEPROM.read(offset + i)))
      break;
  }
  str = String(buffer);

  return offset + maxStrParamLength;
}
//функция для работы с EEPROM
uint16_t writeEEPROMString(uint16_t offset, const String& str) {
  for (byte i = 0; i < maxStrParamLength; i++) {
    if (i < str.length())
      EEPROM.write(offset + i, str[i]);
    else
      EEPROM.write(offset + i, 0);
  }

  return offset + maxStrParamLength;
}
//--//--//--//--



//Страница с настройками
void handleMQTTConfig() {
  String message =
"<!DOCTYPE html>\
<html>\
<head>\
  <title>MQTT Setup</title>\
</head>\
<body>";

  message +=  "<h3>Info</h3>";
  message +=  "Application: " + String(FW_VERSION);
  message +=  "<form action=\"/upd\">";
  message +=  "<button type=\"submit\">Update</button>";
  message +=  "</form>";


  message +=  " <form name=\"mqtt\" method=\"get\" action=\"/store\">\
    <h3>MQTT Setup</h3>\
    Server:<br/>\
    <input type=\"text\" name=\"";
  
  message += String(serverArg) + "\" maxlength=" + String(maxStrParamLength) + " value=\"" + quoteEscape(mqttServer) + "\" onchange=\"document.mqtt.reboot.value=1;\" />\
    (leave blank to ignore MQTT)\
    <br/>\
    Port:<br/>\
    <input type=\"text\" name=\"";

  message += String(portArg) + "\" maxlength=5 value=\"" + String(mqttPort) + "\" onchange=\"document.mqtt.reboot.value=1;\" />\
    <br/>\
    User (if authorization is required on MQTT server):<br/>\
    <input type=\"text\" name=\"";

  message += String(userArg) + "\" maxlength=" + String(maxStrParamLength) + " value=\"" + quoteEscape(mqttUser) + "\" />\
    (leave blank to ignore MQTT authorization)\
    <br/>\
    Password:<br/>\
    <input type=\"password\" name=\"";

  message += String(mqttpswdArg) + "\" maxlength=" + String(maxStrParamLength) + " value=\"" + quoteEscape(mqttPassword) + "\" />\
    <br/>\
    Client:<br/>\
    <input type=\"text\" name=\"";

  message += String(clientArg) + "\" maxlength=" + String(maxStrParamLength) + " value=\"" + quoteEscape(mqttClient) + "\" />\
    <br/>\
    Topic:<br/>\
    <input type=\"text\" name=\"";

  message += String(topicArg) + "\" maxlength=" + String(maxStrParamLength) + " value=\"" + quoteEscape(mqttTopic) + "\" />\
    <p>\
    <input type=\"submit\" value=\"Save\" />\
    <input type=\"hidden\" name=\"";

  message += String(rebootArg) + "\" value=\"0\" />\
  </form>\
</body>\
</html>";

  httpServer.send(200, "text/html", message);
}

//страница для обновления прошивки
void updatFun() {
checkForUpdates();

  String message =
"<!DOCTYPE html>\
<html>\
<head>\
  <title>Update</title>\
  <meta http-equiv=\"refresh\" content=\"5; /mqtt\">\
</head>\
<body>";

  if( newVersion > FW_VERSION ){
     message += "Run update. Hold me please";
  } else{
    message += "Last version";
  }
  message +=
"  <p>\
  Wait for 5 sec. or click <a href=\"/mqtt\">this</a> to return to main page.\
</body>\
</html>";

  httpServer.send(200, "text/html", message);
  
}

String quoteEscape(const String& str) {
  String result = "";
  int start = 0, pos;

  while (start < str.length()) {
    pos = str.indexOf('"', start);
    if (pos != -1) {
      result += str.substring(start, pos) + "&quot;";
      start = pos + 1;
    } else {
      result += str.substring(start);
      break;
    }
  }
  return result;
}


//страница для сохранения настроек в EEPROM
void handleStoreConfig() {
  String argName, argValue;

  Serial.print("/store(");
  for (byte i = 0; i < httpServer.args(); i++) {
    if (i)
      Serial.print(", ");
    argName = httpServer.argName(i);
    Serial.print(argName);
    Serial.print("=\"");
    argValue = httpServer.arg(i);
    Serial.print(argValue);
    Serial.print("\"");

    if (argName == ssidArg) {
      ssid = argValue;
    } else if (argName == passwordArg) {
      password = argValue;
    } else if (argName == serverArg) {
      mqttServer = argValue;
    } else if (argName == portArg) {
      mqttPort = argValue.toInt();
    } else if (argName == userArg) {
      mqttUser = argValue;
    } else if (argName == mqttpswdArg) {
      mqttPassword = argValue;
    } else if (argName == clientArg) {
      mqttClient = argValue;
    } else if (argName == topicArg) {
      mqttTopic = argValue;
    } 
  }
  Serial.println(")");

  writeConfig();

  String message =
"<!DOCTYPE html>\
<html>\
<head>\
  <title>Store Setup</title>\
  <meta http-equiv=\"refresh\" content=\"5; /index.html\">\
</head>\
<body>\
  Configuration stored successfully.";

  if (httpServer.arg(rebootArg) == "1")
    message +=
"  <br/>\
  <i>You must reboot module to apply new configuration!</i>";

  message +=
"  <p>\
  Wait for 5 sec. or click <a href=\"/index.html\">this</a> to return to main page.\
</body>\
</html>";

  httpServer.send(200, "text/html", message);
}


/*
 * MQTT functions
 */

//вытаскиваем данные из топиков, на которые подписались
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;
  Serial.print("MQTT message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
     messageTemp += (char)payload[i]; //супер функция, складывает байты последовательно!
  }
  Serial.println();

  char* topicBody = topic + mqttClient.length() + 1; // Skip "/ClientName" from topic
  if (! strncmp(topicBody, mqttTopic.c_str(), mqttTopic.length())) {
      if (messageTemp == "upd"){
      checkForUpdates();
    } else if (messageTemp == "reb"){
      ESP.restart();
    } 
  }
}

//реконектимся к mqtt
bool mqttReconnect() {
  const uint32_t timeout = 10000;
  static uint32_t lastTime;
  bool result = false;

  if (millis() > lastTime + timeout) {
    Serial.println("Attempting MQTT connection...");
    Serial.println(mqttClient);
    if (mqttUser.length())
      result = pubsubClient.connect(mqttClient.c_str(), mqttUser.c_str(), mqttPassword.c_str());
    else
      result = pubsubClient.connect(mqttClient.c_str());
    if (result) {
      Serial.println(" connected");
      // Resubscribe
      String topic('/');
      topic += mqttClient;
      topic += mqttTopic;
      result = mqtt_subscribe(pubsubClient, topic);
    } else {
      Serial.print(" failed, rc=");
      Serial.println(pubsubClient.state());
    }
    lastTime = millis();
  }

  return result;
}

//подписываемся
bool mqtt_subscribe(PubSubClient& client, const String& topic) {
  Serial.print("Subscribing to ");
  Serial.println(topic);

  return client.subscribe(topic.c_str());
}
//публикуем
bool mqtt_publish(PubSubClient& client, const String& topic, const String& value, boolean retained) {
  Serial.print("Publishing topic ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.println(value);

  return client.publish(topic.c_str(), value.c_str(),retained);
}

//помогаем отправлять данные из других файлов
void helpPublic(String topicc, String myString){
  timeHH ();
  myString += " ";
  myString += hhmmss;
  mqtt_publish(pubsubClient, topicc, myString, false);
}


//Заполняем дату и время
void timeHH (){
  hhmmss = timeClient.getFormattedTime();
  unixt =  timeClient.getEpochTime();

  Serial.print("Unix ");
  Serial.println(unixt); 

  Serial.print("Day ");
  dat = weekday(unixt);
  Serial.println(dat);

  Serial.print("Hourss ");
  time_t hourr =  hour(unixt);
  Serial.println(hourr);
 
  Serial.print("Hourss * 60 = ");
  hourr60 = (hourr + GMT) * 60 ;
  Serial.println(hourr60);
}

//читаем состояние двери
void Door(){
valTec = !digitalRead(Door_Sensor_Pin); // читать Door_Sensor_Pin
if(valTec == 1 && Flag == 0){
  Flag = 1;
  timeHH();
  String myStrings =  "{ \"Value\" : ";
  myStrings  += "\"Open\"";
  myStrings +=  ", \"type\" : ";
  myStrings +=  "\"door\"";
  myStrings += ", \"time\" : ";
  myStrings += unixt;
  myStrings += " , \"iddetect\" : \"";
  myStrings += mqttTopic;
  myStrings += "\"}";

  char msg[255];
  myStrings.toCharArray(msg, 255);

  String topic('/');
  topic += mqttClient;
  topic += mqttTopic;
  topic += '/';
  topic += "detec";
  mqtt_publish(pubsubClient, topic, msg, true);
  
}

if(valTec == 0 && Flag == 1){
  Flag = 0;
  timeHH();
  String myStrings =  "{ \"Value\" : ";
  myStrings += "\"Close\"";
  myStrings +=  ", \"type\" : ";
  myStrings +=  "\"door\"";
  myStrings += ", \"time\" : ";
  myStrings += unixt;
  myStrings += " , \"iddetect\" : \"";
  myStrings += mqttTopic;
  myStrings += "\"}";

  char msg[255];
  myStrings.toCharArray(msg, 255);

  String topic('/');
  topic += mqttClient;
  topic += mqttTopic;
  topic += '/';
  topic += "detec";
  mqtt_publish(pubsubClient, topic, msg, true);
  }
}

//тут отправляем системные данные
void System(){
  if(millis() - timings > 60000 ) {
  timings = millis();
  timeHH();
  int16_t vcc = ESP.getVcc() ;
  String  VccStr = String(vcc);
  uint32_t uptime = millis() / 1000;
  String  myString = "{ ";
  myString += "\"vcc\" : \"";
  myString += VccStr;
  myString += "\", \"time\" : ";
  myString += unixt;
  myString += ", \"uptime\" : ";
  myString += String(uptime);
  myString += " , \"iddetect\" : \"";
  myString += mqttTopic;
  myString += "\"}";
  char msg[255];
  myString.toCharArray(msg, 255);
  
  String topic('/');
  topic += mqttClient;
  topic += mqttTopic;
  topic += '/';
  topic += "system";
  mqtt_publish(pubsubClient, topic, msg, false);
  }
}

//луп для залуп
void loop() {
  if(WiFi.status() != WL_CONNECTED ||
     WiFi.status() == WL_CONNECT_FAILED || 
     WiFi.status() == WL_CONNECTION_LOST ||
     WiFi.status() == WL_DISCONNECTED)
     {
     SmartConnect(); //реконектимся к wiFi в случае чего
     }
  
    httpServer.handleClient(); //крутится вертиться HTTP server

    if (mqttServer.length() && (WiFi.getMode() == WIFI_STA) && WiFi.status() == WL_CONNECTED ) {
    if (! pubsubClient.connected())   
        mqttReconnect();          //переподключаемся в случае чего к MQTT
    if (pubsubClient.connected())
        pubsubClient.loop();    //крутится вертиться MQTT
        timeClient.update();  // запрашиваем время
        System();
        DeepSleep();
  }
    Door();
    delay(1); // For WiFi maintenance

}

