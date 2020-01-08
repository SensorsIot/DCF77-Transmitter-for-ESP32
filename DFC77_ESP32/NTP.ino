
void getNTP() {
  Serial.print("GetNTP ");
  int i = 0;
  do {
    i++;
    if (i > 40) {
      ESP.restart();
    }
    configTime(0, 0, ntpServer);
    setenv("TZ", TZ_INFO, 1);
    delay(500);
  } while (!getLocalTime(&timeinfo));
  Serial.println("Ok");
}
