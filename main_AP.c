#include "bsp.h"
#include "mrfi.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "nwk_types.h"
#include "nwk_api.h"
#include "nwk_frame.h"
#include "nwk.h"


//EZ430 on clock


//interrupt handlers
__interrupt void Timer_A (void);


/* received message handler */
static void processMessage(linkID_t, uint8_t [2], uint8_t);

//initialization functions
static void config_ports();
static void config_interrupts();

//clock variables
unsigned int whatButton, buttonSim, tempCnt;
unsigned char sim,ext_temp;


//functions
void handleButton(int whatButton);
void initVars();
void sendTemp();

/* wireless items */
static uint8_t callBack(linkID_t);
static volatile uint8_t UUDframesem = 0;


void main (void)
{
	WDTCTL = WDTPW + WDTHOLD;//stop watchdog timer
	config_ports();
	config_interrupts();
	initVars();

	BSP_Init();//board specific hardware initialization

	SMPL_Init(callBack);//simpliciTI initialization w/callback

	bspIState_t intState;

	while(1){//main program function


		if(UUDframesem)//if message is waiting
		{
			uint8_t msg[2], len;//define msg[0]=type msg[1]=payload

			if (SMPL_SUCCESS == SMPL_Receive(SMPL_LINKID_USER_UUD, msg, &len))
			{
				processMessage(SMPL_LINKID_USER_UUD, msg, len);//retrieve message

				BSP_ENTER_CRITICAL_SECTION(intState);
				UUDframesem--;//reset semaphore
				BSP_EXIT_CRITICAL_SECTION(intState);
			}
		}


		__bis_SR_register(LPM0_bits + GIE);//sleep in heavenly peace
	}

}

void initVars(){
	buttonSim=0;
	whatButton=0;
	ext_temp = 72;
	tempCnt =0;
	sim=0xFF;
}

static void config_ports(){

	//3 pins buttons OUT
	//1 pin snooze OUT

	//p2.0-2.2 button OUT
	//p2.3 snooze OUT
	//p3.4,3.5 uart

	P2DIR = 0x0F;
	P2OUT = 0x07;

	//USCI  UART config
	P3SEL |= 0x30;//p3.4 Tx p3.5 Rx
	UCA0CTL1 |= UCSSEL_2;// UCLK =SMCLK 8MHz
	UCA0BR0 = 0xD0;// 8MHz/38400BAUD Fit baud to 16 bits - 208.333 -> 11010000...
	UCA0BR1 = 0x00;
	UCA0MCTL = 0x11; /* uart0 8000000Hz 38406bps */
	UCA0CTL1 &= ~UCSWRST;//Initialize USART state machine

	_BIS_SR(GIE);//enable interrupts could also use _EINT();
}

static void config_interrupts(){

	TA0CTL = TASSEL_1 + TACLR + MC_1; //ACLK
	CCTL0 = CCIE; // CCR0 interrupt enabled
	CCR0 = 1199; //12000/(1199+1) = interrupt @ 10Hz

}

/*------------------------------------------------------------------------------
 *functional routines
------------------------------------------------------------------------------*/

void handleButton(int whatButton){

	sim=0xFF;

	switch(whatButton){
	case 1://mode
		//simulate 1110
		sim &= 0x0E;
		break;
	case 2://hour
		//simulate 1101
		sim &= 0x0D;
		break;
	case 3://min
		//simulate 1011
		sim &= 0x0B;
		break;
	case 4://snooze
		//simulate 0111
		sim &= 0x07;
		break;
	default:
		break;
	}

	P2OUT = sim;
	buttonSim=1;
}


static uint8_t callBack(linkID_t lid)
{

	if(lid==SMPL_LINKID_USER_UUD){//only handles unlinked messages.
		UUDframesem++;
	}

	return 0;//leave frame to be read by application.
}

static void processMessage(linkID_t lid, uint8_t msg[2], uint8_t len)
{

	//do something useful
	if (len)
	{
		BSP_TOGGLE_LED1();

	}//first digit indicates what the second digit is
	if(msg[0]==1){//if temperature
		ext_temp=msg[1];
	}
	else if(msg[0]==2){//if button press
		whatButton=msg[1];
		handleButton(whatButton);
	}
	return;
}

void sendTemp(){

	//utilize uart tx
	//test code
	while (!(IFG2&UCA0TXIFG));// USCI_A0 TX buffer ready?
	UCA0TXBUF = ext_temp;// TX -> character

}

/*------------------------------------------------------------------------------
 *interrupt service routines
------------------------------------------------------------------------------*/


#pragma vector=TIMERA0_VECTOR
__interrupt void Timer_A(void){

	//wake each 1/10 second and send temp data
	tempCnt++;
	if(tempCnt==10){
		sendTemp();
		tempCnt=0;
	}
	if(buttonSim){
		buttonSim++;
		if(buttonSim==2){
			P2OUT |= 0x07;//clear simulated button
			buttonSim=0;
		}
	}


	LPM0_EXIT;

}




