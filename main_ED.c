#include "bsp.h"
#include "mrfi.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "nwk_types.h"
#include "nwk_api.h"
#include "nwk_frame.h"
#include "nwk.h"

//EZ430 in remote
//initialization functions
static void config_ports();

//interrupt handlers
__interrupt void ADC10_ISR(void);
__interrupt void Timer_A (void);

/* Temperature offset set at production */
volatile int * tempOffset = (int *)0x10F4;

/* Work loop semaphores */
static volatile uint8_t sSelfMeasureSem = 0;

//functions
void handleButton();


void main (void)
{
	config_ports();

	BSP_Init();

	SMPL_Init(0);

	uint8_t msg[2];

	while (1)
	{
		/* Go to sleep, waiting for interrupt every second to acquire data */

		__bis_SR_register(LPM0_bits);

		/* Time to measure */
		if (sSelfMeasureSem) {
			volatile long temp;
			int degC;
			int results[2];

			/* Get temperature */
			ADC10CTL1 = INCH_10 + ADC10DIV_4;       // Temp Sensor ADC10CLK/5
			ADC10CTL0 = SREF_1 + ADC10SHT_3 + REFON + ADC10ON + ADC10IE + ADC10SR;
			/* Allow ref voltage to settle for at least 30us (30us * 8MHz = 240 cycles)
			 * See SLAS504D for settling time spec
			 */
			__delay_cycles(240);
			ADC10CTL0 |= ENC + ADC10SC;             // Sampling and conversion start
			__bis_SR_register(CPUOFF + GIE);        // LPM0 with interrupts enabled
			results[0] = ADC10MEM;                  // Retrieve result
			ADC10CTL0 &= ~ENC;

			/* Get voltage */
			ADC10CTL1 = INCH_11;                     // AVcc/2
			ADC10CTL0 = SREF_1 + ADC10SHT_2 + REFON + ADC10ON + ADC10IE + REF2_5V;
			__delay_cycles(240);
			ADC10CTL0 |= ENC + ADC10SC;             // Sampling and conversion start
			__bis_SR_register(CPUOFF + GIE);        // LPM0 with interrupts enabled
			results[1] = ADC10MEM;                  // Retrieve result

			/* Stop and turn off ADC */
			ADC10CTL0 &= ~ENC;
			ADC10CTL0 &= ~(REFON + ADC10ON);

			/* oC = ((A10/1024)*1500mV)-986mV)*1/3.55mV = A10*423/1024 - 278
			 * the temperature is transmitted as an integer where 32.1 = 321
			 * hence 4230 instead of 423
			 */
			temp = results[0];
			degC = ((temp - 673) * 4230) / 1024;
			if( (*tempOffset) != 0xFFFF )
			{
				degC += (*tempOffset);
			}

			/* message format,  UB = upper Byte, LB = lower Byte
	      -------------------------------
	      |degC LB | degC UB |  volt LB |
	      -------------------------------
	         0         1          2
			 */
			temp = results[1];

			msg[0] = degC&0xFF;
			msg[1] = (degC>>8)&0xFF;


			/* Get radio ready...awakens in idle state */
			SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_AWAKE, 0);


			/* No AP acknowledgement, just send a single message to the AP */
			SMPL_Send(SMPL_LINKID_USER_UUD, msg, sizeof(msg));

			/* Put radio back to sleep */
			SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SLEEP, 0);

			/* Done with measurement, disable measure flag */
			sSelfMeasureSem = 0;
		}
	}

}



static void config_ports(){

	//4 pins buttons IN
	//2.0-2.4
	//4.3-4.6

	P2DIR = 0x00;
	P2REN = 0x0F;
	P2OUT = 0x0F;//pullup
	P2IES = 0x0F;
	P2IE = 0x0F;//pins 0,1,2,3 buttons

	_BIS_SR(GIE);//enable interrupts could also use _EINT();
}

void handleButton(int whatButton){

	unsigned char msg[2];

	msg[0]=2;
	msg[1]=whatButton;


	SMPL_Send(SMPL_LINKID_USER_UUD, msg, sizeof(msg));


}

/*------------------------------------------------------------------------------
 *interrupt service routines
------------------------------------------------------------------------------*/

#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void)
{
	LPM0_EXIT;        // Clear CPUOFF bit from 0(SR)
}

#pragma vector=TIMERA0_VECTOR
__interrupt void Timer_A (void)
{
	sSelfMeasureSem = 1;
	LPM0_EXIT;        // Clear LPM3 bit from 0(SR)
}

#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void){

	unsigned int i;
	for(i=0;i<50000;i++);//debounce for 50ms

	SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_AWAKE, 0);

	if((P2IN & 0x01) == 0x00){//p2.0 mode
		handleButton(1);
	}
	if((P2IN & 0x02) == 0x00){//p2.1 hour
		handleButton(2);
	}
	if((P2IN & 0x04) == 0x00){//p2.2 min
		handleButton(3);
	}
	if((P2IN & 0x08) == 0x00){//p2.3 snooze
		handleButton(4);
	}

	SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SLEEP, 0);

	P2IFG &= 0x00; //clear interrupt flag

}
