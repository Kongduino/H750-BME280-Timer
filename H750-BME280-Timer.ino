/*
  https://techoverflow.net/2021/09/22/minimal-stm32-hardwaretimer-platformio-arduino-timer-interrupt-blink-example/
  https://techoverflow.net/2021/09/22/how-to-implement-1mhz-interrupt-in-platformio-arduino-on-stm32/
*/

#include "Seeed_BME280.h"
#include <ss_oled.h>
#include <SPI.h>
#include <LoRa.h>
#include "aes.c"
#include "helper.h"

HardwareTimer MyTimer(TIM6);
uint8_t count = 0;
void OnTimer1Interrupt() {
  count++;
  if (count == 3) {
    // MyTimer.pause();
    showData(); // 3 x 10 seconds
    // I probably could pause the timer before calling showData(), and resume it afterwards:
    // showData() takes a few seconds, so the next call will happen not 30 seconds later, but
    // more like 25.
    // MyTimer.resume();
    count = 0;
  }
}

uint8_t pKey[16] = {
  0xde, 0xca, 0xfb, 0xad,
  0xde, 0xad, 0xbe, 0xef,
  'D', 'e', 'c', 'a', 'f', 'B', 'a', 'd'
};

uint8_t LED_Status = 0;
uint32_t lastCheck;
#define BlinkInterval 1000 // 1s
#define BlinkLength 200 // 0.1 s

BME280 bme280;
float MSL = 102360;
uint32_t t0;
#define DelaySomeMore 30000

#define myDIO0 PE7
#define myRESET PE9
#define myNSS PB5
// BW = 7: 125 kHz, CR = 1: 4/5, HM = 0
uint8_t reg1 = 0x72;
// SF = 12: 12, CRC = 0
uint8_t reg2 = 0xC0;
// LDRO = 1, AGCAutoOn = 1
uint8_t reg3 = 0x0C;
// PaSelect = 1, MaxPower = 7: 15 dBm, OutputPower = 15: 17 dBm
uint8_t regpaconfig = 0xFF;
#define REG_OCP 0x0B
#define REG_PA_CONFIG 0x09
#define REG_LNA 0x0c
#define REG_OP_MODE 0x01
#define REG_MODEM_CONFIG_1 0x1d
#define REG_MODEM_CONFIG_2 0x1e
#define REG_MODEM_CONFIG_3 0x26
#define REG_PA_DAC 0x4D
#define PA_DAC_HIGH 0x87
#define MODE_LONG_RANGE_MODE 0x80
#define MODE_SLEEP 0x00
#define MODE_STDBY 0x01
#define MODE_TX 0x03
#define MODE_RX_CONTINUOUS 0x05
#define MODE_RX_SINGLE 0x06

static uint8_t ucBackBuffer[1024];
#define SDA_PIN PIN_WIRE_SDA
#define SCL_PIN PIN_WIRE_SCL
#define RESET_PIN -1
#define OLED_ADDR -1
#define FLIP180 1
#define INVERT 0
#define USE_HW_I2C 1
#define MY_OLED OLED_128x64
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
SSOLED ssoled;

void setup() {
  SysClkFullSpeed();
  Serial.begin(230400);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, HIGH);
  delay(2000);
  LoRa.setPins(myNSS, myRESET, myDIO0); // set CS, reset, IRQ pin
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true); // if failed, do nothing
  }
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125000);
  LoRa.setCodingRate4(5);
  LoRa.setPreambleLength(8);
  LoRa.setTxPower(20, 1);
  LoRa.writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
  delay(10);
  LoRa.writeRegister(REG_PA_CONFIG, regpaconfig);
  LoRa.writeRegister(REG_MODEM_CONFIG_1, reg1);
  LoRa.writeRegister(REG_MODEM_CONFIG_2, reg2);
  LoRa.writeRegister(REG_MODEM_CONFIG_3, reg3);
  delay(10);
  LoRa.writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
  LoRa.dumpRegisters(Serial);
  Serial.println("LoRa init OK.");

  int rc;
  rc = oledInit(&ssoled, MY_OLED, OLED_ADDR, FLIP180, INVERT, USE_HW_I2C, SDA_PIN, SCL_PIN, RESET_PIN, 1000000L); // use standard I2C bus at 400Khz
  if (rc != OLED_NOT_FOUND) {
    char *msgs[] = {(char *)"SSD1306 @ 0x3C", (char *)"SSD1306 @ 0x3D", (char *)"SH1106 @ 0x3C", (char *)"SH1106 @ 0x3D"};
    oledFill(&ssoled, 0, 1);
    oledWriteString(&ssoled, 0, 0, 0, msgs[rc], FONT_NORMAL, 0, 1);
    oledSetBackBuffer(&ssoled, ucBackBuffer);
    delay(2000);
  }
  sprintf(decBuf, "MSL: %.2f HPa", MSL / 100.0);
  oledFill(&ssoled, 0x0, 1);
  oledWriteString(&ssoled, 0, 0, 0, (char *)"BME280 H750 test", FONT_NORMAL, 0, 1);
  oledWriteString(&ssoled, 0, 0, 1, (char *)" Written by Kongduino", FONT_SMALL, 1, 1);
  oledWriteString(&ssoled, 0, 0, 3, (char *)"**Demo**", FONT_LARGE, 0, 1);
  oledWriteString(&ssoled, 0, 0, 7, decBuf, FONT_NORMAL, 0, 1);
  oledSetBackBuffer(&ssoled, ucBackBuffer);
  Serial.println("BME280 H750 test");
  Serial.println(decBuf);
  if (!bme280.init()) {
    Serial.println("Device error!");
    oledFill(&ssoled, 0x0, 1);
    oledWriteString(&ssoled, 0, 0, 3, (char *)"*Error*", FONT_LARGE, 0, 1);
    oledSetBackBuffer(&ssoled, ucBackBuffer);
    while (1) ;
  }
  digitalWrite(LED_BLUE, LOW);
  delay(2000);
  showData();
  MyTimer.setPrescaleFactor(36818);
  MyTimer.setOverflow(65536); // 480000000/7234/32768 = 2 Hz. 1 second at full speed, 2 s at half speed
  // 32768 / 65536 = 8.9 s
  // 36818 / 65536 = 10 s
  MyTimer.attachInterrupt(OnTimer1Interrupt);
  MyTimer.refresh(); // Make register changes take effect
  MyTimer.resume(); // Start
}

void loop() {
  if (LED_Status == 0 && millis() - lastCheck >= BlinkInterval) {
    // LED is off, and we have waited at least "BlinkInterval" (1 s): we will turn it on
    LED_Status = 255;
    digitalWrite(LED_BUILTIN, LED_Status);
    lastCheck = millis();
  } else if (LED_Status == 255 && millis() - lastCheck >= BlinkLength) {
    // LED is on, and we have waited at least "BlinkLength" (0.2 s): we will turn it off
    LED_Status = 0;
    digitalWrite(LED_BUILTIN, LED_Status);
    lastCheck = millis();
  }
  if (Serial.available()) {
    uint8_t ix = 0;
    while (Serial.available()) decBuf[ix++] = Serial.read();
    decBuf[ix] = 0;
    float mymsl = atof(decBuf);
    if (mymsl > 87000.0 && mymsl < 108480.0) {
      MSL = mymsl;
      Serial.printf("MSL set to %.2f\n", MSL);
      showData();
    }
  }
}

void showData() {
  digitalWrite(LED_BLUE, HIGH);
  float temperature, pressure, altitude, humidity;
  // get and print temperatures
  temperature = bme280.getTemperature();
  oledFill(&ssoled, 0x0, 1);
  sprintf(decBuf, "Temp: %.2f C", temperature);
  oledWriteString(&ssoled, 0, 0, 0, decBuf, FONT_NORMAL, 0, 1);
  Serial.println(decBuf);

  // get and print humidity data
  humidity = bme280.getHumidity();
  sprintf(decBuf, "RH:   %.2f%%", humidity);
  oledWriteString(&ssoled, 0, 0, 1, decBuf, FONT_NORMAL, 0, 1);
  Serial.println(decBuf);

  // get and print atmospheric pressure data
  pressure = bme280.getPressure();
  sprintf(decBuf, "HPa:  %.2f", pressure / 100.0);
  oledWriteString(&ssoled, 0, 0, 2, decBuf, FONT_NORMAL, 0, 1);
  Serial.println(decBuf);

  // get and print altitude data
  altitude = bme280.calcAltitude(pressure, MSL);
  sprintf(decBuf, "Alt:  %.2f m", altitude);
  oledWriteString(&ssoled, 0, 0, 3, decBuf, FONT_NORMAL, 0, 1);
  Serial.println(decBuf);

  sprintf(decBuf, "MSL: %.2f HPa", MSL / 100.0);
  oledWriteString(&ssoled, 0, 0, 7, decBuf, FONT_NORMAL, 0, 1);
  oledWriteString(&ssoled, 0, 0, 6, (char*)"Sending packet", FONT_NORMAL, 0, 1);
  Serial.println("Sending packet...");
  oledSetBackBuffer(&ssoled, ucBackBuffer);
  // send packet
  sprintf(decBuf, "Temp: %.2f C, RH: %.2f%%, HPa: %.2f, MSL: %.2f HPa", temperature, humidity, pressure, MSL / 100.0);
  int rslt = encryptECB((uint8_t*)decBuf, pKey);
  LoRa.beginPacket();
  LoRa.write((uint8_t*)encBuf, rslt);
  LoRa.endPacket();
  Serial.println("done!");
  oledWriteString(&ssoled, 0, 0, 6, (char*)"done!         ", FONT_NORMAL, 0, 1);
  oledSetBackBuffer(&ssoled, ucBackBuffer);
  delay(1000);
  oledWriteString(&ssoled, 0, 0, 6, (char*)"     ", FONT_NORMAL, 0, 1);
  oledSetBackBuffer(&ssoled, ucBackBuffer);
  t0 = millis();
  digitalWrite(LED_BLUE, LOW);
  LED_Status = 0;
  lastCheck = millis();
}
