#include <WiFi.h>
#include <WiFiMulti.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <EEPROM.h>
#include "esp_task_wdt.h"
#include "GlobalSetup.h"
#include "SendToCloud_AutoUpdate.h"

// RS232 PINS
#define RXD2 16
#define TXD2 17

unsigned long passed_previousTime = 0;

void setup() {
  // initialize the LCD
  lcd.begin();
  // Turn on the blacklight and print a message.
  lcd.backlight();
  lcd.createChar(0, tank_wheelA);
  lcd.createChar(1, tank_wheelB);
  lcd.createChar(2, tank_wheel_L);
  lcd.createChar(3, tank_wheel_R);
  lcd.createChar(4, tank_head_L);
  lcd.createChar(5, tank_head_R);
  lcd.createChar(6, tank_gun);
  lcd.createChar(7, bullet_mini);
  lcd.home();

  // Note the format for setting a serial port is as follows: Serial2.begin(baud-rate, protocol, RX pin, TX pin);
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  esp_task_wdt_init(60, true);  // 60 วินาทีสำหรับ wdt

  EEPROM.begin(100);

  // ตั้งค่าสำหรับ upload program ครั้งแรก
  // EEPROM.writeUInt(machineID_address, machineID);
  // EEPROM.writeUInt(total_address, 0);
  // EEPROM.commit();

  machineID = EEPROM.readUInt(machineID_address);
  Total = EEPROM.readUInt(total_address);

  Serial.println("machineID: " + String(machineID));
  Serial.println("Total: " + String(Total));

  int numNetworks = sizeof(ssidArray) / sizeof(ssidArray[0]);
  for (int i = 0; i < numNetworks; i++) {
    wifiMulti.addAP(ssidArray[i], password);
  }

  const char OUTPUT_CNT = 5;
  const int OUTPUT_CH[OUTPUT_CNT] = { LED_STATUS, SPARE, BUZZER, LED_RED, LED_GREEN };
  for (int i = 0; i < OUTPUT_CNT; i++)
    pinMode(OUTPUT_CH[i], OUTPUT);

  for (int i = 0; i < OUTPUT_CNT; i++) {
    digitalWrite(OUTPUT_CH[i], HIGH);
    delay(200);
    digitalWrite(OUTPUT_CH[i], LOW);
  };

  digitalWrite(LED_STATUS, LOW);

  lcd.setCursor(5, 0);
  lcd.print("LOADING...");
  onLoad();
  textEnd("          ", 5, 0);
  textEnd("POLIPHARM CO,LTD", 2, 0);
  textEnd("VERSION " + String(FirmwareVer), 4, 1);
  delay(500);
  clearScreen(1);
  textEnd("ID " + String(machineID), 5, 1);
  delay(500);
  clearScreen(0);

  // esp32 auto update setup
  pinMode(button_boot.PIN, INPUT);
  attachInterrupt(button_boot.PIN, isr, RISING);

  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);

  // start program
  xTaskCreatePinnedToCore(autoUpdate, "Task0", 50000, NULL, 10, &Task0, 0);
  xTaskCreatePinnedToCore(mainLoop, "Task1", 5000, NULL, 9, &Task1, 1);
}

void loop() {}

bool checkDeviceState = false;
bool total_update = false;
void mainLoop(void *val) {
  for (;;) {
    if (!min_weight && !max_weight) {
      setMinMax();
      Serial.print("MIN: " + String(min_weight));
      Serial.println(" MAX: " + String(max_weight));
      lcd.clear();
    } else {
      clearScreen(0);
      lcd.noBlink();
      lcd.setCursor(4, 0);
      lcd.print("<< READY >>");

      String Total_SCR = "TOTAL: " + String(Total) + " PCS";
      while (Total_SCR.length() < 20) Total_SCR += " ";
      lcd.setCursor(0, 1);
      lcd.print(Total_SCR);

      lcd.setCursor(10, 3);
      lcd.setCursor(0, 2);
      lcd.print("MIN:" + String(min_weight));
      lcd.setCursor(10, 2);
      lcd.print("MAX:" + String(max_weight));
      clearScreen(3);
      lcd.setCursor(0, 3);
      lcd.print("WEIGHING: ");
      lcd.blink();

      current_weight = readSerial();

      if (current_weight) {
        clearScreen(0);
        lcd.setCursor(7, 0);
        lcd.print("Wait...");
        lcd.noBlink();
        lcd.setCursor(0, 3);
        lcd.print("WEIGHING: " + String(current_weight) + " KG.");
        Total++;
        lcd.setCursor(0, 1);
        lcd.print("TOTAL: " + String(Total) + " PCS");
        EEPROM.writeUInt(total_address, Total);
        EEPROM.commit();
        delay(500);

        Serial.print("MIN: " + String(min_weight));
        Serial.print(" MAX: " + String(max_weight));
        Serial.println(" CURRENT: " + String(current_weight));

        if (current_weight >= min_weight && current_weight <= max_weight) {
          count++;
          Serial.println("Passed: " + String(current_weight) + " KG.");
          clearScreen(3);
          lcd.setCursor(7, 3);
          lcd.print("PASSED");
          digitalWrite(LED_GREEN, HIGH);
          passed_previousTime = millis();
        } else {
          countNC++;
          Serial.println("Fail: " + String(current_weight) + " KG.");
          clearScreen(3);
          lcd.setCursor(4, 3);
          lcd.print("<< FAILED >>");
          alert();
        }
      } else {
        continue;
      }
    }
  }
}

void checkDevice() {
  unsigned long previousMillis1 = 0;
  unsigned long previousMillis2 = 0;
  bool ledState = false;
  String serialMonitor;
  String serialMonitor_cache;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi: NULL");
  lcd.setCursor(0, 1);
  lcd.print("RSSI: NULL");
  lcd.setCursor(0, 2);
  lcd.print("Sensor: ");
  lcd.setCursor(0, 3);
  lcd.print("RS232: NULL");

  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      lcd.setCursor(6, 0);
      lcd.print(WiFi.SSID());
      lcd.setCursor(6, 1);
      int rssi = WiFi.RSSI();
      if (rssi >= -50) {
        lcd.print("Excellent");
      } else if (rssi >= -70) {
        lcd.print("Good     ");
      } else {
        lcd.print("Weak     ");
      }

      digitalWrite(LED_RED, LOW);
      if (millis() - previousMillis1 >= 1000) {
        previousMillis1 = millis();  // บันทึกค่าเวลาปัจจุบัน
        if (ledState) {
          digitalWrite(LED_GREEN, HIGH);
        } else {
          digitalWrite(LED_GREEN, LOW);
        }

        ledState = !ledState;
      }
    } else {
      digitalWrite(LED_GREEN, LOW);
      lcd.setCursor(6, 0);
      lcd.print("NULL        ");
      lcd.setCursor(6, 1);
      lcd.print("NULL        ");
      if (millis() - previousMillis1 >= 1000) {
        previousMillis1 = millis();  // บันทึกค่าเวลาปัจจุบัน
        if (ledState) {
          digitalWrite(LED_RED, HIGH);
        } else {
          digitalWrite(LED_RED, LOW);
        }

        ledState = !ledState;
      }
    }

    if (Serial2.available() > 0) {
      lcd.setCursor(7, 3);
      lcd.print("OK  ");
    } else {
      lcd.setCursor(7, 3);
      lcd.print("null");
    }

    Serial2.read();

    char key = keypad.getKey();
    if (key) {
      lcd.clear();
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_RED, LOW);
      checkDeviceState = true;
      break;
    }
  }
}

void checkKeypad() {
  char key = keypad.getKey();
  String _key = String(key);
  if (key) {
    digitalWrite(BUZZER, HIGH);
    delay(100);
    digitalWrite(BUZZER, LOW);

    if (_key == "A") {
      Serial.println("Check device!");
      checkDevice();
    } else if (_key == "B") {
      Serial.println("Set min-max weight!");
      min_weight = 0;
      max_weight = 0;
    } else if (_key == "C") {
      Serial.println("Reset Counter>>");
      Total = 0;
      EEPROM.writeUInt(total_address, Total);
      EEPROM.commit();
      total_update = true;
    } else if (_key == "D" && Total > 0) {
      Serial.println("Total,Count -1");
      Total--;
      EEPROM.writeUInt(total_address, Total);
      EEPROM.commit();
      Serial.println("TOTAL: " + String(Total) + " PCS");
      total_update = true;

      if (count > 0)
        count--;
    }
  }
}

void setMinMax() {
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("SET MIN-MAX WEIGHT");
  lcd.setCursor(0, 3);
  lcd.print("MIN:");
  lcd.setCursor(10, 3);
  lcd.print("MAX:");
  lcd.blink();

  String min_weight_str = "";
  String max_weight_str = "";
  while (min_weight_str == "" || max_weight_str == "") {
    min_weight_str = "";
    max_weight_str = "";
    lcd.setCursor(14, 3);
    lcd.print("     ");
    lcd.setCursor(4, 3);
    lcd.print("     ");
    min_weight_str = readKeypad(4, 3);

    if (min_weight_str == "") {
      continue;
    } else {
      max_weight_str = readKeypad(14, 3);
      if (max_weight_str.toFloat() < min_weight_str.toFloat())
        max_weight_str = "";
    }
  }

  min_weight = min_weight_str.toFloat();
  max_weight = max_weight_str.toFloat();
}

String readKeypad(int col, int row) {
  String key_cache;
  int index = 0;
  lcd.setCursor(col, row);
  while (true) {
    char key = keypad.getKey();
    String _key = String(key);
    if (key) {
      digitalWrite(BUZZER, HIGH);
      delay(100);
      digitalWrite(BUZZER, LOW);
      if (key_cache.length() <= 4 && _key != "A" && _key != "B" && _key != "C" && _key != "D") {
        if (key_cache.indexOf(".") == -1) {
          key_cache += String(_key);
          lcd.setCursor(col + index, row);
          lcd.print(key_cache[index]);
          index++;
        } else if (_key != ".") {
          key_cache += String(_key);
          lcd.setCursor(col + index, row);
          lcd.print(key_cache[index]);
          index++;
        }
      } else if (_key == "D" && index > 0) {
        lcd.noBlink();
        key_cache = key_cache.substring(0, key_cache.length() - 1);
        index -= 1;
        lcd.setCursor((col + index), row);
        lcd.print(key_cache[index] + " ");
        lcd.setCursor((col + index), row);
        lcd.blink();
      } else if (_key == "C") {
        return key_cache;
      } else if (_key == "B") {
        return "";
      }
    }

    // Serial.println("index: " + String(index) + "key_cache: " + key_cache);
  }
}

// read SerialPort RS232
float readSerial() {
  Serial.println("ReadSerialPort>>");
  checkDeviceState = false;

  while (min_weight && max_weight && !checkDeviceState) {
    if ((millis() - passed_previousTime) > 500) {
      digitalWrite(LED_GREEN, LOW);
    }

    // update screen
    if (total_update) {
      lcd.noBlink();
      String Total_SCR = "TOTAL: " + String(Total) + " PCS";
      while (Total_SCR.length() < 20) Total_SCR += " ";

      lcd.setCursor(0, 1);
      lcd.print(Total_SCR);
      total_update = false;
      lcd.setCursor(10, 3);
      lcd.blink();
    }

    //  check Serial Data cache
    if (Serial2.available() > 0) {
      String readRS232 = Serial2.readString();

      // แยกข้อความด้วย \n
      String lines[2];
      int lineCount = 0;

      int sIndex = 0;
      int eIndex = readRS232.indexOf('\n');
      while (eIndex != -1) {
        lines[lineCount] = readRS232.substring(sIndex, eIndex);
        sIndex = eIndex + 1;
        eIndex = readRS232.indexOf('\n', sIndex);
        lineCount++;
      }
      // เพิ่มข้อความสุดท้าย
      String data = lines[2];
      data.replace("Gross", "");
      data.replace("kg", "");
      data.trim();
      Serial.println("CURRENT WEIGHT: " + String(data));

      float currentWeight = data.toFloat();
      if (currentWeight > 0) {
        digitalWrite(BUZZER, HIGH);
        delay(100);
        digitalWrite(BUZZER, LOW);
        delay(100);
        return currentWeight;
      } else {
        continue;
      }
    } else {
      checkKeypad();
    }
  }

  return 0.00;
}

void onLoad() {
  for (int i = 0; i < 16; i++) {
    lcd.setCursor(i + 1, 2);
    lcd.write(4);
    lcd.write(5);
    lcd.write(6);
    lcd.write(6);

    lcd.setCursor(i, 2);
    lcd.print(" ");
    lcd.setCursor(i, 3);
    lcd.write(2);
    if (i % 2 == 0) {
      lcd.write(0);
      lcd.write(0);
    } else {
      lcd.write(1);
      lcd.write(1);
    }

    lcd.write(3);
    lcd.setCursor(i - 1, 3);
    lcd.print(" ");
    delay(300);
  }

  lcd.setCursor(18, 2);
  lcd.print("  ");
  lcd.setCursor(14, 2);
  lcd.write(6);
  lcd.write(6);

  for (int i = 13; i >= 0; i--) {
    lcd.setCursor(i, 2);
    lcd.write(7);
    delay(200);
    if (i != 13) {
      lcd.setCursor(i, 2);
      lcd.print("  ");
    }
  }
}

void alert() {
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(BUZZER, HIGH);
    lcd.noBacklight();
    delay(100);
    lcd.backlight();
    digitalWrite(LED_RED, LOW);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }
}
