/*
 * Copyright (c) 2013 Erik Bosman <erik@minemu.org>
 *
 * Permission  is  hereby  granted,  free  of  charge,  to  any  person
 * obtaining  a copy  of  this  software  and  associated documentation
 * files (the "Software"),  to deal in the Software without restriction,
 * including  without  limitation  the  rights  to  use,  copy,  modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the
 * Software,  and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 *
 * The  above  copyright  notice  and this  permission  notice  shall be
 * included  in  all  copies  or  substantial portions  of the Software.
 *
 * THE SOFTWARE  IS  PROVIDED  "AS IS", WITHOUT WARRANTY  OF ANY KIND,
 * EXPRESS OR IMPLIED,  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY,  FITNESS  FOR  A  PARTICULAR  PURPOSE  AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM,  DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT,  TORT OR OTHERWISE,  ARISING FROM, OUT OF OR IN
 * CONNECTION  WITH THE SOFTWARE  OR THE USE  OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * (http://opensource.org/licenses/mit-license.html)
 */

#define DEBUG 0

#define F_CPU (8000000u)

#define IO_OUT(P, R) asm volatile("out %0, %1\n\t" : : "I" (_SFR_IO_ADDR(P)), "r" (R)); 
#define IO_CLEAR(P) asm volatile("out %0, r1\n\t" : : "I" (_SFR_IO_ADDR(P))); 

#include <avr/io.h>
#include <avr/sleep.h>
#include <util/delay_basic.h>
#include <stdint.h>

void serial_init(int rate)
{
	UBRR0L = rate;
	UBRR0H = (rate>>8);

	UCSR0C=(1<<UCSZ01)|(1<<UCSZ00);
	UCSR0B=/*(1<<RXEN0)|*/(1<<TXEN0);
}

void serial_write(unsigned char c)
{
	while ( !( UCSR0A & (1<<UDRE0)) );
	UDR0=c;
}

/*
unsigned char serial_read(void)
{
	while ( !(UCSR0A & (1<<RXC0)) );
	return UDR0;
}
*/


#ifdef DEBUG

const static char *hex = "0123456789abcdef";

void serial_write_uint(uint16_t n)
{
	int i;
	for (i=0; i<16 ;i+=4)
		serial_write(hex[(n>>(12-i))&0xf]);

	serial_write(' ');
}

void serial_nl(void)
{
	serial_write('\r');
	serial_write('\n');
}

#endif //DEBUG


/*          Y2            Y1
 *      ..........      ....
 *    __:__    __:__    :____
 *   |  :  |  |  :  |__|:    | X3
 *   |  1  |  |  2   __ : 3  |
 *   |__:__ \ |_____|  |:_ __|
 *      :  \ \   .......:| |
 *    __:__ \ \__:__    _| |__
 *   |  :  | \   :  |  |  ...|... Y0
 *   |  4  |  |  5  |  |  6  |
 *   |__:__ \ |__:__ \ |__:__|
 *      :  \ \   :  \ \   :
 *    __:__ \ \__:__ \ \__:__
 *   |  :  | \   :  | \   :  | X2
 *   |  7  |  |  8  |  |  9  |
 *   |__ __|  |__:__ \ |__:__|
 *     | |  .....:  \ \   :
 *    _| |_:   _____ \ \__:__
 *   |    :|__|     | \   :  | X1
 *   |  C   __   0..|..|..B  |
 *   |_____|  |_____|  |_____|
 *               X0
 */

const static char *map = "0B96C8537412....";

void touch_callback(uint8_t x, uint8_t y)
{
	serial_write(map[(y<<2)+x]);
}


#define BIT0 (1<<0)
#define BIT1 (1<<1)
#define BIT2 (1<<2)
#define BIT3 (1<<3)
#define BIT4 (1<<4)
#define BIT5 (1<<5)
#define BIT6 (1<<6)
#define BIT7 (1<<7)

#define PORT_X PORTB
#define DDR_X  DDRB

#define PORT_Y PORTC
#define DDR_Y  DDRC

#define PORT_SMP PORTD
#define DDR_SMP  DDRD

#define X0 BIT0
#define X1 BIT1
#define X2 BIT2
#define X3 BIT3
#define XMASK  (X0|X1|X2|X3)

#define Y0A BIT0
#define Y1A BIT3
#define Y2A BIT4

#define Y0B BIT1
#define Y1B BIT2
#define Y2B BIT5

#define Y0B_MUX (1)
#define Y1B_MUX (2)
#define Y2B_MUX (5)

#define YMASK  (Y0A|Y0B|Y1A|Y1B|Y2A|Y2B)

#define SMP BIT7

#define DELAY_CYCLES 5
#define NUM_CYCLES   150

#define X_LINES 4
#define Y_LINES 3

enum { OFF, ON };

const uint8_t x_port[X_LINES][2] =
{
	{ 0, X0 },
	{ 0, X1 },
	{ 0, X2 },
	{ 0, X3 },
};

const uint8_t y_dir[Y_LINES][2] =
{
	{ Y0A, Y0B },
	{ Y1A, Y1B },
	{ Y2A, Y2B },
};

const uint8_t y_mux[Y_LINES] =
{
	Y0B_MUX,
	Y1B_MUX,
	Y2B_MUX,
};

void init_ports(void)
{
	DDR_X    = XMASK; /* drive the X lines by default to reduce noise */
	PORT_X   = 0;

	DDR_Y    = 0;
	PORT_Y   = 0;

	DDR_SMP  = 0;
	PORT_SMP = 0;
}

void init_analog_comparator(void)
{
	ACSR = 0x00;
	ADCSRB = 0x00;
	ADCSRB = (1<<ACME);
	ADCSRA &= ~(1<<ADEN);
	DIDR1 = (1<<AIN0D);
}

uint16_t measure(uint8_t x, uint8_t y)
{
	{
		register uint8_t x_on  = x_port[x][ON],
		                 x_off = x_port[x][OFF],
		                 y_on  = y_dir[y][ON],
		                 y_off = y_dir[y][OFF];

		register uint16_t i=NUM_CYCLES;

		while (i--)
		{
			IO_CLEAR(DDR_Y);
			IO_OUT(DDR_Y, y_on);
			IO_OUT(PORT_X, x_on);
			__builtin_avr_delay_cycles(DELAY_CYCLES);
			IO_CLEAR(DDR_Y);
			IO_OUT(DDR_Y, y_off);
			IO_OUT(PORT_X, x_off);
			__builtin_avr_delay_cycles(DELAY_CYCLES);
		}
	}

	DDR_X = 0;
	uint16_t c=0;
	ADMUX = y_mux[y];
	ADCSRB = (1<<ACME);
	IO_OUT(PORT_SMP, SMP);
	IO_OUT(DDR_SMP, SMP);

	while ( (ACSR & (1<<ACO)) )
		c++;

	PORT_SMP = 0;
	DDR_SMP = 0;
	ADCSRB = 0;
	DDR_X = XMASK;  /* drive the X lines by default to reduce noise */

	return c;
}

uint16_t baseline[Y_LINES][X_LINES];
uint8_t  pressed[Y_LINES][X_LINES];

void calbrate(void)
{
	uint8_t i, x, y;

	for(y=0; y<Y_LINES; y++)
		for(x=0; x<X_LINES; x++)
		{
			baseline[y][x] = 0;
			pressed[y][x] = 0;
		}

	for (i=0; i<16; i++)
		for(y=0; y<Y_LINES; y++)
			for(x=0; x<X_LINES; x++)
				baseline[y][x] += measure(x, y);
}


int main(void)
{
	uint8_t x, y;
	uint16_t v;

	serial_init(51);
	calbrate();

	for(;;)
	{
		for(y=0; y<Y_LINES; y++)
			for(x=0; x<X_LINES; x++)
			{
				v = measure(x, y);
//serial_write(map[(y<<2)+x]);
//serial_write_uint(v);
//serial_write_uint(baseline[y][x]);
				uint16_t *b = &baseline[y][x];
				uint8_t *p = &pressed[y][x];

				if (v < (*b>>4)-(*b>>8) )
				{
					if (*p == 0)
						touch_callback(x, y);

					if (*p > 100)
					{
						*b -= *b>>4;
						*b += v;
					}
					else
						*p += 1;
				}
				else
				{
					*p = 0;
					*b -= *b>>4;
					*b += v;
				}
			}
//		serial_nl();
	}
}

