// time sever
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;
char timeStringBuff[50];  //50 chars should be enough

//  multitask
TaskHandle_t Task0;
TaskHandle_t Task1;
TaskHandle_t Task2;

WiFiMulti wifiMulti;

const char* ssidArray[] = {"pcl_plant1", "pcl_plant2",  "pcl_plant3", "pcl_plant4", "weight_table"};
const char* password = "plant172839";

String FirmwareVer = {
  "1.0.0"
};

#define URL_fw_Version "https://raw.githubusercontent.com/Joker-PCL/ESP32_RS232_WeightCount_LP7510C/bin_version.txt"
#define URL_fw_Bin "https://raw.githubusercontent.com/Joker-PCL/ESP32_RS232_WeightCount_LP7510C/fw.bin"

// Google script ID and required credentials
String host = "https://script.google.com/macros/s/";
String GOOGLE_SCRIPT_ID = "AKfycbwPzTux1m1_9ESA7L0y_aIanjtEFkIZIeC8XhohXGwzAlfzjogOTG92L_W6e024Z7vO";  // change Gscript ID

// INPUT AND OUTPUT
const int LED_STATUS = 2;
const int LED_RED = 25;
const int LED_GREEN = 26;
const int BUZZER = 33;

unsigned int currentTime = 0;  // time stamp millis()

int machineID_address = 0;        // machine address EEPROM
unsigned long machineID = 20201;  // machine ID

int total_address = 10;  // total address EEPROM
unsigned int Total = 0;  // total
unsigned int count = 0;  // cache count
unsigned int countNC = 0;  // cache count

float currentWeight = 0;  // cache weight

unsigned long pressTime_countReset = 0;

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 20, 4);

/* Keypad setup */
const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 4;
byte rowPins[KEYPAD_ROWS] = {5, 18, 19, 4};
byte colPins[KEYPAD_COLS] = {13, 12, 14, 27};
char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'.', '0', '.', 'D'}
};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);

// tank label
uint8_t tank_wheelA[8] = { 0x1F, 0x1F, 0x1B, 0x11, 0x11, 0x1B, 0x1F, 0x1F };
uint8_t tank_wheelB[8] = { 0x1F, 0x1F, 0x0E, 0x11, 0x11, 0x0E, 0x1F, 0x1F };
uint8_t tank_wheel_L[8] = { 0x07, 0x0F, 0x1F, 0x1F, 0x1F, 0x1F, 0x0F, 0x07 };
uint8_t tank_wheel_R[8] = { 0x1C, 0x1E, 0x1F, 0x1F, 0x1F, 0x1F, 0x1E, 0x1C };
uint8_t tank_head_L[8] = { 0x00, 0x01, 0x07, 0x0F, 0x1F, 0x1F, 0x1F, 0x1F };
uint8_t tank_head_R[8] = { 0x00, 0x10, 0x1C, 0x1E, 0x1F, 0x1F, 0x1F, 0x1F };
uint8_t tank_gun[8] = { 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x00, 0x00 };
uint8_t bullet_mini[8] = { 0x00, 0x00, 0x0D, 0x1E, 0x1E, 0x0D, 0x00, 0x00 };

// แสดงผลแบบเรียงอักษร
void textEnd(String text, int cols, int rows) {
  for (int i = 0; i < text.length(); i++) {
    lcd.setCursor(cols + i, rows);
    lcd.print(text[i]);
    delay(100);
  }
}

// ลบอักษรหน้าจอแบบกำหนด แถว ตำแหน่ง จำนวน
void clearScreen(int row) {
  for (int i = 0; i < 20; i++) {
    lcd.setCursor(i, row);
    lcd.print(" ");
    // delay(50);
  }
}