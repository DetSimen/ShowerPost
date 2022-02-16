#pragma once

template<class T, const uint8_t N> constexpr uint8_t ArraySize(const T(&)[N]) { return N; };

#pragma region PinDefinitions

//-----------------------------------------------------------------------------------------
//
//          Используемые пины
//
constexpr uint8_t PIN_LED_ALIVE			= LED_BUILTIN;  // встроенный светодиод на 13м пине Uno
constexpr uint8_t PIN_TEMP_SENSOR		= 12;   // Вход датчика DS18B20


constexpr uint8_t PIN_HEATER_RELAY		= 9;    // Пин нагревательного реле
constexpr uint8_t PIN_VENT_RELAY		= 8;    // Пин связанный с нагревателем, но без учета градусника
constexpr uint8_t PIN_1637_DATA			= 7;   // Вход данных дисплея ТМ1637
constexpr uint8_t PIN_1637_CLOCK		= 6;   // Тактирующий вход дисплея ТМ1637


constexpr uint8_t PIN_SCL				= A5;   // I2C Clock DS3231
constexpr uint8_t PIN_SDA				= A4;   // I2C Data DS3231
constexpr uint8_t PIN_BEEPER			= A3;   // Зуммер, + на пин А3, минус на GND


constexpr uint8_t PIN_R_ENCODER_LEFT	= 5;    // пин направления влево правого энкодера
constexpr uint8_t PIN_R_ENCODER_RIGHT	= 4;    // пин направления вправо правого энкодера
constexpr uint8_t PIN_R_ENCODER_BUTTON	= 1;    // Кнопка правого энкодера

constexpr uint8_t PIN_LEFT_ENCODER_LEFT		= 2;    // кнопка направления влево левого энкодера
constexpr uint8_t PIN_LEFT_ENCODER_RIGHT	= 3;    // кнопка направления вправо левого энкодера
constexpr uint8_t PIN_LEFT_ENCODER_BUTTON	= 0;    // Кнопка левого энкодера

#pragma endregion

constexpr uint8_t GALLET_ADDRESS = 0x20;
