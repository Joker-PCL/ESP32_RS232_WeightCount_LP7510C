#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include "cert.h"
#include "time.h"

unsigned long previousMillis = 0;  // will store last time LED was updated
unsigned long previousMillis_2 = 0;
unsigned long previousMillis_3 = 0;
const long interval_update = 900000;       // check up date 15 min
const long interval_sendTocloud = 300000;  // send data 5 min
const long mini_interval = 1000;

struct Button {
  const uint8_t PIN;
  uint32_t numberKeyPresses;
  bool pressed;
};

Button button_boot = {
  0,
  0,
  false
};

/*void IRAM_ATTR isr(void* arg) {
    Button* s = static_cast<Button*>(arg);
    s->numberKeyPresses += 1;
    s->pressed = true;
}*/

void sendToCloud() {
  if (WiFi.status() == WL_CONNECTED) {
    static bool flag = false;
    struct tm timeinfo;

    // Get current time
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }

    strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    String asString(timeStringBuff);
    asString.replace(" ", "-");
    Serial.print("Time:");
    Serial.println(asString);

    // This will send the request to the server
    String url = host + GOOGLE_SCRIPT_ID + "/exec?";
    url += "machineID=" + String(machineID);
    url += "&";
    url += "amount=" + String(count);
    url += "&";
    url += "amountNC=" + String(countNC);

    Serial.print("POST data to spreadsheet:");
    Serial.println(url);

    HTTPClient http;
    http.begin(url.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int httpCode = http.GET();
    Serial.print("HTTP Status Code: ");
    Serial.println(httpCode);

    url = "";

    //getting response from google sheet
    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println("Payload: " + payload);
      count = 0;  // Reset count
      countNC = 0;  // Reset count
    } else {
      Serial.println("Failed to connect to the server");
    }

    http.end();
  }

  delay(2000);
}

void IRAM_ATTR isr() {
  button_boot.numberKeyPresses += 1;
  button_boot.pressed = true;
}

void firmwareUpdate(String version) {
  vTaskDelete(Task1);
  vTaskDelete(Task2);

  if (count > 0)
    sendToCloud();

  lcd.clear();
  digitalWrite(BUZZER, HIGH);
  textEnd("UPDATE FIRMWERE", 2, 0);
  textEnd("VERSION " + version, 3, 1);
  digitalWrite(BUZZER, LOW);
  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  httpUpdate.setLedPin(LED_STATUS, LOW);
  t_httpUpdate_return ret = httpUpdate.update(client, URL_fw_Bin);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }

  textEnd("ERROR RESTART...", 2, 3);
  delay(500);
  ESP.restart();
}

int FirmwareVersionCheck(void) {
  String payload;
  int httpCode;
  String fwurl = "";
  fwurl += URL_fw_Version;
  fwurl += "?";
  fwurl += String(rand());
  Serial.println(fwurl);
  WiFiClientSecure* client = new WiFiClientSecure;

  if (client) {
    client->setCACert(rootCACertificate);

    // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
    HTTPClient https;

    if (https.begin(*client, fwurl)) {  // HTTPS
      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      delay(100);
      httpCode = https.GET();
      delay(100);
      if (httpCode == HTTP_CODE_OK)  // if version received
      {
        payload = https.getString();  // save received version
      } else {
        Serial.print("error in downloading version file:");
        Serial.println(httpCode);
      }
      https.end();
    }
    delete client;
  }

  if (httpCode == HTTP_CODE_OK)  // if version received
  {
    payload.trim();
    if (payload.equals(FirmwareVer)) {
      Serial.printf("\nDevice already on latest firmware version:%s\n", FirmwareVer);
      return false;
    } else {
      Serial.println(payload);
      Serial.println("New firmware detected");
      firmwareUpdate(payload);
    }
  }
  return false;
}

void repeatedCall() {
  unsigned long currentMillis = millis();
  if ((currentMillis - previousMillis) >= interval_update) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    FirmwareVersionCheck();
  }

  if ((currentMillis - previousMillis_2) >= interval_sendTocloud && count > 0) {
    previousMillis_2 = currentMillis;
    sendToCloud();
  }

  // if ((currentMillis - previousMillis_3) >= mini_interval) {
  //   previousMillis_3 = currentMillis;
  //   Serial.print("Check fw version in:");
  //   Serial.print((interval - (currentMillis - previousMillis)) / 1000);
  //   Serial.println("sec.");
  //   Serial.print("Active fw version:");
  //   Serial.println(FirmwareVer);
  //   if (WiFi.status() == WL_CONNECTED) {
  //     Serial.print("Wi-Fi Strength: ");
  //     // Get RSSI value
  //     int rssi = WiFi.RSSI();
  //     if (rssi >= -50) {
  //       Serial.println("Excellent");
  //     } else if (rssi >= -70) {
  //       Serial.println("Good");
  //     } else {
  //       Serial.println("Weak");
  //     }
  //   }
  // }
}

void autoUpdate(void* val) {
  Serial.print("Active firmware version:");
  Serial.println(FirmwareVer);

  Serial.println("Waiting for WiFi");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("SSID: " + WiFi.SSID());
  Serial.println("IP address:");
  Serial.println(WiFi.localIP());

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  FirmwareVersionCheck();
  delay(1000);

  for (;;) {
    if (button_boot.pressed) {  //to connect wifi via Android esp touch app
      Serial.println("Firmware update Starting..");
      firmwareUpdate("<x>");
      button_boot.pressed = false;
    }

    while (wifiMulti.run() != WL_CONNECTED) {
      digitalWrite(LED_STATUS, HIGH);
      delay(500);
      digitalWrite(LED_STATUS, LOW);
      delay(500);
      Serial.print(".");
    }

    digitalWrite(LED_STATUS, HIGH);
    delay(200);
    digitalWrite(LED_STATUS, LOW);
    delay(200);
    repeatedCall();
  }
}