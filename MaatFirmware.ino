//=============================================
//                MA'AT FIRMWARE       
//                    v1.0.1                  
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
//-2 = scale calibration mode
//-1 = not inititlized
//0 = idle
//1 = menu
//2 = alarm
//3 = uploading
int current_mode = -1;
int menu_mode = 0;
int alarm_mode = 0;

//IDLE VARIABLES
boolean idle_showing_reading = false;

//MENU VARIABLES
#define TOP_LEVEL_MENU_LENGTH 5
String top_level_menu[] = {"Alarm Armed: ", "Set Alarm", "Set Time", "Set Disarm Time", "Tare Scale"};
int top_level_menu_position = 0;
//0 = top level
//1 = set alarm
//2 = set time
//3 = disarm time
int menu_position = 0;
int dirty_alarm_time;
int dirty_time;
int dirty_disarm_time;
int time_tick_direction;

//ALARM VARIABLES
int alarmTime = 420;
int disarmTime = 120;
boolean alarmArmed = false;
int currentDisarmTime = 0;
unsigned long startDisarmMillis;

//TIME VARIABLES
int current_time;
int last_time;
boolean alarm_triggered;

byte valid_note_pitches[] = {60, 62, 64, 65, 67, 69, 71, 72};
int valid_note_frequencies[] = {0, 0, 0, 0, 0, 0, 0, 0}; //TODO
int valid_note_durations[] = {100, 200, 400, 800};
int next_pitch_millis;

//SCALE VARIABLES
float scale_calibration = 20580;
float calibration_interval = 1;
float scale_reading;
String scale_reading_string;
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

  lcd.init();

  /*
  //DEBUG: alarm goes off in 1 minute
  Time t = rtc.time();
  alarmTime = t.hr * 60 + t.min + 1;
  alarmArmed = true;
  disarmTime = 10;
  */

  //hold the set button when turning on to enter scale calibration mode
  if(digitalRead(PIN_BTN_SET)) {
    current_mode = -2;
  } else {
    current_mode = 0;
  }

  if(current_mode == -2) {
    lcd.backlight();
    lcdPrintCenter("Entering Scale", 0);
    lcdPrintCenter("Calibration Mode", 1);
    delay(2000);
    return;
  }

  lcd.backlight();
  lcdPrintCenter("Initializing", 0);
  lcdPrintCenter("|--------------|", 1);
  Serial1.print("AT+RST\r\n");
  for(int i = 0; i < 9; i++) {
    String val = "|";
    for(int j = 0; j < 9; j++) {
        if(j < i) val += "#";
        else val += "-";
    }
    val += "-----|";
    lcdPrintCenter(val, 1);
    delay(250);
  }
  
  Serial1.print("AT+CIPMUX=1\r\n");
  for(int i = 0; i < 6; i++) {
    String val = "";
    for(int j = 0; j < 6; j++) {
        if(j < i) val += "#";
        else val += "-";
    }
    val += "|";
    val = "|########" + val;
    lcdPrintCenter(val, 1);
    Serial.println(val);
    delay(250);
  }
  lcdPrintCenter("|##############|", 1);
  delay(500);
  
  lcdPrintCenter("|    -Done-    |", 1);
  delay(1000);
  
  lcdClear();
  lcd.noBacklight();
  
  scale.tare();
}

void loop() {
  long cur_millis = millis();
  
  //Get scale reading
  scale_reading = scale.get_units(1);
  long reading_mult = (long)(scale_reading * 10.0);
  scale_reading_string = String((float)reading_mult / 10.0) + " kg";

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

  //Get time
  Time t = rtc.time();
  current_time = t.hr * 60 + t.min;
  alarm_triggered = (current_time == alarmTime && last_time != alarmTime && alarmArmed);

  if(alarm_triggered) {
    startAlarm();
  }

  if(current_mode == -2) {
    //SCALE CALIBRATION

    float new_calibration = scale_calibration + axis_direction * calibration_interval;
    if(new_calibration != scale_calibration) {
        scale_calibration = new_calibration;
        scale.set_scale(scale_calibration);
    }
    
    if(back_pressdown) {
        lcdPrintCenter("Tare", 0);
        lcdPrintCenter("Scale", 1);
        delay(1000);
        scale.tare();
        delay(1000);
    }

    if(set_pressdown) {
        if(calibration_interval < 1) calibration_interval = 1;
        else if(calibration_interval < 10) calibration_interval = 10;
        else if(calibration_interval < 100) calibration_interval = 100;
        else if(calibration_interval < 1000) calibration_interval = 1000;
        else calibration_interval = 0.1;
    }
    lcdPrintCenter("Factor: " + String(scale_calibration), 0);
    lcdPrintCenter(String(scale_reading) + " kg", 1);
  }
  if(current_mode == -1) return;
  if(current_mode == 0) {
    //IDLE

    Time t = rtc.time();
    lcdPrintCenter(getTimeString(t.hr, t.min), 0);
    
    if(scale_reading > 10) {
        if(!idle_showing_reading) {
            lcd.backlight();
        }
        lcdPrintCenter(scale_reading_string, 1);
        idle_showing_reading = true;
    } else {
        if(idle_showing_reading) {
            lcd.noBacklight();
            lcdPrint("", 1);
        }
        idle_showing_reading = false;
    }

    if(axis_direction != 0 || back_pressdown || set_pressdown) {
        enterMenu();
    }
  } else if(current_mode == 1) {
    //MENU    

    showMenu();
    
  } else if(current_mode == 2) {
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
    
          current_mode = 3;
          uploadReading(average_reading);
      } else {
         scale_readings[scale_readings_index] = scale_reading;
         scale_readings_index++;
         if(scale_readings_index >= SCALE_READINGS_LENGTH) scale_readings_index = 0;
         int percent = (int)((float)(currentDisarmTime * 100) / (float)disarmTime);
         String val = "|";
         for(float i = 0.0; i < 100; i += 7.142857) {
            if(i < percent) val += "#";
            else val += "-";
         }
         val += "|";
         lcdPrintCenter("Disarming", 0);
         lcdPrint(val, 1);
      }
    }
  } else if(current_mode == 3) {
  }
  
 

  last_back_pressed = back_pressed;
  last_set_pressed = set_pressed;
  last_time = current_time;
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
    current_mode = 1;
}

void exitMenu()
{
    lcd.noBacklight();
    lcdClear();

    menu_position = 0;
    top_level_menu_position = 0;
    current_mode = 0;
}

void showMenu()
{
    if(menu_position == 0) {
        //top level
    
        //change top level position using rotary encoder
        top_level_menu_position += axis_direction;
        if(top_level_menu_position < 0) top_level_menu_position = 0;
        if(top_level_menu_position >= TOP_LEVEL_MENU_LENGTH) top_level_menu_position = TOP_LEVEL_MENU_LENGTH - 1;

        String bar = "|";
        for(int i = 0; i < TOP_LEVEL_MENU_LENGTH; i++) {
            if(i == top_level_menu_position) bar += "O";
            else bar += "-";
        }
        bar += "|";
        lcdPrintCenter(bar, 0);
    
        //Display top level menu on screen
        if(top_level_menu_position == 0) {
            lcdPrint(top_level_menu[0] + (alarmArmed ? "ON" : "OFF"), 1);
        } else if(top_level_menu_position == 4) {
            lcdPrintCenter("Tare (" + String(scale_reading) + " kg)", 1);
        } else {
            lcdPrintCenter(top_level_menu[top_level_menu_position], 1);
        }
    
        //If right button pressed, either toggle alarm, or enter sub-menu
        if(set_pressdown) {
            if(top_level_menu_position == 0) {
                alarmArmed = !alarmArmed;
            } else if(top_level_menu_position == 4) {
                lcdPrintCenter("Taring Scale...", 1);
                delay(500);
                scale.tare();
                lcdPrintCenter("Done", 1);
                delay(500);         
            } else {
                menu_position = top_level_menu_position;
                if(menu_position == 1) {
                    dirty_alarm_time = alarmTime;
                } else if(menu_position == 2) {
                    Time t = rtc.time();
                    dirty_time = t.hr * 60 + t.min;
                } else if(menu_position == 3) {
                    dirty_disarm_time = disarmTime;
                }
            }
        }
    
        //If left button pressed, exit menu
        if(back_pressdown) {
            exitMenu();
        }
     } else if(menu_position == 1) {
        //set alarm
        lcdPrintCenter(top_level_menu[1], 0);
    
        //update time using rotary encoder in 10 minute increments
        dirty_alarm_time += axis_direction * 10;
        if(dirty_alarm_time < 0) dirty_alarm_time = 1430;
        if(dirty_alarm_time > 1430) dirty_alarm_time = 0;
    
        //print alarm time to display
        int hours = (int)floor(dirty_alarm_time / 60);
        int minutes = dirty_alarm_time % 60;
        lcdPrintCenter(getTimeString(hours, minutes), 1);
    
        //Discard changes on left pressed, confirm changes on right pressed
        if(back_pressdown) {
            menu_position = 0;
        } else if(set_pressdown) {
            alarmTime = dirty_alarm_time;
            menu_position = 0;
        }
     } else if(menu_position == 2) {
        //set time
        lcdPrintCenter(top_level_menu[2], 0);
    
        //update time using rotary encoder in 1 minute intervals
        dirty_time += axis_direction;
        if(dirty_time < 0) dirty_time = 1439;
        if(dirty_time > 1439) dirty_time = 0;
    
        //print time to display
        int hours = (int)floor(dirty_time / 60);
        int raw_hours = hours;
        int minutes = dirty_time % 60;
        lcdPrintCenter(getTimeString(hours, minutes), 1);
    
        //Discard changes on left pressed, confirm changes on right pressed
        if(back_pressdown) {
            menu_position = 0;
        } else if(set_pressdown) {
            Time t(1992, 05, 12, raw_hours, minutes, 0, Time::kSunday);
            rtc.time(t);
            menu_position = 0;
        }
     } else if(menu_position == 3) {
        //set disarm time
        lcdPrintCenter(top_level_menu[3], 0);
    
        dirty_disarm_time += axis_direction * 10;
        if(dirty_disarm_time < 0) dirty_disarm_time = 0;
        if(dirty_disarm_time > 999) dirty_disarm_time = 999;
        
        lcdPrintCenter(String(dirty_disarm_time) + " seconds", 1);
        //Discard changes on left pressed, confirm changes on right pressed
        if(back_pressdown) {
            menu_position = 0;
        } else if(set_pressdown) {
            disarmTime = dirty_disarm_time;
            menu_position = 0;
        }
     }
}

void startAlarm()
{
    current_mode = 2;
    lcd.backlight();
    lcdPrintCenter("Rise and Shine!!", 0);
    int hours = (int)(alarmTime / 60);
    int minutes = alarmTime % 60;
    lcdPrintCenter("It's " + getTimeString(hours, minutes) + "!", 1);
}

void uploadReading(float reading)
{
    lcd.backlight();
    lcdPrintCenter("Disarming", 0);
    lcdPrint("|    -Done-    |", 1);
    delay(500);
    
    lcdPrint("Uploading Weight", 0);
    lcdPrint("|--------------|", 1);
    Serial1.print("AT+CIPSTART=4,\"TCP\",\"www.lachlansleight.com\",80\r\n");
    for(int i = 0; i < 4; i++) {
        String val = "|";
        for(int j = 0; j < 4; j++) {
            if(j < i) val += "#";
            else val += "-";
        }
        val += "----------|";
        lcdPrintCenter(val, 1);
        delay(250);
    }
    
    String message = String(reading);
    String request = "POST /sandbox/post_scale_reading.php HTTP/1.1\r\nHost: www.lachlansleight.com\r\nContent-Type: text/plain\r\nContent-Length: " + String(message.length()) + "\r\n\r\n" + message + "\r\n\r\n";
    int count = request.length();
    Serial1.print("AT+CIPSEND=4," + String(count) + "\r\n");
    for(int i = 0; i < 2; i++) {
        String val = "|####";
        for(int j = 0; j < 2; j++) {
            if(j < i) val += "#";
            else val += "-";
        }
        val += "--------|";
        lcdPrintCenter(val, 1);
        delay(250);
    }
    
    Serial1.print(request);
    for(int i = 0; i < 8; i++) {
        String val = "|######";
        for(int j = 0; j < 8; j++) {
            if(j < i) val += "#";
            else val += "-";
        }
        val += "|";
        lcdPrintCenter(val, 1);
        delay(250);
    }
    lcdPrint("|##############|", 1);
    delay(500);
    
    lcdPrint("|    -Done-    |", 1);
    delay(1000);
    
    lcdClear();
    lcd.noBacklight();
    current_mode = 0;
}

String getTimeString(int hours, int minutes)
{
    boolean is_pm = hours > 11;
    if(hours > 12) hours -= 12;
    String minutes_string = minutes == 0 ? "00" : (minutes < 10 ? "0" + String(minutes) : String(minutes));
    String hours_string = hours == 0 ? "00" : (hours < 10 ? "0" + String(hours) : String(hours));
    return hours_string + ":" + minutes_string + " " + (is_pm ? "PM" : "AM");
}

void lcdPrint(String value)
{
    lcdPrint(value, 0);
}
void lcdPrint(String value, int row)
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

void lcdPrintCenter(String value)
{
    lcdPrintCenter(value, 0);
}
void lcdPrintCenter(String value, int row)
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

void lcdClear()
{
    lcdPrint("", 0);
    lcdPrint("", 1);
}
