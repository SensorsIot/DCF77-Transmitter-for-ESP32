void sleepForMinutes(int minutes) {
  if (minutes < 2) return;
  uint64_t uSecToMinutes = 60000000;
  esp_sleep_enable_timer_wakeup(minutes * uSecToMinutes);  // this timer works on uSecs, so 60M by minute
  //WiFi_off();
  Serial.print("To sleep... ");
  Serial.print(minutes);
  Serial.println(" minutes");
  Serial.flush();
  esp_deep_sleep_start();
}

void cronCheck() {
  // is this hour in the list?
  boolean work = false;
  for (int n = 0; n < sizeof(hoursToWakeUp); n++) {
    //Serial.println(sizeof(hoursToWakeUp)); Serial.print(work); Serial.print(" "); Serial.print(n); Serial.print(" "); Serial.print(actualHours); Serial.print(" "); Serial.println(hoursToWakeUp[n]);
    if ((actualHours == hoursToWakeUp[n]) or (actualHours == (hoursToWakeUp[n] + 1))){
      work = true;
      // is this the minute to go to sleep?
      if ((actualMinutes > minuteToSleep) and (actualMinutes < minuteToWakeUp)) {
        // go to sleep (minuteToWakeUp - actualMinutes)
        Serial.print(".");
        sleepForMinutes(minuteToWakeUp - actualMinutes);   
      }
      break;
    }
  }
  if (work == false) { // sleep until minuteToWakeUp (take into account the ESP32 can start for some reason between minuteToWakeUp and o'clock)
    if (actualMinutes >= minuteToWakeUp) {
        Serial.print("..");
      sleepForMinutes(minuteToWakeUp + 60 - actualMinutes);
    } else {
      // goto sleep for (minuteToWakeUp - actualMinutes) minutes
        Serial.print("...");
      sleepForMinutes(minuteToWakeUp - actualMinutes);        
    }
  }
}
