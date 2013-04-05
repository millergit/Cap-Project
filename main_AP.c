// EZ430 On Clock

#include "bsp.h"
#include "mrfi.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "nwk_types.h"
#include "nwk_api.h"
#include "nwk_frame.h"
#include "nwk.h"


#define SENT_LENGTH 3;

//interrupt handlers
__interrupt void ADC10_ISR(void);
__interrupt void Timer_A (void);


/* received message handler */
static void processMessage(linkID_t, uint8_t, uint8_t);

//initialization functions
static void config_interrupts();
static void config_ports();
static volatile unsigned int timerCount = 0;

//clock variables
unsigned int temp, alarm, mode, button, whatButton;


//clock fuctions
void doAction();
void initVars();

/* wireless items */
static uint8_t callBack(linkID_t);
static volatile uint8_t UUDframesem = 0;


void main (void)
{
	WDTCTL = WDTPW + WDTHOLD;//stop watchdog timer
	config_interrupts();
	config_ports();
	initVars();

	BSP_Init();//board specific hardware initialization

	SMPL_Init(callBack);//simpliciTI initialization w/callback



	while(1){//main program function


		if(sUUDFrameSem)//if message is waiting
		{
			uint8_t msg[SENT_LENGTH], len;

			if (SMPL_SUCCESS == SMPL_Receive(SMPL_LINKID_USER_UUD, msg, &len))
			{
				processMessage(SMPL_LINKID_USER_UUD, msg, len);//retrieve message

				BSP_ENTER_CRITICAL_SECTION(intState);
				sUUDFrameSem--;//reset semaphore
				BSP_EXIT_CRITICAL_SECTION(intState);
			}
		}

		doAction();
		setTubes();
		display();

		__bis_SR_register(LPM0_bits + GIE);//sleep in heavenly peace
	}

}
//hour, min, mode, snooze

void doAction(){

	if (button){
		switch(whatButton){
		case 1://mode
			break;
		case 2://hour
			break;
		case 3://min
			break;
		case 4://snooze
			break;
		default:
			break;
		}
		button=0;
	}
}

void initVars(){
	month = 1;
	day = 1;
	year = 2000;
	sec,min,pm = 0;
	hour = 12;
	temp =0;
	alarm=0;
	mode=0;
}

static void config_interrupts(){


}

static void config_ports(){

	//3 pins buttons OUT
	//4 pins info xfer OUT
	//1 pin snooze IN

	_BIS_SR(GIE);//enable interrupts could also use _EINT();
}

static uint8_t callBack(linkID_t lid)
{

	if(lid==SMPL_LINKID_USER_UUD){//only handles unlinked messages.
		UUDframesem++;
	}

	return 0;//leave frame to be read by application.
}

static void processMessage(linkID_t lid, uint8_t msg[SENT_LENGTH], uint8_t len)
{
	/* do something useful */
	if (len)
	{
		BSP_TOGGLE_LED1();

	}//first digit indicates what the second digit is
	if(msg[0]=1){//if temperature
		ext_temp=msg[1];
	}
	else if(msg[0]=2){//if button press
		button=1;
		whatButton=msg[1];
	}
	return;
}


#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void){

	for(i=0;i<50000;i++){}//debounce for 50ms

	if((P2IN & 0x07) == 0x06){//p2.0
		button=1;
		whatButton=1;
	}
	else if((P2IN & 0x07) == 0x05){//p2.1
		button=1;
		whatButton=2;
	}
	else if((P2IN & 0x07) == 0x03){//p2.2
		button=1;
		whatButton=3;
	}

	temp[0]=;

	P2IFG &= 0x00; //clear interrupt flag
	_bic_SR_register_on_exit(LPM3_bits);//clear flag
 }
