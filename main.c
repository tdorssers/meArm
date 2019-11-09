/*
 * Title:    meArm-v1
 * Hardware: ATmega8 @ 16 MHz, PCD8544 LCD, SG90 Servos
 *
 * Created: 3-11-2019 10:33:47
 * Author : Tim Dorssers
 */ 

#define F_CPU 16000000

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#include <avr/pgmspace.h>
#include <stdbool.h>
#include <stdlib.h>
#include "uart.h"
#include "servo.h"
#include "pcd8544.h"

// Joysticks
#define ADC_CHANNEL_COUNT    4   // Number of ADC channels we use
volatile uint8_t adc_values[ADC_CHANNEL_COUNT];
volatile bool adc_read = false;

// Function macros
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define adc_stop() ADCSRA &= ~(_BV(ADEN) | _BV(ADSC)) // Disable ADC, stop conversion
#define adc_start() ADCSRA |= (_BV(ADEN) | _BV(ADSC)) // Enable ADC, start conversion

// Servos
int16_t middle = 90, left = 90, right = 90, claw = 90;
enum {MIDDLE, LEFT, RIGHT, CLAW};

// Initialize ADC
static void init_adc(void) {
	// AVCC with external capacitor at AREF pin and ADC Left Adjust Result
	ADMUX = _BV(ADLAR) | _BV(REFS0);
	// free running select, interrupt flag, ADC prescaler of 128
	ADCSRA = _BV(ADFR) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
}

// ADC interrupt
ISR(ADC_vect) {
	static uint8_t adc_selector;
	static bool adc_reading;
	
	// Update adc_values if requested. Note that we are running "one channel behind"
	// in free running mode.
	if (adc_read && adc_reading) {
		if(adc_selector == 0) {
			adc_values[ADC_CHANNEL_COUNT - 1] = ADCH;
		} else {
			adc_values[adc_selector - 1] = ADCH;
		}
	}
	// Next channel
	adc_selector++;
	// If we hit the max channel, go back to channel 0 and check status vars
	if(adc_selector >= ADC_CHANNEL_COUNT) {
		adc_selector = 0;
		// Start updating adc_value array if requested; Otherwise request is completed
		if (adc_read && !adc_reading) { // Start updating
			adc_reading = true;
		} else { // Request is completed
			adc_read = false;
			adc_reading = false;
		}
	}
	// Update ADC channel selection
	ADMUX = (_BV(ADLAR) | _BV(REFS0)) + adc_selector;
}

static void init_buttons(void) {
	PORTD |= _BV(PD2) | _BV(PD3);   // Pull up
}

int main(void) {
	char buffer[12];
	int8_t value;

    uart_init(UART_BAUD_SELECT(9600, F_CPU));
	uart_puts_P("hello\r\n");
	pcd8544_init();
	uart_puts_P("init\r\n");
	pcd8544_clear();
	pcd8544_write_string_P("Initialize",1);
	pcd8544_set_cursor(0, 10);
	pcd8544_write_string_P("meArm", 3);
	pcd8544_render();
    uart_puts_P("done\r\n");
	srvo1_init(); //reset the servo module
	srvo1a_act(); //start servo1 cha
	srvo1b_act(); //start servo1 chb
	init_adc();
	init_buttons();
	sei();
	adc_start();
	_delay_ms(900);
    while (1) {
		_delay_ms(100);
		// Get new readings
		adc_read = true;
		while (adc_read);
		// Clear screen buffer
		pcd8544_clear();
		// Middle servo
		value = (127 - adc_values[MIDDLE]) / 16;
		middle += value;
		middle = constrain(middle, 0, 180);
		srvo1b_setpr(2, middle);
		utoa(middle, buffer, 10);
		pcd8544_set_cursor(0, 0);
		pcd8544_write_string_P("Middle:", 1);
		pcd8544_write_string(buffer, 1);
		pcd8544_draw_vline(68 + value, 0, 16);
		// Left servo
		value = (127 - adc_values[LEFT]) / 16;
		left += value;
		left = constrain(left, 90, 180);
		srvo1b_setpr(1, left);
		utoa(left, buffer, 10);
		pcd8544_set_cursor(0, 10);
		pcd8544_write_string_P("Left:  ", 1);
		pcd8544_write_string(buffer, 1);
		pcd8544_draw_hline(60, 7 - value, 16);
		// Claw servo
		value = (127 - adc_values[CLAW]) / 16;
		claw += value;
		claw = constrain(claw, 50, 100);
		srvo1a_setpr(1, claw);
		utoa(claw, buffer, 10);
		pcd8544_set_cursor(0, 20);
		pcd8544_write_string_P("Claw:  ", 1);
		pcd8544_write_string(buffer, 1);
		pcd8544_draw_vline(68 + value, 20, 16);
		// Right servo
		value = (127 - adc_values[RIGHT]) / 16;
		right += value;
		right = constrain(right, 0, 180);
		srvo1a_setpr(2, right);
		utoa(right, buffer, 10);
		pcd8544_set_cursor(0, 30);
		pcd8544_write_string_P("Right: ", 1);
		pcd8544_write_string(buffer, 1);
		pcd8544_draw_hline(60, 27 - value, 16);
		// Update LCD
		pcd8544_render();
		// Dump values to UART
		for (uint8_t i = 0; i < 4; i++) {
			utoa(adc_values[i], buffer, 10);
			uart_puts(buffer);
			uart_puts_P(", ");
		}
		if (bit_is_clear(PIND, PD2))
			uart_puts_P("true,");
		else
			uart_puts_P("false,");
		if (bit_is_clear(PIND, PD3))
			uart_puts_P("true");
		else
			uart_puts_P("false");
		uart_puts_P("\r\n");
    }
}
