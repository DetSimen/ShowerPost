#pragma region Include

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
#include "src\PCF8574.h"
#include "DEF_Pins.h"



#pragma endregion

#pragma region Interface


TMessageList MessageList(12);   // очередь глубиной 12 сообщений
THardTimers  Timers;            // Таймеры, 10 штук

#pragma region Messages

constexpr uint16_t msg_SoftTimerStart   = 0x110;    // запустить таймер обратного отсчёта
constexpr uint16_t msg_SoftTimerEnds    = 0x111;    // таймер обратного отсчета закончился
//constexpr uint16_t msg_DisplayChange    = 0x112;    // сменить информацию на дисплее
constexpr uint16_t msg_GalletGhanged    = 0x113;    // галетник переключился
constexpr uint16_t msg_EnterClockSetup  = 0x114;    // вход в режим установки часов
constexpr uint16_t msg_ExitClockSetup   = 0x115;    // выход из режима установки часов
constexpr uint16_t msg_SetTemperature   = 0x116;    // вход в режим установки макс. температуры
constexpr uint16_t msg_EnterTempSetup   = 0x117;
constexpr uint16_t msg_ExitTempSetup    = 0x118;
constexpr uint16_t msg_LeftEncoderLong  = 0x120;
constexpr uint16_t msg_RightEncoderLong = 0x121;
constexpr uint16_t msg_LeftEncoderClick = 0x122;
constexpr uint16_t msg_RightEncoderClick = 0x123;
constexpr uint16_t msg_ClockNextFlash   = 0x124;
constexpr uint16_t msg_LeftEncoderMove  = 0x125;
constexpr uint16_t msg_RightEncoderMove = 0x126;  // правый энкодер повращался
constexpr uint16_t msg_NextHeatState    = 0x127;
constexpr uint16_t msg_HeatSetupEnter   = 0x128;
constexpr uint16_t msg_HeatSetupExit    = 0x129;
constexpr uint16_t msg_NextFlashIndex   = 0x12A;
constexpr uint16_t msg_SetMaxTemp       = 0x12B;
constexpr uint16_t msg_TimerHeatSet     = 0x12C;
constexpr uint16_t msg_TimerHeatStart   = 0x12D; 
constexpr uint16_t msg_TimerHeatPause   = 0x12E;
constexpr uint16_t msg_TimerHeatStop    = 0x12F;
constexpr uint16_t msg_DisplayNext      = 0x130;  // показать след. экран в режиме нагревателя

#pragma endregion

#pragma region LedAlive

/// -------------------------------------------------------------------------------------
///
/// Блок светодиода активности 
/// 
constexpr uint32_t LED_ALIVE_PERIOD     = 5000; // Период мигания светодиода активности
constexpr uint32_t LED_ALIVE_ON_TIME    = 100;  // Горит 100 мс, остальное время - не горит
constexpr uint32_t LED_ALIVE_OFF_TIME   = LED_ALIVE_PERIOD - LED_ALIVE_ON_TIME;

TLed ledAlive(PIN_LED_ALIVE, ACTIVE_HIGH); // обьект светодиода

THandle hTimerAlive = INVALID_HANDLE;       // его таймер переключения

#pragma endregion

#pragma region Beeper

// --------------------------------------------------------------------------------------
//
//  Зуммер
//
constexpr uint32_t BEEP_CHANGE_STATE    = 300; //
constexpr uint32_t BEEP_SETUP_END       = 200;
constexpr uint32_t BEEP_CHIRP           = 50;

TDigitalDevice Beeper(PIN_BEEPER);  // зуммер

THandle hTimerBeeper = INVALID_HANDLE;  // таймер зуммера

void  Beep(const uint32_t ADuration);  // функция включения зуммера

#pragma endregion

#pragma region "RTC Clock"

constexpr int8_t MIN_HOUR = 0;
constexpr int8_t MAX_HOUR = 23;
constexpr int8_t MIN_MINUTE = 0;
constexpr int8_t MAX_MINUTE = 59;

THandle hTimerColon = INVALID_HANDLE; // таймер мигания двоеточием

TDateTime Now;      // здесь хранится текущее время
TDateTime SetTime;  // здесь хранится устанавливаемое время

TDS3231 Clock; // объект часов

// void SaveTime(const TDateTime ATime); // сохранение времени в модуле часов

#pragma endregion

#pragma region "Экранчик TM1637"

TM1637 Disp(PIN_1637_CLOCK, PIN_1637_DATA, enTM1637Type::Time); // Дисплейчик

void Display(void);     // Функция отображения

#pragma endregion

#pragma region "Датчик температуры DS18B20"

/// -----------------------------------------------------------------
///
///  Блок даччика температуры
///
///  
constexpr   int8_t ABSOLUTE_MIN_TEMP = 20;
constexpr   int8_t ABSOLUTE_MAX_TEMP = 110;

TDS18B20    TempSensor(PIN_TEMP_SENSOR); // сам сенсор

int8_t      CurrentTemperature = INVALID_TEMPERATURE; // текущая температура

int8_t      MaxTemperature = ABSOLUTE_MIN_TEMP; // Устанавливаемая температура                    

#pragma endregion

#pragma region Gallet

// ------------------------------------------------------------------
//
//  Галетный переключатель
//
constexpr uint8_t GALLET_VALUES[] = { 1,2,0,4,8 };
constexpr uint8_t GALLET_VALUES_SIZE = ArraySize(GALLET_VALUES);

Tpcf8574 Gallet(GALLET_ADDRESS, 0xFF);

#pragma endregion

#pragma region "Энкодеры, левый и правый"
// указатели на функции

using PEncoderClickFunc = void(*)(void);
using PEncoderLongFunc = void(*)(void);
using PEncoderMoveFunc = void(*)(const int8_t);

PEncoderClickFunc onLeftClick   = NULL;
PEncoderLongFunc  onLeftLong    = NULL;
PEncoderMoveFunc  onLeftMove    = NULL;

PEncoderClickFunc onRightClick  = NULL;
PEncoderLongFunc  onRightLong   = NULL;
PEncoderMoveFunc  onRightMove   = NULL ;


TEncoder LeftEncoder(PIN_LEFT_ENCODER_LEFT,PIN_LEFT_ENCODER_RIGHT, PIN_LEFT_ENCODER_BUTTON);
TEncoder RightEncoder(PIN_R_ENCODER_LEFT, PIN_R_ENCODER_RIGHT, PIN_R_ENCODER_BUTTON);

#pragma endregion

#pragma region Heater

constexpr   uint16_t MAX_TIMER_VALUE    = 9999;
constexpr   int8_t   DELTA_TEMP         = 5;    // гистерезис температуры
constexpr   int8_t   MIN_VENT_TEMP      = 30;   // мин. температура вкл вентиляторов

constexpr   uint32_t DISPLAY_TIMER      = 10000;    // показывать таймер с обр. отсчетом
constexpr   uint32_t DISPLAY_TEMP       = 1500;     // показывать температуру

enum class THeaterMode: uint8_t {Temp, MaxTemp, Timer};

THeaterMode HeaterMode = THeaterMode::Temp;

bool TimerStarted = false;

enum class THeatTimerState : uint8_t { Unknown = 0xFF, Run = 0, Pause = 1, Stop = 2, Error = 3 };
THeatTimerState TimerState = THeatTimerState::Unknown;

void SetTimerState(const THeatTimerState ANewState);

uint16_t TimerCurrentValue = 0;

TDigitalDevice HeaterRelay(PIN_HEATER_RELAY, ACTIVE_LOW);
TDigitalDevice VentRelay(PIN_VENT_RELAY, ACTIVE_LOW);

#pragma endregion

#pragma region Program Settings

// --------------------------------------------------------------------------------------
//
//  Программа
//
//
constexpr uint32_t SETUP_TIMEOUT        = 7500;     // таймаут установки значений
constexpr uint32_t COLON_FLASH_TIME     = 500;      // частота мигания двоеточием, полсекунды
constexpr uint32_t SHOW_APP_STATE_TIME  = 1500;     // длительность показа названий режимов  1.5 секунды
constexpr uint16_t TIMER_DEFAULT        = 30;       // таймер нагревателя, нач. значение 30 сек

// состояния программы
//
enum class TAppState : uint8_t {
    Unknown = 0xFF, // не определено
    Heat    = 0x00, // Программируемый режим 1
    Prog1   = 0x01, // Ручное управление
    Off     = 0x02, // Выключено
    Clock   = 0x02, // То же, что и Выкл, но понятнее в тексте
    Prog2   = 0x03, // Программируемый режим 2
    Hand    = 0x04, // Нагрев
    Error   = 0x06  // Ошибка, мало ли что
};

const char* const AppStateNames[] = { "HEAt", "Pr 1", "OFF", "Pr 2", "HAnd", "Err" };

TAppState AppState = TAppState::Unknown;

THandle hTimerTimeOut = INVALID_HANDLE;

void SetAppState(const TAppState ANewAppState);

bool ShowModeName   = false;
bool Flashing       = false;

int8_t  FlashIndex  = -1;
bool    SetupMode   = false;

enum class TTimerMode: uint8_t {Seconds, Minutes};

TTimerMode TimerMode = TTimerMode::Seconds;


#pragma endregion

#pragma region Hand Mode

enum class THandMode: uint8_t {Timer=0, Rotate=1};

constexpr uint16_t HAND_TIMER_DEFAULT   = 30;   // Таймер  по умолчанию 30 секунд 
constexpr uint16_t HAND_ROTATE_DEFAULT  = 40;   // Обороты по умолчанию 40 об/мин
constexpr uint16_t HAND_ROTATE_MAX      = 1800; // Макс обороты

THandMode HandMode = THandMode::Timer;
uint16_t  RotateCurrentValue = HAND_ROTATE_DEFAULT;

#pragma endregion


//-----------------------------------------------------------------------------------------
//
//  Прототипы функций
//
void Dispatch(const TMessage& Msg);  // функция обработки сообщений, прототип
void Stop(void);
void DisplayTimer(const uint16_t AValue);
void StartHeating(void);
void StopHeating(void);


#pragma endregion

//-----------------------------------------------------------------------------------------
//
//  implementation
//

void setup() {                                  // начальные настройки
    Serial.begin(256000);                       // заводим Serial
    stdout = fdevopen([](char ch, FILE* f)->int {return Serial.print(ch); }, NULL);       // перенаправляем в него весь вывод программы
    delay(200);
    puts("Program ShowerPost v1.0 started..."); // и выводим туда приветственное сообщение

    analogReference(EXTERNAL);

    ledAlive.On(); // светодиод активности можно сразу зажечь

    hTimerAlive     = Timers.Add(LED_ALIVE_ON_TIME, TTimerState::Running);  // таймер мигания светодиода активности
    hTimerColon     = Timers.Add(COLON_FLASH_TIME, TTimerState::Running);   // таймер мигания двоеточием и цифирками при установке
    hTimerBeeper    = Timers.Add(BEEP_CHANGE_STATE, TTimerState::Stopped);  // таймер для зуммера, надо же знать, когда его выключить
    hTimerTimeOut   = Timers.Add(SHOW_APP_STATE_TIME, TTimerState::Stopped);  // таймер для разных таймаутов

    AppState = TAppState::Unknown;  // для последующего перехода в правильное состояние

    Disp.Clear();
    Disp.SetBrightness(7);          // яркость TM1637

//    Clock.SetTime(__TIME__);      // первоначальная настройка часов

//    puts(__TIME__);
}

void loop() {                   // главный цыкал приложения

    TempSensor.Read();          // читаем темп. даччик

    Clock.Read();               // читаем часы 3231

    Gallet.Read();              // читаем галетник

    LeftEncoder.Read();         // читаем левый энкодер

    RightEncoder.Read();        // читаем правый энкодер

    // если кто-то из них наклал чонить в очередь сообщений
    // передаем сообщение диспеччеру

    if (MessageList.Available()) Dispatch(MessageList.GetMessage());
}

void Beep(const uint32_t ADuration)
{
#ifdef DEBUG
    printf("Beep %ld\n", ADuration);
#else
    Beeper.On();
#endif
    Timers.Reset(hTimerBeeper);
}

void Display() {
    constexpr char SPACE_SYMBOL = ' ';

    if (ShowModeName) return;

//    Disp.Clear();

    Disp.ShowPoint(false);

    switch (AppState)
    {
    case TAppState::Prog1:
        break;

    case TAppState::Hand: {
        if (!SetupMode) {
            switch (HandMode)
            {
            case THandMode::Timer:
                DisplayTimer(TimerCurrentValue);
                break;
            case THandMode::Rotate:
                Disp.Print(RotateCurrentValue);
                Disp.PrintAt(0, 'r');
                break;
            default:
                break;
            }
        }
        break;
    }

    case TAppState::Clock: {
        if (SetupMode)
            Disp.PrintTime(SetTime.tm_hour, SetTime.tm_min);
        else
            Disp.PrintTime(Now.tm_hour, Now.tm_min);

        if (SetupMode && Flashing) {
            if (FlashIndex < 0) Disp.Clear();
            if (FlashIndex == 0) {
                Disp.PrintAt(0, SPACE_SYMBOL);
                Disp.PrintAt(1, SPACE_SYMBOL);
            }
            if (FlashIndex == 1) {
                Disp.PrintAt(2, SPACE_SYMBOL);
                Disp.PrintAt(3, SPACE_SYMBOL);
            }
        }
        break;
    }
    case TAppState::Prog2:
        break;
    case TAppState::Heat: {

        if (HeaterMode == THeaterMode::Temp) Disp.PrintDeg(CurrentTemperature);
 
        if (HeaterMode == THeaterMode::MaxTemp) {
            Disp.PrintDeg(MaxTemperature);
            Disp.PrintAt(3,'%'); // подчёркивание в режиме макс. Температуры
        }

        if (HeaterMode == THeaterMode::Timer) DisplayTimer(TimerCurrentValue);
      
        if (SetupMode && Flashing) {
            if (FlashIndex < 0)
                Disp.Clear();
            else
                Disp.PrintAt(FlashIndex, SPACE_SYMBOL);
        }
        break;
    }
    default:
        break;
    }

}

void DisplayTimer(const uint16_t AValue) {
    
    char buf[5];

    if (TimerStarted && TimerCurrentValue == 0) {
        Disp.Print("StOP");
        return;
    }

    if (TimerMode == TTimerMode::Seconds) {
        if (SetupMode)
            sprintf(buf, "%04d", AValue);
        else
            sprintf(buf, "%4d", AValue);
        Disp.Print(buf);
    };

    if (TimerMode == TTimerMode::Minutes) {
        uint8_t m = AValue / 60;
        uint8_t s = AValue % 60;
        if (SetupMode) {
            Disp.PrintTime(m, s);
        }
        else {
            sprintf(&buf[0], "%2d", m);
            sprintf(&buf[2], "%02d", s);
            Disp.Print(buf);
        }
    }

    if (TimerState == THeatTimerState::Pause) {
       if(!SetupMode) Disp.PrintAt(0, 'P');
    }

    Disp.ShowPoint(true);

}

void DisplayModeName(TAppState AState) {
    uint8_t max = static_cast<uint8_t>(TAppState::Error);
    uint8_t idx = static_cast<uint8_t>(AState);
    Disp.Clear();
    if (idx <= max) {
        ShowModeName = true;
        Disp.Print(AppStateNames[idx]);
        Timers.SetNewInterval(hTimerTimeOut, SHOW_APP_STATE_TIME);
        Timers.Reset(hTimerTimeOut);
    }
}

int16_t GetGalletIndex(const uint16_t AValue) {

    for (uint8_t i = 0; i < GALLET_VALUES_SIZE; ++i) {
        if (GALLET_VALUES[i] == AValue) return i;
    }

    return INVALID_INDEX;
}

void SetTimerState(const THeatTimerState ANewState)
{
    if (TimerState == ANewState) return;
    TimerState = ANewState;

    switch (TimerState)
    {
    case THeatTimerState::Unknown:
        SetTimerState(THeatTimerState::Error); // это апшыпка
        break;

    case THeatTimerState::Run:
        if (TimerCurrentValue > 0) { // Включаем нагрев и вентиляторы
            TimerStarted = true;
            HeaterMode = THeaterMode::Timer;
            StartHeating();
            Timers.SetNewInterval(hTimerTimeOut, DISPLAY_TIMER);
            Timers.Reset(hTimerTimeOut);
        }
        break;

    case THeatTimerState::Pause: // Пауза. Отключаем нагрев и вентиляторы
        TimerStarted = false;
        StopHeating();
        break;

    case THeatTimerState::Stop: // Режим "Стоп" отключаем нагрев и вентиляторы
        Stop();
        HeaterMode = THeaterMode::Timer;
        Display();
        break;

    case THeatTimerState::Error:
        Disp.Print("Er H");  // Ошибка. Отключаем всё, выводим на Дисплей "Er H" и зависаем 
                             // до перезагрузки   
        Stop();
        ledAlive.On();
        cli();
        abort();
        break;

    default:
        SetTimerState(THeatTimerState::Error); // если попали сюда, это апшыпка. 
        break;
    }

    Display();
}

void SetAppState(const TAppState ANewAppState)
{
    if (AppState == ANewAppState) return;
    AppState = ANewAppState;

    Beep(BEEP_CHANGE_STATE);

    DisplayModeName(AppState);

    SetTimerState(THeatTimerState::Stop);
    

    switch (AppState)
    {
    case TAppState::Unknown: // если мы сюда попадаем, даже случайно, это ошибка
        SetAppState(TAppState::Error);
        break;

    case TAppState::Heat: {
        HeaterMode = THeaterMode::Temp;
        MaxTemperature = CurrentTemperature;// ABSOLUTE_MIN_TEMP;
        TimerCurrentValue = TIMER_DEFAULT;
        TimerStarted = false;
        break;
    }
    case TAppState::Prog1:
        puts("Prog 1");
        break;

    case TAppState::Off:
        puts("OFF");
        break;

    case TAppState::Prog2:
        puts("Prog 2");
        break;

    case TAppState::Hand:
        RotateCurrentValue = HAND_ROTATE_DEFAULT;
        TimerCurrentValue = HAND_TIMER_DEFAULT;
        HandMode = THandMode::Timer;
        break;

    case TAppState::Error: {
//        Stop();
        Timers.Stop();
        ledAlive.On();
        Disp.Print("Err");
        cli();
        abort();
        break;
    }
    default:
        break;
    }

    Display();
}

uint16_t SetMinSecTimer(const char* ATimePtr) {
    char temp[4];
    memset(temp, 0, 4);
    strncpy(temp, &ATimePtr[0], 2);
    uint16_t min = atoi(temp);
    strncpy(temp, &ATimePtr[2], 2);
    uint8_t sec = atoi(temp);

    return 60U * min + sec;
}

void CheckMaxTemp(const int8_t ATemp) {
    if (CurrentTemperature >= MaxTemperature) HeaterRelay.Off();

    if ((MaxTemperature - CurrentTemperature) > DELTA_TEMP) HeaterRelay.On();

    if (CurrentTemperature >= MIN_VENT_TEMP)
        VentRelay.On();
    else
        VentRelay.Off();
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

        SetAppState(TAppState::Error);

        break;
    } // Апшыпка, останавливаем всё

    case msg_Paint: {
        Display();
        break;
    }

    case msg_TimerEnd: {  // пришло сообщение от TimerList
        
        if (Msg.LoParam == hTimerAlive) { // таймер мигания светодиодом, 1 короткая вспышка в 5 секунд
            ledAlive.Toggle();
            uint32_t newInterval = ledAlive.isOn() ? LED_ALIVE_ON_TIME : LED_ALIVE_OFF_TIME;
            Timers.SetNewInterval(hTimerAlive, newInterval);
        }

        if (Msg.LoParam == hTimerBeeper) { // таймер активного зуммера, выключает его

            Timers.Stop(hTimerBeeper); // этот таймер пока больше не нужен, останавливаем его

#ifdef DEBUG
            puts("Beep Off");  // на время отладки не пищит, пишет в порт
#else
            Beeper.Off();
#endif
        }

        if (Msg.LoParam == hTimerColon) { // мигаем двоеточием в часах
            Flashing = !Flashing;

            if ((AppState == TAppState::Clock) && (!ShowModeName))
                Disp.ToggleColon();

            if (SetupMode) Display();
        }

        if (Msg.LoParam == hTimerTimeOut) { // один таймер для разных таймаутов
            
            if (ShowModeName) {       // таймаут показа режимов работы
                ShowModeName = false;
                Display();
                break;
            }

            if (SetupMode) {  // таймаут мигания цифр при установке часов, температуры и т.д
                SetupMode = false;
                if (AppState == TAppState::Heat)  SendMessage(msg_HeatSetupExit);
                if (AppState == TAppState::Clock) SendMessage(msg_ExitClockSetup);
            }

            if (TimerStarted) SendMessage(msg_DisplayNext);

            Timers.Stop(hTimerTimeOut); // останавливаем таймер таймаута, до след. применения
        }

        break;
    }

    case msg_TempChanged: {
        if (Msg.Sender == TempSensor) {
            CurrentTemperature = TempSensor.GetTemperature();
            if (MaxTemperature == INVALID_TEMPERATURE) MaxTemperature = CurrentTemperature;
            if (TimerStarted) CheckMaxTemp(CurrentTemperature);
            Display();
        }
        break;
    }
                                
    case msg_SensorValueChanged: {
        if (Msg.Sender == Gallet) {
            int16_t gValue = GetGalletIndex(Msg.LoParam);
            if (gValue>=0) SendMessage(msg_GalletGhanged, gValue);
        }
        break;
    }

    case msg_GalletGhanged: {
        TAppState newState = static_cast<TAppState>(Msg.LoParam);
        SetAppState(newState);
        break;
    }

    case msg_SecondsChanged: {
        if (TimerState == THeatTimerState::Run) {

            if (TimerCurrentValue > 0)
                --TimerCurrentValue;
            else
                SendMessage(msg_TimerHeatStop);

            if (TimerCurrentValue < 60)
                TimerMode = TTimerMode::Seconds;
            else
                TimerMode = TTimerMode::Minutes;

            Display();
        }
        break;
    }

    case msg_TimeChanged: {
        Now = Clock.GetTime();
        Display();
        break;
    }

    case msg_DateChanged: 
        break;

    case msg_EncoderClick: {
        if (Msg.Sender == LeftEncoder) SendMessage(msg_LeftEncoderClick);
        if (Msg.Sender == RightEncoder) SendMessage(msg_RightEncoderClick);
        break;
    }

    case msg_EncoderLong: {
        if (Msg.Sender == LeftEncoder) SendMessage(msg_LeftEncoderLong);
        if (Msg.Sender == RightEncoder) SendMessage(msg_RightEncoderLong);
        break;
    }

    case msg_EncoderLeft:
    case msg_EncoderRight: {
        if (Msg.Sender == LeftEncoder) SendMessage(msg_LeftEncoderMove, Msg.LoParam);
        if (Msg.Sender == RightEncoder) SendMessage(msg_RightEncoderMove, Msg.LoParam);
        break;
    }

    case msg_LeftEncoderLong: {
        if (AppState == TAppState::Clock) {
            if (SetupMode)
                SendMessage(msg_ExitClockSetup);
            else
                SendMessage(msg_EnterClockSetup);
        } 

        if ((AppState == TAppState::Heat) && (!SetupMode)) {
            if (TimerState != THeatTimerState::Run)
                SendMessage(msg_TimerHeatStart);
            else
                SetTimerState(THeatTimerState::Stop);
        }
        break;
    }

    case msg_EnterClockSetup: {
        SetupMode = true;
        FlashIndex = -1;
        SetTime = Clock.GetTime();
        Timers.SetNewInterval(hTimerTimeOut, SETUP_TIMEOUT);
        SendMessage(msg_Paint);
        break;
    }

    case msg_LeftEncoderClick: {
        if (AppState == TAppState::Clock && SetupMode) SendMessage(msg_ClockNextFlash);
        if (AppState == TAppState::Heat) {
            if (SetupMode)
                SendMessage(msg_RightEncoderClick);
            else {

                if (TimerState == THeatTimerState::Run) SendMessage(msg_TimerHeatPause);
                if (TimerState == THeatTimerState::Pause) SendMessage(msg_TimerHeatStart);
            }
        }
        break;
    }

    case msg_RightEncoderClick: {
        if (AppState == TAppState::Heat) {
            if (Timers.isActive(hTimerTimeOut)) Timers.Reset(hTimerTimeOut);
            if (SetupMode)
                SendMessage(msg_NextFlashIndex);
            else
                SendMessage(msg_NextHeatState);
        }
        if (AppState == TAppState::Hand) {
            if (HandMode == THandMode::Timer)
                HandMode = THandMode::Rotate;
            else
                HandMode = THandMode::Timer;
            Display();
        }

        break;
    }

    case msg_NextHeatState: {

            uint8_t max = static_cast<uint8_t>(THeaterMode::Timer);
            uint8_t curr = static_cast<uint8_t>(HeaterMode);
            if (++curr > max) curr = 0;
            HeaterMode = static_cast<THeaterMode>(curr);
            Display();
        
        break;
    }

    case msg_NextFlashIndex: {
        if (SetupMode && HeaterMode == THeaterMode::Timer) FlashIndex++;
        if (FlashIndex > 3) FlashIndex = -1;
        Display();
        break;
    }

    case msg_ClockNextFlash: {
        Timers.Reset(hTimerTimeOut);
        FlashIndex++;
        if (FlashIndex > 1) FlashIndex = -1;
        break;
    }

    case msg_LeftEncoderMove: {
        int8_t step = Msg.LoParam;
        if (AppState == TAppState::Clock && SetupMode) {
            if (Timers.isActive(hTimerTimeOut)) Timers.Reset(hTimerTimeOut);
            if (FlashIndex == 0) {
                SetTime.tm_hour += step;
                if (SetTime.tm_hour < MIN_HOUR ) SetTime.tm_hour = MAX_HOUR;
                if (SetTime.tm_hour > MAX_HOUR) SetTime.tm_hour = MIN_HOUR;
                Display();
            }
            if (FlashIndex == 1) {
                SetTime.tm_min += step;
                if (SetTime.tm_min < MIN_MINUTE) SetTime.tm_min = MAX_MINUTE;
                if (SetTime.tm_min > MAX_MINUTE) SetTime.tm_min = MIN_MINUTE;
                Display();
            }
        }
        if (AppState == TAppState::Heat && SetupMode) SendMessage(msg_RightEncoderMove, Msg.LoParam);
        break;
    }

    case msg_ExitClockSetup: {
        Clock.SetTime(SetTime.tm_hour, SetTime.tm_min);
        Now = Clock.GetTime();
        SetupMode = false;
        Timers.Stop(hTimerTimeOut);
        Display();
        break;
    }

    case msg_RightEncoderMove: {
        if (Timers.isActive(hTimerTimeOut)) Timers.Reset(hTimerTimeOut);
        if (SetupMode) {
            if (HeaterMode == THeaterMode::MaxTemp) SendMessage(msg_SetMaxTemp, Msg.LoParam);
            if (HeaterMode == THeaterMode::Timer) SendMessage(msg_TimerHeatSet, Msg.LoParam);
        }
        break;
    }

    case msg_RightEncoderLong: {
        if (AppState == TAppState::Heat) {
            if (SetupMode)
                SendMessage(msg_HeatSetupExit);
            else {
                if (TimerState == THeatTimerState::Run) SendMessage(msg_TimerHeatPause);
                SendMessage(msg_HeatSetupEnter);
            }
        }
        break;
    }

    case msg_HeatSetupEnter: {
        if (HeaterMode == THeaterMode::Temp) break;
        SetupMode = true;
        FlashIndex = -1;
        Timers.SetNewInterval(hTimerTimeOut, SETUP_TIMEOUT);
        Display();
        break;
    }

    case msg_HeatSetupExit: {
        SetupMode = false;
        if (TimerCurrentValue < 60)
            TimerMode = TTimerMode::Seconds;
        else
            TimerMode = TTimerMode::Minutes;
        Display();
        break;
    }

    case msg_SetMaxTemp: {
        int8_t step = Msg.LoParam;
        MaxTemperature += step;
        if (MaxTemperature > ABSOLUTE_MAX_TEMP) MaxTemperature = ABSOLUTE_MAX_TEMP;
        if (MaxTemperature < ABSOLUTE_MIN_TEMP) MaxTemperature = ABSOLUTE_MIN_TEMP;
        Display();
        break;
    }

    case msg_TimerHeatSet: {
        int8_t step = Msg.LoParam;
        int16_t value = static_cast<int16_t>(TimerCurrentValue);
        char  buf[5];
        if (FlashIndex < 0) {
            sprintf(buf, "%04d", value);
            value += step;
            if (value < 0) value = 0;
            if (value > int16_t(MAX_TIMER_VALUE)) value = MAX_TIMER_VALUE;
            value = atoi(buf);
            if (value > 59)
                TimerMode = TTimerMode::Minutes;
            else
                TimerMode = TTimerMode::Seconds;
        } 
        else {
            TimerMode = TTimerMode::Minutes;
            sprintf(&buf[0], "%02d", value / 60);
            sprintf(&buf[2], "%02d", value % 60);
            Disp.ShowPoint(true);
            char ch = buf[FlashIndex];
            ch += step;
            if ((FlashIndex & 0x01) == 0x00) {
                if (ch < '0') ch = '5';
                if (ch > '5') ch = '0';
            }
            else {
                if (ch < '0') ch = '9';
                if (ch > '9') ch = '0';
            }
            buf[FlashIndex] = ch;
            value = SetMinSecTimer(buf);
        }

        TimerCurrentValue = value;

        Display();

        break;
    }

    case msg_TimerHeatStart: {
        Beep(300);
        if ((TimerState == THeatTimerState::Stop) || (TimerState == THeatTimerState::Pause))
            SetTimerState(THeatTimerState::Run);
        break;
    }

    case msg_TimerHeatPause: {
        Beep(100);
        SetTimerState(THeatTimerState::Pause);
        break;
    }

    case msg_TimerHeatStop: {
        Beep(1000);
        SetTimerState(THeatTimerState::Stop);
        break;
    }

    case msg_DisplayNext: {

        uint32_t newTime;

        if (TimerCurrentValue < 11) {
            HeaterMode = THeaterMode::Timer;
            Timers.Stop(hTimerTimeOut);
            Display();
            return;
        }

        switch (HeaterMode)
        {
        case THeaterMode::Temp:
            HeaterMode = THeaterMode::MaxTemp;
            newTime = DISPLAY_TEMP;
            break;

        case THeaterMode::MaxTemp:
            HeaterMode = THeaterMode::Timer;
            newTime = DISPLAY_TIMER; ;
            break;

        case THeaterMode::Timer:
            HeaterMode = THeaterMode::Temp;
            newTime = DISPLAY_TEMP;
            break;

        default:
            break;
        }

        Timers.SetNewInterval(hTimerTimeOut, newTime);
        Timers.Reset(hTimerTimeOut);
        Display();
        break;
    }

    default: // если мы пропустили какое сообщение, этот блок выведет в сериал его номер и параметры
        printf("Unhandled message 0x%X, Lo = 0x%X, Hi = 0x%X\n", Msg.Message, Msg.LoParam, Msg.HiParam);
        break;
    }
}

void Stop(void)
{
    TimerStarted = false;
    TimerCurrentValue = TIMER_DEFAULT;
    if (CurrentTemperature!=INVALID_TEMPERATURE) MaxTemperature = CurrentTemperature;

    StopHeating();
}

void StartHeating(void) {
    if (MaxTemperature > CurrentTemperature) HeaterRelay.On();
    if (CurrentTemperature >= MIN_VENT_TEMP) VentRelay.On();
}


void StopHeating(void) {
    HeaterRelay.Off();
    VentRelay.Off();
}
