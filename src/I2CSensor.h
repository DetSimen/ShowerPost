#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "CustomSensor.h"
#include "DEF_Errors.h"




class TI2CCustomSensor : public TCustomSensor {
public:
	static constexpr uint32_t HIGH_SPEED = 400000;
	static constexpr uint32_t LOW_SPEED = 100000;
protected: 
	uint8_t		FDevAddress;
	bool		FFastSpeed;


	void init(void) override {
		FInitNeed = false;
		Wire.begin();
		Wire.setClock(FFastSpeed ? HIGH_SPEED : LOW_SPEED);

		if (!DeviceExist(FDevAddress)) Error(err_i2c_DeviceNotFound);
	}

	void internalRead(void) override{

	}


	TI2CCustomSensor() = delete;
	TI2CCustomSensor(TI2CCustomSensor&) = delete;
	TI2CCustomSensor(TI2CCustomSensor&&) = delete;


public:
	TI2CCustomSensor(const uint8_t ADevAddr): TCustomSensor::TCustomSensor() {
		FDevAddress = ADevAddr;
		FFastSpeed = true;
	}

	static bool DeviceExist(const uint8_t ADevAddr) {
		Wire.beginTransmission(ADevAddr);
		delay(1);
		return (Wire.endTransmission() == 0);
	}

};
