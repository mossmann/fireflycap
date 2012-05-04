#include <msp430g2452.h>
#include <stdint.h>
#include "waveform.h"

#define LEDS_OFF P2OUT = 0; P2DIR = 0xff
//#define LEDS_OFF P2DIR = 0
#define LED0_ON  P2DIR = 0; P2OUT = 0x02; P2DIR = 0x06
#define LED1_ON  P2DIR = 0; P2OUT = 0x04; P2DIR = 0x06
#define LED2_ON  P2DIR = 0; P2OUT = 0x08; P2DIR = 0x18
#define LED3_ON  P2DIR = 0; P2OUT = 0x10; P2DIR = 0x18
#define LED4_ON  P2DIR = 0; P2OUT = 0x04; P2DIR = 0x0c
#define LED5_ON  P2DIR = 0; P2OUT = 0x08; P2DIR = 0x0c
#define LED6_ON  P2DIR = 0; P2OUT = 0x02; P2DIR = 0x12
#define LED7_ON  P2DIR = 0; P2OUT = 0x10; P2DIR = 0x12
#define LED8_ON  P2DIR = 0; P2OUT = 0x04; P2DIR = 0x14
#define LED9_ON P2DIR = 0; P2OUT = 0x10; P2DIR = 0x14
#define LED10_ON P2DIR = 0; P2OUT = 0x02; P2DIR = 0x0a
#define LED11_ON P2DIR = 0; P2OUT = 0x08; P2DIR = 0x0a

const uint8_t led_dir[] = {
		0x06, 0x06, 0x18, 0x18,
		0x0c, 0x0c, 0x12, 0x12,
		0x14, 0x14, 0x0a, 0x0a};
const uint8_t led_out[] = {
		0x02, 0x04, 0x08, 0x10,
		0x04, 0x08, 0x02, 0x10,
		0x04, 0x10, 0x02, 0x08};

#define ONE_SECOND 1500

#define NUM_SLOTS 4

volatile uint16_t lfsr = 0x6d61;

void set_led(uint8_t led)
{
	P2OUT = 0;
	P2DIR = led_dir[led];
	P2OUT = led_out[led];
}

void sleep(uint16_t duration)
{
	TA0CCR0 = (TAR + duration) & 0xffff;
	TA0CCTL0 |= CCIE;
	LPM0;
	TA0CCTL0 &= ~CCIE;
}

/*
 * call with one of:
 * WDT_ADLY_1000: approximately 6.4 s
 * WDT_ADLY_250: approximately 800 ms
 * WDT_ADLY_16: approximately 100 ms
 * WDT_ADLY_1_9: approximately 12.5 ms
 */
void long_sleep(uint16_t duration)
{
	WDTCTL = duration;
	IE1 |= WDTIE;
	LPM3;
}

/* courtesy of wikepedia */
inline void advance_lfsr(void)
{
	lfsr = (lfsr >> 1) ^ (-(lfsr & 1) & 0xB400);
}

/* PRNG */
uint16_t rand16(void)
{
	advance_lfsr();
	advance_lfsr();
	return lfsr;
}

uint8_t rand8(void)
{
	advance_lfsr();
	advance_lfsr();
	return lfsr & 0xff;
}

uint16_t rand(uint8_t bits)
{
	switch (bits) {
	case 16:
		return rand16();
	case 15:
		return rand16() & 0x7fff;
	case 14:
		return rand16() & 0x3fff;
	case 13:
		return rand16() & 0x1fff;
	case 12:
		return rand16() & 0x0fff;
	case 11:
		return rand16() & 0x07ff;
	case 10:
		return rand16() & 0x03ff;
	case 9:
		return rand16() & 0x01ff;
	case 8:
		return rand16() & 0x00ff;
	case 7:
		return rand16() & 0x007f;
	case 6:
		return rand16() & 0x003f;
	case 5:
		return rand16() & 0x001f;
	case 4:
		return rand16() & 0x000f;
	case 3:
		return rand16() & 0x0007;
	case 2:
		return rand16() & 0x0003;
	case 1:
		return rand16() & 0x0001;
	default:
		return 0;
	}
}

/* call with a randomly selected 1 or 0 */
void more_entropy(uint8_t input)
{
	if ((input) && (lfsr != 0xffff))
		lfsr = ~lfsr;
	advance_lfsr();
}

uint16_t pv_voltage(void)
{
	uint16_t adc_val;

	P1REN &= 0xfd;
	ADC10CTL0 = ADC10SHT_0 | ADC10SR | ADC10ON | ADC10IE;
	ADC10CTL1 = INCH_1;
	ADC10AE0 |= 0x02;

	ADC10CTL0 |= ENC | ADC10SC;
	LPM0;
	adc_val = ADC10MEM;

	/* feed the PRNG with the LSB */
	more_entropy(adc_val & 1);

	return adc_val;
}

void test_leds(void)
{
	uint16_t i;

	for (i = 0; i < 12; i++) {
		set_led(i);
		long_sleep(WDT_ADLY_1_9);
	}
	for (i = 0; i < 0xffff; i++) {
		set_led(i % 12);
	}
	LEDS_OFF;
}

void light_show(void)
{
	uint8_t i, j;
	uint8_t rand3, next;

	/*
	 * Flies are numbered 0 through 11.  There are four interleaved time slots.
	 * Zero or one fly may be active during a slot for a maximum of four flies
	 * active simultaneously.
	 */

	/* the fly that is selected for each slot */
	uint8_t fly[NUM_SLOTS] = {0, 3, 6, 9};

	/* the current phase (position within the waveform) for each slot */
	uint8_t phase[NUM_SLOTS] = {0xff, 0xff, 0xff, 0xff};

	/* the P2DIR setting for the fly in each slot */
	uint8_t dir[NUM_SLOTS] = {led_dir[0], led_dir[3], led_dir[6], led_dir[9]};

	/* the P2OUT setting for the fly in each slot */
	uint8_t out[NUM_SLOTS] = {led_out[0], led_out[3], led_out[6], led_out[9]};

	uint16_t delay[NUM_SLOTS] = {0, 0, 0, 0};
	uint16_t sleep_time;

	while (1) {
		/* one PWM period for four interleaved slots */
		for (i = 0; i < 0xff; i++) {
			for (j = 0; j < NUM_SLOTS; j++) {
				sleep(10);
				if (phase[j] == 0xff)
					/* mark this waveform as finished */
					delay[j] = 1;
				P2OUT = 0;
				if (waveform[phase[j]] > i) {
					/* turn on an LED */
					P2DIR = dir[j];
					P2OUT = out[j];
				} else {
					/* all LEDs off */
					P2DIR = 0;
					P2OUT = 0;
				}
			}
			/* about 53 microseconds elapsed */
		}
		for (j = 0; j < NUM_SLOTS; j++) {
			if (delay[j]) {
				if (phase[j]) {
					/* waveform finished but we haven't reset for next time yet */

					/* select a random delay before next fly in this slot */
					delay[j] = 1 + rand(11) + rand(11);  //12 is about 6 flies per minute

					/* select a random LED */
					rand3 = rand8() & 0x7;
					next = 0;
					while ((next == fly[0]) || (next == fly[1]) ||
							(next == fly[2]) || (next == fly[3]) || rand3--)
						next++;
					fly[j] = next;

					dir[j] = 0;
					out[j] = 0;
					phase[j] = 0;
				}
				if (--delay[j] == 0) {
					/* time's up! */
					dir[j] = led_dir[fly[j]];
					out[j] = led_out[fly[j]];
					pv_voltage(); /* just for the entropy */
				}
			} else {
				phase[j]++;
			}
		}
		/* about 13.5 ms elapsed, so about 75 Hz PWM */

		/* if we're waiting on all four slots, go into deep sleep */
		sleep_time = 0xffff;
		for (j = 0; j < NUM_SLOTS; j++)
			if (delay[j] < sleep_time) sleep_time = delay[j];
		if (sleep_time) {
			LEDS_OFF; /* important GPIO configuration for low power */
			for (j = 0; j < NUM_SLOTS; j++)
				delay[j] = delay[j] + 1 - sleep_time;
			while(sleep_time--)
				/* this is close to the right duration but is uncalibrated */
				long_sleep(WDT_ADLY_1_9);
		}
	}
}

void adc_test(void)
{
	while (1) {
		if (pv_voltage() < 0x80)
			set_led(0);
		else
			set_led(1);
		sleep(1000);
	}
}

int main(void)
{
	/* stop watchdog timer */
	WDTCTL = WDTPW | WDTHOLD;

	/* switch to 12 MHz internal oscillator */
	DCOCTL = 0;
	BCSCTL1 = CALBC1_12MHZ;
	DCOCTL = CALDCO_12MHZ;

	/* Port1 all inputs with pull-ups */
	P1DIR = 0;
	P1REN = 0xff;
	
	/* configure ACLK to use internal VLO/2 */
	BCSCTL1 |= DIVA_1;
	BCSCTL3 |= LFXT1S_2;

	/* set SMCLK to MCLK/8 */
	BCSCTL2 |= DIVS_3;

	/* configure Timer_A */
	TA0CTL = TASSEL_2 | MC_2; /* start timer on SMCLK */
	__eint();

	test_leds();

	pv_voltage(); /* just for the entropy */
	pv_voltage(); /* just for the entropy */
	pv_voltage(); /* just for the entropy */
	pv_voltage(); /* just for the entropy */
	pv_voltage(); /* just for the entropy */
	pv_voltage(); /* just for the entropy */
	pv_voltage(); /* just for the entropy */
	pv_voltage(); /* just for the entropy */
	light_show();
	//adc_test();

	/* just in case we get this far */
	while (1) test_leds();
}

void __attribute__((interrupt(WDT_VECTOR))) WDT_ISR(void)
{
	ADC10CTL0 &= ADC10IFG;
	LPM3_EXIT;
}

void __attribute__((interrupt(TIMER0_A0_VECTOR))) TIMER0_A0_ISR(void)
{
	TA0CCTL0 &= ~CCIFG;
	LPM0_EXIT;
}

void __attribute__((interrupt(ADC10_VECTOR))) ADC10_ISR(void)
{
	LPM0_EXIT;
}
