/*
  IBM Wheelwriter hack
  Pin 0: connected through MOSFET to Wheelwriter bus
         Will pull down the bus when set to zero
  Pin 2: ALSO connected to the bus, but listens instead of 
         sending data
 */

#include<QueueArray.h>
//#include<PinChangeInt.h>

static int OUTPUT_PIN = 4; // GPIO pin 4, D2 on WEMOS, bit 4 to toggle (0b00010000)
static int INPUT_PIN = 0;  // GPIO pin 0, D3 on WEMOS, bit 0 to toggle (0b00000001)
static int BUTTON_PIN = 2; // GPIO pin 2, D4 on WEMOS

#define LETTER_DELAY 170
#define CARRIAGE_WAIT_BASE 300
#define CARRIAGE_WAIT_MULTIPLIER 15

#define PULL_BUS_LOW() (WRITE_PERI_REG(PERIPHS_GPIO_BASEADDR,READ_PERI_REG(PERIPHS_GPIO_BASEADDR) |= 0b00010000))
#define LEAVE_BUS_HIGH() (WRITE_PERI_REG(PERIPHS_GPIO_BASEADDR,READ_PERI_REG(PERIPHS_GPIO_BASEADDR) &= 0b11101111))

QueueArray<int> q; // holds the bytes we will send to the bus

int asciiTrans[128] = 
//col: 0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f     row:
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0

//    
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 1
     
//    sp     !     "     #     $     %     &     '     (     )     *     +     ,     -     .     /
     0x00, 0x49, 0x4b, 0x38, 0x37, 0x39, 0x3f, 0x4c, 0x23, 0x16, 0x36, 0x3b, 0xc, 0x0e, 0x57, 0x28, // 2
     
//     0     1     2     3     4     5     6     7     8     9     :     ;     <     =     >     ?
     0x30, 0x2e, 0x2f, 0x2c, 0x32, 0x31, 0x33, 0x35, 0x34, 0x2a ,0x4e, 0x50, 0x00, 0x4d, 0x00, 0x4a, // 3

//     @     A     B     C     D     E     F     G     H     I     J     K     L     M     N     O
     0x3d, 0x20, 0x12, 0x1b, 0x1d, 0x1e, 0x11, 0x0f, 0x14, 0x1F, 0x21, 0x2b, 0x18, 0x24, 0x1a, 0x22, // 4

//     P     Q     R     S     T     U     V     W     X     Y     Z     [     \     ]     ^     _
     0x15, 0x3e, 0x17, 0x19, 0x1c, 0x10, 0x0d, 0x29, 0x2d, 0x26, 0x13, 0x41, 0x00, 0x40, 0x00, 0x4f, // 5
     
//     `     a     b     c     d     e     f     g     h     i     j     k     l     m     n     o
     0x00, 0x01, 0x59, 0x05, 0x07, 0x60, 0x0a, 0x5a, 0x08, 0x5d, 0x56, 0x0b, 0x09, 0x04, 0x02, 0x5f, // 6
     
//     p     q     r     s     t     u     v     w     x     y     z     {     |     }     ~    DEL
     0x5c, 0x52, 0x03, 0x06, 0x5e, 0x5b, 0x53, 0x55, 0x51, 0x58, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00}; // 7
#define pulse_width() { for (int i=0;i<77;i++) { __asm__ __volatile__("nop\n\t"); } }
/*void inline pulse_width() {
  for (int i=0;i<77;i++) { // roughly 16 ticks per microsecond
    __asm__ __volatile__("nop\n\t");
    //use the loop as a timer
  }
}*/

void setup() 
{
  // initialize serial communication at 57600 bits per second:
  Serial.begin(115200);

  Serial.setTimeout(25);
  
  // Digital pins
  pinMode(OUTPUT_PIN, OUTPUT); // pin to send bits
  pinMode(INPUT_PIN, INPUT); // listening pin

  // start the input pin off (meaning the bus is high, normal state)
  LEAVE_BUS_HIGH();
  //PORTD &= 0b11101111;
  pinMode(BUTTON_PIN, INPUT_PULLUP); // for reading the button for testing
}

// the loop routine runs over and over again forever:
void loop() 
{
      static int charCount = 0;
      char buffer[200];
      size_t readLength = 200;
      uint8_t length = 0;  
      
      // read as much as is available
      length = Serial.readBytes( buffer, length-1 );
    
      // null-terminate the data so it acts like a string
      buffer[length] = 0;
    
      // if we have data, so do something with it
      // if we get more than one character, assume fast text
      if (length > 1) {
         Serial.println(length);
         fastText(buffer);
         charCount+=length;
         //Serial.println(length); // return the number of characters printed
         //Bean.setLed(255, 0, 0);
         //Bean.sleep(50);
         //Bean.setLed(0,0,0); 
      }
      else if ( length == 1 )
      {
          // print each character
          for (int i=0; i < length; i++) {
              if (buffer[i] == '\r' or buffer[i] == '\n') {
                  send_return(charCount);
                  charCount = 0; 
              }
              else if (buffer[i] == 0 or buffer[i] == 1 or
                       buffer[i] == 4 or buffer[i] == 21) {
                        // paper-up/down micro-down/micro-up
                        // ctrl-d is 4, ctrl-u is 21
                  paper_vert(buffer[i]);
              }
              else if (buffer[i] == 2 or buffer[i] == 0x7f) { 
                  // left arrow or delete
                  if (charCount > 0) {
                      backspace_no_correct();
                      charCount--;
                  }
              }
              else if (buffer[i] == 6) { // micro-backspace
                  // DOES NOT UPDATE CHARCOUNT!
                  // THIS WILL CAUSE PROBLEMS WITH RETURN!
                  // TODO: FIX THIS ISSUE
                  micro_backspace();
              }
              else {
                  send_letter(asciiTrans[buffer[i]]);
                  charCount++;
              }
          }
          Serial.println(length); // return the number of characters printed
          //Bean.setLed(255, 0, 0);
          //Bean.sleep(50);
          //Bean.setLed(0,0,0); 
      }
      int digital1 = digitalRead(BUTTON_PIN);
      if (digital1 == 0) {
        //fastText("this is really fast");
        print_str("a");
        //send_letter(0b000000001); // 'a'
        //send_letter(0b001011001); // 'b'
        //send_letter(0b000000100); // 'm'
        //send_letter();

        //print_str("aaaaaaaaaaaa");
        
        /*print_str("This is the symphony that schubert never finished!");
        send_return(50);

        print_str("\"the quick brown fox jumps over the lazy dog.\"");
        send_return(46);*/

        /*print_str("ABC");
        send_return(3);

        print_str("55555");
        send_return(5);

        print_str("1234567890");
        send_return(10);
        
        print_str("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        send_return(26);

        print_str("1234567890");
        send_return(10);

        print_str(",./?");
        send_return(4);*/

        //print_str("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
        //send_return(52);

        //print_str("done.");
        //send_return(5);

        //print_strln("The quick brown fox jumps over the lazy dog.");
        /*
        sendByteOnPin(0b00000000);
        delayMicroseconds(60);*/
        /////////////
        //LEAVE_BUS_HIGH();
        //Bean.setLed(255, 0, 0);
        //Bean.sleep(10);
        //Bean.setLed(0,0,0); 
      }
      
      //Bean.sleep(10);  
}

void print_str(char *s) {
  while (*s != '\0') {
    send_letter(asciiTrans[*s++]);
  }
}

void print_strln(char *s) {
  // prints a line and then a carriage return
  print_str(s);
  send_return(strlen(s));
}

inline void sendByteOnPin(int command) {
    // This will actually send 10 bytes,
    // starting with a zero (always)
    // and then the next nine bytes
    
    // when the pin is high (on), the bus is pulled low
    noInterrupts(); // turn off interrupts so we don't get disturbed
    // unrolled for consistency, will send nine bits
    // with a zero initial bit
    // pull low for 5.75us (by turning on the pin)
    PULL_BUS_LOW();
    pulse_width(); // fudge factor for 5.75

    // send low order bit (LSB endian)
    int nextBit = (command & 0b000000001);
    if (nextBit == 0) {
      // turn on
      PULL_BUS_LOW();
      pulse_width(); 
    } else if (nextBit == 1) {
      // turn off
      LEAVE_BUS_HIGH();
      pulse_width(); // special shortened...
    }
    
    // next bit (000000010)
    nextBit = (command & 0b000000010) >> 1;
    if (nextBit == 0) {
      // turn on
      PULL_BUS_LOW();
      pulse_width();
    } else if (nextBit == 1) {
      // turn off
      LEAVE_BUS_HIGH();
      pulse_width();
    }
    
    // next bit (000000100)
    nextBit = (command & 0b000000100) >> 2;
    if (nextBit == 0) {
      // turn on
      PULL_BUS_LOW();
      pulse_width();
    } else if (nextBit == 1) {
      // turn off
      LEAVE_BUS_HIGH();
      pulse_width();
    }
    
    // next bit (000001000)
    nextBit = (command & 0b000001000) >> 3;
    if (nextBit == 0) {
      // turn on
      PULL_BUS_LOW();
      pulse_width();
    } else if (nextBit == 1) {
      // turn off
      LEAVE_BUS_HIGH();
      pulse_width();
    }

    // next bit (000010000)
    nextBit = (command & 0b000010000) >> 4;
    if (nextBit == 0) {
      // turn on
      PULL_BUS_LOW();
      pulse_width();
    } else if (nextBit == 1) {
      // turn off
      LEAVE_BUS_HIGH();
      pulse_width();
    }


    // next bit (000100000)
    nextBit = (command & 0b000100000) >> 5;
    if (nextBit == 0) {
      // turn on
      PULL_BUS_LOW();
      pulse_width();
    } else if (nextBit == 1){
      // turn off
      LEAVE_BUS_HIGH();
      pulse_width();
    }
    
    // next bit (001000000)
    nextBit = (command & 0b001000000) >> 6;
    if (nextBit == 0) {
      // turn on
      PULL_BUS_LOW();
      pulse_width();
    } else if (nextBit == 1) {
      // turn off
      LEAVE_BUS_HIGH();
      pulse_width();
    }

    // next to last bit (010000000)
    nextBit = (command & 0b010000000) >> 7;
    if (nextBit == 0) {
      // turn on
      PULL_BUS_LOW();
      pulse_width();
    } else if (nextBit == 1){
      // turn off
      LEAVE_BUS_HIGH();
      pulse_width();
    }

    // final bit (100000000)
    nextBit = (command & 0b100000000) >> 8;
    if (nextBit == 0) {
      // turn on
      PULL_BUS_LOW();
      pulse_width(); // last one is special :)
    } else if (nextBit == 1){
      // turn off
      LEAVE_BUS_HIGH();
      pulse_width();
    }


    // make sure we aren't still pulling down
    LEAVE_BUS_HIGH();

    // re-enable interrupts
    interrupts();

}

void send_letter(int letter) {
    q.enqueue(0b100100001);
    q.enqueue(0b000001011);
    q.enqueue(0b100100001);
    q.enqueue(0b000000011);
    q.enqueue(letter);
    q.enqueue(0b000001010);
    sendBytes();

    delay(LETTER_DELAY); // before next character
}

void paper_vert(int direction) {
  // 0 == up
  // 1 == down
  // 4 == micro-down
  // 21 == micro-up
  q.enqueue(0b100100001);
  q.enqueue(0b000001011);
  q.enqueue(0b100100001);
  q.enqueue(0b000000101);
  if (direction == 0) { // cursor-up (paper-down)
      q.enqueue(0b000001000);
  } else if (direction == 1) {
      q.enqueue(0b010001000); // cursor-down (paper-up)
  } else if (direction == 4) {
      q.enqueue(0b010000010); // cursor-micro-down (paper-micro-up)
  } else if (direction == 21) {
      q.enqueue(0b000000010); // cursor-micro-up (paper-micro-down)
  }
  q.enqueue(0b100100001);
  q.enqueue(0b000001011);
  sendBytes();
  delay(LETTER_DELAY * 2); // give it a bit more time
}

void backspace_no_correct() {
    q.enqueue(0b100100001);
    q.enqueue(0b000001011);
    q.enqueue(0b100100001);
    q.enqueue(0b000001101);
    q.enqueue(0b000000100);
    q.enqueue(0b100100001);
    q.enqueue(0b000000110);
    q.enqueue(0b000000000);
    q.enqueue(0b000001010);
    q.enqueue(0b100100001);
    sendBytes();
  
    // send one more byte but don't wait explicitly for the response
    // of 0b000000100
    sendByteOnPin(0b000001011);
    delay(LETTER_DELAY * 2); // a bit more time
}

void send_return(int numChars) {
    // calculations for further down
    int byte1 = (numChars * 5) >> 7;
    int byte2 = ((numChars * 5) & 0x7f) << 1;
    
    q.enqueue(0b100100001);
    q.enqueue(0b000001011);
    q.enqueue(0b100100001);
    q.enqueue(0b000001101);
    q.enqueue(0b000000111);
    q.enqueue(0b100100001);
    
    if (numChars <= 23 || numChars >= 26) {
        q.enqueue(0b000000110);

        // We will send two bytes from a 10-bit number
        // which is numChars * 5. The top three bits
        // of the 10-bit number comprise the first byte,
        // and the remaining 7 bits comprise the second
        // byte, although the byte needs to be shifted
        // left by one (not sure why)
        // the numbers are calculated above for timing reasons
        q.enqueue(byte1);
        q.enqueue(byte2); // each char is worth 10
        q.enqueue(0b100100001);
        // right now, the platten is moving, maybe?

    } else if (numChars <= 25) {
        // not sure why this is so different
        q.enqueue(0b000001101);
        q.enqueue(0b000000111);
        q.enqueue(0b100100001);
        q.enqueue(0b000000110);
        q.enqueue(0b000000000);
        q.enqueue(numChars * 10);
        q.enqueue(0b100100001);
        // right now, the platten is moving, maybe?
    }
    
    q.enqueue(0b000000101);
    q.enqueue(0b010010000); //
    q.enqueue(0b100100001);
    sendBytes();

    // send one more byte but don't wait explicitly for the response
    // of 0b001010000
    sendByteOnPin(0b000001011);

    // wait for carriage 
    delay(CARRIAGE_WAIT_BASE + CARRIAGE_WAIT_MULTIPLIER * numChars);
}

void correct_letter(int letter) {
    q.enqueue(0b100100001);
    q.enqueue(0b000001011);
    q.enqueue(0b100100001);
    q.enqueue(0b000001101);
    q.enqueue(0b000000101);
    q.enqueue(0b100100001);
    q.enqueue(0b000000110);
    q.enqueue(0b000000000);
    q.enqueue(0b000001010);
    q.enqueue(0b100100001);
    q.enqueue(0b000000100);
    q.enqueue(letter);
    q.enqueue(0b000001010);
    q.enqueue(0b100100001);
    q.enqueue(0b000001100);
    q.enqueue(0b010010000);
}

void micro_backspace() {
    q.enqueue(0b100100001);
    q.enqueue(0b000001110);
    q.enqueue(0b011010000);
    q.enqueue(0b100100001);
    q.enqueue(0b000001011);
    q.enqueue(0b100100001);
    q.enqueue(0b000001101);
    q.enqueue(0b000000100);
    q.enqueue(0b100100001);
    q.enqueue(0b000000110);
    q.enqueue(0b000000000);
    q.enqueue(0b000000010);
    q.enqueue(0b100100001);
    sendBytes();

    // send one more byte but don't wait explicitly for the response
    // of 0b000000100
    sendByteOnPin(0b000001011);
}

void sendBytes() {
    while (!q.isEmpty()) {
        //Serial.println("sending bytes!");
        sendByteOnPin(q.dequeue());
        // wait for low then high (for a zero)
        //PORTD |= 0b01000000;
        //int pinStatus = ((PIND & 0b01000000) >> 6);
        //Serial.println(pinStatus);
        
        while ((READ_PERI_REG(PERIPHS_GPIO_BASEADDR) & 0b00000001) == 1) {
          // busy
          Serial.println("busy");
        }
        while ((READ_PERI_REG(PERIPHS_GPIO_BASEADDR) & 0b00000001) == 0) {
          Serial.println("still busy");
          // busy
        }
        //delayMicroseconds(60);
        pulse_width(); // wait a bit before sending next char
    }
}
void fastText(char *s) {
    q.enqueue(0b100100001);
    q.enqueue(0b000001011);
    q.enqueue(0b100100001);
    q.enqueue(0b000001001);
    q.enqueue(0b000000000);
    q.enqueue(0b100100001);
    q.enqueue(0b000001010);
    q.enqueue(0b000000000);
    q.enqueue(0b100100001);
    q.enqueue(0b000001101);
    q.enqueue(0b000000110);
    q.enqueue(0b100100001);
    q.enqueue(0b000000110);
    q.enqueue(0b010000000);
    q.enqueue(0b000000000);
    q.enqueue(0b100100001);
    q.enqueue(0b000000101);
    q.enqueue(0b010000000);
    q.enqueue(0b100100001);
    q.enqueue(0b000001101);
    q.enqueue(0b000010010);
    q.enqueue(0b100100001);
    q.enqueue(0b000000110);
    q.enqueue(0b010000000);
    // if odd, send 0:
    if (strlen(s) % 2 == 1 or strlen(s) < 26) {
        q.enqueue(0b000000000);
    } else {
        q.enqueue(0b000000101);
    }
    
    // letters start here
    while (*s != '\0') {
        q.enqueue(0b100100001);
        q.enqueue(0b000000011);
    
        q.enqueue(asciiTrans[*s++]);
        
        q.enqueue(0b000001010);
    }

    q.enqueue(0b100100001);
    q.enqueue(0b000001001);
    q.enqueue(0b000000000);
    /*
    q.enqueue(0b100100001);
    q.enqueue(0b000001100);
    q.enqueue(0b001000000);
    q.enqueue(0b100100001);
    q.enqueue(0b000001101);
    q.enqueue(0b000000111);
    q.enqueue(0b100100001);
    q.enqueue(0b000000110);
    q.enqueue(0b000000001);
    q.enqueue(0b011110100);
    q.enqueue(0b100100001);
    q.enqueue(0b000000101);
    q.enqueue(0b010010000);*/
    Serial.println("About to send bytes.");
    sendBytes();
    delay(LETTER_DELAY * 2); // a bit more time
}



