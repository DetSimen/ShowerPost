#pragma once

class TTimerOne;
using PTimerOne = TTimerOne*;

using PTimer1EventFunc = void(*)(void);

class TTimerOne {
public:
	static constexpr uint16_t MIN_RPM = 30;
	static constexpr uint16_t MAX_RPM = 1800;
protected:
	bool		FActive;

	uint16_t	FCompareValue;

	uint16_t	FCompAValue;
	uint16_t	FCompBValue;


	void Init();

	PTimer1EventFunc onCompareA;
	PTimer1EventFunc onCompareB;

public:
	TTimerOne(PTimer1EventFunc ACompA, PTimer1EventFunc ACompB = NULL);;

	inline bool isActive(void) const { return FActive; }

	void CompA(void);
	void CompB(void);

	void Stop(void);


	void Run(const uint16_t ARPM);

	void SetRPM(const uint16_t ANewRPM);
};