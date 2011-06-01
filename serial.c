/*
  serial.c - serial functions.
  Part of Arduino - http://www.arduino.cc/

  Copyright (c) 2005-2006 David A. Mellis

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General
  Public License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place, Suite 330,
  Boston, MA  02111-1307  USA

*/

#include <math.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

// Define constants and variables for buffering incoming serial data.  We're
// using a ring buffer (I think), in which rx_buffer_head is the index of the
// location to which to write the next incoming character and rx_buffer_tail
// is the index of the location from which to read.
#ifdef __AVR_ATmega328P__
#define RX_BUFFER_SIZE 256
#else
#define RX_BUFFER_SIZE 64
#endif

#define TX_BUFFER_SIZE 16

unsigned char rx_buffer[RX_BUFFER_SIZE];

int rx_buffer_head = 0;
int rx_buffer_tail = 0;

void beginSerial(long baud)
{
	UBRR0H = ((F_CPU / 16 + baud / 2) / baud - 1) >> 8;
	UBRR0L = ((F_CPU / 16 + baud / 2) / baud - 1);
	
	/* baud doubler off  - Only needed on Uno XXX */
  UCSR0A &= ~(1 << U2X0);
          
	// enable rx and tx
  UCSR0B |= 1<<RXEN0;
  UCSR0B |= 1<<TXEN0;
	
	// enable interrupt on complete reception of a byte
  UCSR0B |= 1<<RXCIE0;
	
	// defaults to 8-bit, no parity, 1 stop bit
}


unsigned char tx_buffer[TX_BUFFER_SIZE];
unsigned char tx_buffer_head = 0;
volatile unsigned char tx_buffer_tail = 0;

void serialWrite(unsigned char c) {

  if ((!(UCSR0A & (1 << UDRE0))) || (tx_buffer_head != tx_buffer_tail)) {
    // maybe checking if buffer is empty is not necessary, 
    // not sure if there can be a state when the data register empty flag is set
    // and read here without the interrupt being executed
    // well, it shouldn't happen, right?

    // data register is not empty, use the buffer
    unsigned char newhead = tx_buffer_head + 1;
    newhead %= TX_BUFFER_SIZE;

    // wait until there's a space in the buffer
    while (newhead == tx_buffer_tail) ;

    tx_buffer[tx_buffer_head] = c;
    tx_buffer_head = newhead;

    // enable the Data Register Empty Interrupt
    sei();
    UCSR0B |=  (1 << UDRIE0);

    } 
    else {
	UDR0 = c;
    }
}

// interrupt called on Data Register Empty
SIGNAL(USART_UDRE_vect) {
  // temporary tx_buffer_tail 
  // (to optimize for volatile, there are no interrupts inside an interrupt routine)
  unsigned char tail = tx_buffer_tail;

  // get a byte from the buffer	
  unsigned char c = tx_buffer[tail];
  // send the byte
	 UDR0 = c;

  // update tail position
  tail ++;
  tail %= TX_BUFFER_SIZE;

  // if the buffer is empty,  disable the interrupt
  if (tail == tx_buffer_head) {
    UCSR0B &=  ~(1 << UDRIE0);
  }

  tx_buffer_tail = tail;
}

// Returns true if there is any data in the read buffer
int serialAnyAvailable()
{
  return (rx_buffer_head != rx_buffer_tail);
}

int serialRead()
{
	// if the head isn't ahead of the tail, we don't have any characters
	if (serialAnyAvailable()) {
		return -1;
	} else {
		unsigned char c = rx_buffer[rx_buffer_tail];
		rx_buffer_tail = (rx_buffer_tail + 1) % RX_BUFFER_SIZE;
		return c;
	}
}

void serialFlush()
{
	// don't reverse this or there may be problems if the RX interrupt
	// occurs after reading the value of rx_buffer_head but before writing
	// the value to rx_buffer_tail; the previous value of rx_buffer_head
	// may be written to rx_buffer_tail, making it appear as if the buffer
	// were full, not empty.
	rx_buffer_head = rx_buffer_tail;
}

SIGNAL(USART_RX_vect)
{
	unsigned char c = UDR0;
	int i = (rx_buffer_head + 1) % RX_BUFFER_SIZE;

	// if we should be storing the received character into the location
	// just before the tail (meaning that the head would advance to the
	// current location of the tail), we're about to overflow the buffer
	// and so we don't write the character or advance the head.
	if (i != rx_buffer_tail) {
		rx_buffer[rx_buffer_head] = c;
		rx_buffer_head = i;
	}
}

// void printMode(int mode)
// {
//  // do nothing, we only support serial printing, not lcd.
// }

void printByte(unsigned char c)
{
	serialWrite(c);
}

// void printNewline()
// {
//  printByte('\n');
// }
// 
void printString(const char *s)
{
	while (*s)
		printByte(*s++);
}

// Print a string stored in PGM-memory
void printPgmString(const char *s)
{
  char c;
	while ((c = pgm_read_byte_near(s++)))
		printByte(c);
}

void printIntegerInBase(unsigned long n, unsigned long base)
{ 
	unsigned char buf[8 * sizeof(long)]; // Assumes 8-bit chars. 
	unsigned long i = 0;

	if (n == 0) {
		printByte('0');
		return;
	} 

	while (n > 0) {
		buf[i++] = n % base;
		n /= base;
	}

	for (; i > 0; i--)
		printByte(buf[i - 1] < 10 ?
			'0' + buf[i - 1] :
			'A' + buf[i - 1] - 10);
}

void printInteger(long n)
{
	if (n < 0) {
		printByte('-');
		n = -n;
	}

	printIntegerInBase(n, 10);
}

void printFloat(double n)
{
  double integer_part, fractional_part;
  fractional_part = modf(n, &integer_part);
  printInteger(integer_part);
  printByte('.');
  printInteger(round(fractional_part*1000));
}
