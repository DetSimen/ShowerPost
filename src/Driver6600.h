#pragma once

#include <Arduino.h>
#include "CustomDevice.h"

enum class TMotorDir : bool { Dir_CCW = false, Dir_CW = true };

class TDriver6600 :public TCustomDevice {
protected:
	static constexpr uint8_t MOTOR_DISABLE = HIGH;
	static constexpr uint8_t MOTOR_ENABLE  = LOW;
protected:
	uint8_t FpinDIR;
	uint8_t FpinStep;
	uint8_t FpinEN;

	TMotorDir FMotorDir;

	void Init() {
		FInitNeed = false;

		puts("Motor Init");
		
		pinMode(FpinEN, OUTPUT);
		digitalWrite(FpinEN, MOTOR_DISABLE);

		pinMode(FpinDIR, OUTPUT);
		digitalWrite(FpinDIR, static_cast<bool>(FMotorDir));

		pinMode(FpinStep, OUTPUT);

		SetDeviceState(TDeviceState::Off);
	}

public:
	TDriver6600(const uint8_t ApinDir, const uint8_t ApinStep, const uint8_t ApinEN) :TCustomDevice() {
		FpinDIR = ApinDir;
		FpinStep = ApinStep;
		FpinEN = ApinEN;
		FInitNeed = true;
		FEnableChangeMsg = false;
		FMotorDir = TMotorDir::Dir_CW;
		FDeviceState = TDeviceState::Unknown;
	}

	void On(void) override {
		if (FInitNeed) Init();
		digitalWrite(FpinDIR, HIGH);// static_cast<uint8_t>(FMotorDir));
		digitalWrite(FpinEN, MOTOR_ENABLE);
		puts("Motor ON");
		SetDeviceState(TDeviceState::On);
	}

	void Off(void) override {
		puts("Motor OFF");
		digitalWrite(FpinEN, MOTOR_DISABLE);
		SetDeviceState(TDeviceState::Off);
	}

	void Pulse() {
		if (FDeviceState == TDeviceState::On) {
			digitalWrite(FpinStep, !digitalRead(FpinStep));
		}
		else
			digitalWrite(FpinStep, HIGH);
	}

	void SetDirection(const TMotorDir ADir) {
		if (FMotorDir == ADir) return;
		FMotorDir = ADir;
		bool running = isOn();
		Off();
		digitalWrite(FpinDIR, static_cast<bool>(FMotorDir));
		if (running) On();
	}
};