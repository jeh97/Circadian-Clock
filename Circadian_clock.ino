#include <Wire.h>
#include <DS3231.h>



#include <math.h>
#include <time.h>

#define LATITUDE 30.2672
#define LONGITUDE -97.7431


const int redPin = 9;
const int greenPin = 10;
const int bluePin = 11;
const int masterPotPin = 3;   // Potentiometer for master brightness
const int redPotPin = 0;      // Potentiometer for red brightness
const int greenPotPin = 1;    // Potentiometer for green brightness
const int bluePotPin = 2;     // Potentiometer for blue brightness

const int modePin = 5;        // Switch between circadian clock or basic light


const long max_red = 255;
const long max_green = 255;
const long max_blue = 255;

int masterVal = 0;
int redVal = 0;
int greenVal = 0;
int blueVal = 0;

int brightness = 255;

byte colors[3] = {0,0,0};

int val = 0;


long cur_sunrise_day = 2415018;
double _sunrise_t   = .25;
double _solarnoon_t = .50;
double _sunset_t    = .75;

double TO_RAD = acos(-1.0)/180.0;;
double TO_DEG = 180.0/acos(-1.0);

tm sunset_tm;
tm sunrise_tm;
tm current_tm;

DS3231 RTCclock;
RTCDateTime dt;


/**
* Method to get the integer number of days from 1/1/1900 to the given time
* @param when the time
* @return the number of days since 1/1/1900
*/
long days_since_1900(time_t when) {
  tm* timeinfo;
  timeinfo = localtime(&when);
  long days = timeinfo->tm_yday + 1;
//  Serial.print("days: "); Serial.println(days);
  long year = timeinfo->tm_year + 1900 - 1;
//  Serial.print("year: "); Serial.println(year);
  long days_since_0001_ = 365 * year + year/400 - year/100 + year/4 + days;
//  Serial.print("days_since_0001_: "); Serial.println(days_since_0001_);
  long days_since_1900_ = days_since_0001_ - 693595L;
//  Serial.print("days_since_1900_: "); Serial.println(days_since_1900_);
  free(timeinfo);
  return days_since_1900_;
  }

/**
* Method to make a time_t value for the given Y/M/D combo
*/
time_t time_for_date(int year, int month, int day) {
  time_t when;
  tm* timeinfo;
  timeinfo=localtime(&when);
  timeinfo->tm_year = year;
  timeinfo->tm_mon = month;
  timeinfo->tm_mday = day;
  timeinfo->tm_sec = 0;
  timeinfo->tm_min = 0;
  timeinfo->tm_hour = 12;
  timeinfo->tm_isdst = 0;
  when = mktime(timeinfo);
//  Serial.print(timeinfo->tm_year);
//  Serial.print("/");
//  Serial.print(timeinfo->tm_mon);
//  Serial.print("/");
//  Serial.println(timeinfo->tm_mday);

  timeinfo = localtime(&when);
//  Serial.print(timeinfo->tm_year);
//  Serial.print("/");
//  Serial.print(timeinfo->tm_mon);
//  Serial.print("/");
//  Serial.println(timeinfo->tm_mday);
  when = mktime(timeinfo);
//  Serial.print("When: "); Serial.println(when);
  free(timeinfo);
  return when;
  }

/**
* Method to find the struct tm representing the given fraction of a day
*/
tm* timefromdecimalday(double day,tm* timeinfo) {
  double hours  = 24.0*day;
  int h       = hours;
  double minutes  = (hours-h)*60;
  int m       = minutes;
  double seconds  = (minutes - m)*60;
  int s       = seconds;
  timeinfo->tm_hour = h;
  timeinfo->tm_min = m;
  timeinfo->tm_sec = s;
  return timeinfo;
  }

void calculate() {
  time_t when = get_time();
  
  tm* timeinfo = localtime(&when);
  long day = days_since_1900(when);
  
  double time = (timeinfo->tm_hour + timeinfo->tm_min/60.0 + timeinfo->tm_sec/3600.0)/24.0;
  double timezone = -5;

  double Jday = day+2415018.5+time-timezone/24;   // Julian day  
  double Jcent = (Jday-2451545.0)/36525.0;         // Julian century 

  
  double Manom = 357.52911+Jcent*(35999.05029-0.0001537*Jcent);
  double Mlong    = 280.46646+fmod(Jcent*(36000.76983+Jcent*0.0003032),360); 
  double Eccent   = 0.016708634-Jcent*(0.000042037+0.0001537*Jcent);
  double Mobliq   = 23+(26+((21.448-Jcent*(46.815+Jcent*(0.00059-Jcent*0.001813))))/60)/60;
  double obliq    = Mobliq+0.00256*cos(TO_RAD*(125.04-1934.136*Jcent));
  double vary     = tan(TO_RAD*(obliq/2))*tan(TO_RAD*(obliq/2));
  double Seqcent  = sin(TO_RAD*(Manom))*(1.914602-Jcent*(0.004817+0.000014*Jcent))+sin(TO_RAD*(2*Manom))*(0.019993-0.000101*Jcent)+sin(TO_RAD*(3*Manom))*0.000289;  
  double Struelong= Mlong+Seqcent;
  double Sapplong = Struelong-0.00569-0.00478*sin(TO_RAD*(125.04-1934.136*Jcent));
  double declination = (asin(sin(TO_RAD*(obliq))*sin(TO_RAD*(Sapplong))))/TO_RAD; 

  double eqtime   = 4*(vary*sin(2*TO_RAD*(Mlong))/TO_RAD-2*Eccent*sin(TO_RAD*(Manom))+4*Eccent*vary*sin(TO_RAD*(Manom))*cos(2*TO_RAD*(Mlong))-0.5*vary*vary*sin(4*TO_RAD*(Mlong))-1.25*Eccent*Eccent*sin(2*TO_RAD*(Manom)));

  double hourangle= (acos(cos(TO_RAD*(90.833))/(cos(TO_RAD*(LATITUDE))*cos(TO_RAD*(declination)))-tan(TO_RAD*(LATITUDE))*tan(TO_RAD*(declination))))/TO_RAD;

  _solarnoon_t  = (720-4*LONGITUDE-eqtime+timezone*60)/1440;
  _sunrise_t    = _solarnoon_t-hourangle*4/1440;
  _sunset_t     = _solarnoon_t+hourangle*4/1440;
  }

time_t get_time() {
  RTCDateTime cur_time = RTCclock.getDateTime();
  printDateTime(cur_time);
  time_t when;
  tm* timeinfo;
  timeinfo=localtime(&when);
  timeinfo->tm_year = cur_time.year-1900;
  timeinfo->tm_mon = cur_time.month - 1;
  timeinfo->tm_mday = cur_time.day;
  timeinfo->tm_hour = cur_time.hour;
  timeinfo->tm_min = cur_time.minute;
  timeinfo->tm_sec = cur_time.second;
  timeinfo->tm_isdst = 0;
  when = mktime(timeinfo);
  free(timeinfo);
  return when;
  }

int brightness_for_time(time_t cur_time) {
  // compare cur_time to _sunrise_t and _sunset_t and return brightness
  timefromdecimalday(_sunrise_t,&sunrise_tm);
  Serial.print("Sunrise at: "); Serial.print(sunrise_tm.tm_hour); Serial.print(":"); Serial.print(sunrise_tm.tm_min); Serial.print(":"); Serial.println(sunrise_tm.tm_sec);

  timefromdecimalday(_sunset_t,&sunset_tm);
  Serial.print("Sunset at: "); Serial.print(sunset_tm.tm_hour); Serial.print(":"); Serial.print(sunset_tm.tm_min); Serial.print(":"); Serial.println(sunset_tm.tm_sec);

  current_tm = *localtime(&cur_time);
  Serial.print("Current at: "); Serial.print(current_tm.tm_hour); Serial.print(":"); Serial.print(current_tm.tm_min); Serial.print(":"); Serial.println(current_tm.tm_sec);
  long seconds_until_sunrise = (sunrise_tm.tm_sec  - current_tm.tm_sec)
                             + (sunrise_tm.tm_min  - current_tm.tm_min) * 60L
                             + (sunrise_tm.tm_hour - current_tm.tm_hour) * 3600L;
  long seconds_since_sunset =  (current_tm.tm_sec  - sunset_tm.tm_sec)
                             + (current_tm.tm_min  - sunset_tm.tm_min) * 60L
                             + (current_tm.tm_hour - sunset_tm.tm_hour) * 3600L;

  Serial.print("Seconds until sunrise: "); Serial.println(seconds_until_sunrise);
  Serial.print("Seconds since sunset: "); Serial.println(seconds_since_sunset);
  if (seconds_until_sunrise > 1500L) {
    return 0;
  } else if (seconds_until_sunrise > 0L) {
    return (1500-seconds_until_sunrise)/1500.0*255;
  } else if (seconds_since_sunset < 0L) {
    return 255;
  } else if (seconds_since_sunset < 1500L) {
    return ((1500-seconds_since_sunset)/1500.0*255);
  }
  return 0;
  }

void printDateTime(RTCDateTime cur_time) {
  Serial.print("Raw data: ");
  Serial.print(cur_time.year);   Serial.print("-");
  Serial.print(cur_time.month);  Serial.print("-");
  Serial.print(cur_time.day);    Serial.print(" ");
  Serial.print(cur_time.hour);   Serial.print(":");
  if (cur_time.minute < 10) Serial.print("0");
  Serial.print(cur_time.minute); Serial.print(":");
  Serial.print(cur_time.second); Serial.println("");
  }

void circadian_mode() {
  // get day
  time_t when = get_time();
  long days_since = days_since_1900(when);
  // if sunrise/sunset have been calculated for this day
  if (days_since == cur_sunrise_day) {
    // pass
  }
  // if not
  else {
    calculate();
    cur_sunrise_day = days_since;
  }
  brightness = brightness_for_time(when);
  Serial.print("Brightness: "); Serial.println(brightness);
  double masterFrac = masterVal/255.0;
  double brightnessFrac = brightness/255.0;
  redVal = colors[0]*brightnessFrac*masterFrac*max_red/255L;
  greenVal = colors[1]*brightnessFrac*masterFrac*max_green/255L;
  blueVal = colors[2]*brightnessFrac*masterFrac*max_blue/255L;
  if (redVal > 0 || greenVal > 0 || blueVal > 0) {
    if (redVal == 0) redVal = 1;
    if (greenVal == 0) greenVal = 1;
    if (blueVal == 0) blueVal = 1;
  }
  analogWrite(redPin, redVal);
  analogWrite(greenPin, greenVal);
  analogWrite(bluePin,blueVal);
  Serial.print("                                    red: ");
  Serial.print(redVal);
  Serial.print("  green: ");
  Serial.print(greenVal);
  Serial.print("  blue: ");
  Serial.println(blueVal);


  }

void normal_mode() {
  // check master brightness
  redVal = colors[0]*masterVal/255L*max_red/255L;
  greenVal = colors[1]*masterVal/255L*max_green/255L;
  blueVal = colors[2]*masterVal/255L*max_blue/255L;
  Serial.print("                          red: ");Serial.print(redVal);
  Serial.print(" green: ");Serial.print(greenVal);
  Serial.print(" blue: ");Serial.println(blueVal);
  analogWrite(redPin, redVal);
  analogWrite(greenPin, greenVal);
  analogWrite(bluePin, blueVal);
  }

void get_colors() {
  masterVal = analogRead(masterPotPin)/4;
  colors[0] = analogRead(redPotPin)/4;
  colors[1] = analogRead(greenPotPin)/4;
  colors[2] = analogRead(bluePotPin)/4;
  Serial.print("Master: ");
  Serial.print(masterVal);
  Serial.print("  ");
  Serial.print(colors[0]);
  Serial.print(", ");
  Serial.print(colors[1]);
  Serial.print(", ");
  Serial.println(colors[2]);
  
  }

void setup() {
  Serial.begin(9600);
  pinMode(redPin,OUTPUT);
  pinMode(greenPin,OUTPUT);
  pinMode(bluePin,OUTPUT);
  pinMode(masterPotPin,INPUT);
  pinMode(redPotPin,INPUT);
  pinMode(greenPotPin,INPUT);
  pinMode(bluePotPin,INPUT);
  pinMode(modePin,INPUT);
  RTCclock.begin();
  //RTCclock.setDateTime(__DATE__,__TIME__);

  
  }

void loop() {
  val = digitalRead(modePin);
  get_colors();
  // Circadian Clock mode
  if (val == HIGH) {
//    Serial.println("Mode pin HIGH");
    circadian_mode();
  } 
  // Normal light mode
  else {
//    Serial.println("Mode pin LOW");
    normal_mode();
  }
  Serial.println();
  Serial.println();

  }
