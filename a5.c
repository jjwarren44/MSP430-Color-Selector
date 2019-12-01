#include <msp430.h>
#include <libemb/conio/conio.h>
#include <libemb/serial/serial.h>

#include "colors.h"
#include "dtc.h"

/******
 *
 *    CONSTANTS
 *
 ******/


// large number of data structures should be here
// use `const` keyword

/* Color display (r,g,b) display, always have P1.0 = 0 when display
 * r, g, or b, so this only pertains to P2OUT */
const char colorPatterns[] = {0b10101101, 0b01000000, 0b00000101};

// NUMBER PATTERS
// 0, 1, 2, 3, 4, ..., E, F
const char P2OUT_Patterns[] = {0b00000000, 0b11100001, 0b10001000, 0b11000000, 0b01110001, 
		   	       0b01000100, 0b00000100, 0b11100000, 0b00000000, 0b01100000,
		   	       0b00100000, 0b00000101, 0b00001100, 0b10000001, 0b00001100, 0b00101100};


// All we care about is BIT0 / P1.0
const char P1OUT_Patterns[] = {0b01000001, 0b01000001, 0b01000000, 0b01000000, 0b01000000, 
			      0b01000000, 0b01000000, 0b01000001, 0b01000000, 0b01000000,
		   	      0b01000000, 0b01000000, 0b01000001, 0b01000000, 0b01000000, 0b01000000};

/******
 *
 *    GLOBALS
 *
 ******/

int color = 0;			// red = 0, green = 1, blue = 2
char cursor = 0;
volatile unsigned int *ADC10_value = &TA0CCR1;	// Hold ADC10 value, update to other timer in interrupt
unsigned char hexValue = 0b00000000;	// Will be assigned *ADC10_value >> 2
int printCounter = 0; // Increment each print interrupt, when reaches 1000 then print

/******
 *
 *    INITIALIZATION
 *
 ******/
int main(void)
{
	/* WIZARD WORDS ***************************/
	WDTCTL   = WDTPW | WDTHOLD;               // Disable Watchdog
	BCSCTL1  = CALBC1_1MHZ;                   // Run @ 1MHz
	DCOCTL   = CALDCO_1MHZ;

	serial_init(9600);
	P1SEL &= ~(BIT1); // Reset UART_RXD (is set in serial_init)
	P1SEL2 &= ~(BIT1); // Reset UART_RXD (is set in serial_init)

	/* GPIO ***********************************/
	// pin initialization
	P1DIR = BIT6; // Red as output
	P1SEL |= BIT6;	// Enable PWM on port 1.6

	P2DIR |= BIT1|BIT4;
	P2SEL |= BIT1|BIT4; //TA 1.2 on P2.4 TA1.1 on P2.1
	
	// use P2.6 and P2.7 as GPIO
	P2SEL  &= ~(BIT6|BIT7);
	P2SEL2 &= ~(BIT6|BIT7);

	// Enable button interrupt
	P1IE = BIT3;
	P1IES = BIT3;

	/* TIMER A0 *******************************/
	// timer 0 initialization
	TA0CCR0 = 1023; // Max possible value for ADC10
	TA0CCTL0 = CCIE; // Enable interrupts for 7 segment display
	TA0CCTL1 = OUTMOD_7; // Output mode set to reset/set
	TA0CTL = TASSEL_2|MC_1; //SMCLK, up mode

	/* TIMER A1 *******************************/
	// timer 1 initialization
	TA1CCR0 = 1023; // Max possible value for ADC10
	TA1CCTL0 = CCIE; // Enable interrupts for printing color to screen
	TA1CTL = TASSEL_2|MC_1;	// SMCLK, up mode

	// Outputs for green and blue timers (how often should it blink)
	TA1CCTL1 = OUTMOD_7; // Output mode = reset/set for TA1.1 (green)
     	TA1CCTL2 = OUTMOD_7; // Output mode = reset/set for TA1.2 (blue)
	TA1CTL = TASSEL_2|MC_1; //SMCLK, up mode
	
	P1DIR |= BIT0|BIT1|BIT5|BIT7; // All 7 segment displays set to output
	P1OUT = BIT1|BIT5|BIT7; // Turn on 7 segment displays
	P2DIR = -1;
	P2OUT = ~(BIT6);
	P1OUT |= ~(BIT0);

	/* ADC10CTL *******************************/
	initialize_dtc(INCH_4, &TA0CCR1); // or &TA1CCR1 or &TA1CCR2

/******
 *
 *    MAIN LOOP (THERE ISN'T ONE)
 *
 ******/


	// go to sleep with interrupts enabled
	__bis_SR_register(LPM1_bits|GIE);

	// never return
	return 0;
}

/******
 *
 *    INTERRUPTS
 *
 ******/
#pragma vector=TIMER0_A0_VECTOR
__interrupt void timer0 (void)
{
	// insert timer0 code here

	// 1. turn off digits
	P1OUT &= ~(BIT7|BIT5|BIT1);

	// 2. Convert ADC10_Value to 8 bit number (0 - 255)
	hexValue = *ADC10_value >> 2;

	// 3. turn on next digit
	if (cursor == 0) {
		hexValue &= 0b00001111;
		P2OUT = P2OUT_Patterns[hexValue];
		P1OUT = P1OUT_Patterns[hexValue];
		P1OUT |= BIT7;
	} else if (cursor == 1) {
		hexValue >>= 4;
		P2OUT = P2OUT_Patterns[hexValue];
		P1OUT = P1OUT_Patterns[hexValue];
		P1OUT |= BIT5;
	} else {
		P2OUT = colorPatterns[color];
		P1OUT |= BIT1;
		P1OUT &= ~(BIT0);	// Always set P1.0 to 0 for color indicator
	}

	// move ahead cursor for next time
	cursor++;
	cursor %= 3;
	// 4. leave the interrupt
}

#pragma vector=TIMER1_A0_VECTOR
__interrupt void timer1 (void)
{
	printCounter++; // Increment print counter

	if (printCounter > 1000) {
		// Get values from TAxCCRx for each color and bitshift by 7 to leave 3 MSB
		unsigned char red = TA0CCR1 >> 7;
		unsigned char green = TA1CCR1 >> 7;
		unsigned char blue = TA1CCR2 >> 7;

		// Calculate index for color array
		int index = red + (green*8) + (blue*64);

       		// print out current color
		cio_printf("%s\n\r", colors[index]);
		printCounter = 0; // reset print counter
	}
	
}

#pragma vector=PORT1_VECTOR
__interrupt void button (void)
{
	// insert button code here
	if (color == 0) {
		initialize_dtc(INCH_4, &TA1CCR1);
		ADC10_value = &TA1CCR1; // Point ADC10_value to TA1CCR1
	} else if (color == 1) {
		initialize_dtc(INCH_4, &TA1CCR2);
		ADC10_value = &TA1CCR2; // Point ADC10_value to TA1CCR2
	} else {
		initialize_dtc(INCH_4, &TA0CCR1);
		ADC10_value = &TA0CCR1; // Point ADC10_value to TA0CCR1
	}

	// Increment color
	color ++;
	color %= 3; // If three, go to 0

	// button debounce routine
	while (!(BIT3 & P1IN)) {} // is finger off of button yet?
	__delay_cycles(32000);    // wait 32ms
	P1IFG &= ~BIT3;           // clear interrupt flag
}
