#include <msp430.h>

/*
 * main.c
 */

//MSP430 NUMBER 0

static void config_clocks();
static void config_interrupts();
static void config_ports();

//interrupt handlers
__interrupt void Port_2(void);
__interrupt void Timer0_A0 (void);
__interrupt void USCI0RX_ISR(void);

//clock variables
unsigned int month, day, year, sec, min, hour, pm, mode, whatButton;
unsigned int almH, almM, almPm, almWatch, almTrigger;
unsigned char temp;

//Display items
unsigned int tube[6];//1 is hour
unsigned int tubeSel, digit;
unsigned char tubeTable[10]={0x0F,0x1F,0x2F,0x3F,0x4F,0x5F,0x6F,0x7F,0x8F,0x9F};
unsigned char nmizTable[6]={0xC7,0xCF,0xD7,0xDF,0xE7,0xEF};

//clock functions
void handleButton(int whatButton);
void setTubes();
void display();
void clockTick();
void getTemp();
void checkAlarm();
void initVars();

int main(void) {
	WDTCTL = WDTPW + WDTHOLD;	// Stop watchdog timer

	volatile unsigned int i;
	for(i=0;i<0xFFFF;i++);//delay brain so that others may initialize first

	config_clocks();
	config_interrupts();
	config_ports();
	initVars();

	while(1){

		setTubes();

		for(tubeSel=0;tubeSel<6;tubeSel++){
			display();
		}

		__bis_SR_register(LPM1_bits + GIE);//sleep in heavenly peace
	}
}


//hour, min, mode, snooze, alarm on/off

static void config_clocks(){

	//for no crystal
	DCOCTL = CALDCO_1MHZ;//DCO frequency step set
	BCSCTL1 = CALBC1_1MHZ;//basic lock system control reg1 freq. range set

	BCSCTL2 = 0x06;//SM clock is DO clock/8=125000Hz; main is dco

	BCSCTL3 |= LFXT1S_2; // LFXT1 = VLO

	//if crystal
	//BCSCTL3 = 0b00000000;//Aclk is crystal, bits 3&2 determine capacitor select.
	//1,6,10,12.5pF


}

static void config_interrupts(){
	//no crystal, use more accurate DO clock
	TA0CTL = TASSEL_2 + TACLR + MC_1 + ID_3; //SMCLK, clear TAR, count up, SM/8 = 15625Hz;
	CCTL0 = CCIE; // CCR0 interrupt enabled
	CCR0 = 15624; //15625/(15624+1) = interrupt @ 1Hz

	//crystal
	//CCR0 = 0x7FFF; //32768/(32767) = interrupt @ 1Hz


}

void initVars(){
	month = 1;
	day = 1;
	year = 2000;
	sec=0;
	min=0;
	pm=0;
	almM=0;
	almPm=0;
	hour=12;
	almH = 12;
	mode=0;
	almWatch=0;
	tubeSel = 6;
	digit=0;
	whatButton=0;
	temp = 69;
	int i;
	for(i=0;i<6;i++){tube[i]=0;}
}

static void config_ports(){
	//p1.0 is pm led OUT
	//p1.1 Rx
	//p1.2 Tx
	//p1.3 NOTHING	
	//p1.4-1.7 are X OUT

	//p2.0-2.2 are modes IN
	
	//p2.3 is Z OUT
	//p2.4, p2.5 2 to 4 converter OUT
	//p2.6 NOTHING
	//p2.7 alarm OUT.

	P1DIR = 0xF1;//0b11110001

	P2DIR = 0xB8;//0b1011 1000
	P2OUT = 0xB7;//0b1011 0111  2-4 start NMI in gnd//pullup resistor for inputs
	P2REN = 0x07;//0b0000 0111 enable resistor
	P2SEL = 0x00;
	P2IES = 0x07;//0b0000 0111    high to low transition to generate

	//config UART
	P1SEL = BIT1 + BIT2 ;// P1.1 = RXD, P1.2=TXD
	P1SEL2 = BIT1 + BIT2;
	UCA0CTL1 |= UCSSEL_2;// SMCLK
	UCA0BR0 = 0x1A;// 1MHz/38400BAUD Fit baud to 16 bits - 26.042 -> 00011010...
	UCA0BR1 = 0x00;/* uart0 1000000Hz 38461bps */
	UCA0MCTL = 0x00;// Modulation UCBRSx = 1
	UCA0CTL1 &= ~UCSWRST;// **Initialize USCI state machine**
	IE2 |= UCA0RXIE;// Enable USCI_A0 RX interrupt
	//end UART
	
	P2IE |= 0x07;//interrupt enable 0000 0111
	P2IFG &= 0x00;//clear flag to start

	_BIS_SR(GIE);//enable interrupts could also use _EINT();
}


/*------------------------------------------------------------------------------
 *functional routines
------------------------------------------------------------------------------*/

void handleButton(int whatButton){


	switch(whatButton){
	case 1://mode
		mode++;
		mode=mode%4;
		if(mode==2){//if alm mode
			if(almPm){
				P1OUT |= 0x01;
			}
			else{
				P1OUT &= ~01;
			}
		}
		else{
			if(pm){
				P1OUT |= 0x01;
			}
			else{
				P1OUT &= ~01;
			}
		}
		break;
	case 2://hour
		switch(mode){
		case 0://time
			hour++;
			sec=0;
			if(hour==12){
				pm ^= 1;
			}
			if(hour==13){
				hour=1;
			}
			break;
		case 1://date
			month++;
			sec=0;
			day=1;
			if(month==13){
				month=1;
				year++;
				if(year>2099){year=2000;}
			}
			break;
		case 2://alarm set
			almH++;
			if(almH==12){
				almPm ^= 1;
			}
			if(almH==13){
				almH=1;
			}
			break;
		default:
			break;
		}
		break;
		case 3://min
			switch(mode){
			case 0://time
				min++;
				sec=0;
				if(min>59){
					min=0;
				}
				break;
			case 1://date
				day++;
				if(day>28){
					if(month==2){
						if((year%4==0)&&(day<30)){
							if(year%100==0){
								if((year%400)!=0){day=1;month++;}
							}
						}
						else{
							day=1;
							month++;
						}
					}
					else if((month==4||month==6||month==9||month==11)&&day==31){
						day=1;
						month++;
					}
					else if((month==1||month==3||month==5||month==7||month==8||month==10||month==12)&&day==32){
						day=1;
						month++;
					}
				}
				break;
			case 2://alarm set
				almM++;
				if(almM>59){
					almM=0;
				}
				break;
			default:
				break;
			}
			break;
	}
}

void setTubes(){

	switch(mode){
	case 0://time
		tube[0]=hour/10;
		tube[1]=hour%10;
		tube[2]=min/10;
		tube[3]=min%10;
		tube[4]=sec/10;
		tube[5]=sec%10;
		break;
	case 1://date
		tube[0]=month/10;
		tube[1]=month%10;
		tube[2]=day/10;
		tube[3]=day%10;
		tube[4]=(year/10)%10;
		tube[5]=year % 10;
		break;
	case 2://alarm set
		tube[0]=almH/10;
		tube[1]=almH%10;
		tube[2]=almM/10;
		tube[3]=almM%10;
		tube[4]=0;
		tube[5]=0;
		break;
	case 3://temp
		tube[0]=0;
		tube[1]=0;
		tube[2]=0;
		tube[3]=(temp/100)%10;
		tube[4]=(temp/10)%10;
		tube[5]=temp%10;
		break;
	default:
		break;
	}
}

void display(){

	//z is 1
	//2,3 are 2 to 4
	//x is 4-7
	unsigned char store1;
	unsigned char store2;

	store1 = P1OUT;//save port 1 state
	store1 |= 0xF0;//set pins to change to 1
	store2 = P2OUT;//save port 2 state
	store2 |= 0x38;

	digit = tube[tubeSel];//set digit

	store1 &= tubeTable[digit];//determine BCD out

	P1OUT = store1;//set X

	//send NMI with info
	store2 &= nmizTable[tubeSel];//determine nmi and Z

	P2OUT = store2;//set nmi and Z

	//hold for 10000 cycles
	int i;
	for(i=0;i<10000;i++){}
	P2OUT |= 0x30;//clear NMI


}



void checkAlarm(){

	if((hour==almH)&&(min==almM)&&(almPm==pm)&&(sec==1)&&(almWatch==0)){
		P2OUT &= ~0x80;//send alarm beacon on p1.7
		almWatch=1;
	}
	if(almWatch){
		almWatch++;
	}
	if(almWatch==3){//beacon off after 1s
			almWatch = 0;
			P2OUT |= 0x80;//clear alarm
	}

}

void clockTick(){
	sec++;
	if(sec==60){
		sec=0;
		min++;
	}
	if(min==60){
		min=0;
		hour++;
	}
	if((hour==12)&&(min==0)&&(sec==0))
	{
		if(pm)
		{
			day++;
			pm=0;
		}else{
			pm=1;
		}
	}
	if(hour==13)
	{
		hour=1;
	}
	if(day>28)
	{
		if(month==2)
		{
			if((year%4==0)&&(day<30))
			{
				if(year%100==0)
				{
					if((year%400)!=0){day=1;month++;}
				}
			}
			else{
				day=1;
				month++;
			}
		}
		else if((month==4||month==6||month==9||month==11)&&day==31)
		{
			day=1;
			month++;
		}
		else if((month==1||month==3||month==5||month==7||month==8||month==10||month==12)&&day==32){
			day=1;
			month++;
		}
	}
	if(month==13){
		month=1;
		year++;
		if(year>2099){year=2000;}
	}
}

/*------------------------------------------------------------------------------
 *interrupt service routines
------------------------------------------------------------------------------*/
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A0 (void)
{

	clockTick();
	checkAlarm();

	//set pm led
	if(mode==2){
		if(almPm){
			P1OUT |= 0x01;
		}
		else{
			P1OUT &= ~01;
		}
	}
	else{
		if(pm){
			P1OUT |= 0x01;
		}
		else{
			P1OUT &= ~01;
		}
	}

	LPM1_EXIT; //clear flag


}

#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
{

	unsigned int i;
	for(i=0;i<10000;i++);//debounce for 10ms

	//if((P1IN & 0x80)== 0x00){//if set pressed
		if((P2IN & 0x01) == 0x00){//0000 0110 p2.0 mode
			handleButton(1);
		}
		if((P2IN & 0x02) == 0x00){//0000 0101 p2.1 hr
			handleButton(2);
		}
		if((P2IN & 0x04) == 0x00){//0000 0011 p2.2 min
			handleButton(3);
		}
	//}


	P2IFG = 0x00;//clear interrupt flag

	LPM1_EXIT;//clear flag
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
{
	temp = UCA0RXBUF;
	IFG2 &= ~UCA0RXIFG;//clear flag
}

