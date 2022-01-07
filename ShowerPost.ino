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
constexpr uint8_t PIN_1637_CLOCK    = 12;           // Тактирующий вход дисплея ТМ1637
constexpr uint8_t PIN_1637_DATA     = 11;           // Вход данных дисплея ТМ1637
constexpr uint8_t PIN_TEMP_SENSOR   = 10;           // Вход датчика DS18B20
constexpr uint8_t PIN_HEATER_RELAY  = 9;            // Пин нагревательного реле
constexpr uint8_t PIN_BEEPER        = A3;           // Зуммер, + на пин А3, минус на GND
constexpr uint8_t PIN_GALLET        = A0;           // Галетник на пине А0

constexpr uint8_t PIN_LEFT_ENCODER_LEFT     = 4;    // кнопка направления влево левого энкодера
constexpr uint8_t PIN_LEFT_ENCODER_RIGHT    = 3;    // кнопка направления вправо левого энкодера
constexpr uint8_t PIN_LEFT_ENCODER_BUTTON   = 2;    // Кнопка левого энкодера



constexpr uint16_t msg_SoftTimerStart   = 0x110;    // запустить таймер обратного отсчёта
constexpr uint16_t msg_SoftTimerEnds    = 0x111;    // таймер обратного отсчета закончился
constexpr uint16_t msg_DisplayChange    = 0x112;    // сменить информацию на дисплее
constexpr uint16_t msg_GalletGhanged    = 0x113;    // галетник переключился
constexpr uint16_t msg_EnterClockSetup  = 0x114;    // вход в режим установки часов
constexpr uint16_t msg_ExitClockSetup   = 0x115;    // выход из режима установки часов

/// -------------------------------------------------------------------------------------
///
/// Блок светодиода активности 
/// 
constexpr uint32_t LED_ALIVE_PERIOD     = 5000; // Период мигания светодиода активности
constexpr uint32_t LED_ALIVE_ON_TIME    = 100;  // Горит 100 мс, остальное время - не горит
constexpr uint32_t LED_ALIVE_OFF_TIME   = LED_ALIVE_PERIOD - LED_ALIVE_ON_TIME;

TLed ledAlive(PIN_LED_ALIVE, ACTIVE_HIGH); // обьект светодиода

THandle hTimerAlive = INVALID_HANDLE;       // его таймер переключения

// --------------------------------------------------------------------------------------
//
//
//
constexpr uint32_t BEEP_CHANGE_STATE    = 300; //
constexpr uint32_t BEEP_SETUP_END       = 200;
constexpr uint32_t BEEP_CHIRP           = 50;

TDigitalDevice Beeper(PIN_BEEPER);  // зуммер

THandle hTimerBeeper = INVALID_HANDLE;  // таймер зуммера

void  Beep(const uint32_t ADuration);  // функция включения зуммера

/// -------------------------------------------------------------------------------------
///
/// Блок часов
/// 
/// 
THandle hTimerColon = INVALID_HANDLE; // таймер мигания двоеточием

TDateTime Now; // здесь хранится текущее/устанавливаемое время

TDS3231 Clock; // объект часов

void SaveTime(const TDateTime ATime); // сохранение времени в модуле часов

/// ---------------------------------------------------------------------------------------
/// 
///  Блок TM1637
///
TM1637 Disp(PIN_1637_CLOCK, PIN_1637_DATA, enTM1637Type::Time); // Дисплейчик

void Display(void);     // Функция отображения

/// -----------------------------------------------------------------
///
///  Блок даччика температуры
/// 
/// 

TDS18B20 TempSensor(PIN_TEMP_SENSOR); // сам сенсор

int8_t      CurrentTemperature = INVALID_TEMPERATURE; // текущая температура

int8_t      MaxTemperature = 0; // Устанавливаемая температура                    

// ------------------------------------------------------------------
//
//  Галетный переключатель
//
// Test mine {0,195,322,453,663,689}
// { 0,209,337,430,498 }

constexpr uint16_t GALLET_DELTA = 5;
#ifdef MINE
const uint16_t GALLET_VALUES[] = { 689,663,453,322,195,GALLET_DELTA }; // это мои значения галетника
#else
const uint16_t GALLET_VALUES[] = { GALLET_DELTA,217,351,446,518 };      // это твои
#endif
const uint8_t  GALLET_VALUES_SIZE = sizeof(GALLET_VALUES) / sizeof(GALLET_VALUES[0]);

uint8_t GalletCurrentIndex = 0xFF;

int16_t GetGalletIndex(const uint16_t AValue);

TAnalogSensor GalletSwitch(PIN_GALLET, true);

// --------------------------------------------------------------------------------------
//
//  Блок левого энкодера
// 
TEncoder LeftEncoder(4, 3, 2);

// ------------------------------------------------------------------
//
//                Блок  нагревателя
//
//



TDigitalDevice RelayHeater(PIN_HEATER_RELAY, ACTIVE_LOW);

// --------------------------------------------------------------------------------------
//
//  Программа
//
//
constexpr uint32_t SETUP_TIMEOUT = 10000;
constexpr uint32_t COLON_FLASH_TIME = 500;
constexpr uint32_t SHOW_APP_STATE_TIME = 1500;

// состояния программы
//
enum class TAppState : uint8_t {
    Unknown = 0xFF, // не определено
    Heat    = 0x00, // Программируемый режим 1
    Prog1   = 0x01, // Ручное управление
    Off     = 0x02, // Выключено
    Prog2   = 0x03, // Программируемый режим 2
    Hand    = 0x04, // Нагрев
    Stop    = 0x05, // Стоп, служебный режим, всё выкл и остановлено
    Error   = 0x06  // Ошибка, мало ли что
};

const char* AppStateNames[] = { "HEAt", "Pr 1", "OFF", "Pr 2", "HAnd", "StOP", "Err" };

TAppState AppState = TAppState::Unknown;

bool ShowAppStateName = false;

THandle hTimerTimeOut = INVALID_HANDLE;

void SetAppState(const TAppState ANewAppState);

using TEncoderClickFunc = void(*)(void);
using TEncoderUpDownFunc = void(*)(int8_t);

bool    SetupMode = false;
bool    Flashing = false;
uint8_t FlashIndex = 0;

void EnterSetupMode(void);
void ExitSetupMode(void);

TEncoderClickFunc  onLeftEncoderClick = NULL;
TEncoderUpDownFunc onLeftEncoderUpDown = NULL;

void ChangeFlashIndex(void);
void ChangeClockDigits(int8_t AStep);

//-----------------------------------------------------------------------------------------
//
//  Прототипы функций
//
void Dispatch(const TMessage& Msg);  // функция обработки сообщений, прототип



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

    hTimerAlive     = Timers.Add(LED_ALIVE_ON_TIME, TTimerState::Running);
    hTimerColon     = Timers.Add(COLON_FLASH_TIME, TTimerState::Running);
    hTimerBeeper    = Timers.Add(BEEP_CHANGE_STATE, TTimerState::Stopped);
    hTimerTimeOut   = Timers.Add(SHOW_APP_STATE_TIME, TTimerState::Stopped);

    GalletSwitch.SetReadInterval(200);
    GalletSwitch.SetGap(10);

    Disp.Clear();
    Disp.SetBrightness(7);

    SetAppState(TAppState::Stop);

 //   puts("End setup");


//    Clock.SetTime(__TIME__);  // первоначальная настройка часов

}

void loop() {                   // бесконечный цикл

    Clock.Read();

    GalletSwitch.Read();

    LeftEncoder.Read();

    if (TempSensor) TempSensor.Read();

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

void SaveTime(const TDateTime ATime)
{
    Clock.SetTime(ATime.tm_hour, ATime.tm_min);
}

//
//  функция отображения, отображает время или температуру, в зав-ти от состояния
//
void Display() {
    constexpr char SPACE_SYMBOL = ' ';

    Disp.Clear();

    uint8_t stateIdx = static_cast<uint8_t>(AppState);

    if (ShowAppStateName) {
        Disp.Print(AppStateNames[stateIdx]);
        return;
    }

    switch (AppState)
    {
    case TAppState::Prog1:
        break;
    case TAppState::Hand:
        break;
    case TAppState::Off:
        Disp.PrintTime(Now.tm_hour, Now.tm_min);
        if (SetupMode && Flashing) {
            if (FlashIndex == 0) {
                Disp.PrintAt(0, SPACE_SYMBOL);
                Disp.PrintAt(1, SPACE_SYMBOL);
            }
            else {
                Disp.PrintAt(2, SPACE_SYMBOL);
                Disp.PrintAt(3, SPACE_SYMBOL);
            }
        }
        break;
    case TAppState::Prog2:
//        Disp.Print(AppStateNames[stateIdx]);
        break;
    case TAppState::Heat:
        Disp.PrintDeg(CurrentTemperature);
        if (SetupMode && Flashing) Disp.Clear();
        break;
    case TAppState::Stop:
//        Disp.Print(AppStateNames[stateIdx]);
        break;
    case TAppState::Error:
//        Disp.Print(AppStateNames[stateIdx]);
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

void SetAppState(const TAppState ANewAppState)
{
    if (AppState == ANewAppState) return;
    AppState = ANewAppState;
    
    uint8_t idx = static_cast<uint8_t>(AppState);
    uint8_t lastIdx = uint8_t(TAppState::Error) + 1;
    
    if (idx < lastIdx) {
        ShowAppStateName = true;
        Timers.SetNewInterval(hTimerTimeOut, SHOW_APP_STATE_TIME);
        Timers.Reset(hTimerTimeOut);
    }

    switch (AppState)
    {
    case TAppState::Unknown:
        break;
    case TAppState::Heat:
        break;
    case TAppState::Prog1:
        break;
    case TAppState::Off:
        break;
    case TAppState::Prog2:
        break;
    case TAppState::Hand:
        break;
    case TAppState::Stop:
        RelayHeater.Off();
        break;
    case TAppState::Error:
        break;
    default:
        break;
    }

    Display();
}

void EnterSetupMode(void)
{
    if (AppState == TAppState::Off) SendMessage(msg_EnterClockSetup);
}

void ExitSetupMode(void)
{
    if (!SetupMode) return;
    SetupMode = !SetupMode;
    if (AppState == TAppState::Off) {
        SaveTime(Now);
        SendMessage(msg_ExitClockSetup);
    }
    onLeftEncoderClick = NULL;
    onLeftEncoderUpDown = NULL;
}

void ChangeFlashIndex(void)
{
    FlashIndex++;
    if (FlashIndex > 1) FlashIndex = 0;
    Timers.Reset(hTimerTimeOut);
}

void ChangeClockDigits(int8_t AStep)
{
    constexpr int8_t MIN_HOURS = 0;
    constexpr int8_t MAX_HOURS = 23;

    constexpr int8_t MIN_MINS = 0;
    constexpr int8_t MAX_MINS = 59;

    Timers.Reset(hTimerTimeOut);

    int8_t hour = Now.tm_hour;
    int8_t min = Now.tm_min;

    if (FlashIndex == 0) hour += AStep;
    if (FlashIndex == 1) min += AStep;
    
    if (hour < MIN_HOURS) hour = MAX_HOURS;
    if (hour > MAX_HOURS) hour = MIN_HOURS;

    if (min < MIN_MINS) min = MAX_MINS;
    if (min > MAX_MINS) min = MIN_MINS;

    Now.tm_hour = hour;
    Now.tm_min = min;
    
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

    case msg_TimerEnd: {  // пришло сообщение от TimerList
        
        if (Msg.LoParam == hTimerAlive) { // таймер мигания светодиодом, 1 короткая вспышка в 5 секунд
            ledAlive.Toggle();
            uint32_t newInterval = ledAlive.isOn() ? LED_ALIVE_ON_TIME : LED_ALIVE_OFF_TIME;
            Timers.SetNewInterval(hTimerAlive, newInterval);
        }

        if (Msg.LoParam == hTimerBeeper) { // таймер активного зуммера, выключает его
#ifdef DEBUG
            puts("Beep Off");  // на время отладки не пищит, пишет в порт
#else
            Beeper.Off();
#endif
            Timers.Stop(hTimerBeeper); // этот таймер пока больше не нужен, останавливаем его
        }

        if (Msg.LoParam == hTimerColon) {
            Flashing = !Flashing;
            if ((AppState == TAppState::Off)&&(!ShowAppStateName))
                Disp.ToggleColon();
            else
                Disp.ShowPoint(false);
            if (SetupMode) Display();
        }

        if (Msg.LoParam == hTimerTimeOut) {

            Timers.Stop(hTimerTimeOut);

            if (ShowAppStateName) ShowAppStateName = !ShowAppStateName;
            
            if (SetupMode) ExitSetupMode();

            Display();
        }

        break;
    }

    case msg_TemperatureChanged: {      // изменилась температура
        if (Msg.Sender == TempSensor) {
            CurrentTemperature = TempSensor.GetTemperature();
            Display();
        }
        break;
    }
                                
    case msg_SensorValueChanged: {
        int16_t gValue = GetGalletIndex(Msg.LoParam);
        printf("Gallet change to %d\n", Msg.LoParam);
        SendMessage(msg_GalletGhanged, gValue);
        break;
    }

    case msg_TimeChanged: {  // изменилось время (часы/минуты)
        if (!SetupMode) {    // если не в режиме сетап
            Now = Clock.GetTime();
            Display();      // то сразу и отобразим
        }
        break;
    }

    case msg_GalletGhanged: { // галетник повернулся, смена состояния программы
        SetAppState(TAppState::Stop);
        Beep(BEEP_CHANGE_STATE);
        TAppState newState = static_cast<TAppState>(Msg.LoParam);
        SetAppState(newState);
        break;
    }

    case msg_EncoderBtnLong: {  // Длинное нажатие энкодера
        
        if (Msg.Sender == LeftEncoder) { // Если это левый энкодер
            
            if (SetupMode)        // вкл/выкл режим установки
                ExitSetupMode();
            else
                EnterSetupMode();
        }

        break;
    }

    case msg_SecondsChanged:
    case msg_DateChanged:
        break;

    case msg_EnterClockSetup: { // Установка времени
        SetupMode = true;       // 
        FlashIndex = 0;         // Первыми устанавливаем часы

        Timers.SetNewInterval(hTimerTimeOut, SETUP_TIMEOUT);// таймаут 10 секунд
        Timers.Reset(hTimerTimeOut);// Если ничего не нажали, выход без сохранения

        onLeftEncoderClick = ChangeFlashIndex;   // обработчики нажатия энкодера
        onLeftEncoderUpDown = ChangeClockDigits; // и вращения влево/вправо
        
        Display();
        break;
    }

    case msg_ExitClockSetup: { // Выход из установки времени
        Beep(300);
        Display();
        break;
    }

    case msg_EncoderBtnPress: {
        if (Msg.Sender == LeftEncoder) {
            if (onLeftEncoderClick != NULL) onLeftEncoderClick();
        }
        break;
    }

    case msg_EncoderLeft:
    case msg_EncoderRight: {
        int8_t step = (Msg.Message == msg_EncoderLeft) ? -1 : 1; // направление вращения

        if (Msg.Sender == LeftEncoder) { // если это левый энкодер, и назначена функция обработки
            if (onLeftEncoderUpDown != NULL) onLeftEncoderUpDown(step);// вызовем её
        }
        break;
    }

    default: // если мы пропустили какое сообщение, этот блок выведет в сериал его номер
        printf("Unhandled message 0x%X\n", Msg.Message);
        break;
    }
}
