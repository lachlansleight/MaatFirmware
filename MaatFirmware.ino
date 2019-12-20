//=============================================
//                MA'AT FIRMWARE       
//                    v1.0.0                  
//=============================================

//PINS
#define PIN_SCL_DAT 4
#define PIN_SCL_SCK 5

#define PIN_BUZZER 6

#define PIN_BTN_SET 14
#define PIN_BTN_UP 16
#define PIN_BTN_DWN 10

#define PIN_RTC_DAT 8
#define PIN_RTC_CLK 7
#define PIN_RTC_RST 9

#define PIN_TEMP A3

//LIBRARIES
#include <math.h>

#include <DS1302.h>
DS1302 rtc(PIN_RTC_RST, PIN_RTC_DAT, PIN_RTC_CLK);

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2);

#include <HX711.h>
HX711 scale;


//MODE VARIABLES
//-1 = not inititlized
//0 = idle
//1 = menu
//2 = alarm
//3 = uploading
int currentMode = -1;
int menuMode = 0;
int alarmMode = 0;

//IDLE VARIABLES
boolean idle_showing_reading = false;

//MENU VARIABLES
#define TOP_LEVEL_MENU_LENGTH 4
String top_level_menu[] = {"1/4: Alarm: ", "2/4: SET ALARM", "3/4: SET TIME", "4/4: DISARM TIME"};
int topLevelMenuPosition = 0;
//0 = top level
//1 = set alarm
//2 = set time
//3 = disarm time
int menuPosition = 0;
int dirtyAlarmTime;
int dirtyTime;
int dirtyDisarmTime;
int timeTickDirection;

//ALARM VARIABLES
int alarmTime = 420;
int disarmTime = 120;
boolean alarmArmed = false;
int currentDisarmTime = 0;
unsigned long startDisarmMillis;

//TIME VARIABLES
int currentTime;
int lastTime;
boolean alarmTriggered;

byte validNotePitches[] = {60, 62, 64, 65, 67, 69, 71, 72};
int validNoteFrequencies[] = {0, 0, 0, 0, 0, 0, 0, 0}; //TODO
int validNoteDurations[] = {100, 200, 400, 800};
int nextPitchMillis;

//SCALE VARIABLES
float scale_calibration = 6150;
float scale_reading;
#define SCALE_READINGS_LENGTH 100
float scale_readings[100];
int scale_readings_index;
float average_reading;

//INPUT VARIABLES
int back_hold_time = 500;
boolean back_pressed;
boolean set_pressed;
int axis_direction;

boolean back_pressdown;
boolean back_pressup;
boolean set_pressdown;
boolean set_pressup;

boolean last_back_pressed;
boolean last_set_pressed;

boolean back_triggered;

unsigned long set_press_time;

unsigned long last_millis;

void setup() {
  Serial.begin(9600);
  Serial1.begin(115200);

  scale.begin(PIN_SCL_DAT, PIN_SCL_SCK);
  scale.set_scale(scale_calibration);
  scale.tare();

  lcd.init();

  /*
  //DEBUG: alarm goes off in 1 minute
  Time t = rtc.time();
  alarmTime = t.hr * 60 + t.min + 1;
  alarmArmed = true;
  disarmTime = 10;
  */

  currentMode = 0;

  lcd.backlight();
  LcdPrint("Initializing");
  Serial1.print("AT+RST\r\n");
  delay(1000);
  
  LcdPrint("Initializing.");
  delay(1000);
  
  LcdPrint("Initializing..");
  Serial1.print("AT+CIPMUX=1\r\n");
  LcdPrint("Initializing...");
  delay(1000);
  
  LcdPrint("Done!");
  delay(1000);
  
  LcdPrint("");
  lcd.noBacklight();
}

void loop() {
  long cur_millis = millis();
  
  //Get scale reading
  scale_reading = scale.get_units(1);

  //Get input
  set_pressed = digitalRead(PIN_BTN_SET);
  int up_pressed = digitalRead(PIN_BTN_UP);
  int down_pressed = digitalRead(PIN_BTN_DWN);
  axis_direction = up_pressed ? (down_pressed ? 0 : 1) : (down_pressed ? -1 : 0);

  set_pressdown = set_pressed && !last_set_pressed;
  set_pressup = !set_pressed && last_set_pressed;

  if(set_pressdown) {
    set_press_time = millis();
  }

  if(set_pressup) {
    if(millis() - set_press_time < back_hold_time) set_pressdown = true;
    else set_pressdown = false;

    back_triggered = false;
  } else {
    set_pressdown = false;
  }

  if(set_pressed && (millis() - set_press_time > back_hold_time) && !back_triggered) {
    back_pressdown = true;
    back_triggered = true;
  } else {
    back_pressdown = false;
  }

  if(set_pressdown) Serial.println("Set");
  if(back_pressdown) Serial.println("Back");
  else Serial.println(String(set_pressed) + "  " + String(up_pressed) + "  " + String(down_pressed));

  //Get time
  Time t = rtc.time();
  currentTime = t.hr * 60 + t.min;
  alarmTriggered = (currentTime == alarmTime && lastTime != alarmTime && alarmArmed);

  if(alarmTriggered) {
    startAlarm();
  }

  if(currentMode == -1) return;
  if(currentMode == 0) {
    //IDLE
    
    if(scale_reading > 10) {
        if(!idle_showing_reading) {
            lcd.backlight();
        }
        LcdPrint(String(scale_reading) + " kg");
        idle_showing_reading = true;
    } else {
        if(idle_showing_reading) {
            lcd.noBacklight();
            LcdPrint("");
        }
        idle_showing_reading = false;
    }

    if(axis_direction != 0 || back_pressdown || set_pressdown) {
        enterMenu();
    }
  } else if(currentMode == 1) {
    //MENU    

    showMenu();
    
  } else if(currentMode == 2) {
    if(scale_reading < 50) {
      startDisarmMillis = 0;
      currentDisarmTime = 0;

      //TODO: make the tone nicer / more interesting
      if(millis() % 1000 < 500) {
        int pitch = (int)((millis() % 1000) / 100);
        if(pitch == 0) tone(PIN_BUZZER, 3200);
        else if(pitch == 1) tone(PIN_BUZZER, 4000);
        else if(pitch == 2) tone(PIN_BUZZER, 3200);
        else if(pitch == 3) tone(PIN_BUZZER, 4000);
        else tone(PIN_BUZZER, 3600);
      } else noTone(PIN_BUZZER);
      
    } else {
      if(startDisarmMillis == 0) {
         startDisarmMillis = millis();
         noTone(PIN_BUZZER);
      }
      currentDisarmTime = (int)((float)(cur_millis - startDisarmMillis) / 1000.0);
      if(currentDisarmTime > disarmTime) {
         float sum = 0;
          for(int i = 0; i < SCALE_READINGS_LENGTH; i++) {
            sum += scale_readings[i];
          }
          average_reading = sum / (float)SCALE_READINGS_LENGTH;
    
          currentMode = 3;
          uploadReading(average_reading);
      } else {
         scale_readings[scale_readings_index] = scale_reading;
         scale_readings_index++;
         if(scale_readings_index >= SCALE_READINGS_LENGTH) scale_readings_index = 0;
         int percent = (int)((float)(currentDisarmTime * 100) / (float)disarmTime);
         LcdPrint("Disarming: " + String(percent) + "%");
      }
    }
  } else if(currentMode == 3) {
  }
  
 

  last_back_pressed = back_pressed;
  last_set_pressed = set_pressed;
  lastTime = currentTime;
  last_millis = cur_millis;
  axis_direction = 0;
}

void enterMenu()
{
    //reset idle variables
    idle_showing_reading = false;

    //turn on LCD
    lcd.backlight();

    //Enter menu mode
    currentMode = 1;
}

void exitMenu()
{
    lcd.noBacklight();
    LcdPrint("");

    menuPosition = 0;
    topLevelMenuPosition = 0;
    currentMode = 0;
}

void showMenu()
{
    if(menuPosition == 0) {
        //top level
    
        //change top level position using rotary encoder
        topLevelMenuPosition += axis_direction;
        if(topLevelMenuPosition < 0) topLevelMenuPosition = 0;
        if(topLevelMenuPosition >= TOP_LEVEL_MENU_LENGTH) topLevelMenuPosition = TOP_LEVEL_MENU_LENGTH - 1;
    
        //Display top level menu on screen
        if(topLevelMenuPosition == 0) {
            LcdPrint(top_level_menu[0] + (alarmArmed ? "ON" : "OFF"));
        } else {
            LcdPrint(top_level_menu[topLevelMenuPosition]);
        }
    
        //If right button pressed, either toggle alarm, or enter sub-menu
        if(set_pressdown) {
            if(topLevelMenuPosition == 0) {
                alarmArmed = !alarmArmed;
            } else {
                menuPosition = topLevelMenuPosition;
                if(menuPosition == 1) {
                    dirtyAlarmTime = alarmTime;
                } else if(menuPosition == 2) {
                    Time t = rtc.time();
                    dirtyTime = t.hr * 60 + t.min;
                } else if(menuPosition == 3) {
                    dirtyDisarmTime = disarmTime;
                }
            }
        }
    
        //If left button pressed, exit menu
        if(back_pressdown) {
            exitMenu();
        }
     } else if(menuPosition == 1) {
        //set alarm
    
        //update time using rotary encoder in 10 minute increments
        dirtyAlarmTime += axis_direction * 10;
        if(dirtyAlarmTime < 0) dirtyAlarmTime = 1430;
        if(dirtyAlarmTime > 1430) dirtyAlarmTime = 0;
    
        //print alarm time to display
        int hours = (int)floor(dirtyAlarmTime / 60);
        int minutes = dirtyAlarmTime % 60;
        boolean PM = hours > 11;
        if(hours > 12) hours -= 12;
        String minutesString = minutes == 0 ? "00" : (minutes < 10 ? "0" + String(minutes) : String(minutes));
        String hoursString = hours == 0 ? "00" : (hours < 10 ? "0" + String(hours) : String(hours));
        LcdPrint("ALARM: " + hoursString + ":" + minutesString + " " + (PM ? "PM" : "AM"));
    
        //Discard changes on left pressed, confirm changes on right pressed
        if(back_pressdown) {
            menuPosition = 0;
        } else if(set_pressdown) {
            alarmTime = dirtyAlarmTime;
            menuPosition = 0;
        }
     } else if(menuPosition == 2) {
        //set time
    
        //update time using rotary encoder in 1 minute intervals
        dirtyTime += axis_direction;
        if(dirtyTime < 0) dirtyTime = 1439;
        if(dirtyTime > 1439) dirtyTime = 0;
    
        //print time to display
        int hours = (int)floor(dirtyTime / 60);
        int rawHours = hours;
        int minutes = dirtyTime % 60;
        boolean PM = hours > 11;
        if(hours > 12) hours -= 12;
        String minutesString = minutes == 0 ? "00" : (minutes < 10 ? "0" + String(minutes) : String(minutes));
        String hoursString = hours == 0 ? "00" : (hours < 10 ? "0" + String(hours) : String(hours));
        LcdPrint("TIME: " + hoursString + ":" + minutesString + " " + (PM ? "PM" : "AM"));
    
        //Discard changes on left pressed, confirm changes on right pressed
        if(back_pressdown) {
            menuPosition = 0;
        } else if(set_pressdown) {
            Time t(1992, 05, 12, rawHours, minutes, 0, Time::kSunday);
            rtc.time(t);
            menuPosition = 0;
        }
     } else if(menuPosition == 3) {
        //set disarm time
    
        dirtyDisarmTime += axis_direction * 10;
        if(dirtyDisarmTime < 0) dirtyDisarmTime = 0;
        if(dirtyDisarmTime > 999) dirtyDisarmTime = 999;
        
        LcdPrint("DISARM: " + String(dirtyDisarmTime) + " sec");
        //Discard changes on left pressed, confirm changes on right pressed
        if(back_pressdown) {
            menuPosition = 0;
        } else if(set_pressdown) {
            disarmTime = dirtyDisarmTime;
            menuPosition = 0;
        }
     }
}

void startAlarm()
{
    currentMode = 2;
    lcd.backlight();
}

void uploadReading(float reading)
{
    lcd.backlight();
    LcdPrint("Connecting...");
    Serial1.print("AT+CIPSTART=4,\"TCP\",\"www.lachlansleight.com\",80\r\n");
    delay(1000);
    
    LcdPrint("Sending...");
    String message = String(reading);
    String request = "POST /sandbox/post_scale_reading.php HTTP/1.1\r\nHost: www.lachlansleight.com\r\nContent-Type: text/plain\r\nContent-Length: " + String(message.length()) + "\r\n\r\n" + message + "\r\n\r\n";
    int count = request.length();
    Serial1.print("AT+CIPSEND=4," + String(count) + "\r\n");
    delay(500);
    
    Serial1.print(request);
    delay(2000);
    
    LcdPrint("Done!");
    delay(1000);
    
    LcdPrint("");
    lcd.noBacklight();
    currentMode = 0;
}

void LcdPrint(String value)
{
    LcdPrint(value, 0);
}
void LcdPrint(String value, int row)
{
  if(value.length() > 16) {
    value = value.substring(16);
  }
  while(value.length() < 16) {
    value += " ";
  }
  lcd.setCursor(0, row);
  lcd.print(value);
}

void LcdPrintCenter(String value)
{
    LcdPrintCenter(value, 0);
}
void LcdPrintCenter(String value, int row)
{
  if(value.length() > 16) {
    value = value.substring(16);
  }

  int spaceCount = 16 - value.length();
  while(spaceCount > 0) {
    if(spaceCount % 2 == 0) value = value + " ";
    else value = " " + value;
    spaceCount--;
  }
  lcd.setCursor(0, row);
  lcd.print(value);
}
