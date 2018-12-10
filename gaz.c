#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "onewire.h"
#include "ds18x20.h"

uint8_t TIMER_ONESEC = 0;
uint8_t TIMER_SEC = 0;
uint8_t TIMER_MINUTE = 0;
uint8_t TIMER_CORRECT = 0;

int temp=0;

void LED_ON(void){ PORTD&=~(1<<4); }
void LED_OFF(void){ PORTD|=(1<<4); }
void GAZ_ON(void){ PORTD&=~(1<<5); LED_ON();}
void GAZ_OFF(void){ PORTD|=(1<<5); LED_OFF();}

//DS18B20
#define MAXSENSORS 2
uint8_t gSensorIDs[MAXSENSORS][OW_ROMCODE_SIZE];
uint8_t nSensors;
uint8_t curSensors=0;
int DS18B20SensorsValues[MAXSENSORS];
int FLAG_DS18B20=1;
int i;
//DS18B20
uint8_t search_sensors(void){
	uint8_t i;
	uint8_t id[OW_ROMCODE_SIZE];
	uint8_t diff;//, nSensors;
	nSensors = 0;
	for( diff = OW_SEARCH_FIRST; 
		diff != OW_LAST_DEVICE && nSensors < MAXSENSORS ; )
	{
		DS18X20_find_sensor( &diff, &id[0] );
		if( diff == OW_PRESENCE_ERR ) {
			break;
		}
		if( diff == OW_DATA_ERR ) {
			break;
		}
		
		for (i=0;i<OW_ROMCODE_SIZE;i++)
			gSensorIDs[nSensors][i]=id[i];
		nSensors++;
	}	
	return nSensors;
}

// Timer
ISR(TIMER1_OVF_vect) { // 100 Hz
	TCNT1 = 0x10000 - (F_CPU/1024/100);

	// Time correction
	TIMER_CORRECT++;
	if (TIMER_CORRECT > 13) {
		TIMER_CORRECT = 0;
		TCNT1--;
	}
	TIMER_ONESEC++; //milisec

	if (TIMER_ONESEC > 99) {
		TIMER_ONESEC = 0;
		TIMER_SEC++; //sec

		if (TIMER_SEC > 59) {
			TIMER_SEC = 0;
			TIMER_MINUTE++; //minute
			FLAG_DS18B20=1;
		}
	}
}

int main(void){
	// Watchdog
	//wdt_enable(WDTO_1S);

	// set the clock speed to 8MHz
	// set the clock prescaler. First write CLKPCE to enable setting of clock the
	// next four instructions.
	CLKPR=(1<<CLKPCE);
	CLKPR=0; // 8 MHZ
	_delay_loop_1(0); // 60us

	TIMSK = (1<<TOIE1);
	TCCR1B = ((1<<CS10) | (1<<CS12));
	TCNT1 = 0x10000 - (F_CPU/1024/100); 
	sei();

	DDRD|=(1<<(5));
	DDRD|=(1<<(4));

	//DS18B20
	uint8_t subzero, cel=0, cel_frac_bits;
	nSensors = search_sensors();
	for (i=0;i<nSensors;i++) {
		DS18X20_start_meas( DS18X20_POWER_EXTERN, &gSensorIDs[i][0] );
	}

	LED_ON(); _delay_ms(500); wdt_reset();
	//LED_OFF(); _delay_ms(500); wdt_reset();
	GAZ_ON(); _delay_ms(500); wdt_reset();
	//GAZ_OFF(); _delay_ms(500); wdt_reset();

	while(1){
		// reset timer of Watchdog
		wdt_reset();

                /////////////////////////////////
		if (FLAG_DS18B20==1) {
			if (nSensors > 0) {
				if ( DS18X20_read_meas( &gSensorIDs[curSensors][0], &subzero, &cel, &cel_frac_bits) == DS18X20_OK ) {
					DS18B20SensorsValues[curSensors] = (int)cel;
					if (subzero == 1)
						DS18B20SensorsValues[curSensors] = -1*DS18B20SensorsValues[curSensors];
				}

				curSensors++;
				if (curSensors >= nSensors)
					curSensors = 0;
				if ( DS18X20_start_meas( DS18X20_POWER_EXTERN, &gSensorIDs[curSensors][0] ) == DS18X20_OK ) {
				}
			}
			FLAG_DS18B20=0;
		}

		//////////////////////////////////
		temp = DS18B20SensorsValues[0];

		if (TIMER_MINUTE == 10 || temp >= 24) GAZ_OFF();
		if (TIMER_MINUTE >= 30 && temp < 24) {
			GAZ_ON();
			TIMER_MINUTE = 0;
		}

		if (temp < 0) {
		        temp *= -1;
			LED_ON(); _delay_ms(2500);
			LED_OFF(); _delay_ms(3000); 
		};

		for(i = 1; i <= (temp/10); i++) {
        		LED_ON(); _delay_ms(350);
	        	LED_OFF(); _delay_ms(350);
		}
    
		_delay_ms(2000);
    
		for(i = 1; i <= (temp%10); i++) {
        		LED_ON(); _delay_ms(350);        
		        LED_OFF(); _delay_ms(350);        
		}
		_delay_ms(5000);
      
		return (0);
	}
}
