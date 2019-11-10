/*
 * Title:    meArm-v1
 * Hardware: ATmega8 @ 16 MHz, PCD8544 LCD, SG90 Servos, 2-axis joysticks
 *
 * Created: 3-11-2019 10:33:47
 * Author : Tim Dorssers
 *
 * The controller board has a PCD8544-based LCD screen connected to the AVR
 * hardware SPI pins and two 2-axis joystick modules connected to the AVR ADC
 * pins to interface with the user. The PCD8544 driver supports font scaling
 * and line drawing and sends all data at once using a screen buffer of 504
 * bytes in AVR RAM.
 * Four SG90 servos are connected to two pins of PORTD and two pins of PORTB
 * and are driven by a software PWM implementation using the 16-bit Timer1
 * output compare interrupt.
 * The joystick buttons are used to save and restore servo positions.
 * Debugging data is sent to the hardware UART.
 */ 

#define F_CPU 16000000

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#include <avr/pgmspace.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "uart.h"
#include "servo.h"
#include "pcd8544.h"

char buffer[12];

// Joysticks
#define ADC_CHANNEL_COUNT    4   // Number of ADC channels we use
volatile uint8_t adc_values[ADC_CHANNEL_COUNT];
volatile bool adc_read = false;
	
// Function macros
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define adc_stop() ADCSRA &= ~(_BV(ADEN) | _BV(ADSC)) // Disable ADC, stop conversion
#define adc_start() ADCSRA |= (_BV(ADEN) | _BV(ADSC)) // Enable ADC, start conversion
#define init_buttons() PORTD |= _BV(PD2) | _BV(PD3)   // Pull up button inputs
#define display_value_str_P(__str, __val) display_value_str_p(PSTR(__str), __val)

// Servos
uint8_t set_ang[] = {90, 90, 90, 90};
uint8_t curr_ang[] = {90, 90, 90, 90};
uint8_t saved_ang[] = {90, 90, 90, 90};
enum {MIDDLE, LEFT, RIGHT, CLAW};

// Program space strings
const char on_P[] PROGMEM = "on";
const char off_P[] PROGMEM = "off";

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

// Write uint8_t to LCD with label string stored in progmem
void display_value_str_p(const char *str, uint8_t val) {
	utoa(val, buffer, 10);
	pcd8544_write_string_p(str, 1);
	pcd8544_write_string(buffer, 1);
}

// Handle joystick input
static void update_positions(void) {
	int8_t val;

	// Clear screen buffer
	pcd8544_clear();
	// Middle servo
	val = (127 - adc_values[MIDDLE]) / 16;
	// Keep between 0 and 180
	if (set_ang[MIDDLE] >= val)
		set_ang[MIDDLE] -= val;
	else
		set_ang[MIDDLE] = 0;
	set_ang[MIDDLE] = min(set_ang[MIDDLE], 180);
	// Show on LCD
	display_value_str_P("Middle:", set_ang[MIDDLE]);
	pcd8544_draw_vline(76 + val, 0, 16);
	// Left servo
	val = (127 - adc_values[LEFT]) / 16;
	set_ang[LEFT] += val;
	// Keep between 90 and 180
	set_ang[LEFT] = constrain(set_ang[LEFT], 90, 180);
	// Show on LCD
	pcd8544_set_cursor(0, 8);
	display_value_str_P("Left:  ", set_ang[LEFT]);
	pcd8544_draw_hline(68, 7 - val, 16);
	// Claw servo
	val = (127 - adc_values[CLAW]) / 16;
	// Keep between 0 and 120
	if (set_ang[CLAW] >= -val)
		set_ang[CLAW] += val;
	else
		set_ang[CLAW] = 0;
	set_ang[CLAW] = min(set_ang[CLAW], 120);
	// Show on LCD
	pcd8544_set_cursor(0, 16);
	display_value_str_P("Claw:  ", set_ang[CLAW]);
	pcd8544_draw_vline(76 + val, 17, 16);
	// Right servo
	val = (127 - adc_values[RIGHT]) / 16;
	// Keep between 0 and 180
	if (set_ang[RIGHT] >= -val)
		set_ang[RIGHT] += val;
	else
		set_ang[RIGHT] = 0;
	set_ang[RIGHT] = min(set_ang[RIGHT], 180);
	// Show on LCD
	pcd8544_set_cursor(0, 24);
	display_value_str_P("Right: ", set_ang[RIGHT]);
	pcd8544_draw_hline(68, 24 - val, 16);
	// Button 1
	pcd8544_set_cursor(0, 32);
	pcd8544_write_string_P("Knob 1:", 1);
	if (bit_is_clear(PIND, PD2)) {
		// Save angles
		memcpy(saved_ang, set_ang, sizeof(saved_ang));
		pcd8544_write_string_p(on_P, 1);
	} else
		pcd8544_write_string_p(off_P, 1);
	// Button 2
	pcd8544_set_cursor(0, 40);
	pcd8544_write_string_P("Knob 2:", 1);
	if (bit_is_clear(PIND, PD3)) {
		// Restore angles
		memcpy(set_ang, saved_ang, sizeof(set_ang));
		pcd8544_write_string_p(on_P, 1);
	} else
		pcd8544_write_string_p(off_P, 1);
	// Update LCD
	pcd8544_render();
}

// Move servos
static void servo_move(void) {
	for (uint8_t i = 0; i < sizeof(curr_ang); i++) {
		if (curr_ang[i] < set_ang[i])
			curr_ang[i]++;
		if (curr_ang[i] > set_ang[i])
			curr_ang[i]--;
	}
	// Set servo positions
	srvo1b_setpr(2, curr_ang[MIDDLE]);
	srvo1b_setpr(1, curr_ang[LEFT]);
	srvo1a_setpr(1, curr_ang[CLAW]);
	srvo1a_setpr(2, curr_ang[RIGHT]);
	// Delay before next movement
	_delay_ms(5);
}

// Show welcome screen
static void display_welcome(void) {
	pcd8544_clear();
	pcd8544_write_string_P("meArm", 3);
	pcd8544_set_cursor(0, 24);
	pcd8544_write_string_P("Press joystickbutton to     start", 1);
	pcd8544_render();
}

int main(void) {
	uart_init(UART_BAUD_SELECT(9600, F_CPU));
	init_adc();
	init_buttons();
	pcd8544_init();
	srvo1_init(); //reset the servo module
	srvo1a_act(); //start servo1 cha
	srvo1b_act(); //start servo1 chb
	sei(); // enable interrupts
	adc_start();
	adc_read = true;
	display_welcome();
	// Wait for any button press
	while (bit_is_set(PIND, PD2) && bit_is_set(PIND, PD3));
	// Main loop
    while (1) {
		// Update servo positions
		for (uint8_t i = 0; i < 9; i++)
			servo_move();
		// Get new readings
		adc_read = true;
		while (adc_read);
		// Parse readings
		update_positions();
		// Dump values to UART
		for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++) {
			utoa(adc_values[i], buffer, 10);
			uart_puts(buffer);
			uart_puts_P(", ");
		}
		uart_puts_P("\r\n");
    }
}
