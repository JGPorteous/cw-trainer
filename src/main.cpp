/****************************************
  Arduino Morse code Trainer by Tom Lewis, N4TL.   February 21, 2016
  This sketch sends a few random Morse code characters, then waits for the student to send them back with an external keyer.
  If the same characters are sent back the sketch sends new random characters. If they are wrong the sketch sends the same characters over.
  The PS2 keyboard allows the user to change some operational parameters.
  See the CW Trainer article.docx file for more information.
  73 N4TL

  Uses code by Glen Popiel, KW5GP, found in his book, Arduino for Ham Radio, published by the ARRL.
  Modified and added LCD display by Glen Popiel - KW5GP

  Uses Arduino Morse Library by Erik Linder SM0RVV and Mark VandeWettering K6HX
  Contact: sm0rvv at google mail. Released 2011 under GPLv3 Version 0.2

  Uses MORSE ENDECODER Library by raronzen
  Copyright (C) 2010, 2012 raron GNU GPLv3 license (http://www.gnu.org/licenses)
  Contact: raronzen@gmail.com  (not checked too often..)
  Details: http://raronoff.wordpress.com/2010/12/16/morse-endecoder/

  Uses the library for the Adafruit RGB 16x2 LCD Shield by Limor Fried/Ladyada
  for Adafruit Industries http://www.adafruit.com/products/714. BSD license.

  Oct. 2016 - Modified to replace PS-2 keyboard functions with LCD menus and buttons
  by Mike Hughes, KC1DMR. Latest source at https://github.com/mfhughes128/cw-trainer

  April 2020 - 
   * Using Platform IO IDE
   * - Correct Folder Layout
   * - Forward Declaration of Functions
   * U8g2 library with 0.91" LCD
   * Serial Key Input
*****************************************/

#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <Morse.h>
#include <MorseEnDecoder.h>  // Morse EnDecoder Library
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C lcd(U8G2_R0); 
#define headerText "CW Trainer [ZS6JGP]"
// These #defines make it easy to set the LCD backlight color
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00

//Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();  // LCD class
char line_buf[20];
 
// Application preferences global
//  Char set values:
//    1 = 26 alpha characters
//    2 = numbers
//    3 = punctuation characters
//    4 = all characters in alphabetical order
//    5 = all characters in Koch order. Two prefs set range-
//            KOCH_NUM is number to use
//            KOCH_SKIP is number to skip
//    6 = reserved
#define SAVED_FLG 0     // will be 170 if settings have been saved to EEPROM
#define GROUP_NUM 1     // expected number of cw characters to be received
#define GROUP_DLY 2     // delay before sending (in 0.01 sec increments)
#define KEY_SPEED 3     // morse keying speed (WPM)
#define CHAR_SET  4     // defines which character set to send the student.
#define KOCH_NUM  5     // how many character to use
#define KOCH_SKIP 6     // characters to skip in the Koch table
#define OUT_MODE  7     // 0 = Key, 1 = Speaker
#define NUM_PREFS 8     // number of entries in the preference list
byte prefs[NUM_PREFS];  // Table of preference values

//=========================================
// There is a document at the ARRL that tells how to measure CW speed by sending PARIS. 
// The length of time it takes to send PARIS in seconds divided in to 60 gives the speed in WPM
// with the Key_speed_adj = -2, 
// and the characters delay = 0, the measured output speed is
// 20 wpm measured to be 19.9 wpm
// 25 wpm measured to be 25.3 wpm
// 30 wpm measured to be 30.8 wpm
//
// and the characters delay = 10, the measured output speed is
// 20 wpm measured to be 17.7 wpm
// 25 wpm measured to be 21.6 wpm
// 30 wpm measured to be 25.6 wpm
//
// and the characters delay = 20, the measured output speed is
// 20 wpm measured to be 15.7 wpm
// 25 wpm measured to be 18.9 wpm
// 30 wpm measured to be 21.9 wpm
//=========================================
int Key_speed_adj = -2; // correction for keying speed

// IO definitions
const byte morseInPin = 4; // Pin for input
const byte beep_pin = 5;  // Pin for CW tone
const byte key_pin = 6;   // Pin for CW Key

// Forward Declared Functions
byte prefs_set(byte pref, int val);
byte get_mode();
void morse_trainer();
void morse_decode();
void set_prefs();
void paris_test();
uint8_t readButtons(void);
void prefs_init();
void lcdWrite(const char *s);
void lcdWrite(const char *s, uint32_t delay);
void lcdWrite(char *s);
void lcdWriteHeader(const char *s);

//Button Definitions (Required as this was in the Adafruit_RGBLCDShield class
#define BUTTON_UP 0x08
#define BUTTON_DOWN 0x04
#define BUTTON_LEFT 0x10
#define BUTTON_RIGHT 0x02
#define BUTTON_SELECT 0x01

//HEaders
#define decoderHeader "Decoder"
#define parisTestHeader "PARIS Test"
#define settingsHeader "Settings"

//====================
// Setup Function
//====================
void setup()
{
  // Start serial debug port
  Serial.begin(9600);
  //while(!Serial);
  lcd.setDrawColor(BLUE);
  Serial.println("N4TL CW Trainer");
  
  lcd.begin();
  //lcdWrite("ZS6JGP", 500);
  //lcdWrite("CW Trainer",500);
  //lcdWrite("de N4TL",500);
  lcdWriteHeader("CW Trainer [ZS6JGP]");
  lcd.setDrawColor(WHITE);
  Serial.println("Starting... ");
  // Initialize application preferences
  prefs_init();

}  // end setup()


//====================
// Main Loop Function
//====================
void loop()
{
  byte op_mode;

  // Run the main menu
  op_mode = get_mode();

  // Dispatch the selected operation
  switch (op_mode) {
    case 1:
      morse_trainer();
      break;
    case 2:
      morse_decode();
      break;
    case 3:
      set_prefs();
      break;
    case 4:
      paris_test();
      break;
  }  //end dispatch switch  
}  // end loop()


//====================
// Operating Mode Menu Function
//====================
byte get_mode()
{
  // Main menu strings
  const static char msg0[] PROGMEM = "CW Trainer ";
  const static char msg1[] PROGMEM = "Start";
  const static char msg2[] PROGMEM = "Decoder  ";
  const static char msg3[] PROGMEM = "Settings";
  const static char msg4[] PROGMEM = "PARIS Test ";
  const static char* const main_menu[] PROGMEM = {msg0, msg1, msg2, msg3, msg4};
  const byte n_entry = 4;  // number of menu options

  int entry = 1;  // current menu option
  byte buttons = 0;
  boolean done = false;

  // Clear the LCD and display menu heading
  // lcd.fillScreen(ST7735_WHITE);
  // lcd.setTextColor(ST7735_BLACK, ST7735_WHITE);
  lcd.setCursor(0,0);
  strcpy_P(line_buf, (char*)pgm_read_word(&(main_menu[0])));
  lcdWrite(line_buf, 1000);
  
  do {
    // display this menu option on 2nd line
    strcpy_P(line_buf, (char*)pgm_read_word(&(main_menu[entry])));
    lcdWrite(line_buf, 10);  // short delay for readability

    // wait for a button press then handle it.
    while(!(buttons = readButtons()));  // wait for button press
    if (buttons & BUTTON_UP) --entry;
    if (buttons & BUTTON_DOWN) ++entry;

    if (entry > 4)
      entry = 0;
    if (entry < 0)
      entry = 4;
    entry = constrain(entry, 1, n_entry);
    
    if (buttons & BUTTON_SELECT) {
      while(readButtons());  // wait for button release
      done = true; 
    }
  } while(!done);
  
  return entry;
}  // end get_mode()

//====================
// LCD Write 
//====================
void lcdWrite(const char *s, uint32_t d) {
  lcdWrite(s);
  delay(d);
}

void lcdWriteMorseIn(const char *s, uint32_t d) {
  lcd.setColorIndex(BLUE);
  // lcd.clearBuffer();         // clear the internal memory
  lcd.setFont(u8g2_font_logisoso16_tr);  // choose a suitable font at https://github.com/olikraus/u8g2/wiki/fntlistall
  lcd.drawStr(20,32,s);  // write something to the internal memory
  
  lcdWriteHeader(headerText);
  lcd.sendBuffer();         // transfer internal memory to the display
  delay(d);
}
void lcdWriteHeader(const char *s) {
  lcd.setColorIndex(GREEN);
  //lcd.clearBuffer();         // clear the internal memory
  lcd.setFont(u8g2_font_5x7_tf);  // choose a suitable font at https://github.com/olikraus/u8g2/wiki/fntlistall
  lcd.drawStr(0, 10, s);  // write something to the internal memory
}

void lcdWrite(const char *s) {
  lcd.setColorIndex(BLUE);
  lcd.clearBuffer();         // clear the internal memory
  lcd.setFont(u8g2_font_logisoso16_tr);  // choose a suitable font at https://github.com/olikraus/u8g2/wiki/fntlistall
  lcd.drawStr(0,32,s);  // write something to the internal memory
  
  lcdWriteHeader(headerText);
  lcd.sendBuffer();         // transfer internal memory to the display

}

void lcdWritePrefs(const char *prefItem, const char *prefValue) {
  lcd.setColorIndex(WHITE);
  lcd.clearBuffer();         // clear the internal memory
  lcd.setFont(u8g2_font_t0_12b_mf);  // choose a suitable font at https://github.com/olikraus/u8g2/wiki/fntlistall
  lcd.drawStr(0,20, prefItem);  // write something to the internal memory
  lcd.setFont(u8g2_font_5x7_tf);  // choose a suitable font at https://github.com/olikraus/u8g2/wiki/fntlistall
  lcd.drawStr(0,30, prefValue);  // write something to the internal memory

  lcdWriteHeader(settingsHeader);
  lcd.sendBuffer();         // transfer internal memory to the display

}

void lcdWrite(char *s) {
  lcd.clearBuffer();         // clear the internal memory
  lcd.setFont(u8g2_font_logisoso18_tr);  // choose a suitable font at https://github.com/olikraus/u8g2/wiki/fntlistall
  lcd.drawStr(0, 32, s);  // write something to the internal memory
  lcd.sendBuffer();         // transfer internal memory to the display
}


void lcdWrite(char *s, char* h) {
  lcd.clearBuffer();         // clear the internal memory
  lcd.setFont(u8g2_font_logisoso18_tr);  // choose a suitable font at https://github.com/olikraus/u8g2/wiki/fntlistall
  lcd.drawStr(0, 32, s);  // write something to the internal memory
  lcdWriteHeader(h);
  lcd.sendBuffer();         // transfer internal memory to the display
}
//====================
// Set Preferences menu Function
//====================
void set_prefs()
{
  // Prefs menu strings
  const static char prf0[] PROGMEM = "Saving";
  const static char prf1[] PROGMEM = "Code Group Size";
  const static char prf2[] PROGMEM = "Character Delay";
  const static char prf3[] PROGMEM = "Code Speed";
  const static char prf4[] PROGMEM = "Character Set";
  const static char prf5[] PROGMEM = "Koch No";
  const static char prf6[] PROGMEM = "Skip Characters";
  const static char prf7[] PROGMEM = "Out: 0=key,1=spk";
  const static char* const prefs_menu[] PROGMEM = {prf0, prf1, prf2, prf3, prf4, prf5,prf6,prf7};

  byte pref = 1;  // current pref
  int p_val;
  int tmp;
  byte buttons = 0;
  boolean next = false; 
  boolean done = false;
  char line_buf[20];
  char menu_val[10];

  // Clear the display
  // lcd.fillScreen(ST7735_BLACK);
  Serial.println("Set preferences");

  // Loop displaying preference values and let user change them
  do {
    // Display the selected preference
    lcd.setCursor(0,0);
    strcpy_P(line_buf, (char*)pgm_read_word(&(prefs_menu[pref])));
    // strcpy_P(menu_val, (char*)pgm_read_word(&(prefs[pref])));
    lcdWritePrefs(line_buf, "");
    p_val = prefs[pref];

    do {
      // Display the pref current value and wait for button press
      // lcd.setCursor(0,1);
       // lcdWrite(" = ");
       // lcdWrite( "" + p_val);
      
      // String str = String(prefs[pref]);
      // str.toCharArray(menu_val, p_val);
      // Serial.printSerial.print("line_buf: "); Serial.println(line_buf);
      // Serial.print("p_val: "); Serial.println(p_val);
      
      dtostrf(p_val, 2,0, menu_val);
      // Serial.print("menu_val: "); Serial.println(menu_val);

      lcdWritePrefs(line_buf, menu_val);
     
      
       // lcdWrite(p_val);
      // lcdWrite(line_buf + " = " + p_val);
      // lcdWrite("          ");
      delay(250);
      while(!(buttons = readButtons())){
        // Serial.print(".");
      }

      // Handle button press in priority order
      if (buttons & BUTTON_SELECT) {
        next = true;
        done = true;
      } else if (buttons & BUTTON_UP) {
        tmp = --pref;
        pref = constrain(tmp, 1, NUM_PREFS-1);
        next = true;
      } else if (buttons & BUTTON_DOWN) {
        tmp = ++pref;
        // Serial.print("tmp: "); Serial.println(tmp);
        if (tmp > NUM_PREFS-1) //If menu out of bounds, set to first entry
          tmp = 1;
        pref = constrain(tmp, 1, NUM_PREFS-1);
        next = true;
      } else if (buttons & BUTTON_RIGHT) {
        tmp = ++p_val;
        p_val = prefs_set(pref, tmp);
      } else if (buttons & BUTTON_LEFT) {
        tmp = --p_val;
        p_val = prefs_set(pref, tmp);
      }
      
    }while(!next);  // end of values loop    
    next = false;  // Reset next prefs flag.
  } while(!done);  // end of prefs loop

  // Signal user select button was detected
  // lcd.fillScreen(ST7735_BLACK);
  strcpy_P(line_buf, (char*)pgm_read_word(&(prefs_menu[SAVED_FLG])));
  // lcd.setCursor(0,0);
  // lcdWrite(line_buf, 500);
 
  // Save all prefs to EEPROM before returning.
  prefs_set(SAVED_FLG, 170);  // Set prefs saved flag
  for (int i=0; i<NUM_PREFS; i++) {
    EEPROM.write(i, prefs[i]);
  }

  lcdWrite("Saved!", 500);
  
  // wait for button release
  while(readButtons());

}  // end set_prefs()


//====================
// Morse code trainer function
//====================
void morse_trainer()
{
  
  //Character sets
  const static char koch[] PROGMEM = {'K','M','R','S','U','A','P','T','L','O','W','I','.','N','J','E','F',
  '0','Y','V',',','G','5','/','Q','9','Z','H','3','8','B','?','4','2','7','C','1','D','6','X','\0'};
  const static char alpha[] PROGMEM = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F','G',
  'H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',',','.','/','?','\0'};
  const static char* const char_sets[] PROGMEM = {alpha, koch};
  char ch_buf[41];  // Buffer for character set
  byte cset, lo, hi; // Specify set of characters to send

  char cw_tx[17];   // Buffer for test string
  char cw_rx;       // Received character
  byte rx_cnt = 0;  // Count of received characters

  // Miscelaneous loop parameters
  byte i,j;
  boolean error = false;
  byte buttons;

  // Morse sender parameters
  byte _speed;
  byte _pin;
  byte _mode;
  

  // Init ===========================================================
  Serial.println("Morse trainer started");
  randomSeed(micros()); // random seed = microseconds since start.

  // Setup Morse receiver
  morseDecoder morseInput = morseDecoder(morseInPin, MORSE_KEYER, MORSE_ACTIVE_LOW);
  
  // Setup Morse sender
  _speed = prefs[KEY_SPEED] + Key_speed_adj;
  switch (prefs[OUT_MODE]) {
    case 0:  // Digital (key) output
      _pin = key_pin;
      _mode = 0;
      break;
    case 1:  // Analog (beep) output
      _pin = beep_pin;
      _mode = 1;
      break;
  }
  Morse morse = Morse(_pin, _speed, _mode);  
  
  // Setup character set
  // Note: The high limit on random() is exclusive, so 'hi' is the table index + 1 
  switch (prefs[CHAR_SET]) {
    case 1:  // alpha characters
      cset = 0;
      lo = 10;
      hi = 36;
      break;
    case 2:  // numbers
      cset = 0;
      lo = 0;
      hi = 10;
      break;
    case 3:  // punctuation
      cset = 0;
      lo = 36;
      hi = 40;
      break;
    case 4:  // all alphabetic
      cset = 0;
      lo = 0;
      hi = 40;
      break;
    case 5:  // Koch order
      cset = 1;
      lo = prefs[KOCH_SKIP];
      hi = prefs[KOCH_NUM];
      break;
    case 6:  // Koch order (same as 5 for now)
      cset = 1;
      lo = prefs[KOCH_SKIP];
      hi = prefs[KOCH_NUM];
      break;
  }
  strcpy_P(ch_buf, (char*)pgm_read_word(&(char_sets[cset])));    // Copy the chosen character set to working buffer

  // Start training loop =======================================================
  do {
    Serial.print("\nTop of the send loop  ");
    //// lcd.fillScreen(ST7735_BLACK);

    // Send characters to trainee
    for (i = 0; i < (prefs[GROUP_NUM]); i++)
    {
      if (!error) {  // if no error on last round, generate new text.
        j = random(lo, hi);
        cw_tx[i] = ch_buf[j];
      }

      if (prefs[GROUP_DLY] > 0) {  //Wait out delay between characters
        delay(prefs[GROUP_DLY] * 10);
      }

      char cDisplay[3] = "  ";
      cDisplay[0] = cw_tx[i];
      cDisplay[1] = '\n';

      char* cPtr = &cw_tx[i];
      //char* charPtr = &myChar;

      lcdWrite(cDisplay, 10); //cw_tx[i]);  // Display the sent char
      morse.send(cw_tx[i]);   // Send the character
      Serial.print(cw_tx[i]); // debug print
    }

    // Now check the trainee's sending
    Serial.print("\nTop of the check loop ");
    error = false;
    rx_cnt = 0;
    //lcd.setCursor(0, 1); // Set the cursor to bottom line, left
    morseInput.setspeed(prefs[KEY_SPEED]);
    // Serial.print("Key speed for decoding set to: "); Serial.println(prefs[KEY_SPEED]);
    do {  
      morseInput.decode();  // Start decoder and check char when it comes in
      if (morseInput.available()) {
        char cw_rx = morseInput.read();
        if (cw_rx != ' ') {  // Skip spaces
          lcdWriteMorseIn(&cw_rx, 500);
          Serial.print(cw_rx);
          if (cw_rx != cw_tx[rx_cnt]) error = true;
          ++rx_cnt;
        }
      }
      if (buttons = readButtons()) break;
    } while (rx_cnt < prefs[GROUP_NUM] && !error);

    // Set backlignt according to trainee's performance
    if (error) {
      //JGP lcd.setBacklight(RED);
    } else {
      //JGP lcd.setBacklight(WHITE);      
    }

    delay(100);  //0.1 sec pause at the end of the loop.

  } while(!(buttons));

  //TODO Decode and handle buttons
  //    select = exit
  //    up/dn = chg code speed (sets error so same string repeats)
  //    left/right = chg group size

  while(readButtons());  // wait for button release

}  // end morse_trainer()


//=====================================
// CW decoder only, use this section to check your keyers output to this decoder.
//=====================================
void morse_decode()
{
  char cw_rx;
  byte button;
  byte ch_cnt = 0;
  char caCwRx[9] = "      \n";

  morseDecoder morseInput = morseDecoder(morseInPin, MORSE_KEYER, MORSE_ACTIVE_LOW);

  Serial.println("Morse decoder started");
  // // lcd.fillScreen(ST7735_BLACK);
  //lcd.setCursor(0, 1);
  // JGP lcd.leftToRight();
  lcdWrite(" ", decoderHeader);
  
  do {
    morseInput.decode();  // Decode incoming CW
    if (morseInput.available()) {  // If there is a character available
      cw_rx = morseInput.read();  // Read the CW character
      if (cw_rx != ' ')
      {

        if (ch_cnt == 8) {
          for (int i =0; i < 8; i++)
            caCwRx[i] = ' ';
          //lcd.setCursor(0,1);
          //lcdWrite("                ");
          //lcd.setCursor(0,1);
          Serial.print('\n');
          ch_cnt = 0;
        } 
        Serial.print(cw_rx); // send character to the debug serial monitor
        //String cwRxStr(cw_rx);
      // char* cPtr = &cw_rx;
        
        //caCwRx[0] = cw_rx;
        //caCwRx[1] = '\n';
        //cwRxStr.toCharArray(caCwRx, cw_rx);
  
        //lcdWrite(caCwRx); //cw_rx);  // Display the CW character
        ++ch_cnt;
        caCwRx[ch_cnt] = cw_rx;
        caCwRx[ch_cnt + 1] = '\n';
        Serial.print(" caCwRx: "); Serial.println(caCwRx);
        lcdWrite(caCwRx, decoderHeader);
      }
    } // if != ' ' / not space

  } while (!(button = readButtons()));

  while (readButtons());
}  // end of morse_decode()


//=====================================
// "PARIS" test routine
//=====================================
void paris_test()
{
  char cw_tx[]= "PARIS";
  char cw_txMsg[] = "               ";

  // Morse sender parameters
  byte _speed;
  byte _pin;
  byte _mode;

  boolean done = false;
  
  // Setup Morse sender
  _speed = prefs[KEY_SPEED] + Key_speed_adj;
  switch (prefs[OUT_MODE]) {
    case 0:  // Digital (key) output
      _pin = key_pin;
      _mode = 0;
      break;
    case 1:  // Analog (beep) output
      _pin = beep_pin;
      _mode = 1;
      break;
  }
  Morse morse = Morse(_pin, _speed, _mode);  

  // Loop sending until a button is pressed
  
  do
  {
    //char fullString[];

    Serial.print("\nTop of the send loop  ");
    // // lcd.fillScreen(ST7735_BLACK);
    delay(1000);  // one second between each paris
    
    // Send characters 
    for (int i = 0; i < 5; i++)
    {
      if (readButtons()) done = true;  // if button down, this is last loop
      if (prefs[GROUP_DLY] > 0) {  //Wait out delay between characters
        delay(prefs[GROUP_DLY] * 10);
      }
      //char *p = const_cast<char*>cw_tx[i];
      //fullString += &cw_tx[i];
      //msg = cw_tx[i];
      cw_txMsg[i] = cw_tx[i];

      lcdWrite(cw_txMsg, parisTestHeader);  // Display the sent char

      morse.send(cw_tx[i]);   // Send the character
      // Serial.print(cw_tx[i]); // debug print
    }
    for (int j = 0; j <= sizeof(cw_tx); j++)
      cw_txMsg[j] = ' ';
    //cw_txMsg = "     ";
  } while (!done);

  while (readButtons());  //wait for button to be released
}  // end of paris_test()



//===========================
// Restore app preferences from EEPROM if
// values are saved, else set to defaults.
// Send values to serial port on debug. 
//===========================
void prefs_init()
{
  // Restore app settings from the EEPROM if the saved
  // flag value is 170, otherwise init to defaults.
  if (EEPROM.read(0) == 170)
  {
    for (int idx = 0; idx < NUM_PREFS; idx++)
    {
      prefs_set(idx,EEPROM.read(idx));
    }
  }
  else
  {
    prefs_set(SAVED_FLG, 0);  // Prefs not saved
    prefs_set(GROUP_NUM, 1);  // Send/receive groups of 1 char to start
    prefs_set(GROUP_DLY, 0);  // Send/receive with no delay
    prefs_set(KEY_SPEED, 25); // Send at 25 wpm to start
    prefs_set(CHAR_SET, 5);   // Use Koch order char set
    prefs_set(KOCH_NUM, 5);   // Use first 5 char in Koch set
    prefs_set(KOCH_SKIP, 0);  // Don't skip over any char to start
    prefs_set(OUT_MODE, 1);   // Output to speaker
  }
}


//========================
// Set preference specified in arg1 to value in arg2
// Constrain prefs values to defined limits
// Echo value to serial port
//========================
byte prefs_set(byte pref, int val)
{
  const byte lo_lim[] {0, 1, 0, 10, 1, 1, 0, 0};  // Table of lower limits of preference values
  const byte hi_lim[] {170, 15, 30, 30, 6, 40, 39, 1};  // Table of uppper limits of preference values
  byte new_val;
  byte indx;

  // Set new value
  indx = constrain(pref, 0, NUM_PREFS-1);                // Constrain index, just to be safe
  // Serial.print("prefs_set:  val: "); Serial.print(val); Serial.print(" indx: "); Serial.print(indx); Serial.print(" hi_lim[indx]: "); Serial.println(hi_lim[indx]);
  if (val > hi_lim[indx])
    val = lo_lim[indx];

  if (val < lo_lim[indx])
    val = hi_lim[indx];

  new_val = constrain(val, lo_lim[indx], hi_lim[indx]);  // Set new value within defined limits

  // Dispatch on preference index to do debug print
  switch (indx) {
    case SAVED_FLG:
      Serial.print("Saved flag = ");
      break;
    case GROUP_NUM:
      Serial.print("Group size = ");
      break;
    case GROUP_DLY:
      Serial.print("Inter-character Delay = ");
      break; 
    case KEY_SPEED:
      Serial.print("Key speed = ");
      break;
    case CHAR_SET:
      Serial.print("Character set = ");
      break;
    case KOCH_NUM:
      Serial.print("Koch number = ");
      break;
    case KOCH_SKIP:
      if (new_val >= prefs[KOCH_NUM]) new_val = prefs[KOCH_NUM]-1;
      Serial.print("Skip = ");
      break;
    case OUT_MODE:
      Serial.print("Output mode = ");
      break;
    default:
      Serial.print("Preference index out of range\n");
      return new_val;
  }

  // Print and save new value before returning it
  Serial.println(new_val);
  prefs[indx] = new_val;
  return new_val;
}


//====================
// Button and Serial Reader
//====================
 
uint8_t readButtons(void) {
  uint8_t reply = 0; //0x1F;
 
  //Read Serial
  char serialInput;
  while (Serial.available()>0 ) {
    serialInput = Serial.read();
    // Serial.print("Got: "); Serial.println(serialInput);
    
    switch (serialInput) {
      case ' ': //Select
        reply = 0x1;
        break;
      case 'd': // Right
        reply = 0x2;
        break;
      case 's': // Down
        reply = 0x4;
        break;
      case 'w': // Up
        reply = 0x8;
        break;
      case 'a': // Left
        reply = 0x10;
        break;
    }
    //Serial.println(reply);
  }
  
  //I2C Buttons
  //for (uint8_t i=0; i<5; i++) {
//    reply &= ~((_i2c.digitalRead(_button_pins[i])) << i);
  //}
  
  return reply;
}

