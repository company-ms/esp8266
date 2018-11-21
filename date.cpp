#include <Arduino.h>

uint32_t  sleepTime = 600000000; // 10 мин.
//600000000; // 10 мин.
//60000000; // 1 мин.

int32_t previousMillisTimeSleep = 0;
int32_t intervalTimeSleep = 60000; //переодичьность проверки для сна



time_t extern hourr60;
int extern dat;
extern long unixt;//тянем переменную из .ino

void timeHH();

//функция сна по времени
//09:00 = 540
//21:00  = 1260
void DeepSleep(){
  if (unixt > 1539952902 and ( millis() - previousMillisTimeSleep > intervalTimeSleep)){
    
  previousMillisTimeSleep = millis(); 
  timeHH();
    if(hourr60 <= 540 or hourr60 >= 1260 or (dat==7 /*спим если сб или вс*/ or dat==1) ){
      Serial.println("");
    Serial.println("sleeppp");
    Serial.println("");
    ESP.deepSleep(sleepTime);
    }else {
      Serial.println("");
    Serial.println("No sleep");
      Serial.println("");
    }
  } 
}
