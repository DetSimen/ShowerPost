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

template<class T, const uint8_t N> constexpr uint8_t ArraySize(const T(&)[N]) { return N; };

#pragma endregion

#pragma region Interface


TMessageList MessageList(12);   // очередь глубиной 12 сообщений
THardTimers  Timers;            // Таймеры, 10 штук

//-----------------------------------------------------------------------------------------
//
//          Используемые пины
//
constexpr uint8_t PIN_LED_ALIVE     = LED_BUILTIN;  // встроенный светодиод на 13м пине Uno
constexpr uint8_t PIN_1637_CLOCK            = 12;   // Тактирующий вход дисплея ТМ1637
constexpr uint8_t PIN_1637_DATA             = 11;   // Вход данных дисплея ТМ1637
constexpr uint8_t PIN_TEMP_SENSOR           = 10;   // Вход датчика DS18B20
constexpr uint8_t PIN_HEATER_RELAY          = 9;    // Пин нагревательного реле


constexpr uint8_t PIN_SCL                   = A5;   // I2C Clock DS3231
constexpr uint8_t PIN_SDA                   = A4;   // I2C Data DS3231
constexpr uint8_t PIN_BEEPER                = A3;   // Зуммер, + на пин А3, минус на GND

constexpr uint8_t PIN_GALLET                = A0;   // Галетник на пине А0

constexpr uint8_t PIN_R_ENCODER_LEFT        = 5;    // пин направления влево правого энкодера
constexpr uint8_t PIN_R_ENCODER_RIGHT       = 7;    // пин направления вправо правого энкодера
constexpr uint8_t PIN_R_ENCODER_BUTTON      = 6;    // Кнопка правого энкодера

constexpr uint8_t PIN_LEFT_ENCODER_LEFT     = 4;    // кнопка направления влево левого энкодера
constexpr uint8_t PIN_LEFT_ENCODER_RIGHT    = 2;    // кнопка направления вправо левого энкодера
constexpr uint8_t PIN_LEFT_ENCODER_BUTTON   = 3;    // Кнопка левого энкодера



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
constexpr uint16_t msg_SetTimer         = 0x12C;

/// -------------------------------------------------------------------------------------
///
/// Блок светодиода активности 
/// 
constexpr uint32_t LED_ALIVE_PERIOD     = 5000; // Период мигания светодиода активности
constexpr uint32_t LED_ALIVE_ON_TIME    = 100;  // Горит 100 мс, остальное время - не горит
constexpr uint32_t LED_ALIVE_OFF_TIME   = LED_ALIVE_PERIOD - LED_ALIVE_ON_TIME;

TLed ledAlive(PIN_LED_ALIVE, ACTIVE_HIGH); // обьект светодиода

THandle hTimerAlive = INVALID_HANDLE;       // его таймер переключения

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
// 
// 

constexpr uint16_t GALLET_DELTA = 10;  // разброс показаний галетника плюс минус эта величина
#ifdef MINE
const uint16_t GALLET_VALUES[] = { 689,663,453,322,195,GALLET_DELTA }; // это мои значения галетника
#else
const uint16_t GALLET_VALUES[] = { GALLET_DELTA,217,351,446,518 };      // это твои
#endif

constexpr uint8_t GALLET_VALUES_SIZE = ArraySize(GALLET_VALUES);

uint8_t GalletCurrentIndex = 0xFF;

int16_t GetGalletIndex(const uint16_t AValue);

TAnalogSensor GalletSwitch(PIN_GALLET, true);

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

constexpr   uint16_t MAX_TIMER_VALUE = 9999;

enum class THeaterMode: uint8_t {Temp, MaxTemp, Timer};

THeaterMode HeaterMode = THeaterMode::Temp;

bool TimerStarted = false;

uint16_t TimerCurrentValue = 0;

TDigitalDevice RelayHeater(PIN_HEATER_RELAY, ACTIVE_LOW);

#pragma endregion

#pragma region Program Settings

// --------------------------------------------------------------------------------------
//
//  Программа
//
//
constexpr uint32_t SETUP_TIMEOUT        = 10000;    // таймаут установки значений
constexpr uint32_t COLON_FLASH_TIME     = 500;      // частота мигания двоеточием, полсекунды
constexpr uint32_t SHOW_APP_STATE_TIME  = 1500;     // длительность показа названий режимов
constexpr uint16_t TIMER_DEFAULT        = 30;

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


//-----------------------------------------------------------------------------------------
//
//  Прототипы функций
//
void Dispatch(const TMessage& Msg);  // функция обработки сообщений, прототип
void Stop(void);
void DisplayTimer(const uint16_t AValue);


#pragma endregion

//-----------------------------------------------------------------------------------------
//
//  implementation
//

int srl_putchar(char ch, FILE* F) { // служебная функция вывода в сериал
    return Serial.print(ch);
}

void setup() {                                  // начальные настройки
    Serial.begin(256000);                       // заводим Serial
    stdout = fdevopen(srl_putchar, NULL);       // перенаправляем в него весь вывод программы
    delay(200);
    puts("Program ShowerPost v1.0 started..."); // и выводим туда приветственное сообщение

    ledAlive.On();

    hTimerAlive     = Timers.Add(LED_ALIVE_ON_TIME, TTimerState::Running);
    hTimerColon     = Timers.Add(COLON_FLASH_TIME, TTimerState::Running);
    hTimerBeeper    = Timers.Add(BEEP_CHANGE_STATE, TTimerState::Stopped);
    hTimerTimeOut   = Timers.Add(SHOW_APP_STATE_TIME, TTimerState::Stopped);

    AppState = TAppState::Unknown;

    GalletSwitch.SetReadInterval(250);
    GalletSwitch.SetGap(10);

    Disp.Clear();
    Disp.SetBrightness(2);

//    Clock.SetTime(__TIME__);  // первоначальная настройка часов

//    puts(__TIME__);
}

void loop() {                   // бесконечный цикл

    TempSensor.Read();

    Clock.Read();

    GalletSwitch.Read();

    LeftEncoder.Read();

    RightEncoder.Read();

    if (MessageList.Available()) Dispatch(MessageList.GetMessage());
}

void Beep(const uint32_t ADuration)
{
#ifdef DEBUG
    puts("Beep ON");
#else
    Beeper.On();
#endif
    Timers.Reset(hTimerBeeper);
}

void Display() {
    constexpr char SPACE_SYMBOL = ' ';

    if (ShowModeName) return;

    Disp.Clear();

    Disp.ShowPoint(false);

    switch (AppState)
    {
    case TAppState::Prog1:
        break;
    case TAppState::Hand:
        break;
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
        if (HeaterMode == THeaterMode::MaxTemp) Disp.PrintDeg(MaxTemperature);
        if (HeaterMode == THeaterMode::Timer) DisplayTimer(TimerCurrentValue);
        Disp.ShowPoint(HeaterMode == THeaterMode::MaxTemp);
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
    sprintf(buf, "%04d", AValue);
    Disp.Print(buf);
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
        uint16_t min = GALLET_VALUES[i] - GALLET_DELTA;
        uint16_t max = GALLET_VALUES[i] + GALLET_DELTA;
        if ((AValue >= min) && (AValue <= max)) return i;
    }

    return INVALID_INDEX;
}

void SetAppState(const TAppState ANewAppState)
{
    if (AppState == ANewAppState) return;
    AppState = ANewAppState;

    Beep(BEEP_CHANGE_STATE);

    DisplayModeName(AppState);

    Stop();

    switch (AppState)
    {
    case TAppState::Unknown: // если мы сюда попадаем, даже случайно, это ошибка
        SetAppState(TAppState::Error);
        break;

    case TAppState::Heat: {
        HeaterMode = THeaterMode::Temp;
        MaxTemperature = 0;// ABSOLUTE_MIN_TEMP;
        TimerCurrentValue = 30;
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
        puts("Hand");
        break;

    case TAppState::Error: {
//        Stop();
        puts("Error");
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
    }

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

        if (Msg.LoParam == hTimerColon) {
            Flashing = !Flashing;

            if ((AppState == TAppState::Clock) && (!ShowModeName))
                Disp.ToggleColon();
//            else
//                Disp.ShowPoint(false);

            if (SetupMode) Display();
        }

        if (Msg.LoParam == hTimerTimeOut) {
            
            if (ShowModeName) {
                ShowModeName = false;
                Display();
            }

            if (SetupMode) {
                SetupMode = false;
                SendMessage(msg_Paint);
            }

            Timers.Stop(hTimerTimeOut);
        }

        break;
    }

    case msg_TempChanged: {
        if (Msg.Sender == TempSensor) {
            CurrentTemperature = TempSensor.GetTemperature();
            Display();
        }
        break;
    }
                                
    case msg_SensorValueChanged: {
        if (Msg.Sender == GalletSwitch) {
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

    case msg_SecondsChanged:
        break;

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
            if (HeaterMode == THeaterMode::Timer) SendMessage(msg_SetTimer, Msg.LoParam);
        }
        break;
    }

    case msg_RightEncoderLong: {
        if (AppState == TAppState::Heat) {
            if (SetupMode)
                SendMessage(msg_HeatSetupExit);
            else
                SendMessage(msg_HeatSetupEnter);
        }
        break;
    }

    case msg_HeatSetupEnter: {
        if (HeaterMode == THeaterMode::Temp) break;
        SetupMode = true;
        FlashIndex = -1;
        TimerCurrentValue = TIMER_DEFAULT;
        MaxTemperature = CurrentTemperature;
        Timers.SetNewInterval(hTimerTimeOut, SETUP_TIMEOUT);
        Display();
        break;
    }

    case msg_HeatSetupExit: {
        SetupMode = false;
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

    case msg_SetTimer: {
        int8_t step = Msg.LoParam;
        int16_t value = static_cast<int16_t>(TimerCurrentValue);
        char  buf[5];
        sprintf(buf, "%04d", value);
        if (FlashIndex < 0) {
            value += step;
            if (value < 0) value = 0;;
            if (value > int16_t(MAX_TIMER_VALUE)) value = MAX_TIMER_VALUE;
        } 
        else {
            char ch = buf[FlashIndex];
            ch += step;
            if (ch < '0') ch = '9';
            if (ch > '9') ch = '0';
            buf[FlashIndex] = ch;
            value = atoi(buf);
        }

        TimerCurrentValue = value;
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
    TimerCurrentValue = 0;
    MaxTemperature = ABSOLUTE_MIN_TEMP;
    RelayHeater.Off();
}
