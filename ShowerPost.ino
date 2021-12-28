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
#include "src\AnalogSensor.h"
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
constexpr uint8_t PIN_GALLET        = A0;           // Галетник на пине А0

constexpr uint8_t PIN_LEFT_ENCODER_BUTTON   = 2;    // Кнопка левого энкодера
constexpr uint8_t PIN_LEFT_ENCODER_LEFT     = 4;    // кнопка направления влево левого энкодера
constexpr uint8_t PIN_LEFT_ENCODER_RIGHT    = 3;    // кнопка направления вправо левого энкодера



constexpr uint16_t msg_SoftTimerStart   = 0x110;
constexpr uint16_t msg_SoftTimerEnds    = 0x111;
constexpr uint16_t msg_DisplayChange    = 0x112;    // сменить информацию на дисплее
constexpr uint16_t msg_GalletGhanged    = 0x113;    // галетник переключился

/// -------------------------------------------------------------------------------------
///
/// Блок светодиода активности 
/// 
constexpr uint32_t LED_ALIVE_PERIOD     = 5000;
constexpr uint32_t LED_ALIVE_ON_TIME    = 100;
constexpr uint32_t LED_ALIVE_OFF_TIME   = LED_ALIVE_PERIOD - LED_ALIVE_ON_TIME;

TLed ledAlive(PIN_LED_ALIVE, ACTIVE_HIGH);

THandle hTimerAlive = INVALID_HANDLE;

// --------------------------------------------------------------------------------------
//
//
//
constexpr uint32_t BEEP_CHANGE_STATE = 300;

TDigitalDevice Beeper(PIN_BEEPER);

THandle hTimerBeeper = INVALID_HANDLE;

void  Beep(const uint32_t ADuration);

/// -------------------------------------------------------------------------------------
///
/// Блок часов
/// 
/// 
THandle hTimerColon = INVALID_HANDLE;

TDateTime Now;

TDS3231 Clock;

/// ---------------------------------------------------------------------------------------
/// 
///  Блок TM1637
///
TM1637 Disp(PIN_1637_CLOCK, PIN_1637_DATA, enTM1637Type::Time);

void Display(void);

// ----------------------------------------------------------------------------------------
//
//  Галетный переключатель
//
// Test mine {0,195,322,453,663,689}
// { 0,209,337,430,498 }

constexpr uint16_t GALLET_DELTA = 5;
#ifdef MINE
const uint16_t GALLET_VALUES[] = { 689,663,453,322,195,GALLET_DELTA };
#else
const uint16_t GALLET_VALUES[] = { GALLET_DELTA,209,337,430,498 };
#endif
const uint8_t  GALLET_VALUES_SIZE = sizeof(GALLET_VALUES) / sizeof(GALLET_VALUES[0]);

uint8_t GalletCurrentValue = 0xFF;

int16_t GetGalletIndex(const uint16_t AValue);

TAnalogSensor GalletSwitch(PIN_GALLET, true);

// --------------------------------------------------------------------------------------
//
//  Блок левого энкодера
// 
TEncoder LeftEncoder(4, 3, 2);


// --------------------------------------------------------------------------------------
//
//  Программа
//
//
constexpr uint32_t SETUP_TIMEOUT = 10000;
constexpr uint32_t COLON_FLASH_TIME = 500;
constexpr uint32_t SHOW_APP_STATE_TIME = 1000;

enum class TAppState : uint8_t { Unknown = 0xFF, Stop = 0, Hand = 1, Prog2 = 2, Off = 3, Prog1 = 4, Heat = 5 };
const char* AppStateNames[] = { "StOP","HAnd","Pr 2","OFF", "Pr 1", "HEAt" };

THandle hTimerShowAppState = INVALID_HANDLE;
THandle hTimerTimeout = INVALID_HANDLE;

TAppState  AppState = TAppState::Unknown;

void  SetAppState(const TAppState ANewState);

bool ShowAppStateName = false;

bool SetupMode = false;
bool Flashing = false;
uint8_t FlashIndex = 0;

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

    ledAlive.On();

    hTimerAlive = Timers.Add(LED_ALIVE_ON_TIME, TTimerState::Running);
    hTimerShowAppState = Timers.Add(SHOW_APP_STATE_TIME, TTimerState::Stopped);
    hTimerColon = Timers.Add(COLON_FLASH_TIME, TTimerState::Running);
    hTimerBeeper = Timers.Add(BEEP_CHANGE_STATE, TTimerState::Stopped);
    hTimerTimeout = Timers.Add(SETUP_TIMEOUT, TTimerState::Stopped);

    GalletSwitch.SetReadInterval(200);
    GalletSwitch.SetGap(10);

    Disp.Clear();
    Disp.SetBrightness(7);

//    Clock.SetTime(__TIME__);  // первоначальная настройка часов

}

void loop() {                   // бесконечный цикл

    Clock.Read();

    GalletSwitch.Read();

    LeftEncoder.Read();

    if (MessageList.Available()) Dispatch(MessageList.GetMessage());
}


void Beep(const uint32_t ADuration)
{
    Timers.Reset(hTimerBeeper);
    //Beeper.On();
    puts("Beep On");
}

//
//  функция отображения, отображает время или температуру, в зав-ти от состояния
//
void Display() {
    if (ShowAppStateName) {
        Disp.ShowPoint(false);
        Timers.Stop(hTimerColon);
        uint8_t idx = static_cast<uint8_t>(AppState);
        Disp.Print(AppStateNames[idx]);
        Timers.Reset(hTimerShowAppState);
        return;
    }
    Disp.Clear();

    switch (AppState)
    {
    case TAppState::Unknown:
        break;
    case TAppState::Stop:
        break;
    case TAppState::Hand:
        break;
    case TAppState::Prog2:
        break;
    case TAppState::Off:
        Disp.PrintTime(Now.tm_hour, Now.tm_min);
        if (SetupMode && Flashing) {
            Disp.PrintAt(0, ' ');
            Disp.PrintAt(1, ' ');
        }
        break;
    case TAppState::Prog1:
        break;
    case TAppState::Heat:
        break;
    default:
        break;
    }
}


int16_t GetGalletIndex(const uint16_t AValue) {

    for (uint8_t i = 0; i < GALLET_VALUES_SIZE; ++i) {
        uint16_t min = GALLET_VALUES[i] - 5;
        uint16_t max = GALLET_VALUES[i] + 5;
        if ((AValue >= min) && (AValue <= max)) return i;
    }

    return INVALID_INDEX;
}

void SetAppState(const TAppState ANewState)
{
    if (AppState == ANewState) return;
    AppState = ANewState;
    ShowAppStateName = true;
    Display();
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

    case msg_TimerEnd: {
        
        if (Msg.LoParam == hTimerAlive) {
            ledAlive.Toggle();
            uint32_t newInterval = ledAlive.isOn() ? LED_ALIVE_ON_TIME : LED_ALIVE_OFF_TIME;
            Timers.SetNewInterval(hTimerAlive, newInterval);
        }

        if (Msg.LoParam == hTimerBeeper) {
//            Beeper.Off();
            puts("Beep Off");
            Timers.Stop(hTimerBeeper);
        }

        if (Msg.LoParam == hTimerShowAppState) {
            Timers.Stop(hTimerShowAppState);
            ShowAppStateName = false;
            if (AppState == TAppState::Off) {
                Timers.Reset(hTimerColon);
            }
            else {
//                Timers.Stop(hTimerColon);
                Disp.ShowPoint(false);
            }

            Display();
        }

        if (Msg.LoParam==hTimerColon){
            if (SetupMode) {
                Flashing = !Flashing;
                Display();
            }
            if (AppState == TAppState::Off) Disp.ToggleColon();
        }

        if (Msg.LoParam == hTimerTimeout) {
            Timers.Stop(hTimerTimeout);
            SetupMode = false;
            Flashing = false;
            Display();
        }

        break;
    }

    case msg_EncoderBtnLong: {
        Beep(150);
        SetupMode = !SetupMode;
        Timers.Reset(hTimerColon);
        if (SetupMode) Timers.Reset(hTimerTimeout);
        Flashing = !SetupMode;
        Display();
        break;
    }

    case msg_SensorValueChanged: {
        int16_t gValue = GetGalletIndex(Msg.LoParam);
        if (GalletCurrentValue != gValue) SendMessage(msg_GalletGhanged, gValue);
        break;
    }
    case msg_SecondsChanged:
        break;

    case msg_TimeChanged: {
        Now = Clock.GetTime();
        if (AppState == TAppState::Off) Display();
        break;
    }

    case msg_DateChanged:
    case msg_TempChanged:
        break;

    case msg_GalletGhanged: {
        int16_t galValue = Msg.LoParam;
        if (galValue != INVALID_INDEX) {
            Beep(BEEP_CHANGE_STATE);
            TAppState newstate = static_cast<TAppState>(galValue + 1);
            SetAppState(newstate);
        }
        break; }

    default: // если мы пропустили какое сообщение, этот блок выведет в сериал его номер
        printf("Unhandled message 0x%X\n", Msg.Message);
        break;
    }
}
