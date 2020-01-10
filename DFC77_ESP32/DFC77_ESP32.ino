/*
  based on this sketch: https://github.com/aknik/ESP32/blob/master/DFC77/DFC77_esp32_Solo.ino

  Some functions are inspired by work of G6EJD ( https://www.youtube.com/channel/UCgtlqH_lkMdIa4jZLItcsTg )

  Refactor by DeltaZero, converts to syncronous, added "cron" that you can bypass, see line 29
                                                    The cron does not start until 10 minutes from reset (see constant onTimeAfterReset)
  Every clock I know starts to listen to the radio at aproximatelly the hour o'clock, so cron takes this into account

  Alarm clocks from Junghans: Every hour (innecesery)
  Weather Station from Brigmton: 2 and 3 AM
  Chinesse movements and derivatives: 1 o'clock AM
*/


#include <WiFi.h>
#include <Ticker.h>
#include <Time.h>



#include "credentials.h"  // If you put this file in the same forlder that the rest of the tabs, then use "" to delimiter,
                          // otherwise use <> or comment it and write your credentials directly on code
                          // const char* ssid = "YourOwnSSID";
                          // const char* password = "YourSoSecretPassword";
                          
#define LEDBUILTIN 5      // This is the pin for a Wemos board
#define ANTENNAPIN 15     // You MUST adapt this pin to your preferences
// #define CONTINUOUSMODE // Uncomment this line to bypass de cron and have the transmitter on all the time

// cron (if you choose the correct values you can even run on batteries)
// If you choose really bad this minutes, everything goes wrong, so minuteToWakeUp must be greater than minuteToSleep
#define minuteToWakeUp  55 // Every hoursToWakeUp at this minute the ESP32 wakes up get time and star to transmit
#define minuteToSleep   8 // If it is running at this minute then goes to sleep and waits until minuteToWakeUp


byte hoursToWakeUp[] = {0,1,2,3}; // you can add more hours to adapt to your needs
                      // When the ESP32 wakes up, check if the actual hour is in the list and
                      // runs or goes to sleep until next minuteToWakeUp

Ticker tickerDecisec; // TBD at 100ms

//complete array of pulses for a minute
//0 = no pulse, 1=100ms, 2=200ms
int impulseArray[60];
int impulseCount = 0;
int actualHours, actualMinutes, actualSecond, actualDay, actualMonth, actualYear, DayOfW;
long dontGoToSleep = 0;
const long onTimeAfterReset = 600000; // Ten minutes

const char* ntpServer = "es.pool.ntp.org"; // enter your closer pool or pool.ntp.org
const char* TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";  // enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)

struct tm timeinfo;

void setup() {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  Serial.begin(115200);
  Serial.println();
  Serial.println("DCF77 transmitter");

  if (esp_sleep_get_wakeup_cause() == 0) dontGoToSleep = millis();

  ledcSetup(0, 77500, 8); // DCF77 frequency
  ledcAttachPin(ANTENNAPIN, 0); // This Pin, or another one you choose, has to be attached to the antenna

  pinMode (LEDBUILTIN, OUTPUT);
  digitalWrite (LEDBUILTIN, LOW); // LOW if LEDBUILTIN is inverted like in Wemos boards

  WiFi_on();
  getNTP();
  WiFi_off();
  show_time();
  
  CodeTime(); // first conversion just for cronCheck
#ifndef CONTINUOUSMODE
  if ((dontGoToSleep == 0) or ((dontGoToSleep + onTimeAfterReset) < millis())) cronCheck(); // first check before start anything
#else
  Serial.println("CONTINUOUS MODE NO CRON!!!");
#endif

  // sync to the start of a second
  Serial.print("Syncing... ");
  int startSecond = timeinfo.tm_sec;
  long count = 0;
  do {
    count++;
    if(!getLocalTime(&timeinfo)){
      Serial.println("Error obtaining time...");
      delay(3000);
      ESP.restart();
    }
  } while (startSecond == timeinfo.tm_sec);

  tickerDecisec.attach_ms(100, DcfOut); // from now on calls DcfOut every 100ms
  Serial.print("Ok ");
  Serial.println(count);
}

void loop() {
  // There is no code inside the loop. This is a syncronous program driven by the Ticker
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
  if (actualSecond == 60) actualSecond = 0;

  int n, Tmp, TmpIn;
  int ParityCount = 0;

  //we put the first 20 bits of each minute at a logical zero value
  for (n = 0; n < 20; n++) impulseArray[n] = 1;
  
  // set DST bit
  if (timeinfo.tm_isdst == 0) {
    impulseArray[18] = 2; // CET or DST OFF
  } else {
    impulseArray[17] = 2; // CEST or DST ON
  }
  
  //bit 20 must be 1 to indicate active time
  impulseArray[20] = 2;

  //calculates the bits for the minutes
  TmpIn = Bin2Bcd(actualMinutes);
  for (n = 21; n < 28; n++) {
    Tmp = TmpIn & 1;
    impulseArray[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
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
  impulseArray[59] = 0; // No pulse
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
        digitalWrite(LEDBUILTIN, LOW);
        ledcWrite(0, 0);
      }
      break;
    case 1:
      if (impulseArray[actualSecond] == 1) {
        digitalWrite(LEDBUILTIN, HIGH);
        ledcWrite(0, 127);
      }
      break;
    case 2:
      digitalWrite(LEDBUILTIN, HIGH);
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
        show_time();
#ifndef CONTINUOUSMODE
        if ((dontGoToSleep == 0) or ((dontGoToSleep + onTimeAfterReset) < millis())) cronCheck();
#else
        Serial.println("CONTINUOUS MODE NO CRON!!!");
#endif
      }
      break;
  }
  if(!getLocalTime(&timeinfo)){
    Serial.println("Error obtaining time...");
    delay(3000);
    ESP.restart();
  }
  CodeTime();
}
