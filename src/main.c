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

#define DEBUG

#define F_CPU (8000000u)

#define IO_OUT(P, R) asm volatile("out %0, %1\n\t" : : "I" (_SFR_IO_ADDR(P)), "r" (R)); 
#define IO_CLEAR(P) asm volatile("out %0, r1\n\t" : : "I" (_SFR_IO_ADDR(P))); 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay_basic.h>
#include <stdint.h>

void serial_init(int rate)
{
	UBRR0L = rate;
	UBRR0H = (rate>>8);

	UCSR0C=(1<<UCSZ01)|(1<<UCSZ00);
	UCSR0B=(1<<RXEN0)|(1<<TXEN0);
}

void serial_write(unsigned char c)
{
	while ( !( UCSR0A & (1<<UDRE0)) );
	UDR0=c;
}

#define SERIAL_AVAILABLE (UCSR0A & (1<<RXC0))

unsigned char serial_read(void)
{
	while ( !(UCSR0A & (1<<RXC0)) );
	return UDR0;
}


#ifdef DEBUG

const static char *hex = "0123456789abcdef";

const static char *debug_command="debug";
static uint8_t debug_i=0, debug_on=0;

void debug_poll(void)
{
	if ( !SERIAL_AVAILABLE )
		return;

	uint8_t c = serial_read();

	if (c != debug_command[debug_i])
		debug_i = 0;

	if (c == debug_command[debug_i])
	{
		debug_i++;
		if (debug_command[debug_i] == '\0')
		{
			debug_on = !debug_on;
			debug_i = 0;
		}
	}
}

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

/* from no signal -> signal */
#define SIGNAL_THRESHOLD(baseline_x_16) ( ((baseline_x_16)>>4)-((baseline_x_16)>>9) )

/* from signal -> no signal, differs from SIGNAL_THRESHOLD to prevent oscillation */
#define NO_SIGNAL_THRESHOLD(baseline_x_16) ( ((baseline_x_16)>>4)-((baseline_x_16)>>10) )

#define SUSTAINED_SIGNAL_COUNT (2)
#define BASELINE_AVG(baseline_x_16) (baseline_x_16>>4)

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
	ADCSRB = (1<<ACME);
	ADCSRA &= ~(1<<ADEN);
	ACSR = 1<<ACIC;
	TCCR1A = 0;
	TCCR1C = 0x00;
	TCCR1B = (1<<ICNC1) | (0 /* stop */);
	sei();
}

static uint16_t loop=0;

ISR(TIMER1_CAPT_vect) /* chip sets ICR1 on capture */
{
	TIMSK1 = 0; /* disable interrupt */
	loop = 0;
}

uint16_t measure(uint8_t x, uint8_t y)
{
	loop = 1;
	TCNT1 = 0;
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
	ADMUX = y_mux[y];
	ADCSRB = (1<<ACME);
	TCCR1B = (1<<ICNC1) | (1 /* clock select prescaler 1 */ );
	TIFR1 = 1<<ICF1;         /* clear(!) flag */
	TIMSK1 = 1<<ICIE1;
	IO_OUT(PORT_SMP, SMP);
	IO_OUT(DDR_SMP, SMP);

	while ( loop );

	TCCR1B = (1<<ICNC1) | (0 /* stop */);

	PORT_SMP = 0;
	DDR_SMP = 0;
	ADCSRB = 0;
	DDR_X = XMASK;  /* drive the X lines by default to reduce noise */

	return ICR1;
}

uint16_t baseline[Y_LINES*X_LINES]; /* +/- 16x avg no-signal baseline */
uint8_t  pressed[Y_LINES*X_LINES];

void calibrate(void)
{
	uint8_t i, x, y;

	uint16_t *b = baseline;
	uint8_t  *p = pressed;

	for(i=0; i<Y_LINES*X_LINES; i++)
		*b++ = *p++ = 0;

	for (i=0; i<16; i++)
	{
		b = baseline;
		for(y=0; y<Y_LINES; y++)
			for(x=0; x<X_LINES; x++)
				*b++ += measure(x, y);

	}
}


int main(void)
{
	uint8_t x, y;
	uint16_t v;

	serial_init(51);
	init_ports();
	init_analog_comparator();
	calibrate();

	for(;;)
	{
#ifdef DEBUG
		debug_poll();
#endif
		uint16_t *b = baseline;
		uint8_t *p = pressed;

		for(y=0; y<Y_LINES; y++)
			for(x=0; x<X_LINES; x++)
			{
				v = measure(x, y);
#ifdef DEBUG
				if (debug_on)
				{
					serial_write(map[(y<<2)+x]);
					serial_write_uint(v);
					serial_write_uint(*b);
				}
#endif
				if (*p && v < NO_SIGNAL_THRESHOLD(*b))
				{
					if (*p > 100)
						*b += v - BASELINE_AVG(*b);
					else
						*p += 1;
				}
				else if (*p == 0 && v < SIGNAL_THRESHOLD(*b) )
				{
					*p += 1;
				}
				else
				{
					*p = 0;
					*b += v - BASELINE_AVG(*b);
				}

				if (*p == SUSTAINED_SIGNAL_COUNT)
					touch_callback(x, y);

				b++;
				p++;
			}
#ifdef DEBUG
		if (debug_on)
			serial_nl();
#endif
	}
}

