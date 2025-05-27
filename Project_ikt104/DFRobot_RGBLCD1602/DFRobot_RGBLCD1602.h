/*!
 * @file DFRobot_RGBLCD1602.h
 * @brief DFRobot_RGBLCD1602 class infrastructure
 * @copyright	Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @licence     The MIT License (MIT)
 * @maintainer [yangfeng](feng.yang@dfrobot.com)
 * @version  V1.0
 * @date  2021-09-24
 * @url https://github.com/DFRobot/DFRobot_RGBLCD1602
 *
 * @brief  Ported to Mbed OS
 * @author Ken Henry Andersen
 * @date   2024-01-31
 */

#ifndef __DFRobot_RGBLCD1602_H__
#define __DFRobot_RGBLCD1602_H__

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "mbed.h"

/*!
 *  @brief Device I2C Address (7-bit)
 */
#define LCD_ADDRESS_7BIT (0x7C >> 1)

/*
Change the RGB Address value based on
the hardware version
--------------------------------------------
      Moudule       | Version | RGBAddr 7bit
--------------------------------------------
 LCD1602 Module     |  V1.0   |    0x60
--------------------------------------------
 LCD1602 Module     |  V1.1   |    0x6B
--------------------------------------------
 LCD1602 RGB Module |  V1.0   |    0x60
--------------------------------------------
 LCD1602 RGB Module |  V2.0   |    0x2D
--------------------------------------------
*/
#define RGB_ADDRESS_V10_7BIT (0x60) // LCD1601 RGB Module V1.0
#define RGB_ADDRESS_V20_7BIT (0x2D) // LCD1601 RGB Module V2.0

/*!
 *  @brief color define
 */
#define WHITE 0
#define RED 1
#define GREEN 2
#define BLUE 3

#define REG_MODE1 0x00
#define REG_MODE2 0x01
#define REG_OUTPUT 0x08

/*!
 *  @brief commands
 */
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

/*!
 *  @brief flags for display entry mode
 */
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

/*!
 *  @brief flags for display on/off control
 */
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

/*!
 *  @brief flags for display/cursor shift
 */
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

/*!
 *  @brief flags for function set
 */
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

class DFRobot_RGBLCD1602 {
public:
  /**
   * @fn DFRobot_RGBLCD1602
   * @brief Constructor
   */
  DFRobot_RGBLCD1602(I2C *i2c_p = NULL, uint8_t rgbAddr7b = RGB_ADDRESS_V20_7BIT,
                     uint8_t lcdAddr7b = LCD_ADDRESS_7BIT, uint8_t lcdCols = 16,
                     uint8_t lcdRows = 2);

  /**
   * @fn init
   * @brief initialize the LCD and master IIC
   */
  void init();

  /**
   * @fn clear
   * @brief clear the display and return the cursor to the initial position
   * (position 0)
   */
  void clear();

  /**
   * @fn home
   * @brief return the cursor to the initial position (0,0)
   */
  void home();

  /**
   * @fn noDisplay
   * @brief Turn off the display
   */
  void noDisplay();

  /**
   * @fn display
   * @brief Turn on the display
   */
  void display();

  /**
   * @fn stopBlink
   * @brief Turn  off the blinking showCursor
   */
  void stopBlink();

  /**
   * @fn blink
   * @brief Turn on  the blinking showCursor
   */
  void blink();

  /**
   * @fn noCursor
   * @brief Turn off the underline showCursor
   */
  void noCursor();

  /**
   * @fn cursor
   * @brief Turn on the underline showCursor
   */
  void cursor();

  /**
   * @fn scrollDisplayLeft
   * @brief scroll left to display
   */
  void scrollDisplayLeft();

  /**
   * @fn scrollDisplayRight
   * @brief scroll right to display
   */
  void scrollDisplayRight();

  /**
   * @fn leftToRight
   * @brief This is for text that flows Left to Right
   */
  void leftToRight();

  /**
   * @fn rightToLeft
   * @brief This is for text that flows Right to Left
   */
  void rightToLeft();

  /**
   * @fn noAutoscroll
   * @brief This will 'left justify' text from the showCursor
   */
  void noAutoscroll();

  /**
   * @fn autoscroll
   * @brief This will 'right justify' text from the showCursor
   */
  void autoscroll();

  /**
   * @fn customSymbol
   * @brief Allows us to fill the first 8 CGRAM locations with custom characters
   * @param location substitute character range (0-7)
   * @param charmap  character array the size is 8 bytes
   */
  void customSymbol(uint8_t location, uint8_t charmap[]);

  /**
   * @fn setCursor
   * @brief set cursor position
   * @param col columns optional range 0-15
   * @param row rows optional range 0-1，0 is the first row, 1 is the second row
   */
  void setCursor(uint8_t col, uint8_t row);

  /**
   * @fn setRGB
   * @brief set RGB
   * @param r  red   range(0-255)
   * @param g  green range(0-255)
   * @param b  blue  range(0-255)
   */
  void setRGB(uint8_t r, uint8_t g, uint8_t b);

  /**
   * @fn setPWM
   * @brief set backlight PWM output
   * @param color  backlight color  Preferences：REG_RED\REG_GREEN\REG_BLUE
   * @param pwm  color intensity   range(0-255)
   */
  void setPWM(uint8_t color, uint8_t pwm) {
    setReg(color, pwm);
    if (_rgbAddr7b == 0x6B) {
      setReg(0x07, pwm);
    }
  }

  /**
   * @fn setColor
   * @brief backlight color
   * @param color  backlight color  Preferences： WHITE\RED\GREEN\BLUE
   */
  void setColor(uint8_t color);

  /**
   * @fn closeBacklight
   * @brief close the backlight
   */
  void closeBacklight() { setRGB(0, 0, 0); }

  /**
   * @fn setColorWhite
   * @brief set the backlight to white
   */
  void setColorWhite() { setRGB(255, 255, 255); }

  /**
   * @fn write
   * @brief write character
   * @param data the written data
   */
  virtual size_t write(uint8_t data);

  /**
   * @fn command
   * @brief send command
   * @param data the sent command
   */
  void command(uint8_t data);

  /**
   * @fn setBacklight
   * @brief set the backlight
   * @param mode  true indicates the backlight is turned on and set to white,
   * false indicates the backlight is turned off
   */
  void setBacklight(bool mode);

  void printf(const char *fmt_p, ...);

private:
  /**
   * @fn begin
   * @brief the initialization function
   * @param row rows optional range 0-1，0 is the first row, 1 is the second row
   * @param charSize  character size LCD_5x8DOTS\LCD_5x10DOTS
   */
  void begin(uint8_t rows, uint8_t charSize = LCD_5x8DOTS);

  /**
   * @fn send
   * @brief set cursor
   * @param data_p the data to send
   * @param len length of the data
   */
  void send(uint8_t *data_p, uint8_t len);

  /**
   * @fn setReg
   * @brief Configure related registers
   * @param addr  The address of the register to be operated on
   * @param data  The data to be written
   */
  void setReg(uint8_t addr, uint8_t data);

  I2C *_i2c_p;
  uint8_t _showFunction;
  uint8_t _showControl;
  uint8_t _showMode;
  uint8_t _initialized;
  uint8_t _numLines, _currLine;
  uint8_t _lcdAddr7b;
  uint8_t _lcdAddr8b;
  uint8_t _rgbAddr7b;
  uint8_t _rgbAddr8b;
  uint8_t _cols;
  uint8_t _rows;

public:
  uint8_t REG_RED = 0;   // pwm2
  uint8_t REG_GREEN = 0; // pwm1
  uint8_t REG_BLUE = 0;  // pwm0
  uint8_t REG_ONLY = 0;  // pwm0
};

#endif
