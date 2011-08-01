/*--------------------------------------------------------------------------
  Danni Debounce Code
  -edited by Mike Szczys 8/1/2011
  -Fully working on ATmega168. Can debounce up to eight buttons on the same
   I/O port at the same time with repeat and short/long press options. All
   LED use in this program is for feedback purposes and can be changed to
   other functions.

   NOTE: This script DOES NOT use internal pull-up resistors. If you don't
         have external pull-ups for the buttons you'll need to edit this
         to use the internal resistors.
--------------------------------------------------------------------------*/

#define F_CPU 1000000

#include <avr/io.h>
#include <avr/interrupt.h>

//button definitions
#define KEY_DDR		DDRB
#define KEY_PORT	PORTB
#define KEY_PIN		PINB
#define KEY0		7	//Mode button
#define KEY1		6	//Next
#define KEY2		5	//+
#define KEY3		4       //-

//LED definitions
#define LED_DDR		DDRD
#define LED_PORT	PORTD
#define LED0		7
#define LED1		6
#define LED2		5


//Debounce
#define REPEAT_MASK   (1<<KEY0 | 1<<KEY1 | 1<<KEY2 | 1<<KEY3)   // repeat: key1, key2 
#define REPEAT_START   50      // after 500ms 
#define REPEAT_NEXT   20      // every 200ms
volatile unsigned char key_press;
volatile unsigned char key_state;
volatile unsigned char key_rpt;

/*--------------------------------------------------------------------------
  FUNC: 8/1/11 - Used to read debounced button presses
  PARAMS: A keymask corresponding to the pin for the button you with to poll
  RETURNS: A keymask where any high bits represent a button press
--------------------------------------------------------------------------*/
unsigned char get_key_press( unsigned char key_mask )
{
  cli();			// read and clear atomic !
  key_mask &= key_press;	// read key(s)
  key_press ^= key_mask;	// clear key(s)
  sei();
  return key_mask;
}

/*--------------------------------------------------------------------------
  FUNC: 8/1/11 - Used to check for debounced buttons that are held down
  PARAMS: A keymask corresponding to the pin for the button you with to poll
  RETURNS: A keymask where any high bits is a button held long enough for
		its input to be repeated
--------------------------------------------------------------------------*/
unsigned char get_key_rpt( unsigned char key_mask ) 
{ 
  cli();               // read and clear atomic ! 
  key_mask &= key_rpt;                           // read key(s) 
  key_rpt ^= key_mask;                           // clear key(s) 
  sei(); 
  return key_mask; 
} 

/*--------------------------------------------------------------------------
  FUNC: 8/1/11 - Used to read debounced button released after a short press
  PARAMS: A keymask corresponding to the pin for the button you with to poll
  RETURNS: A keymask where any high bits represent a quick press and release
--------------------------------------------------------------------------*/
unsigned char get_key_short( unsigned char key_mask ) 
{ 
  cli();         // read key state and key press atomic ! 
  return get_key_press( ~key_state & key_mask ); 
} 

/*--------------------------------------------------------------------------
  FUNC: 8/1/11 - Used to read debounced button held for REPEAT_START amount
	of time.
  PARAMS: A keymask corresponding to the pin for the button you with to poll
  RETURNS: A keymask where any high bits represent a long button press
--------------------------------------------------------------------------*/
unsigned char get_key_long( unsigned char key_mask ) 
{ 
  return get_key_press( get_key_rpt( key_mask )); 
} 

void initTimers(void) {
  cli();
  TCCR0B = 1<<CS02 | 1<<CS00;	//divide by 1024
  TIMSK0 = 1<<TOIE0;		//enable overflow interrupt
  sei();
}

void initIO(void) {
  //Setup buttons
  KEY_DDR &= ~(1<<KEY0 | 1<<KEY1 | 1<<KEY2 | 1<<KEY3);

  //Setup LEDs
  LED_DDR = 0xFF;	//All pins as outputs
  LED_PORT = 0xFF;	//set port bits
}

int main(void) {
  initIO();
  initTimers();

  while(1) {
    //Simplest form; debounce a button press on KEY0
    if( get_key_press( 1<<KEY0 )) 
      LED_PORT ^= 1<<LED0; 

    //Detect short or long presses on KEY1 and react accordingly
    if( get_key_short( 1<<KEY1 )) 
      LED_PORT ^= 1<<LED1; 

    if( get_key_long( 1<<KEY1 )) 
      LED_PORT ^= 1<<LED2; 

    //Detect when a button is held down and repeat the task
    if( get_key_press( 1<<KEY2 ) || get_key_rpt( 1<<KEY2 )){ 
      unsigned char i = LED_PORT; 

      i = (i & 0x07) | ((i << 1) & 0xF0); 
      if( i < 0xF0 ) 
        i |= 0x08; 
      LED_PORT = i;      
    }
  }
}

//Interrupt service routine
ISR(TIMER0_OVF_vect)           // every 10ms
{
  static unsigned char ct0, ct1, rpt;
  unsigned char i;

  TCNT0 = (unsigned char)(signed short)-(((F_CPU / 1024) * .01) + 0.5);   // preload for 10ms

  i = key_state ^ ~KEY_PIN;    // key changed ?
  ct0 = ~( ct0 & i );          // reset or count ct0
  ct1 = ct0 ^ (ct1 & i);       // reset or count ct1
  i &= ct0 & ct1;              // count until roll over ?
  key_state ^= i;              // then toggle debounced state
  key_press |= key_state & i;  // 0->1: key press detect

  if( (key_state & REPEAT_MASK) == 0 )   // check repeat function 
     rpt = REPEAT_START;      // start delay 
  if( --rpt == 0 ){ 
    rpt = REPEAT_NEXT;         // repeat delay 
    key_rpt |= key_state & REPEAT_MASK; 
  } 
}
