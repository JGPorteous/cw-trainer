/*          MORSE ENDECODER
 
 - Morse encoder / decoder classes for the Arduino.

 Copyright (C) 2010-2012 raron

 GNU GPLv3 license:

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 

 Contact: raronzen@gmail.com

 
 History:
 2012.01.31 - raron - Tiny update for Arduino 1.0 as well as retaining
                        pre-Arduino 1.0 compatibility. Fixed header comments.
 2010.12.06 - raron - Cleaned up code a bit. Added the "MN digraph" ---. for exclamation mark(!) for decoding.
                Still encod as the "KW digraph" -.-.-- though
 2010.12.04 - raron - Encode morse class implemented
 2010.12.03 - raron - Conversion to class begun, decode morse class implemented
 2010.12.02 - raron - Changed back to signed timers to avoid overflow (if left running for a long time)
 2010.11.30 - raron - Morse punctuation added (except $ - the dollar sign)
 2010.11.29 - raron - Added echo on/off command
 2010.11.28 - raron - Added a simple audio Morse signal filter. Added a simple command parser
 2010.11.27 - raron - Added Morse encoding via reverse-dichotomic path tracing.
                Thus using the same Morse table / binary tree for encoding and decoding.
 2010.11.11 - raron - Rewrite for the Arduino
 1992.01.06 - raron - "Morse decoder 3.5", a 68000 Assembler version for the Amiga 600
                using a binary tree (dichotomic table) for Morse decoding
*/ 


#include "MorseEnDecoder.h"



// character index position of the binary morse tree top.
//const int morseTreetop = 31;   // This is for ITU international Morse Code (no punctuation)
const int morseTreetop = 63;   // This is for ITU with puncutation, but without non-english extensions

const int morseTableLength = (morseTreetop*2)+1;
const int morseTreeLevels = log(morseTreetop+1)/log(2);


// Morse code binary tree table (or, dichotomic search table)
// TODO: have this table in PROGMEM!

// ITU - International Morse code table only
//char morseTable[] = "5H4S?V3I?F?U??2E?L?R???A?P?W?J1 6B?D?X?N?C?K?Y?T7Z?G?Q?M8??O9?0";

// This is the table for ITU with punctuation (but without non-english characters - for now)
char morseTable[] = "*5*H*4*S***V*3*I***F***U?*_**2*E***L\"**R*+.****A***P@**W***J'1* *6-B*=*D*/"
                    "*X***N***C;*!K*()Y***T*7*Z**,G***Q***M:8*!***O*9***0*";



morseDecoder::morseDecoder(int decodePin, boolean listenAudio, boolean morsePullup)
{
  morseInPin = decodePin;
  morseAudio = listenAudio;
  activeLow = morsePullup;

  if (morseAudio == false)
  {
    pinMode(morseInPin, INPUT);
    if (activeLow) digitalWrite (morseInPin, HIGH);
  }

  // Some initial values  
  wpm = 13;
  AudioThreshold = 700;
  debounceDelay = 20;
  dotTime = 1200 / wpm;       // morse dot time length in ms
  dashTime = 3 * 1200 / wpm;
  wordSpace = 7 * 1200 / wpm;

  morseTableJumper = (morseTreetop+1)/2;
  morseTablePointer = morseTreetop;
 
  morseKeyer = LOW;
  morseSignalState = LOW;
  lastKeyerState = LOW;

  gotLastSig = true;
  morseSpace = true;
  decodedMorseChar = '\0';
  
  lastDebounceTime = 0;
  markTime = 0;
  spaceTime = 0;
}



void morseDecoder::setspeed(int value)
{
  wpm = value;
  if (wpm <= 0) wpm = 1;
  dotTime = 1200 / wpm;
  dashTime = 3 * 1200 / wpm;
  wordSpace = 7 * 1200 / wpm;
}



boolean morseDecoder::available()
{
  if (decodedMorseChar) return true; else return false;
}



char morseDecoder::read()
{
  char temp = decodedMorseChar;
  decodedMorseChar = '\0';
  return temp;
}





morseEncoder::morseEncoder(int encodePin)
{
  morseOutPin = encodePin;

  // some initial values
  sendingMorse = false;
  encodeMorseChar = '\0';

  wpm = 13;
  dotTime = 1200 / wpm;       // morse dot time length in ms
  dashTime = 3 * 1200 / wpm;
  wordSpace = 7 * 1200 / wpm;
 
}



void morseEncoder::setspeed(int value)
{
  wpm = value;
  if (wpm <= 0) wpm = 1;
  dotTime = 1200 / wpm;
  dashTime = 3 * 1200 / wpm;
  wordSpace = 7 * 1200 / wpm;
}



boolean morseEncoder::available()
{
  if (sendingMorse) return false; else return true;
}



void morseEncoder::write(char temp)
{
  if (!sendingMorse && temp != '*') encodeMorseChar = temp;
}



 

void morseDecoder::decode()
{
  currentTime = millis();
  
  // Read Morse signals
  if (morseAudio == false)
  {
    // Read the Morse keyer (digital)
    morseKeyer = digitalRead(morseInPin);
    if (activeLow) morseKeyer = !morseKeyer;

    // If the switch changed, due to noise or pressing:
    if (morseKeyer != lastKeyerState) lastDebounceTime = currentTime; // reset timer
  
    // debounce the morse keyer
    if ((currentTime - lastDebounceTime) > debounceDelay)
    {
      // whatever the reading is at, it's been there for longer
      // than the debounce delay, so take it as the actual current state:
      morseSignalState = morseKeyer;
      
      // differentiante mark and space times
      if (morseSignalState) markTime = lastDebounceTime; 
      else spaceTime = lastDebounceTime;
    }
  } else {
    // Read Morse audio signal
    audioSignal = analogRead(morseInPin);
    if (audioSignal > AudioThreshold)
    {
      // If this is a new morse signal, reset morse signal timer
      if (currentTime - lastDebounceTime > dotTime/2)
      {
        markTime = currentTime;
        morseSignalState = true; // there is currently a Morse signal
      }
      lastDebounceTime = currentTime;
    } else {
      // if this is a new pause, reset space time
      if (currentTime - lastDebounceTime > dotTime/2 && morseSignalState == true)
      {
        spaceTime = lastDebounceTime; // not too far off from last received audio
        morseSignalState = false;     // No more signal
      }
    }
  }
  


  // Decode morse code
  if (!morseSignalState)
  {
    if (!gotLastSig)
    {
      if (morseTableJumper > 0)
      {
        // if pause for more than half a dot, get what kind of signal pulse (dot/dash) received last
        if (currentTime - spaceTime > dotTime/2)
        {
          // if signal for more than 1/4 dotTime, take it as a morse pulse
          if (spaceTime-markTime > dotTime/4)
          {
            // if signal for less than half a dash, take it as a dot
            if (spaceTime-markTime < dashTime/2)
            {
               morseTablePointer -= morseTableJumper;
               morseTableJumper /= 2;
               gotLastSig = true;
            }
            // else if signal for between half a dash and a dash + one dot (1.33 dashes), take as a dash
            else if (spaceTime-markTime < dashTime + dotTime)
            {
              morseTablePointer += morseTableJumper;
              morseTableJumper /= 2;
              gotLastSig = true;
            }
          }
        }
      } else { // error if too many pulses in one morse character
        //Serial.println("<ERROR: unrecognized signal!>");
        decodedMorseChar = '#'; // error mark
        gotLastSig = true;
        morseTableJumper = (morseTreetop+1)/2;
        morseTablePointer = morseTreetop;
      }
    }
    // Write out the character if pause is longer than 2/3 dash time (2 dots) and a character received
    if ((currentTime-spaceTime >= (dotTime*2)) && (morseTableJumper < ((morseTreetop+1)/2)))
    {
      decodedMorseChar = morseTable[morseTablePointer];
      morseTableJumper = (morseTreetop+1)/2;
      morseTablePointer = morseTreetop;
    }
    // Write a space if pause is longer than 2/3rd wordspace
    if (currentTime-spaceTime > (wordSpace*2/3) && morseSpace == false)
    {
      //Serial.print(" ");
      decodedMorseChar = ' ';
      morseSpace = true ; // space written-flag
    }

  } else {
    // while there is a signal, reset some flags
    gotLastSig = false;
    morseSpace = false;
  }
  
  // Save the morse keyer state for next round
  lastKeyerState = morseKeyer;
}







void morseEncoder::encode()
{
  currentTime = millis();

  if (!sendingMorse && encodeMorseChar)
  {
    // change to capital letter if not
    if (encodeMorseChar > 'Z') encodeMorseChar -= 'z'-'Z';
  
    // Scan for the character to send in the Morse table
    int i;
    for (i=0; i<morseTableLength; i++) if (morseTable[i] == encodeMorseChar) break;
    int morseTablePos = i+1;  // 1-based position
  
    // Reverse dichotomic / binary tree path tracing
  
    // Find out what level in the binary tree the character is
    int test;
    for (i=0; i<morseTreeLevels; i++)
    {
      test = (morseTablePos + (0x0001 << i)) % (0x0002 << i);
      if (test == 0) break;
    }
    int startLevel = i;
    morseSignals = morseTreeLevels - i; // = the number of dots and/or dashes
    morseSignalPos = 0;
  
    // Travel the reverse path to the top of the morse table
    if (morseSignals > 0)
    {
      // build the morse signal (backwards from last signal to first)
      for (i = startLevel; i<morseTreeLevels; i++)
      {
        int add = (0x0001 << i);
        test = (morseTablePos + add) / (0x0002 << i);
        if (test & 0x0001 == 1)
        {
          morseTablePos += add;
          // Add a dot to the temporary morse signal string
          morseSignalString[morseSignals-1 - morseSignalPos++] = '.';
        } else {
          morseTablePos -= add;
          // Add a dash to the temporary morse signal string
          morseSignalString[morseSignals-1 - morseSignalPos++] = '-';
        }
      }
    } else {  // unless it was on the top to begin with (A space character)
      morseSignalString[0] = ' ';
      morseSignalPos = 1;
      morseSignals = 1; // cheating a little; a wordspace for a "morse signal"
    }
    morseSignalString[morseSignalPos] = '\0';
  
  /*
    if (morseTablePos-1 != morseTreetop)
    {
      Serial.println();
      Serial.print("..Hm..error? MorseTablePos = ");
      Serial.println(morseTablePos); 
    }
  */
  
    // start sending the the character
    sendingMorse = true;
    sendingMorseSignalNr = 0;
    sendMorseTimer = currentTime;
    if (morseSignalString[0] != ' ') digitalWrite(morseOutPin, HIGH);
  }


  // Send Morse signals to output
  if (sendingMorse)
  {
    switch (morseSignalString[sendingMorseSignalNr])
    {
      case '.': // Send a dot (actually, stop sending a signal after a "dot time")
        if (currentTime - sendMorseTimer >= dotTime)
        {
          digitalWrite(morseOutPin, LOW);
          sendMorseTimer = currentTime;
          morseSignalString[sendingMorseSignalNr] = 'x'; // Mark the signal as sent
        }
        break;
      case '-': // Send a dash (same here, stop sending after a dash worth of time)
        if (currentTime - sendMorseTimer >= dashTime)
        {
          digitalWrite(morseOutPin, LOW);
          sendMorseTimer = currentTime;
          morseSignalString[sendingMorseSignalNr] = 'x'; // Mark the signal as sent
        }
        break;
      case 'x': // To make sure there is a pause between signals and letters
        if (sendingMorseSignalNr < morseSignals-1)
        {
          // Pause between signals in the same letter
          if (currentTime - sendMorseTimer >= dotTime)
          {
            sendingMorseSignalNr++;
            digitalWrite(morseOutPin, HIGH); // Start sending the next signal
            sendMorseTimer = currentTime;       // reset the timer
          }
        } else {
          // Pause between letters
          if (currentTime - sendMorseTimer >= dashTime)
          {
            sendingMorseSignalNr++;
            sendMorseTimer = currentTime;       // reset the timer
          }
        }
        break;
      case ' ': // Pause between words (minus pause between letters - already sent)
      default:  // Just in case its something else
        if (currentTime - sendMorseTimer > wordSpace - dashTime) sendingMorseSignalNr++;
    }
    if (sendingMorseSignalNr >= morseSignals)
    {
      // Ready to encode more letters
      sendingMorse = false;
      encodeMorseChar = '\0';
    }
  }
}














