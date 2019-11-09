/*
 * servo.c
 * multiple servo driven by ATmega328p TIMER1 output compare channels
 * assumption: servo not sensitive to off-state timing (10ms - 20ms)
 * 
 * https://dannyelectronics.wordpress.com/2017/02/10/driving-multiple-servos-using-avr-timer1-output-compare-channels/
 *
 * Modified by Tim Dorssers
 */ 

#include "servo.h"

//global variables
//for comp ch a
static volatile uint16_t _stmra_pr; //servo timer period, in ticks
static volatile uint8_t _sindexa; //servo index from 0..SRVO_CNT
//servo definitions: pin mask + period
//pin mask = 0 -> output on that channel disabled
static servo_t srvosa[SRVOA_CNT+1] = {
	{0<<0, PSDIV(20000)}, //for the off period: no output, 20,000 ticks
	{1<<5, PSDIV(1500)}, //servo ch 1
	{1<<6, PSDIV(1500)}, //servo ch 2
};

//for comp ch b
static volatile uint16_t _stmrb_pr; //servo timer period, in ticks
static volatile uint8_t _sindexb; //servo index from 0..SRVO_CNT
//servo definitions: pin mask + period
//pin mask = 0 -> output on that channel disabled
static servo_t srvosb[SRVOB_CNT+1] = {
	{0<<0, PSDIV(20000)}, //for the off period: no output, 20,000 ticks
	{1<<0, PSDIV(1500)}, //servo ch 3
	{1<<1, PSDIV(1500)}, //servo ch 4
};

//servo isr for comp ch a
ISR(TIMER1_COMPA_vect) {
	SRVOA_PORT &= ~srvosa[_sindexa].pin; //turn off the previous servo
	_sindexa = (_sindexa == SRVOA_CNT) ? 0 : (_sindexa + 1); //advance the servo index
	SRVOA_PORT |= srvosa[_sindexa].pin; //turn on the servo pin
	_stmra_pr = srvosa[_sindexa].pr; //load the servo period
	OCR1A += _stmra_pr; //set the next match point
}

//servo isr for comp ch b
ISR(TIMER1_COMPB_vect) {
	SRVOB_PORT &= ~srvosb[_sindexb].pin; //turn off the previous servo
	_sindexb = (_sindexb == SRVOB_CNT) ? 0 : (_sindexb + 1); //advance the servo index
	SRVOB_PORT |= srvosb[_sindexb].pin; //turn on the servo pin
	_stmrb_pr = srvosb[_sindexb].pr; //load the servo period
	OCR1B += _stmrb_pr; //set the next match point
}

void srvo1_init(void) {
#if defined(__AVR_ATmega8__)|| defined(__AVR_ATmega128__)
	TIFR |= (1<<OCF1A) | (1<<OCF1B); //clear compa/b flag by writing '1' to it
	TIMSK &= ~((1<<OCIE1A) | (1<<OCIE1B)); //disable compa/b interrupts
#else
	TIFR1 |= (1<<OCF1A) | (1<<OCF1B); //clear compa/b flag by writing '1' to it
	TIMSK1 &= ~((1<<OCIE1A) | (1<<OCIE1B)); //disable compa/b interrupts
#endif
	TCNT1 = 0; //initialize the counter
#if PRESCALER == 8
	TCCR1B = (0<<CS12) | (1<<CS11) | (0<<CS10); //set prescaler, start the timer
#else
	TCCR1B = (0<<CS12) | (0<<CS11) | (1<<CS10); //start the timer
#endif
}

//for comp ch a
//activate compa interrupt
void srvo1a_act(void) {
	//initialize the pin
	for (_sindexa = 0; _sindexa<=SRVOA_CNT; _sindexa++) {
		SRVOA_DDR |= srvosa[_sindexa].pin;
		SRVOA_PORT &= ~(srvosa[_sindexa].pin); //servo pin as output, idles low
	}

	_sindexa = 0; //reset the servo index
	SRVOA_PORT |= 1<<srvosa[_sindexa].pin; //turn on the pin
	_stmra_pr = srvosa[_sindexa].pr; //load the servo period

	//configure compa
	OCR1A = TCNT1 + _stmra_pr; //set the next match point
#if defined(__AVR_ATmega8__)|| defined(__AVR_ATmega128__)
	TIFR |= 1<<OCF1A; //clear the flag
	TIMSK|= 1<<OCIE1A; //enable the interrupt
#else
	TIFR1 |= 1<<OCF1A; //clear the flag
	TIMSK1|= 1<<OCIE1A; //enable the interrupt
#endif
}

//change servo parameters
//user has to perform error checking
//pin = pin mask. 0 to disable a channel
void srvo1a_set(uint8_t sindex, uint8_t pin, uint16_t pr) {
	srvosa[sindex].pin = pin; //update the pin mask
	if (pr < MIN_PULSE_WIDTH) // treat values less than 544 as angles in degrees
		srvosa[sindex].pr = map(pr, 0, 180, PSDIV(MIN_PULSE_WIDTH), PSDIV(MAX_PULSE_WIDTH));
	else
		srvosa[sindex].pr = PSDIV(pr); //update the period
}

//change servo parameters - pin
//user has to perform error checking
//pin = pin mask. 0 to disable a channel
void srvo1a_setpin(uint8_t sindex, uint8_t pin) {
	srvosa[sindex].pin = pin; //update the pin mask
}

//change servo parameters - period
//user has to perform error checking
void srvo1a_setpr(uint8_t sindex, uint16_t pr) {
	if (pr < MIN_PULSE_WIDTH) // treat values less than 544 as angles in degrees
		srvosa[sindex].pr = map(pr, 0, 180, PSDIV(MIN_PULSE_WIDTH), PSDIV(MAX_PULSE_WIDTH));
	else
		srvosa[sindex].pr = PSDIV(pr); //update the period
}

//for comp ch b
//activate compb interrupt
void srvo1b_act(void) {
	//initialize the pin
	for (_sindexb = 0; _sindexb<=SRVOB_CNT; _sindexb++) {
		SRVOB_DDR |= srvosb[_sindexb].pin;
		SRVOB_PORT &= ~srvosb[_sindexb].pin; //servo pin as output, idles low
	}

	_sindexb = 0; //reset the servo index
	SRVOB_PORT |= srvosb[_sindexb].pin; //turn on the pin
	_stmrb_pr = srvosb[_sindexb].pr; //load the servo period

	//configure compb
	OCR1B = TCNT1 + _stmrb_pr; //set the next match point
#if defined(__AVR_ATmega8__)|| defined(__AVR_ATmega128__)
	TIFR |= 1<<OCF1B; //clear the flag
	TIMSK|= 1<<OCIE1B; //enable the interrupt
#else
	TIFR1 |= 1<<OCF1B; //clear the flag
	TIMSK1|= 1<<OCIE1B; //enable the interrupt
#endif
}

//change servo parameters
//user has to perform error checking
//pin = pin mask. 0 to disable a channel
void srvo1b_set(uint8_t sindex, uint8_t pin, uint16_t pr) {
	srvosb[sindex].pin = pin; //update the pin mask
	if (pr < MIN_PULSE_WIDTH) // treat values less than 544 as angles in degrees
		srvosb[sindex].pr = map(pr, 0, 180, PSDIV(MIN_PULSE_WIDTH), PSDIV(MAX_PULSE_WIDTH));
	else
		srvosb[sindex].pr = PSDIV(pr); //update the period
}

//change servo parameters - pin
//user has to perform error checking
//pin = pin mask. 0 to disable a channel
void srvo1b_setpin(uint8_t sindex, uint8_t pin) {
	srvosb[sindex].pin = pin; //update the pin mask
}

//change servo parameters - period
//user has to perform error checking
void srvo1b_setpr(uint8_t sindex, uint16_t pr) {
	if (pr < MIN_PULSE_WIDTH) // treat values less than 544 as angles in degrees
		srvosb[sindex].pr = map(pr, 0, 180, PSDIV(MIN_PULSE_WIDTH), PSDIV(MAX_PULSE_WIDTH));
	else
		srvosb[sindex].pr = PSDIV(pr); //update the period
}
