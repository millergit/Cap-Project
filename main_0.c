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
__interrupt void Timer1_A3 (void);

//clock variables
unsigned int month, day, year, sec, min, hour, pm, mode, button, whatButton;
unsigned int almH, almM, almPm, almWatch;
unsigned int temp[3];

//Display items
unsigned int tube[6];//1 is hour
unsigned int tubeSel, digit;

//clock fuctions
void doAction();
void setTubes();
void display();
void clockTick();
void checkAlarm();
void initVars();

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
	config_interrupts();
	config_ports();
	initVars();

	while(1){


		doAction();
		setTubes();

		for(tubeSel=0;tubeSel<6;tubeSel++){
			display();
		}

		__bis_SR_register(LPM3_bits + GIE);//sleep in heavenly peace
	}
}


//hour, min, mode, snooze, alarm on/off

static void config_clocks(){

	//for no crystal
	DCOCTL = CALDCO_1MHZ;//DCO frequency step set
	BCSCTL1 = CALBC1_1MHZ;//basic lock system control reg1 freq. range set


	BCSCTL2 = 0b00000110;//SM clock is DO clock/8=125000Hz; main is dco

	BCSCTL3 = 0b00100000;//ACLK is VLOCLK @ 12kHz

	//if crystal
	//BCSCTL3 = 0b00000000;//Aclk is crystal, bits 3&2 determine capacitor select.
	//1,6,10,12.5pF


}

static void config_interrupts(){
	//no crystal, use more accurate DO clock
	TA0CTL = TASSEL_2 + TACLR + MC_1 + ID_3; //SMCLK, clear TAR, count up, SM/8 = 15625Hz;
	CCTL0 = CCIE; // CCR0 interrupt enabled
	CCR0 = 0x3D08; //15625/(15624+1) = interrupt @ 1Hz

	//crystal
	//CCR0 = 0x7FFF; //32768/(32767) = interrupt @ 1Hz


}

void initVars(){
	month = 1;
	day = 1;
	year = 2000;
	sec,min,pm,almM,almPm = 0;
	hour,almH = 12;
	temp = 0;
	mode = 0;
	almWatch = 0;
	tubeSel = 6;
	digit=0;
	int i;
	for(i=0;i<6;i++){tube[i]=0;}
}

static void config_ports(){
	//p1.0 is pm led OUT
	//p1.1  is Z OUT
	//p1.4-1.7 are X OUT
	//p2.0-2.3 are modes IN
	//p2.4-2.6 from ez430 IN
	//p2.7 alarm OUT
	//default is input

	P1DIR = 0xFF;//all output
	P1OUT = 0b00001100;//all off except NMI to GND through 2to4

	P2DIR = 0x80;//p2.7 output

	P2REN |= 0x7F;//enable resistor
	P2OUT &= 0x7F;//pullup resistor/clear out of 2.7
	P2IES |= 0x7F;//high to low transition to generate
	P2IFG &= 0x80;//clear flag to start
	P2IE |= 0x7F;//interrupt enable

	_BIS_SR(GIE);//enable interrupts could also use _EINT();
}


/*------------------------------------------------------------------------------
 *functional routines
------------------------------------------------------------------------------*/

void doAction(){

	if (button){
		switch(whatButton){
		case 1://mode
			mode++;
			mode=mode%4;
			break;
		case 2://hour
			switch(mode){
			case 0://time
				hour++;
				sec=0;
				if(hour>12){
					hour=1;
					pm ^= 1;
				}
				break;
			case 1://date
				month++;
				sec=0;
				if(month>12){
					year++;
					if(year>2099){year=2000;}
				}
				break;
			case 2://alarm set
				almH++;
				if(almH>12){
					almH=1;
					almPm ^= 1;
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
				if(day>=28){
					if(month==2){
						if(year%4==0){
							if(year%100==0){
								if(year%400==0){break;}
								else{day=0;}
							}
							else{break;}
						}
						else if(day==28){
							day=0;
						}
					}
					else if((month==4||month==6||month==9||month==11)&&day==30){
						day=0;
					}
					else if((month==1||month==3||month==5||month==7||month==8||month==10||month==12)&&day==31){
						day=0;
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
		button=0;//button handled
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
		tube[3]=0;
		tube[4]=0;
		tube[5]=0;
		break;
	default:
		break;
	}
}

void display(){

	//z is 1
	//2,3 are 2 to 4
	//x is 4-7

	P1OUT |= 0b11110000;//set out to all ones momentarily
	digit = tube[tubeSel];

	//set output X
	switch(digit){
	case 0:
		P1OUT &= 0x0F;
		break;
	case 1:
		P1OUT &= 0x1F;
		break;
	case 2:
		P1OUT &= 0x2F;
		break;
	case 3:
		P1OUT &= 0x3F;
		break;
	case 4:
		P1OUT &= 0x4F;
		break;
	case 5:
		P1OUT &= 0x5F;
		break;
	case 6:
		P1OUT &= 0x6F;
		break;
	case 7:
		P1OUT &= 0x7F;
		break;
	case 8:
		P1OUT &= 0x8F;
		break;
	case 9:
		P1OUT &= 0x9F;
		break;
	}

	//send NMI with info
	switch(tubeSel){
	case 0:
		P1OUT &= 0xF1;
		break;
	case 1:
		P1OUT &= 0xF3;
		break;
	case 2:
		P1OUT &= 0xF5;
		break;
	case 3:
		P1OUT &= 0xF7;
		break;
	case 4:
		P1OUT &= 0xF9;
		break;
	case 5:
		P1OUT &= 0xFB;
		break;
	}

	//hold for 1000 cycles
	for(i=0;i<1000;i++){}
	P1OUT |= 0x0C;//clear NMI


}

void checkAlarm(){

	if(almWatch){
		almWatch++;
		if(almWatch == 10000){//10,000/1MHz =.01s*12000(other MSP)=120 cycles to see alarm
			almWatch = 0;
			P2OUT &= ~0x80;//clear alarm
		}
	}
	else if((almH==hour)&&(almM==min)&&(pm=almPm)){
		P2OUT |= 0x80;//send alarm beacon on p1.7
		almWatch=1;
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
	if(hour==13){
		hour=1;
		if(pm){
			day++;
			pm=0;
		}else{
			pm=1;
		}
	}
	if(day>=28){
		if(month==2){
			if(year%4==0){
				if(year%100==0){
					if(year%400==0){break;}
					else{day=0;month++;}
				}
				else{break;}
			}
			else if(day==28){
				day=0;
				month++;
			}
		}
		else if((month==4||month==6||month==9||month==11)&&day==30){
			day=0;
			month++;
		}
		else if((month==1||month==3||month==5||month==7||month==8||month==10||month==12)&&day==31){
			day=0;
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
#pragma vector=TIMER1_A3_VECTOR
__interrupt void Timer1_A3 (void)
{

	clockTick();
	checkAlarm();
	if(mode==2){//if alm set
		if(almPm){
			P1OUT |= 0x80;
		}
		else{
			P1OUT &= ~01;
		}
	}
	else{
		if(pm){
			P1OUT |= 0x80;
		}
		else{
			P1OUT &= ~01;
			}
	}

	_bic_SR_register_on_exit(LPM3_bits); //clear flag


}

#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
 {

	P2IFG &= 0x00; //clear interrupt flag

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

	//implement logic for temp collection?

	_bic_SR_register_on_exit(LPM3_bits);//clear flag
 }
