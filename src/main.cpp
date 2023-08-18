/*
   TKH-RV-Display
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_HX8357.h>
#include <Adafruit_STMPE610.h>
#include <math.h>

// ESP32 Definitions
#define STMPE_CS 32
#define TFT_CS   15
#define TFT_DC   33
#define SD_CS    14

#define TFT_RST -1


// Use hardware SPI and the above for CS/DC
Adafruit_HX8357  tft = Adafruit_HX8357( TFT_CS,  TFT_DC,  TFT_RST);
Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);

// This is calibration data for the raw touch data to the screen coordinates
// For rotation 1, these put the buttons at the top of the screen
#define TS_MINX 3800
#define TS_MAXX 100
#define TS_MINY 100
#define TS_MAXY 3750

// Redefine original colors, add additional colors to match those available with the ILI9341 library
#define BLACK       0x0000  ///<   0,   0,   0
#define NAVY        0x000F  ///<   0,   0, 123
#define DARKGREEN   0x03E0  ///<   0, 125,   0
#define DARKCYAN    0x03EF  ///<   0, 125, 123
#define MAROON      0x7800  ///< 123,   0,   0
#define PURPLE      0x780F  ///< 123,   0, 123
#define OLIVE       0x7BE0  ///< 123, 125,   0
#define LIGHTGREY   0xC618  ///< 198, 195, 198
#define DARKGREY    0x7BEF  ///< 123, 125, 123
#define BLUE        0x001F  ///<   0,   0, 255
#define GREEN       0x07E0  ///<   0, 255,   0
#define CYAN        0x07FF  ///<   0, 255, 255
#define RED         0xF800  ///< 255,   0,   0
#define MAGENTA     0xF81F  ///< 255,   0, 255
#define YELLOW      0xFFE0  ///< 255, 255,   0
#define WHITE       0xFFFF  ///< 255, 255, 255
#define ORANGE      0xFD20  ///< 255, 165,   0
#define GREENYELLOW 0xAFE5  ///< 173, 255,  41
#define PINK        0xFC18  ///< 255, 130, 198

#define MYGREY      0xDEFB  ///< 220, 220, 220
#define MYGREEN     0x640
#define DOTOFFSET   16
#define FORWARD      1
#define STOPPED      0
#define BACKWARD    -1


//==============================================================================
// Global Variables
//==============================================================================
int           myindex=0;
int           xoffset = 0;
int           yoffset = 160;
int           creep = 0;
volatile int  creepIntCounter;                                // shared by loop() and ISR - compiler must not optimize
int           totalcreepIntCounter;
hw_timer_t *  creepTimer = NULL;                              // pointer to my timer "creepTimer"
portMUX_TYPE  semaCreepTimer = portMUX_INITIALIZER_UNLOCKED;  // semaphore "CreepTimer"


//--------------------------------------------------------------------------------
// Function definitions
//--------------------------------------------------------------------------------


//====================================================
// Interrupt Service Routine for creep timer
// Every time the creep timer interrupt fires, the
// moving current dots advance by one distance unit
//====================================================

void IRAM_ATTR isrA()                                    // the ISR code should reside in internal RAM (IRAM), not Flash memory
{
  portENTER_CRITICAL_ISR(&semaCreepTimer);               // entering critical path
  creepIntCounter++;                                     // that's all we do here. Goodbye.
  portEXIT_CRITICAL_ISR(&semaCreepTimer);                // exiting critical path
}


//==========================================
// Draw horizontal line
//==========================================
void hotwireH(uint xmin, uint xmax, uint ymin, uint thick, uint backColor, uint foreColor, uint directn, uint progress)
{
uint circleNumber;
tft.fillRect(xmin, ymin, xmax-xmin, thick, backColor);
circleNumber=1;
//while (circleNumber<10)
while (xmin + thick + (circleNumber-1)*DOTOFFSET + progress < xmax)
  {
  if (directn==FORWARD)    tft.fillCircle (xmin+thick/2+(circleNumber-1)*DOTOFFSET+progress,ymin+thick/2,4,foreColor);
  if (directn==BACKWARD)   tft.fillCircle (xmax-thick/2-(circleNumber-1)*DOTOFFSET-progress,ymin+thick/2,4,foreColor);
  circleNumber++;
  }
}
//==========================================
// Draw vertical line
//==========================================
void hotwireV(uint ymin, uint ymax, uint xmin, uint thick, uint backColor, uint foreColor, uint directn, uint progress)
{
uint circleNumber;

//  fillRect( x,   y,    w,         h,          color)
tft.fillRect(xmin, ymin, thick,     ymax-ymin,  backColor);

circleNumber=1;
//while (circleNumber<10)
while (ymin + thick + (circleNumber-1)*DOTOFFSET + progress < ymax)
  {
//                             fillCircle (x0,           y0,                                               r, color)
  if (directn==FORWARD)    tft.fillCircle (xmin+thick/2, ymin+thick/2+(circleNumber-1)*DOTOFFSET+progress, 4, foreColor);
  if (directn==BACKWARD)   tft.fillCircle (xmin+thick/2, ymax-thick/2-(circleNumber-1)*DOTOFFSET-progress, 4, foreColor);
  circleNumber++;
  }
}

//--------------------------------------------------------------------------------
// SETUP
//--------------------------------------------------------------------------------
void setup()
  {
  Serial.begin(115200);
  Serial.println("Setup starting");

  creepTimer = timerBegin(0, 80, true);                   // 0->Timer number (A)   80-> 1 microsecond at 80 MHz   true-> rising edge
  timerAttachInterrupt(creepTimer, &isrA, true);          // fire ISR isrA on edge of creepTimer
  timerAlarmWrite(creepTimer, 50000, true);               // 50000 -> 50 milliseconds   true-> reload!
  timerAlarmEnable(creepTimer);                           // activate the timer


  tft.begin();
  tft.setRotation(1);
  tft.setTextWrap(false);
  tft.fillScreen(WHITE);

  tft.fillRect(220, 140, 40, 33, ORANGE);
  tft.drawRect(220, 140, 40, 33, BLUE);

  }

//--------------------------------------------------------------------------------
// LOOOOOOP
//--------------------------------------------------------------------------------
void loop()
  {
  if (creepIntCounter > 0)
    {
    portENTER_CRITICAL(&semaCreepTimer);                  // entering critical path
    creepIntCounter--;                                    // ok I see the interrupt
    portEXIT_CRITICAL(&semaCreepTimer);                   // exiting critical path

    hotwireH(40,400,30,9,MYGREY,RED,FORWARD,creep);
    hotwireH(40,380,50,9,MYGREY,WHITE,STOPPED,creep);
    hotwireH(40,380,70,9,MYGREY,MYGREEN,BACKWARD,creep);

    hotwireV(30,280,400,9,MYGREY,RED,FORWARD,creep);
    hotwireV(40,280,440,9,MYGREY,WHITE,STOPPED,creep);
    hotwireV(40,280,460,9,MYGREY,MYGREEN,BACKWARD,creep);

    totalcreepIntCounter++;

    creep++;
    if (creep == DOTOFFSET)   creep=0;

//   Serial.print("creepTimer triggered an interrupt. Total number: ");
//   Serial.println(totalcreepIntCounter);
    }


}
