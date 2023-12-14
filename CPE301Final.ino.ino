/* CPE 301 Final Project
 * By: Carson Ramocinski and Benjamin Steinberg
 */

#include <Stepper.h>
#include <LiquidCrystal.h>
#include <RTClib.h>
#include "DHT.h"

// Set up ports and registers
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal lcd(4, 3, 30, 24, 26, 28);
volatile unsigned char* port_b = (unsigned char*) 0x25;
volatile unsigned char* ddr_b = (unsigned char*) 0x24;
volatile unsigned char* pin_b = (unsigned char*) 0x23;
volatile unsigned char* port_k = (unsigned char*) 0x108;
volatile unsigned char* ddr_k  = (unsigned char*) 0x107;
volatile unsigned char* pin_k  = (unsigned char*) 0x106;
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADCH_DATA = (unsigned int*) 0x79;
volatile unsigned int* my_ADCL_DATA = (unsigned int*) 0x78;
unsigned char WATER_LEVEL_PORT = A0;
#define RDA 0x80
#define TBE 0x20
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;
volatile unsigned char *myTCCR1A  = (unsigned char *) 0x80;
volatile unsigned char *myTCCR1B  = (unsigned char *) 0x81;
volatile unsigned char *myTCCR1C  = (unsigned char *) 0x82;
volatile unsigned char *myTIMSK1  = (unsigned char *) 0x6F;
volatile unsigned char *myTIFR1   = (unsigned char *) 0x36;
volatile unsigned int  *myTCNT1   = (unsigned int *) 0x84;
RTC_DS1307 RTC; // clock

// Sensor conditions
#define TEMPF 70
#define WATERLEVEL 100

// states
enum state {
   disable = 0,
   idle = 1,
   run = 2,
   error = 3
};

// Initial State
enum state stat = disable;


const int stepsPerRevolution = 2038;
// Creates an instance of stepper class
// Pins entered in sequence IN1-IN3-IN2-IN4 for proper step sequence
Stepper myStepper = Stepper(stepsPerRevolution, 23, 27, 25, 29);

void setup() {
  adc_init();
  U0init(9600);
  lcd.begin(16, 2);
  dht.begin();
  // PB7 input button, PB6-PB2 are output pins
  *port_b &= 0b01111111;
  *ddr_b &= 0b01111111;
  *ddr_b |= 0b01111110;
  *ddr_k &= 0x01 << 2; // pin k2 input
  *port_k |= 0x10;
  PORTD |= (1 << PD0) | (1 << PD1);
  RTC.begin(); // clock
  DateTime now = DateTime(2023, 12, 10, 0, 0, 0);
  RTC.adjust(now);
}

void loop() {
  DateTime now = RTC.now();
  unsigned int w = adc_read(WATER_LEVEL_PORT);
  float f = temperatureRead(true);
  float h = humidity();

  if(*pin_k & (0x01 << 2)){
    myStepper.setSpeed(5);
    myStepper.step(stepsPerRevolution);
  }
  // States
  switch(stat) {
    case disable:
      disabled_state();
      break;
    case idle:
      idle_state();
      break;
    case error:
      error_state();
      break;
    case run:
      running_state();
      break;
    default:
      break;
  }
}

// state functions
void disabled_state() { 
  lcd.clear();
  lcd.noDisplay();

  *port_b &= 0b10000001; 
  *port_b |= (0x01 << 3); 
  
  while ( (*pin_b & (1 << 7)) == 0) { }

  stat = idle;
  lcd.display();
}

void idle_state() {
  *port_b |= 0b01000000; 
  *port_b &= 0b01000000; 
  unsigned int w = water_level();
  float t = temperatureRead(true);
  float h = humidity();
  lcd_th(t, h);
  if (w < WATERLEVEL) stat = error;
  else if (t > TEMPF) stat = run;
  U0putchar('O');
  U0putchar('F');
  U0putchar('F');
  U0putchar('\n');
  timeStamp(now);
}

void error_state() {
  *port_b |= 0b00100000; 
  *port_b &= 0b00100000; 
  lcd.clear();
  lcd.print("Low Water");

  unsigned int w = water_level();
  // loop while water level below minimum
  while (w < WATERLEVEL) {
    w = water_level();
    lcd.setCursor(0, 1);
    lcd.print("Level:");
    lcd.setCursor(7, 1);
    lcd.print(w);
}
  stat = idle;
  lcd.clear();
  U0putchar('O');
  U0putchar('F');
  U0putchar('F');
  U0putchar('\n');
  timeStamp(now);
}

void running_state()
{
  *port_b |= 0b00010000; 
  *port_b &= 0b00010000; 
  float f = temperatureRead(true);
  float h = humidity();

  if (water_level() < WATERLEVEL) stat = water;
  else if (f > TEMPF) {
    lcd_th(f, h);
    return running_state();
  }
  else {
    lcd.clear();
    stat = idle;
  }
  U0putchar('O');
  U0putchar('N');
  U0putchar('\n');
  timeStamp(now);
}

unsigned int water_level() {
  return adc_read(WATER_LEVEL_PORT);
}

float temperatureRead(bool F) {
  float t;
  if (F) t = dht.readTemperature(true); 
  else t = dht.readTemperature();       
  return t;
}

float humidity() {
  float h = dht.readHumidity();
  return h;
}

void lcd_th(float t, float h) {
  lcd.setCursor(0, 0);
  lcd.print("Temp:  Humidity:");
  lcd.setCursor(0, 1);
  lcd.print(t);
  lcd.setCursor(7, 1);
  lcd.print(h);
}

void adc_init() {
  *my_ADCSRA |= 0x80; 
  *my_ADCSRA &= 0b11011111; 
  *my_ADCSRA &= 0b11110111; 
  *my_ADCSRA &= 0b11111000; 

  *my_ADCSRB &= 0b11110111; 
  *my_ADCSRB &= 0b11111000; 

  *my_ADMUX &= 0b01111111; 
  *my_ADMUX |= 0b01000000; 
  *my_ADMUX &= 0b11011111; 
  *my_ADMUX &= 0b11100000; 
}

unsigned int adc_read(unsigned char adc_channel_num)
{
  *my_ADMUX &= 0b11100000;
  *my_ADMUX &= 0b11011111;
  if (adc_channel_num > 7) {
    adc_channel_num -= 8;
    *my_ADCSRB |= 0b00001000;
  }
  *my_ADMUX += adc_channel_num;
  *my_ADCSRA |= 0b01000000;
  while ((*my_ADCSRA & 0x40) != 0);
  return pow(2 * (*my_ADCH_DATA & (1 << 0)), 8) + pow(2 * (*my_ADCH_DATA & (1 << 1)), 9) + *my_ADCL_DATA; 
}

// Timer setup function
void setup_timer_regs()
{
  // setup the timer control registers
  *myTCCR1A= 0x00;
  *myTCCR1B= 0X00;
  *myTCCR1C= 0x00;
  
  // reset the TOV flag
  *myTIFR1 |= 0x01;
  
  // enable the TOV interrupt
  *myTIMSK1 |= 0x6E;
}

// TIMER OVERFLOW ISR
ISR(TIMER1_OVF_vect)
{
  int currentTicks;
  // Stop the Timer
  *myTCCR1B &= 0xF8;
  // Load the Count
  *myTCNT1 =  (unsigned int) (65535 -  (unsigned long) (currentTicks));
  // Start the Timer
  *myTCCR1B |=  0x00000001;
  // if it's not the STOP amount
  if(currentTicks != 65535)
  {
    // XOR to toggle PB6
    *portB ^= 0x40;
  }
}

void U0init(unsigned long U0baud)
{
//  Students are responsible for understanding
//  this initialization code for the ATmega2560 USART0
//  and will be expected to be able to intialize
//  the USART in differrent modes.
//
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}

unsigned char U0kbhit()
{
  return (RDA & *myUCSR0A);
}

unsigned char U0getchar()
{
  return *myUDR0;
}

void U0putchar(unsigned char U0pdata)
{
  while(!(TBE & *myUCSR0A));
  *myUDR0 = U0pdata;
}

void timeStamp(DateTime now){
  int year = now.year();
  int month = now.month();
  int day = now.day();
  int hour = now.hour();
  int minute = now.minute();
  int second = now.second();
  char numbers[10] = {'0','1','2','3','4','5','6','7','8','9'}; // used to print chars
  // get appropriate time values to convert to chars
  int onesYear = year % 10; 
  int tensYear = year / 10 % 10;
  int onesMonth = month % 10;
  int tensMonth = month / 10 % 10;
  int onesDay = day % 10;
  int tensDay = day / 10 % 10;
  int onesHour = hour % 10;
  int tensHour = hour / 10 % 10;
  int onesMinute = minute % 10;
  int tensMinute = minute / 10 % 10;
  int onesSecond = second % 10;
  int tensSecond = second / 10 % 10;
  
  U0putchar('M');
  U0putchar(':');
  U0putchar('D');
  U0putchar(':');
  U0putchar('Y');
  U0putchar(' ');
  U0putchar('H');
  U0putchar(':');
  U0putchar('M');
  U0putchar(':');
  U0putchar('S');
  U0putchar(' ');
  U0putchar(numbers[tensMonth]);
  U0putchar(numbers[onesMonth]);
  U0putchar(':');
  U0putchar(numbers[tensDay]);
  U0putchar(numbers[onesDay]);
  U0putchar(':');
  U0putchar(numbers[tensYear]);
  U0putchar(numbers[onesYear]);
  U0putchar(' ');
  U0putchar(numbers[tensHour]);
  U0putchar(numbers[onesHour]);
  U0putchar(':');
  U0putchar(numbers[tensMinute]);
  U0putchar(numbers[onesMinute]);
  U0putchar(':');
  U0putchar(numbers[tensSecond]);
  U0putchar(numbers[onesSecond]);
  U0putchar('\n');
}