// BRAIN MODE

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
__interrupt void Port_2(void);
__interrupt void Port_4(void);


/* received message handler */
static void processMessage(linkID_t, uint8_t, uint8_t);

//initialization functions
static void config_ports();
static void config_interrupts();

//clock variables
unsigned int whatButton, buttonSim;


//fuctions
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
	initVars();

	BSP_Init();//board specific hardware initialization

	SMPL_Init(callBack);//simpliciTI initialization w/callback



	while(1){//main program function


		if(sUUDFrameSem)//if message is waiting
		{
			uint8_t msg[4], len;

			if (SMPL_SUCCESS == SMPL_Receive(SMPL_LINKID_USER_UUD, msg, &len))
			{
				processMessage(SMPL_LINKID_USER_UUD, msg, len);//retrieve message

				BSP_ENTER_CRITICAL_SECTION(intState);
				sUUDFrameSem--;//reset semaphore
				BSP_EXIT_CRITICAL_SECTION(intState);
			}
		}


		__bis_SR_register(LPM0_bits + GIE);//sleep in heavenly peace
	}

}

void initVars(){
	buttonSim=0;
	whatButton=0;
}

static void config_ports(){

	//3 pins buttons OUT
	//4 pins info xfer OUT
	//1 pin snooze IN

	//2.0-2.4
	//4.3-4.6


	_BIS_SR(GIE);//enable interrupts could also use _EINT();
}

static void config_interrupts(){

	TA0CTL = TASSEL_1 + TACLR + MC_1; //ACLK
	CCTL0 = CCIE; // CCR0 interrupt enabled
	CCR0 = 0x04AF; //12000/(1199+1) = interrupt @ 10Hz

}

/*------------------------------------------------------------------------------
 *functional routines
------------------------------------------------------------------------------*/

void handleButton(){


	switch(whatButton){
	case 1://mode
		//simulate 110

		break;
	case 2://hour
		//simulate 101

		break;
	case 3://min
		//simulate 011

		break;
	case 4://snooze
		//simulate 0 press out specific pin

		break;
	default:
		break;
	}
	buttonSim=1;//set to be cleared in 100ms.

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

void sendTemp(){

	//UART
	//p3.4 Tx
	//p3.5 Rx
	P3SEL |= 0x30; // P3.4,5 = USART0 TXD/RXD
	ME1 |= UTXE0 + URXE0; // Enable USART0 TXD/RXD
	U0CTL |= CHAR; // 8-bit character
	U0TCTL |= SSEL3; // UCLK =SMCLK 8MHz
	U0BR0 = 0xD0; // 8MHz/38400BAUD Fit baud to 16 bits - 208.333 -> 11010000...
	U0BR1 = 0x00;
	U0MCTL = 0x11; /* uart0 8000000Hz 38406bps */
	U0CTL &= ˜SWRST; // Initialize USART state machine
	IE1 |= URXIE0 + UTXIE0; // Enable USART0 RX/TX interrupt


	 while (!(IFG2&UCA0TXIFG));                 // USCI_A0 TX buffer ready?
	  UCA0TXBUF = 'H';                     // TX -> RXed character


}

/*------------------------------------------------------------------------------
 *interrupt service routines
------------------------------------------------------------------------------*/

#pragma vector=TIMERA0_VECTOR
__interrupt void Timer_A(void){

	//wake each 1/10th second and send temp data
	sendTemp();

	if(buttonSim){//button sim clear
		buttonSim++;
		if(buttonSim>2){
			P1OUT |= 0xFF;
		}
	}

}


#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void){

	unsigned int i;
	for(i=0;i<50000;i++);//debounce for 50ms

	if((P2IN & 0x01) == 0x00){//p2.0 mode
		handleButton(1);
	}
	if((P2IN & 0x02) == 0x00){//p2.1 hour
		handleButton(2);
	}
	if((P2IN & 0x04) == 0x00){//p2.2 min
		handleButton(3);
	}

	P2IFG &= 0x00; //clear interrupt flag
	_bic_SR_register_on_exit(LPM3_bits);//clear flag
}

#pragma vector=PORT4_VECTOR
__interrupt void Port_4(void){

	//maybe debounce
	if((P4IN & 0x08) == 0x00){//p4.3
		sendTemp();
	}

	P4IFG &= 0x00; //clear interrupt flag

	_bic_SR_register_on_exit(LPM3_bits);//clear flag
}
