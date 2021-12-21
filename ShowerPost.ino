/*
    Name:       ShowerPost.ino
    Created:	13.12.2021 19:00:55
    Author:     DtS
*/
#include <Wire.h>
#include <Arduino.h>
#include "src\Messages.h" 
#include "src\DEF_Message.h"
#include "src\HardTimers.h"
#include "src\TLed.h"
#include "src\TM1637.h"
#include "src\DS18B20.h"
#include "src\DS3231.h"
#include "src\dtsEncoder.h"


TMessageList MessageList(12);   // очередь глубиной 12 сообщений
THardTimers  Timers;            // Таймеры, 10 штук

//-----------------------------------------------------------------------------------------
//
//          Используемые пины
//
constexpr uint8_t PIN_LED_ALIVE     = LED_BUILTIN;  // встроенный светодиод на 13м пине Uno
constexpr uint8_t PIN_1637_CLOCK    = 11;           // Тактирующий вход дисплея ТМ1637
constexpr uint8_t PIN_1637_DATA     = 12;           // Вход данных дисплея ТМ1637
constexpr uint8_t PIN_TEMP_SENSOR   = 10;           // Вход датчика DS18B20
constexpr uint8_t PIN_BEEPER        = A3;           // Зуммер, + на пин А3, минус на GND

constexpr uint8_t PIN_LEFT_ENCODER_BUTTON   = 2;    // Кнопка левого энкодера
constexpr uint8_t PIN_LEFT_ENCODER_LEFT     = 4;    // кнопка направления влево левого энкодера
constexpr uint8_t PIN_LEFT_ENCODER_RIGHT    = 3;    // кнопка направления вправо левого энкодера



constexpr uint16_t msg_SoftTimerStart = 0x110;
constexpr uint16_t msg_SoftTimerEnds = 0x111;

//-----------------------------------------------------------------------------------------
//
//   Блок светодиода активности, период 5 секунд, 100мс горит, остальное время - нет
//
constexpr uint32_t  ALIVE_PERIOD = 5000;            // период повторения
constexpr uint32_t  ALIVE_ON_TIME = 100;            // время горения, мс
constexpr uint32_t  ALIVE_OFF_TIME = ALIVE_PERIOD - ALIVE_ON_TIME; // время негорения, период-время горения

TLed    ledAlive(PIN_LED_ALIVE, ACTIVE_HIGH);   // светодиод, подключен через 220 Ом на землю

THandle hTimerAlive = INVALID_HANDLE;           // Таймер светодиода активности

//-----------------------------------------------------------------------------------------
//
//   Блок дисплея ТМ1637
//
constexpr uint32_t COLON_FLASH_TIME = 500;      // время мигания двоеточия, 500мс

enum class TDisplayState: uint8_t {Unknown, Time=1, Temp=2, Timer=3, Dumb}; // состояния отображения, время,температура,таймер

TDisplayState DisplayState = TDisplayState::Unknown; // первоначальное состояние не определено
void SetDisplayState(const TDisplayState ANewState); // функция смены состояния отображения
void Display(void);                                  // функция отображения

THandle hTimerColon = INVALID_HANDLE;       // Таймер мигания двоеточия в часах

TM1637 Disp(PIN_1637_CLOCK, PIN_1637_DATA, enTM1637Type::Time); // обьект дисплея


//-----------------------------------------------------------------------------------------
//
//  Блок температурного сенсора  DS18B20
//

TDS18B20 TempSensor(PIN_TEMP_SENSOR); // объект температурного сенсора

//-----------------------------------------------------------------------------------------
//
//  Блок часов реального времени  DS3231, подключен А4 - Data, A5 - Clock
//
TDS3231 Clock;  //  объект часов реального времени


// ----------------------------------------------------------------------------------------
//
//      Блок левого энкодера
//
//

TEncoder    LeftEncoder(PIN_LEFT_ENCODER_LEFT, PIN_LEFT_ENCODER_RIGHT, PIN_LEFT_ENCODER_BUTTON);

void OnLeftButtonPress();
void OnLeftRotary(const int8_t AStep);
void OnLeftLongButton(void);

// ---------------------------------------------------------------------------------------
//
//  Блок зуммера
//
//

constexpr uint32_t BEEP_BUTTON_TIME = 50U;
constexpr uint32_t BEEP_CHIRP_TIME = 5U;

TDigitalDevice Beeper(PIN_BEEPER, ACTIVE_HIGH);

THandle     hTimerBeeper;

void    Beep(const uint16_t ABeepTime = BEEP_BUTTON_TIME);



//-----------------------------------------------------------------------------------------
//
//  Глобальные переменные
//

int8_t    CurrTemperature = INVALID_TEMPERATURE; // последняя измеренная температура
int16_t   CurrentTimerValue = 0;
bool      SoftTimerRun = false;
volatile bool Flashing = false;
char      TimerDigits[]="0000";
int8_t    CurrentDigitIdx = 3;
TDateTime SaveTime;

bool SetupMode = false;


//-----------------------------------------------------------------------------------------
//
//  Прототипы функций
//
void Dispatch(const TMessage& Msg);  // функция обработки сообщений



//-----------------------------------------------------------------------------------------
//
//  implementation
//

int srl_putchar(char ch, FILE* F) { // служебная функция вывода в сериал
    return Serial.print(ch);
}

void setup() {                                  // начальные настройки
    Serial.begin(115200);                       // заводим Serial
    stdout = fdevopen(srl_putchar, NULL);       // перенаправляем в него весь вывод программы
    delay(200);
    puts("Program ShowerPost v1.0 started..."); // и выводим туда приветственное сообщение

    ledAlive.On(); // зажигаем светодиод активности
    hTimerAlive = Timers.Add(ALIVE_ON_TIME, TTimerState::Running);// и взводим его таймер

    hTimerColon = Timers.Add(500, TTimerState::Running);  // взводим таймер мигания двоеточия
    hTimerBeeper = Timers.Add(BEEP_BUTTON_TIME, TTimerState::Stopped);

    SetDisplayState(TDisplayState::Time); // первоначально устанавливаем отображение времени


//    Clock.SetTime(__TIME__);  // первоначальная настройка часов

}

void loop() {                   // бесконечный цикл
    Clock.Read();               // опрашиваем сенсоры 

    LeftEncoder.Read();

    TempSensor.Read();          // и устройства
                                // и при получении от них сообщения
                                // передаём его в функцию диспетчера
                                //

    if (MessageList.Available()) Dispatch(MessageList.GetMessage());
}



//
//  функция смены того, что мы будем отображать на дисплее
//
void SetDisplayState(const TDisplayState ANewState)
{
    if (DisplayState == ANewState) return;  // если состояние не поменялось - ничего делать не надо
    DisplayState = ANewState;               // иначе запомним новое состояние 
    Display();                              // и вызовем функцию отображения
}

//
//  функция отображения, отображает время или температуру, в зав-ти от состояния
//
void Display(void)
{
    

    switch (DisplayState) // и в зависимости от текущего состояния
    {
    case TDisplayState::Time:       // выведем либо время
        Disp.PrintTime(Clock.GetHour(), Clock.GetMinute()); //которое возьмём из RTC
        if (SetupMode && Flashing) Disp.Clear();
        break;

    case TDisplayState::Temp:       // либо температуру
        Disp.ShowPoint(false);      // погасим двоеточие
        Disp.PrintDeg(CurrTemperature); // выведем из глоб. переменной температуру
        break;

    case TDisplayState::Timer: { 
        if (!SetupMode) {
            memset(TimerDigits, '0', 5);
            sprintf(TimerDigits, "%04u", CurrentTimerValue);
            Disp.Print(CurrentTimerValue);
        }
        else {
            Disp.Print(TimerDigits);
            if (Flashing) Disp.PrintAt(CurrentDigitIdx, ' ');
        }
    }

    default:
        break;
    }
}

void Beep(const uint16_t ABeepTime)
{
    Timers.SetNewInterval(hTimerBeeper, ABeepTime);
    Timers.Reset(hTimerBeeper);
    Beeper.On();
}

void SwitchToNextDisplayMode(void) {
    const uint8_t DUMB_VALUE = static_cast<uint8_t>(TDisplayState::Dumb);

    uint8_t dispState = static_cast<uint8_t>(DisplayState);

    if (++dispState == DUMB_VALUE) dispState = 1;

    TDisplayState newDispState = static_cast<TDisplayState>(dispState);

    SetDisplayState(newDispState);
}

void OnLeftButtonPress()
{
    if (!SetupMode) 
        SwitchToNextDisplayMode();
    else {
        if (CurrentDigitIdx > 0)
            CurrentDigitIdx--;
        else
            CurrentDigitIdx = 3;
    }
    Display();
}

void OnLeftLongButton(void) {
    if (DisplayState!=TDisplayState::Temp) SetupMode = !SetupMode;
    Flashing = !SetupMode;
    Display();
}

void OnLeftRotary(const int8_t AStep)
{
    if (SetupMode) {
        if (DisplayState == TDisplayState::Timer) {
            char ch = TimerDigits[CurrentDigitIdx];
             ch += AStep;
             if (ch < '0') ch = '0';
             if (ch > '9') ch = '9';
             TimerDigits[CurrentDigitIdx] = ch;
             CurrentTimerValue = atoi(TimerDigits);
        }
        Display();
    }
}


//
// Главная функция диспетчер, сюда стекаются все сообщения 
// от всех сенсоров и устройств, она распихивает их дальше, либо обрабатывает сама
//
//
void Dispatch(const TMessage& Msg) {

    switch (Msg.Message)
    {
    case msg_Error: {
        PClass Sender = PClass(Msg.Sender);
        if (Sender != NULL)
            printf("Error in module %S\n", Sender->GetClassName());
        else
            puts("Unknown error.");
        break;
    }

    case msg_TimerEnd: { // сообщение Таймер кончился
        if (Msg.LoParam == hTimerAlive) { // если это таймер светодиода активности
            ledAlive.Toggle(); // переключаем светодиод
            uint32_t newInterval = ledAlive.isOn() ? ALIVE_ON_TIME : ALIVE_OFF_TIME;//и меняем интервал
            Timers.SetNewInterval(hTimerAlive, newInterval);//чтоб он горел/не горел определенное время
        }

        if (Msg.LoParam == hTimerColon) {
            if (DisplayState == TDisplayState::Time)
                Disp.ToggleColon(); // если это таймер двоеточия, 
                                    // переключаем двоеточие on/off
            if (SetupMode) {
                Flashing = !Flashing;
                Display();
            }
        }


        if (Msg.LoParam == hTimerBeeper) {
            Beeper.Off();
            Timers.Stop(hTimerBeeper);
        }

        break;
    }

    case msg_TempChanged: {// сообщение: изменилась температура

        if (Msg.Sender == TempSensor) {

            CurrTemperature = TempSensor.GetTemperature();// запомним её в глоб. переменной

            if (DisplayState == TDisplayState::Temp)  Display();
        }
        break;
    }

    case msg_TimeChanged: {// сообщение: изменилось время (раз в минуту), приходит из DS3231
        if (DisplayState == TDisplayState::Time) Display(); // просто отобразим изменившееся время
        break;
    }

    case msg_EncoderBtnPress: {
        if (Msg.Sender == LeftEncoder) {
            Beep();
            OnLeftButtonPress();
        }
        break;
    }

    case msg_EncoderLeft:
    case msg_EncoderRight: {
        if (DisplayState == TDisplayState::Timer) {
            if (!SoftTimerRun) {
                int8_t step = (Msg.Message == msg_EncoderRight) ? 1 : -1;
                Beep(BEEP_CHIRP_TIME);
                OnLeftRotary(step);
            }
        }
        break;
    }


    case msg_SoftTimerStart:
        Disp.Clear();
        SoftTimerRun = true;
        Flashing = true;
        break;

    case msg_SoftTimerEnds:
        SoftTimerRun = false;
        Flashing = false;
//        CurrentTimerValue = 0;
        if (CurrentTimerValue == 0) {
            SetDisplayState(TDisplayState::Time);
            Beep(1000);
        }
        else {
            Disp.Print(CurrentTimerValue);
        }
        break;

    case msg_EncoderBtnLong:
        OnLeftLongButton();
        break;

    case msg_SecondsChanged: 
        if (SoftTimerRun) {
            if (CurrentTimerValue > 0)
                CurrentTimerValue--;
            else
                SendMessage(msg_SoftTimerEnds);

            if (DisplayState==TDisplayState::Timer) Display();
        }
        break;

    case msg_DateChanged:
        break;

    default: // если мы пропустили какое сообщение, этот блок выведет в сериал его номер
        printf("Unhandled message 0x%X\n", Msg.Message);
        break;
    }
}
