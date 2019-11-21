arduino esp8266
/*
  based on this sketch: https://github.com/aknik/ESP32/blob/master/DFC77/DFC77_esp32_Solo.ino

  This is an example file for using the time function in ESP8266 or ESP32 tu get NTP time
  It offers two functions:

  - getNTPtime(struct tm * info, uint32_t ms) where info is a structure which contains time
  information and ms is the time the service waits till it gets a response from NTP.
  Each time you cann this function it calls NTP over the net.

  If you do not want to call an NTP service every second, you can use
  - getTimeReducedTraffic(int ms) where ms is the the time between two physical NTP server calls. Betwwn these calls,
  the time structure is updated with the (inaccurate) timer. If you call NTP every few minutes you should be ok

  The time structure is called tm and has teh following values:

  Definition of struct tm:
  Member  Type  Meaning Range
  tm_sec  int seconds after the minute  0-61*
  tm_min  int minutes after the hour  0-59
  tm_hour int hours since midnight  0-23
  tm_mday int day of the month  1-31
  tm_mon  int months since January  0-11
  tm_year int years since 1900
  tm_wday int days since Sunday 0-6
  tm_yday int days since January 1  0-365
  tm_isdst  int Daylight Saving Time flag

  because the values are somhow akwardly defined, I introduce a function makeHumanreadable() where all values are adjusted according normal numbering.
  e.g. January is month 1 and not 0 And Sunday or monday is weekday 1 not 0 (according definition of MONDAYFIRST)

  Showtime is an example on how you can use the time in your sketch

  The functions are inspired by work of G6EJD ( https://www.youtube.com/channel/UCgtlqH_lkMdIa4jZLItcsTg )
*/


#include <WiFi.h>
#include <Ticker.h>
#include <Time.h>
#include <credentials.h>

const char* ssid = mySSID;
const char* password = myPASSWORD;


Ticker tickerSetLow;

const byte ledPin = 4;  // built-in LED

//complete array of pulses for three minutes
//0 = no pulse, 1=100msec, 2=200msec
int impulseArray[60];
int impulseCount = 0;
int actualHours, actualMinutes, actualSecond, actualDay, actualMonth, actualYear, DayOfW;

const char* NTP_SERVER = "ch.pool.ntp.org";
const char* TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";  // enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)

struct tm timeinfo;
unsigned long lastEntryLED, lastEntryTime;
long unsigned lastNTPtime;
time_t now;


void setup() {
  Serial.begin(115200);
  Serial.println("\n\nDCF77 transmitter\n");
  WiFi.disconnect();
  delay(1000);

  tickerSetLow.attach_ms(100, DcfOut );

  ledcSetup(0, 77500, 8);
  ledcAttachPin(5, 0); // Pin 5 has to be attached to the antenna

  pinMode (ledPin, OUTPUT);
  digitalWrite (ledPin, LOW);

  WiFi.begin(ssid, password);

  int counter;
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    if (counter > 10) ESP.restart();
    Serial.print ( "." );
  }

  Serial.println("\n\nWiFi connected\n\n");

  configTime(0, 0, NTP_SERVER);  // See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for Timezone codes for your region
  setenv("TZ", TZ_INFO, 1);
  if (getNTPtime(10)) {  // wait up to 10 sec to sync
    Serial.println(&timeinfo, "Time now: %B %d %Y %H:%M:%S (%A)");
  } else {
    Serial.println("Time not set");
    ESP.restart();
  }
  showTime(&timeinfo);
}

void loop() {
  getTimeReducedTraffic(120);
  CodeTime();
}

void CodeTime() {
  DayOfW = timeinfo.tm_wday;
  if (DayOfW == 0) DayOfW = 7;
  actualDay = timeinfo.tm_mday;
  actualMonth = timeinfo.tm_mon + 1;
  actualYear = timeinfo.tm_year - 100;
  actualHours = timeinfo.tm_hour;
  actualMinutes = timeinfo.tm_min + 1; // DCF77 transmitts the next minute
  if (actualMinutes >= 60) {
    actualMinutes = 0;
    actualHours++;
  }
  actualSecond = timeinfo.tm_sec;

  int n, Tmp, TmpIn;
  int ParityCount = 0;

  //we put the first 20 bits of each minute at a logical zero value
  for (n = 0; n < 20; n++) impulseArray[n] = 1;

  // set CET bit
  impulseArray[18] = 2;

  //bit 20 must be 1 to indicate active time
  impulseArray[20] = 2;

  //calculates the bits for the minutes
  TmpIn = Bin2Bcd(actualMinutes);
  for (n = 21; n < 28; n++) {
    Tmp = TmpIn & 1;
    impulseArray[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  };
  if ((ParityCount & 1) == 0)
    impulseArray[28] = 1;
  else
    impulseArray[28] = 2;

  //calculates bits for the hours
  ParityCount = 0;
  TmpIn = Bin2Bcd(actualHours);
  for (n = 29; n < 35; n++) {
    Tmp = TmpIn & 1;
    impulseArray[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  if ((ParityCount & 1) == 0)
    impulseArray[35] = 1;
  else
    impulseArray[35] = 2;
  ParityCount = 0;

  //calculate the bits for the actual Day of Month
  TmpIn = Bin2Bcd(actualDay);
  for (n = 36; n < 42; n++) {
    Tmp = TmpIn & 1;
    impulseArray[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  TmpIn = Bin2Bcd(DayOfW);
  for (n = 42; n < 45; n++) {
    Tmp = TmpIn & 1;
    impulseArray[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  //calculates the bits for the actualMonth
  TmpIn = Bin2Bcd(actualMonth);
  for (n = 45; n < 50; n++) {
    Tmp = TmpIn & 1;
    impulseArray[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  //calculates the bits for actual year
  TmpIn = Bin2Bcd(actualYear);   // 2 digit year
  for (n = 50; n < 58; n++) {
    Tmp = TmpIn & 1;
    impulseArray[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  //equal date
  if ((ParityCount & 1) == 0)
    impulseArray[58] = 1;
  else
    impulseArray[58] = 2;

  //last missing pulse
  impulseArray[59] = 0;
}

int Bin2Bcd(int dato) {
  int msb, lsb;
  if (dato < 10)
    return dato;
  msb = (dato / 10) << 4;
  lsb = dato % 10;
  return msb + lsb;
}



void DcfOut() {
  switch (impulseCount++) {
    case 0:

      if (impulseArray[actualSecond] != 0) {
        digitalWrite(ledPin, LOW);
        ledcWrite(0, 0);
      }
      break;
    case 1:
      if (impulseArray[actualSecond] == 1) {
        digitalWrite(ledPin, HIGH);
        ledcWrite(0, 127);
      }
      break;
    case 2:
      digitalWrite(ledPin, HIGH);
      ledcWrite(0, 127);
      break;
    case 9:
      impulseCount = 0;


      if (actualSecond == 1 || actualSecond == 15 || actualSecond == 21  || actualSecond == 29 ) Serial.print("-");
      if (actualSecond == 36  || actualSecond == 42 || actualSecond == 45  || actualSecond == 50 ) Serial.print("-");
      if (actualSecond == 28  || actualSecond == 35  || actualSecond == 58 ) Serial.print("P");


      if (impulseArray[actualSecond] == 1) Serial.print("0");
      if (impulseArray[actualSecond] == 2) Serial.print("1");

      if (actualSecond == 59 ) {
        Serial.println();
        showTime(&timeinfo);
      }
      break;
  };
};

bool getNTPtime(int sec) {
  char time_output[30];

  {
    uint32_t start = millis();
    do {
      time(&now);
      localtime_r(&now, &timeinfo);
      Serial.print(".");
      delay(10);
    } while (((millis() - start) <= (1000 * sec)) && (timeinfo.tm_year < (2016 - 1900)));
    if (timeinfo.tm_year <= (2016 - 1900)) return false;  // the NTP call was not successful
    Serial.print("now ");  Serial.println(now);
    strftime(time_output, 30, "%a  %d-%m-%y %T", localtime(&now));
    Serial.println(time_output);
    Serial.println();
  }
  return true;
}

void getTimeReducedTraffic(int sec) {
  tm *ptm;
  if ((millis() - lastEntryTime) < (1000 * sec)) {
    now = lastNTPtime + (int)(millis() - lastEntryTime) / 1000;
  } else {
    lastEntryTime = millis();
    lastNTPtime = time(&now);
    now = lastNTPtime;
    Serial.println("Get NTP time");
  }
  ptm = localtime(&now);
  timeinfo = *ptm;
}

void showTime(tm *localTime) {
  Serial.print(localTime->tm_mday);
  Serial.print('/');
  Serial.print(localTime->tm_mon + 1);
  Serial.print('/');
  Serial.print(localTime->tm_year - 100);
  Serial.print('-');
  Serial.print(localTime->tm_hour);
  Serial.print(':');
  Serial.print(localTime->tm_min);
  Serial.print(':');
  Serial.print(localTime->tm_sec);
  Serial.print(" Day of Week ");
  if (localTime->tm_mday = 0) localTime->tm_mday = 7;
  Serial.println(localTime->tm_wday);
}
