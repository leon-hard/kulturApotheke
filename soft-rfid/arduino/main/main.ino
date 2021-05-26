/* License: GPLv3
 * RFID Decoding from http://www.mikrocontroller.net/articles/RFID_Türmodul , rfid.cpp by S. Seegel
 * Author: Lukas K. <lu@0x83.eu> 
 * Antenna Drive: PD5
 * Demod: PA0
 * Tag detect: PB2
 * TXD: PD1
*/
#define F_CPU 16000000
#include <util/delay.h> 
#include <avr/io.h>
#include "uart.h"
#include <avr/interrupt.h>
#include <stdlib.h>
unsigned volatile char rawtag[8];
unsigned char tag[5];
unsigned volatile char bytecnt=0;
unsigned volatile char bitcount=0;;
unsigned char insync = 0;
unsigned char tag_flag;
unsigned int last[7];
unsigned int sync = 0;
unsigned int synctimeout = 0;
unsigned int val;
unsigned char mode = 2;
unsigned char s,t;
#define SYNCTIMEOUT 500
#define THRES 500

void InjectByte(uint8_t b)
{
  if (bytecnt < 8)
  {
    rawtag[bytecnt++] = b;
    
    if (bytecnt == 8)
    {
      ADCSRA &= ~(1<<ADATE);
      bytecnt = 0;
    }
  }
}

ISR(ADC_vect) {

  //uart_putc('i');

  //_delay_ms(1000);
  
  //val = (last[0]+last[1]+last[2]+last[3]+last[4]+last[5]+last[6]+ADCW)/8;
  val = (last[0]+last[1]+last[2]+ADCW)/4;
  if(val > THRES) {
    s |= (1<<0);
    
  }
  else {
    s &=~(1<<0);
    
  }
  if(((s&3) == 0) || ((s&3) == 3)) { //no edge
    t++;
  }
  else { //any edge
    if(t>5) {
      sync <<= 1;
      bitcount++;
      t=0;
      if((s&3) == mode) {
        sync |= 1;
        PORTB |= (1<<PB0);
      }
      /*if((s&3) == 2) {
        PORTB &= ~(1<<PB0);
      }*/
      if (insync)
      {
        if ((bitcount % 8) == 0){
          InjectByte(sync);
        }
        if (bitcount == 64) {
          insync = 0;
          PORTB &= ~(1<<PB1);
        }
      }
      else
        if ((sync & 0x3FF) == 0x1FF)
        {
          insync = 1;
          PORTB |= (1<<PB1);
          bitcount = 9;
          bytecnt=0;
          InjectByte(sync);
          synctimeout = 0;
        }
        else {
          synctimeout++;
        }
      
      if(synctimeout > SYNCTIMEOUT) {
        mode^=3;
        synctimeout=0;
      }
    }
  }
  

  last[2] = last[1];
  last[1] = last[0];
  last[0] = ADCW;
  
  s<<=1;
}
  
void adcinit(void) {
  //ADMUX = (1<<REFS0)|(1<<REFS1); //2,56V Reference
  
  ADMUX = (1<<REFS0); //5V Reference
  ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) |(1<<ADIE)|(1<<ADATE); //Auto trigger, free running ~20kSps @ 16MHz XTAL
}

void start_adc(void) {
  ADCSRA |= (1<<ADATE)|(1<<ADSC);
}

void Execute(void) {
  uint8_t ibit, obit, i, lp, p;
  tag_flag=0;
  //calc line parities
  i=0;
  p=0;
  lp=0;
  for (ibit=9; ibit<59; ibit++) {
    lp ^= (rawtag[ibit / 8] >> (7 - (ibit % 8))) & 0x01;

    i++;
    if (i==5) {
      i=0;
      p |= lp;
    }
  }

  //calc line parities
  for (i=0; i<4; i++) {//Check 4 columns
    lp = 0;
    for (ibit=9+i; ibit<9+55+i; ibit+=5)
      lp ^= (rawtag[ibit / 8] >> (7 - (ibit % 8))) & 0x01;
    p |= lp;
  }

  //all parity bits ok ?
  if (!p) {
    tag[0] = 0; tag[1] = 0; tag[2] = 0; tag[3] = 0; tag[4] = 0;
    //Remove parity bits
    ibit = 9;
    for (obit=0; obit<40; obit++) {
      if ((rawtag[ibit / 8] >> (7 - (ibit % 8))) & 0x01)
        tag[obit / 8] |= 1 << (7 - (obit % 8));

      ibit++;
      if (((obit + 1) % 4) == 0)
        ibit++;
    }

    tag_flag = 1;
    PORTB |= (1<<PB2);
  }
  else {
    PORTB &= ~(1<<PB2);
  }
}

void setup(void) {
  uart_init();

  
  DDRD |= (1<<PD5);
  DDRB |= (1<<PB0) | (1<<PB1) | (1<<PB2) | (1<<PB3);
  TCCR1A |= (1<<COM1A0);
  TCCR1B |= (1<<WGM12) | (1<<CS10);
  OCR1A = 56;
  adcinit();
  _delay_ms(1000);
  sei();
  //start_adc();
  
  //while (ADCSRA &(1<<ADATE));
  

}

void loop() {

  //uart_putc('c');

  //_delay_ms(1000);

  
  
  unsigned char j;
  char ubuf[10];
  //start_adc();
  start_adc();
  //while (ADCSRA &(1<<ADATE)); 
  j=0;
  Execute();
  if(tag_flag) {
    while(j<5) {
      itoa(tag[j], ubuf, 16);
      uart_puts(ubuf);
      uart_putc(' ');
      j++;
    }
    uart_putc('\n');
  }
  
}
