// 
// 
// 

#include <Arduino.h>
#include <avr\interrupt.h>
#include <stdio.h>
#include "src\Timer1.h"

PTimerOne TimerOne = nullptr;



ISR(TIMER1_COMPA_vect) {
	if (TimerOne!=nullptr) TimerOne->CompA();
	return;
}

ISR(TIMER1_COMPB_vect) {
	if (TimerOne!=nullptr) TimerOne->CompB();
	return;
}

void TTimerOne::Init() {

	if (FCompareValue == 0) {
		FActive = false;
		TCCR1B = 0;
		return;
	}

	cli();
	TCCR1B = 0;
	TCCR1A = 0; // clear register
	TCCR1B = _BV(WGM12); // set Timer1 to CTC mode
	TCCR1B |= 2;		// divider = 8
	TIMSK1 |= _BV(OCIE1A); // enable Timer1 COMPA interrupt
	OCR1A = FCompareValue;
	TCNT1 = 0;
	sei(); // enable global interrupts
	FActive = true;

}

TTimerOne::TTimerOne(PTimer1EventFunc ACompA, PTimer1EventFunc ACompB)
{
	FActive = false;
	FCompAValue = 0;
	FCompBValue = 0;
	onCompareA = ACompA;
	onCompareB = ACompB;
	FCompareValue = 0;
	TimerOne = this;
}


void TTimerOne::CompA(void)
{
	TCNT1 = 0;
	if (onCompareA != nullptr) onCompareA();
}

void TTimerOne::CompB(void)
{
	TCNT1 = 0;
	if (onCompareB != nullptr) onCompareB();
}

void TTimerOne::Stop(void)
{
	FActive = false;
	TCCR1B = 0x00;
	FCompareValue = 0;
}

void TTimerOne::Run(const uint16_t ARPM)
{
	FActive = true;
	SetRPM(ARPM);
}


void TTimerOne::SetRPM(const uint16_t ANewRPM)
{
	bool oldActive = FActive;
	uint16_t imp = 200L * ANewRPM / 60L;
	FCompareValue = (F_CPU >> 4) / imp;
	Init();
	if (!oldActive) Stop();
}
