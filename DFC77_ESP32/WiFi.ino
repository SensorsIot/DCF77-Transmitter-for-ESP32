
void WiFi_on() {
  Serial.print("Connecting WiFi...");
  WiFi.begin(ssid, password);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    if (counter > 10) ESP.restart();
    Serial.print ( "." );
    counter++;
  }
  Serial.println();
  Serial.println("WiFi connected");
}

void WiFi_off() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi disconnected");
  Serial.flush();
}
