/*
 * servo.h
 * multiple servo driven by ATmega328p TIMER1 output compare channels
 * assumption: servo not sensitive to off-state timing (10ms - 20ms)
 *
 * https://dannyelectronics.wordpress.com/2017/02/10/driving-multiple-servos-using-avr-timer1-output-compare-channels/
 *
 * Modified by Tim Dorssers
 */ 


#ifndef SERVO_H_
#define SERVO_H_

#include <avr/io.h>
#include <avr/interrupt.h>

#define SRVOA_PORT PORTD
#define SRVOA_DDR DDRD
#define SRVOA_CNT 2 //srvo counts

#define SRVOB_PORT PORTB
#define SRVOB_DDR DDRB
#define SRVOB_CNT 2 //srvo counts

#define F_CPU 16000000

#define PRESCALER 8
#define PSDIV(us) ((us) * (F_CPU / 1000000 / PRESCALER)) //convert us to ticks

#define MIN_PULSE_WIDTH       544     // the shortest pulse sent to a servo
#define MAX_PULSE_WIDTH      2400     // the longest pulse sent to a servo

typedef struct {
	uint8_t pin; //servo pin mask, for 8-bit types
	uint16_t pr; //on duration
} servo_t;

#define map(x,in_min,in_max,out_min,out_max) (((x)-(in_min))*((out_max)-(out_min))/((in_max)-(in_min))+(out_min))

extern void srvo1_init(void);
extern void srvo1a_act(void);
extern void srvo1a_set(uint8_t sindex, uint8_t pin, uint16_t pr);
extern void srvo1a_setpin(uint8_t sindex, uint8_t pin);
extern void srvo1a_setpr(uint8_t sindex, uint16_t pr);
extern void srvo1b_act(void);
extern void srvo1b_set(uint8_t sindex, uint8_t pin, uint16_t pr);
extern void srvo1b_setpin(uint8_t sindex, uint8_t pin);
extern void srvo1b_setpr(uint8_t sindex, uint16_t pr);

#endif /* SERVO_H_ */