/****************************************
  Arduino Morse code Trainer by Tom Lewis, N4TL.   February 21, 2016
  This sketch sends a few random Morse code characters, then waits for the student to send them back with an external keyer.
  If the same characters are sent back the sketch sends new random characters. If they are wrong the sketch sends the same characters over.
  The PS2 keyboard allows the user to change some operational parameters.
  See the CW Trainer article.docx file for more information.
  73 N4TL

  Uses code by Glen Popiel, KW5GP, found in his book, Arduino for Ham Radio, published by the ARRL.

  Modified and added LCD display by Glen Popiel - KW5GP

  Uses MORSE ENDECODER Library by raronzen
  Copyright (C) 2010, 2012 raron
  GNU GPLv3 license (http://www.gnu.org/licenses)
  Contact: raronzen@gmail.com  (not checked too often..)
  Details: http://raronoff.wordpress.com/2010/12/16/morse-endecoder/
*****************************************/

#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <Morse.h>
#include <MorseEnDecoder.h>  // Morse EnDecoder Library
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>

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

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();  // LCD class
char line_buf[17];

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
#define SAVED_FLG 0   // will be 170 if settings have been saved to EEPROM
#define GROUP_NUM 1   // expected number of cw characters to be received
#define KEY_SPEED 2   // morse keying speed (WPM)
#define CHAR_SET  3   // defines which character set to send the student.
#define KOCH_NUM  4   // how many character to use
#define KOCH_SKIP 5   // characters to skip in the Koch table
#define OUT_MODE  6   // 0 = Key, 1 = Speaker
byte prefs[7];        // Table of preference values

//=========================================
// The speed of the sent characters was measured with MRP40.
// the sent code was slow, an adjustment is necessary to speed it up.
// with an Key_speed_adj = 3, the measured output speed is
// 20 wpm measured to be 19.9 wpm
// 25 wpm measured to be 24.8 wpm
// 30 wpm measured to be 28.7 wpm
//=========================================
byte Key_speed_adj = 3; // correction for keying speed

// IO definitions
const byte morseInPin = 2;
const byte beep_pin = 11;  // Pin for CW tone
const byte key_pin = 12;   // Pin for CW Key


//====================
// Setup Function
//====================
void setup()
{
 
  // Start serial debug port
  Serial.begin(9600);
  while(!Serial);
  Serial.println("N4TL CW Trainer");

  // Start LCD
  lcd.begin(16, 2);
  lcd.setBacklight(WHITE);

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
  }  //end dispatch switch  
}  // end loop()


//====================
// Operating Mode Menu Function
//====================
byte get_mode()
{
  // Main menu strings
  const static char msg0[] PROGMEM = "N4TL CW Trainer ";
  const static char msg1[] PROGMEM = ">Start Trainer  ";
  const static char msg2[] PROGMEM = ">Start Decoder  ";
  const static char msg3[] PROGMEM = ">Set Preferences";
  const static char* const main_menu[] PROGMEM = {msg0, msg1, msg2, msg3};

  byte entry = 1;
  byte buttons = 0;
  boolean done = false;
  
  // Clear the LCD and display menu heading
  lcd.clear();
  lcd.setCursor(0,0);
  strcpy_P(line_buf, (char*)pgm_read_word(&(main_menu[0])));
  lcd.print(line_buf);
  
  do {
    lcd.setCursor(0, 1);
    strcpy_P(line_buf, (char*)pgm_read_word(&(main_menu[entry])));
    lcd.print(line_buf);
    delay(250);
    while(!(buttons = lcd.readButtons()));  // wait for button press
    if (buttons & BUTTON_UP) --entry;
    if (buttons & BUTTON_DOWN) ++entry;
    entry = constrain(entry, 1, 3);
    if (buttons & BUTTON_SELECT) {
      while(lcd.readButtons());  // wait for button release
      done = true; 
    }
  } while(!done);
  
  return entry;
}  // end get_mode()


//====================
// Set Preferences menu Function
//====================
void set_prefs()
{
  // Prefs menu strings
  const static char prf0[] PROGMEM = "Saving to EEPROM";
  const static char prf1[] PROGMEM = "Code Group Size:";
  const static char prf2[] PROGMEM = "Code Speed:     ";
  const static char prf3[] PROGMEM = "Character Set:  ";
  const static char prf4[] PROGMEM = "Koch Number:    ";
  const static char prf5[] PROGMEM = "Skip Characters:";
  const static char prf6[] PROGMEM = "Out:0=key,1=spk";
  const static char* const prefs_menu[] PROGMEM = {prf0, prf1, prf2, prf3, prf4, prf5,prf6};

  byte pref = 1;
  const byte n_prefs = sizeof(prefs)-1;
  byte p_val = 0;
  byte tmp;
  byte buttons = 0;
  boolean next = false;
  boolean done = false;

  // Clear the display
  lcd.clear();
  Serial.println("Set preferences");

  // Loop displaying preference values and let user change them
  do {
    // Display the selected preference
    lcd.setCursor(0,0);
    strcpy_P(line_buf, (char*)pgm_read_word(&(prefs_menu[pref])));
    lcd.print(line_buf);
    p_val = prefs[pref];

    do {
      // Display the pref current value and wait for button press
      lcd.setCursor(0,1);
      lcd.print(" = ");
      lcd.print(p_val);
      lcd.print("          ");
      delay(250);
      while(!(buttons = lcd.readButtons()));

      // Handle button press in priority order
      if (buttons & BUTTON_SELECT) {
        next = true;
        done = true;
      } else if (buttons & BUTTON_UP) {
        tmp = --pref;
        pref = constrain(tmp, 1, n_prefs);
        next = true;
      } else if (buttons & BUTTON_DOWN) {
        tmp = ++pref;
        pref = constrain(tmp, 1, n_prefs);
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
  lcd.clear();
  strcpy_P(line_buf, (char*)pgm_read_word(&(prefs_menu[SAVED_FLG])));
  lcd.setCursor(0,0);
  lcd.print(line_buf);
  delay(500);
  
  // Save all prefs to EEPROM before returning.
  prefs_set(SAVED_FLG, 170);  // Set perfs saved flag
  for (int i=0; i<7; i++) {
    EEPROM.write(i, prefs[i]);
  }
  
  // wait for button release
  while(lcd.readButtons());

}  // end set_prefs()


//====================
// Morse code trainer function
//====================
void morse_trainer()
{
  
  //Character sets
  const static char koch[] PROGMEM = {'K','M','R','S','U','A','P','T','L','O','W','I','.','N','J','E','F',
  '0','Y','V','.','G','5','/','Q','9','Z','H','3','8','B','?','4','2','7','C','1','D','6','X','\0'};
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
  switch (prefs[CHAR_SET]) {
    case 1:  // alpha characters
      cset = 0;
      lo = 10;
      hi = 35;
      break;
    case 2:  // numbers
      cset = 0;
      lo = 0;
      hi = 9;
      break;
    case 3:  // punctuation
      cset = 0;
      lo = 36;
      hi = 39;
      break;
    case 4:  // all alphabetic
      cset = 0;
      lo = 0;
      hi = 39;
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
    lcd.clear();
    lcd.setCursor(0, 0); // Set the cursor to top line, left

    // Send characters to trainee
    for (i = 0; i < (prefs[GROUP_NUM]); i++)
    {
      if (!error) { // if no error on last round, generate new text.
        j = random(lo, hi);
        cw_tx[i] = ch_buf[j];
      }

      lcd.print(cw_tx[i]);  // Display the sent char
      morse.send(cw_tx[i]);   // Send the character
      Serial.print(cw_tx[i]); // debug print
    }

    // Now check the trainee's sending
    Serial.print("\nTop of the check loop ");
    error = false;
    rx_cnt = 0;
    lcd.setCursor(0, 1); // Set the cursor to bottom line, left
    do {  
      morseInput.decode();  // Start decoder and check char when it comes in
      if (morseInput.available()) {
        char cw_rx = morseInput.read();
        if (cw_rx != ' ') {  // Skip spaces
          lcd.print(cw_rx);
          Serial.print(cw_rx);
          if (cw_rx != cw_tx[rx_cnt]) error = true;
          ++rx_cnt;
        }
      }
      if (buttons = lcd.readButtons()) break;
    } while (rx_cnt < prefs[GROUP_NUM] && !error);

    // Set backlignt according to trainee's performance
    if (error) {
      lcd.setBacklight(RED);
    } else {
      lcd.setBacklight(WHITE);      
    }

    delay(1000);  //Leave time for display to catch up and show the bad character.

  } while(!(buttons));

  //TODO Decode and handle buttons
  //    select = exit
  //    up/dn = chg code speed (sets error so same string repeats)
  //    left/right = chg group size

  while(lcd.readButtons());  // wait for button release

}  // end morse_trainer()


//=====================================
// CW decoder only, use this section to check your keyers output to this decoder.
//=====================================
void morse_decode()
{
  char cw_rx;
  byte button;
  byte ch_cnt = 0;

  morseDecoder morseInput = morseDecoder(morseInPin, MORSE_KEYER, MORSE_ACTIVE_LOW);

  Serial.println("Morse decoder started");
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.leftToRight();

  do {
    morseInput.decode();  // Decode incoming CW
    if (morseInput.available()) {  // If there is a character available
      cw_rx = morseInput.read();  // Read the CW character
      if (ch_cnt == 16) {
        lcd.setCursor(0,1);
        lcd.print("                ");
        lcd.setCursor(0,1);
        Serial.print('\n');
        ch_cnt = 0;
      }
      Serial.print(cw_rx); // send character to the debug serial monitor
      lcd.print(cw_rx);  // Display the CW character
      ++ch_cnt;
    }
  } while (!(button = lcd.readButtons()));

  while (lcd.readButtons());
}  // end of morse_decode()
/*

  while (true)  // decode cw, if D is entered the sketch stays in the short loop.
  {
    if (firstpass2 == 0)
    {
      lcd.clear();
      lcd.setCursor(0, 1);
      firstpass2++;
      lcd.print("CW Decoder");
      lcd.setCursor(0, 0);
    }
    morseInput.decode();  // Decode incoming CW
    if (morseInput.available())  // If there is a character available
    {
      cw_rx = morseInput.read();  // Read the CW character
      Serial.print(cw_rx); // send character to the debug serial monitor
      lcd.print(cw_rx);  // Display the CW character
      count++;
      if (count > 31) count = 0;
      if (count == 16) lcd.setCursor(0, 1);
      if (count == 0) lcd.setCursor(0, 0);
    }
  }  // end of while
*/




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
    prefs_set(SAVED_FLG, 170);
    for (int idx = 1; idx < 7; idx++)
    {
      prefs_set(idx,EEPROM.read(idx));
    }
  }
  else
  {
    prefs_set(SAVED_FLG, 0);  // Prefs not saved
    prefs_set(GROUP_NUM, 1);  // Send/receive groups of 1 char to start
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
byte prefs_set(unsigned int indx, byte val)
{
  const byte lo_lim[] {0, 1, 20, 1, 1, 0, 0};  // Table of lower limits of preference values
  const byte hi_lim[] {170, 15, 30, 6, 40, 39, 1};  // Table of uppper limits of preference values
  byte new_val;

  // Set new value
  new_val = constrain(val, lo_lim[indx], hi_lim[indx]);

  // Dispatch on preference index to do debug print
  switch (indx) {
    case SAVED_FLG:
      Serial.print("Saved flag = ");
      break;
    case GROUP_NUM:
      Serial.print("Group size = ");
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
  }

  // Print and save new value before returning it
  Serial.println(new_val);
  prefs[indx] = new_val;
  return new_val;
}

