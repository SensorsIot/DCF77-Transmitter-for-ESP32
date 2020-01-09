void show_time() {
  Serial.print(&timeinfo, "Time now: %B %d %Y %H:%M:%S (%A) %Z ");
  if (timeinfo.tm_isdst == 0) {
    Serial.println("DST=OFF");
  } else {
    Serial.println("DST=ON");
  }
}
