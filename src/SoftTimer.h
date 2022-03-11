#pragma once

class TDownCounter {
public:
	static constexpr uint8_t MAX_SECONDS = 59;

	enum class TCounterState : uint8_t { Unknown, Run, Pause, Stop };
	enum class TCounterMode : uint8_t { Seconds, Minutes };

protected:
	uint16_t FInitValue;
	uint16_t FCounter;
	TCounterState   FState = TCounterState::Unknown;
	TCounterMode	FMode = TCounterMode::Seconds;
	

	void Tick() {
		if (FCounter == 0) return;
		if (FState == TCounterState::Run) {
			FCounter--;
			setMode();
			SendMessage(msg_CounterTick, GetMinutes(), GetSeconds());
			if (FCounter == 0) {
				SendMessage(msg_CounterEnd);
				Stop();
			}
		}
	}

	void setMode() {
		if (FCounter > TDownCounter::MAX_SECONDS)
			FMode = TCounterMode::Minutes;
		else
			FMode = TCounterMode::Seconds;
	}

public:
	TDownCounter(const uint16_t AInterval = 0) {
		FInitValue = FCounter = AInterval;
		setMode();
		Stop();
	}

	bool isRunning() { return (FState == TCounterState::Run); }

	inline	void Stop()  { FState = TCounterState::Stop; }
	inline	void Start() { 
		if(FCounter>0) FState = TCounterState::Run; 
	}
	inline	void Pause() { FState = TCounterState::Pause; }
			
	void Reset() {
		bool runAfter = isRunning();
		Stop();
		FCounter = FInitValue;
		if (runAfter) Start();
	}
	
	inline	void Run() { Run(FInitValue); }
			
	void Run(const uint16_t ATimeToRun) {
		if (FInitValue != ATimeToRun) FInitValue = ATimeToRun;
		FCounter = ATimeToRun;
		setMode();
		Start();
	}

	inline	TCounterMode GetMode() const { return FMode; }

	void SetInterval(uint16_t ANewInterval) {
		Stop();
		FInitValue = FCounter = ANewInterval;
	}

	inline uint16_t GetCounter() const { return FCounter; }

	inline uint8_t GetMinutes() const { return (FCounter / (MAX_SECONDS + 1)); }
	inline uint8_t GetSeconds() const { return (FCounter % (MAX_SECONDS + 1)); }

	TDownCounter& operator --(int) {
		Tick();
		return *this;
	}

	inline TCounterState GetState() const { return FState; }
};