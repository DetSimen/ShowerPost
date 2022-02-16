#pragma once
#include <Arduino.h>
#include "TClass.h"
#include "I2CSensor.h"


class Tpcf8574 : public TI2CCustomSensor {
public:
	static constexpr uint8_t PCF8574_DEFAULT_ADDRESS = 0x20;
	static constexpr uint8_t PCF8574A_DEFAULT_ADDRESS = 0x38;

protected:
	uint8_t		FMask;
	uint8_t		FLastValue;

	void init(void) override {
		TI2CCustomSensor::init();
		if (!isError()) {
			Wire.beginTransmission(FDevAddress);
			Wire.write(FMask);
		}
	}

	void internalRead(void) override {
		Wire.requestFrom(FDevAddress, 1U);
		uint8_t  value = Wire.read() ^ 0xFF;
		if (FLastValue == value) return;
		FLastValue = value;
		PostMessage(msg_SensorValueChanged, value);
	}

	Tpcf8574() = delete;
	Tpcf8574(Tpcf8574&) = delete;
	Tpcf8574(Tpcf8574&&) = delete;

public:
	Tpcf8574(const uint8_t ADevAddr = PCF8574_DEFAULT_ADDRESS, const uint8_t AMask = 0b11111111) :TI2CCustomSensor(ADevAddr) {
		FInitNeed = true;
		FReadInterval = 200;
		FLastValue = 0xFF;
	}

};