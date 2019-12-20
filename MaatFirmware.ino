//=============================================
//                MA'AT FIRMWARE       
//                    v0.0.1                  
//=============================================

//PINS
#define PIN_SCL_D 4
#define PIN_SCL_C 5

//#define PIN_ROT_B 6
//#define PIN_ROT_A 7
#define PIN_BTN_T 6
#define PIN_BTN_B 7
#define PIN_BTN_R 8
#define PIN_BTN_L 9

#define PIN_BUZ 10

#define PIN_RTC_D 14
#define PIN_RTC_C 15

//LIBRARIES
#include <math.h>

//#include <LucRotary.h>
//LucRotary rotary = LucRotary(PIN_ROT_A, PIN_ROT_B);

#include <DS1302.h>
DS1302 rtc(A0, PIN_RTC_D, PIN_RTC_C);

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 8, 2);

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
boolean left_pressed;
boolean right_pressed;
int rotary_direction;
unsigned long last_rotary_millis;

boolean left_pressdown;
boolean left_pressup;
boolean right_pressdown;
boolean right_pressup;

boolean last_left_pressed;
boolean last_right_pressed;

unsigned long last_millis;

void setup() {
  Serial.begin(9600);
  Serial1.begin(115200);

  scale.begin(PIN_SCL_D, PIN_SCL_C);
  scale.set_scale(scale_calibration);
  scale.tare();

  lcd.init();

  //Time t(1992, 5, 12, 3, 12, 00, Time::kSunday);
  //rtc.time(t);

  /*
  //DEBUG: alarm goes off in 1 minute
  Time t = rtc.time();
  alarmTime = t.hr * 60 + t.min + 1;
  alarmArmed = true;
  disarmTime = 10;
  */
  
  //alarmTime = 7 * 60 + 30; //7:30am

  //runComponentTest();
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
  //LcdPrint("");
  lcd.noBacklight();
  
  //attachInterrupt(digitalPinToInterrupt(PIN_ROT_A), get_rotary, CHANGE);
}

/*
void get_rotary()
{
     if(millis() - last_rotary_millis < 50) return;
    
    int val_a = digitalRead(PIN_ROT_A);
    int val_b = digitalRead(PIN_ROT_B);
    if(val_b == val_a) rotary_direction = 1;
    else rotary_direction = -1;

    last_rotary_millis = millis();
}
*/

void runComponentTest()
{
  LcdPrint("Running Test...");
  Serial.println("Running Test...");
  delay(500);
  Serial.println("Time test:");
  delay(500);
  Time t = rtc.time();
  Serial.println("Time: " + String(t.hr) + ":" + String(t.min));
  LcdPrint("Time: " + String(t.hr) + ":" + String(t.min));
  delay(500);
  
}

void loop() {
  long cur_millis = millis();
  //Get scale reading
  scale_reading = scale.get_units(1);

  //Get input
  left_pressed = digitalRead(PIN_BTN_L);
  right_pressed = digitalRead(PIN_BTN_R);
  int up_pressed = digitalRead(PIN_BTN_T);
  int down_pressed = digitalRead(PIN_BTN_B);
  rotary_direction = up_pressed ? (down_pressed ? 0 : 1) : (down_pressed ? -1 : 0);
  Serial.println(rotary_direction);
  //rotary_direction = rotary.getDirection();

  left_pressdown = left_pressed && !last_left_pressed;
  left_pressup = !left_pressed && last_left_pressed;
  right_pressdown = right_pressed && !last_right_pressed;
  right_pressup = !right_pressed && last_right_pressed;

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
    //noTone(PIN_BUZ);
    
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

    if(rotary_direction != 0 || left_pressed || right_pressed) {
        enterMenu();
    }
  } else if(currentMode == 1) {
    //MENU    

    //noTone(PIN_BUZ);
    showMenu();
    
  } else if(currentMode == 2) {
    if(scale_reading < 50) {
      startDisarmMillis = 0;
      currentDisarmTime = 0;

      //TODO, this will sound horrible
      if(millis() % 1000 < 500) {
        int pitch = (int)((millis() % 1000) / 100);
        if(pitch == 0) tone(PIN_BUZ, 3200);
        else if(pitch == 1) tone(PIN_BUZ, 4000);
        else if(pitch == 2) tone(PIN_BUZ, 3200);
        else if(pitch == 3) tone(PIN_BUZ, 4000);
        else tone(PIN_BUZ, 3600);
      } else noTone(PIN_BUZ);
      
    } else {
      //noTone(PIN_BUZ);
      if(startDisarmMillis == 0) {
         startDisarmMillis = millis();
         noTone(PIN_BUZ);
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
  
 

  last_left_pressed = left_pressed;
  last_right_pressed = right_pressed;
  lastTime = currentTime;
  last_millis = cur_millis;
  rotary_direction = 0;
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
    lcd.print("");

    menuPosition = 0;
    topLevelMenuPosition = 0;
    currentMode = 0;
}

void showMenu()
{
    if(menuPosition == 0) {
        //top level
    
        //change top level position using rotary encoder
        topLevelMenuPosition += rotary_direction;
        if(topLevelMenuPosition < 0) topLevelMenuPosition = 0;
        if(topLevelMenuPosition >= TOP_LEVEL_MENU_LENGTH) topLevelMenuPosition = TOP_LEVEL_MENU_LENGTH - 1;
    
        //Display top level menu on screen
        if(topLevelMenuPosition == 0) {
            LcdPrint(top_level_menu[0] + (alarmArmed ? "ON" : "OFF"));
        } else {
            LcdPrint(top_level_menu[topLevelMenuPosition]);
        }
    
        //If right button pressed, either toggle alarm, or enter sub-menu
        if(right_pressdown) {
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
        if(left_pressdown) {
            exitMenu();
        }
     } else if(menuPosition == 1) {
        //set alarm
    
        //update time using rotary encoder in 10 minute increments
        dirtyAlarmTime += rotary_direction * 10;
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
        if(left_pressdown) {
            menuPosition = 0;
        } else if(right_pressdown) {
            alarmTime = dirtyAlarmTime;
            menuPosition = 0;
        }
     } else if(menuPosition == 2) {
        //set time
    
        //update time using rotary encoder in 1 minute intervals
        dirtyTime += rotary_direction;
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
        if(left_pressdown) {
            menuPosition = 0;
        } else if(right_pressdown) {
            Time t(1992, 05, 12, rawHours, minutes, 0, Time::kSunday);
            rtc.time(t);
            menuPosition = 0;
        }
     } else if(menuPosition == 3) {
        //set disarm time
    
        dirtyDisarmTime += rotary_direction * 10;
        if(dirtyDisarmTime < 0) dirtyDisarmTime = 0;
        if(dirtyDisarmTime > 999) dirtyDisarmTime = 999;
        
        LcdPrint("DISARM: " + String(dirtyDisarmTime) + " sec");
        //Discard changes on left pressed, confirm changes on right pressed
        if(left_pressdown) {
            menuPosition = 0;
        } else if(right_pressdown) {
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
  String a = "";
  String b = "";
  if(value.length() < 8) {
    a = value;
    while(a.length() < 8) a += " ";
    b = "        ";
  } else {
    a = value.substring(0, 8);
    b = value.substring(8);
    while(b.length() < 8) b += " ";
  }
  
  lcd.setCursor(0, 0);
  lcd.print(a);
  lcd.setCursor(0, 1);
  lcd.print(b);
}
