// An Airsoft project that attaches a transparent HUD to an Airsoft gun. The device detects the number of BBs shot
// and shows a count of how many BBs are left in the magazine. It can also help keep track of the number of kills
// and deaths during a match. A XIAO ESP32-C3 micro controller is used together with a SSD1309 transparent OLED display.
// BBs are detected as they travel through a mock suppressor that contains a small IR Transmitter LED and IR Receiver sensor.
//
// Video:               https://youtu.be/BZJjDX5xyRM
// Full documentation:  AirsoftHUD.pdf
// Wiring Diagram:      WiringDiagram.fzz
// 3D Models:           In folder "3DModels"
//
// NOTE: Device won't connect to PC via USB when in deep sleep. Therefore, first switch on / wake up before you upload code

//#define DEBUG 1

#ifdef DEBUG
#define DebugPrintf(...) Serial.printf(__VA_ARGS__)
#define DebugPrintln(...) Serial.println(__VA_ARGS__)
#else
#define DebugPrintln(...)
#define DebugPrintf(...)
#endif

#include <Wire.h>         // I2C interface
#include <U8g2lib.h>      // Display library

#include "Button.h"
#include "StateToggleTimer.h"

// Define pins on XIAO ESP32-C3
static const int pinIRReceiver = A0;
static const int pinIRTransmitter = D6;
static const int pinFrontButton = D2;
static const int pinBackButton = D7;

// Define minimum time between shots
static const int maxRPS = 25;                                       // Rounds per second when in full auto
static const unsigned long minTimeBetweenShots = (1000 / maxRPS);   // Time in milliseconds between shots in full auto

// Read/write persistant data using EEPROM
#include <Preferences.h>
Preferences persistedData;

// Transparent OLED display SSD1309
char displayText[64];
bool bUpdateDisplay = true;
U8G2_SSD1309_128X64_NONAME2_F_HW_I2C display(U8G2_R3);

// Define buttons and ISRs
Button frontButton(pinFrontButton);
Button backButton(pinBackButton);
void frontButtonISR() { frontButton.onInterrupt(); }
void backButtonISR() { backButton.onInterrupt(); }

// Keep track of deep sleep status
bool bGotoSleep = false;
bool bIsSleeping = false;

// Keep track of kills/deaths
int numKills = 0;
int numDeaths = 0;

// Keep track of shots
int numShots = 0;
int maxNumBBsInMagazine = 135;   // EMG / KRYTAC 200rd/50rd Selectable Capacity Magazine actually only takes 135 BBs

// Flash display when magazine is running low on BBs
static const int numBBsToStartFlashing = 10;
StateToggleTimer flashNumBBsLeft(250);

// Keep track of when BBs are detected
volatile bool bbDetected = false;
volatile unsigned long bbDetectedTime = millis();

// ISR when BB is detected
void bbDetectedISR()
{
  bbDetected = true;
  bbDetectedTime = millis();
}

void setup()
{
#ifdef DEBUG
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n********* Airsoft HUD *********\n");
#endif

  // Restore data from previous power down
  LoadData();

  // Setup IR transmitter LED
  pinMode(pinIRTransmitter, OUTPUT);
  digitalWrite(pinIRTransmitter, HIGH);

  // Setup interrupt to detect BBs. When a BB passes between the IR transmitter and receiver, the voltage will change from HIGH to LOW, i.e. use FALLING
  attachInterrupt(pinIRReceiver, bbDetectedISR, FALLING);

  // Setup buttons
  frontButton.Setup(frontButtonISR);
  backButton.Setup(backButtonISR);

  // Setup display
  bUpdateDisplay = true;
  display.setI2CAddress(0x3d << 1);   // Address is 0x3d, but for some reason the U8g2 lib adds a right shift 1 during setup, so we left shift 1
  display.setBusClock(4000000);
  display.begin();
}

void loop()
{
  unsigned long nowTime = millis();

  // To avoid double counting a single BB, we only count one BB during the minimum time defined by the RPS when in full auto
  if (bbDetected && (nowTime - bbDetectedTime) > minTimeBetweenShots)
  {
      numShots++;
      bbDetected = false;
      bUpdateDisplay = true;
      DebugPrintf("Num shots = %d\n", numShots);
  }

  // A button interrupt will wake the device up, so make sure that we're not in the middle of a button press while going into deep sleep
  if (bGotoSleep)
  {
    if (!frontButton.IsPressed() && !backButton.IsPressed())
    {
      bGotoSleep = false;
      DeepSleep();
    }
  }
  else
  {
    if (frontButton.WasLongPressed())   // Power down / deep sleep
    {
      DebugPrintln("Front button long press");
      bGotoSleep = true;
    }
    else if (backButton.WasLongPressed())   // Reset stats
    {
      DebugPrintln("Back button long press");
      numKills = 0;
      numDeaths = -1;   // Button up from this long press will trigger an interrupt, adding an extra numDeaths count, so reset to -1, not 0
    }
    else if (frontButton.IsPressed() && backButton.IsPressed())   // Reset numShots when switching to a new magazine
    {
      DebugPrintln("Both buttons pressed");
      frontButton.Reset();
      backButton.Reset();
      numShots = 0;
      bUpdateDisplay = true;
      flashNumBBsLeft.Stop();
    }
    else
    {
      if (frontButton.WasPressed())   // Another kill :-)
      {
        DebugPrintln("Front button pressed");

        if (bIsSleeping)
        {
            WakeUp();
        }
        else
        {
          bUpdateDisplay = true;
          numKills++;
        }
      }

      if (backButton.WasPressed())    // Another death :-(
      {
        DebugPrintln("Back button pressed");
        bUpdateDisplay = true;
        numDeaths++;
      }
    }
  }

  UpdateDisplay();
}

void UpdateDisplay()
{
  // Don't display number of BBs shot, but rather how many BBs are left in magazine
  int numShotsLeft = max(maxNumBBsInMagazine - numShots, 0);
  FlashIfLowOnBBs(numShotsLeft);

  // Only update the display when really needed
  if (bUpdateDisplay)
  {
    bUpdateDisplay = false;

    display.clearBuffer();

    if (flashNumBBsLeft.IsOn())
    {
      // Extra logic to center align digits by adding extra spaces based on the number of digits in the number
      sprintf(displayText, "%s%s%d", (numShotsLeft < 100) ? " " : "", (numShotsLeft < 10) ? " " : "", numShotsLeft);
      display.setFont(u8g2_font_fub25_tn);
      display.drawStr((numShotsLeft >= 100) ? 5 : 0, 25, displayText);
    }

    display.setFont(u8g2_font_7x13_tf);
    display.drawStr(15, 60,"Kills");

    sprintf(displayText, "%d", numKills);
    display.setFont(u8g2_font_fub14_tn);
    display.drawStr(15, 80, displayText);

    display.setFont(u8g2_font_7x13_tf);
    display.drawStr(15, 100,"Deaths");

    sprintf(displayText, "%d", numDeaths);
    display.setFont(u8g2_font_fub14_tn);
    display.drawStr(15, 120, displayText);

    display.sendBuffer();
  }
}

// If magazine is almost empty, flash the number of BBs left
void FlashIfLowOnBBs(int numShotsLeft)
{
  if ((numShotsLeft == numBBsToStartFlashing) && !flashNumBBsLeft.HasStarted())
  {
    flashNumBBsLeft.Start();
  }
  
  if ((numShotsLeft <= numBBsToStartFlashing) && flashNumBBsLeft.HasStateChanged())
  {
    bUpdateDisplay = true;
  }

  unsigned long nowTime = millis();
  flashNumBBsLeft.Update(nowTime);
}

// No physiscal power switch, just put device into deep sleep. The spec for XIAO ESP32-C3 says it only uses 44uA
// when in deep sleep, so it should last for several months on a 800mAh battery
void DeepSleep()
{
  bIsSleeping = true;

  display.setPowerSave(1);              // Power down display
  detachInterrupt(pinIRReceiver);       // Disable interrupt that detects BBs
  digitalWrite(pinIRTransmitter, LOW);  // Switch IR LED off
  SaveData();                           // Save current data to EEPROM

  // Setup deep sleep to wake up on a front button press
  esp_deep_sleep_enable_gpio_wakeup(1ULL << D2, ESP_GPIO_WAKEUP_GPIO_LOW);

  // Now we're ready to deep sleep
  esp_deep_sleep_start();
  
}

// Wakeup from deep sleep
void WakeUp()
{
  bIsSleeping = false;

  LoadData();                                             // Restore data from previous power down
  display.setPowerSave(0);                                // Power on display
  digitalWrite(pinIRTransmitter, HIGH);                   // Switch IR LED on
  attachInterrupt(pinIRReceiver, bbDetectedISR, FALLING); // Enable interrupt that detects BBs
}

// When powering up, data is always reset, so save info before powering down
void SaveData()
{
  persistedData.begin("AirsoftHUD", false);
  persistedData.putInt("numKills", (int32_t)numKills);
  persistedData.putInt("numDeaths", (int32_t)numDeaths);
  persistedData.putInt("numShots", (int32_t)numShots);
  persistedData.end();
}

void LoadData()
{
  persistedData.begin("AirsoftHUD", false);
  numKills = persistedData.getInt("numKills");
  numDeaths = persistedData.getInt("numDeaths");
  numShots = persistedData.getInt("numShots");
  persistedData.end();
}