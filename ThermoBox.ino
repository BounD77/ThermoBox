//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// проект термошкафа из холодильника. датчик AM2302, 2 вентилятора подключены через ШИМ
#include <arduino.h>
#include <EEPROM.h>
#include "class_noDELAY.h"
#include "DHT.h"
#include <LiquidCrystal.h> //посмотрим на англицкий пока
//#include <LiquidCrystal_1602_RUS.h>
#define DHTPIN 2     // what digital pin we're connected to
#define EEPROMLEN 21 //количество байт, хранящихся в EEPROM, следующим хранится CRC - при увеличении не забыть поправить!!!
// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

#define DEBUGMODE 1  // после окончания отладки закомментируем

#define KEYSELECT 5  //соответствия клавишей 
#define KEYLEFT 4
#define KEYUP 2
#define KEYDOWN 3
#define KEYRIGHT 1

void(* resetFunc) (void) = 0;//объявляем функцию reset с адресом 0

#if defined(__LGT8FX8E__) // для WAVGAT при считывании А0 нужно корректировать на опорное, иначе не работает кнопка SELECT

#endif
int numMenu = 10 ; //количество пунктов меню
char* menuName[] = {"Fan Speed",
                    "Condenser Speed",
                    "Destination Temp",
                    "Destination Humi",
                    "Hysteresis Temp",
                    "Hysteresis Humi",
                    "Relay Delay",
                    "Menu Timeout",
                    "Auto Save",
                    "CLEAR EEPROM"
                   };
byte fanSpeedCurrent = 5; //рабочая скорость
byte fanSpeedMin = 0; //максимальная скорость
byte fanSpeedMax = 10; //максимальная скорость
bool fanSpeedOn = true; //флаг включено вентилятора обдува

byte coilSpeedCurrent = 7; //рабочая скорость
byte coilSpeedMin = 0; //максимальная скорость
byte coilSpeedMax = 10; //максимальная скорость
bool coilSpeedOn = true; //флаг включено вентилятора испарителя

float destTemp = 12.0; // поддеживаемая температура
float destTempMin = 3.0;
float destTempMax = 28.0;

float hystTemp = 2.0; // температурный гистерезис
float hystTempMin = 0.3;
float hystTempMax = 4.0;

float destHumi = 76.0; // поддеживаемая влажность
float destHumiMin = 30.0;
float destHumiMax = 90.0;

float hystHumi = 2.0; // гистерезис влажности
float hystHumiMin = 0.5;
float hystHumiMax = 4.0;

int pinIn     = A0; // аналоговый порт кнопок
int pinRelay  = 13; //реле включения холодильника нормально включено
int pinPWMFan = 3; //порт нижнего вентилятора
int pinPWMCoil = 11; //порт вентилятора испарителя

int keyValue  =  0; // Состояние покоя
bool innerMenu = false; // признак нахождения в меню, не выводим основной экран
byte itemMenu = 1; //номер пункта меню при движении
byte timeToExitMenu = 7 ; //время по неактивности кнопок выход из меню в основной экран (10 сек)
byte timeToExitMenuMin = 1 ;
byte timeToExitMenuMax = 15 ;
bool flagAutoSave = false; //автосохранение в EEPROM после выхода по таймауту из меню, без SELECT, не думаю что хорошая идея, но вдруг...
bool flagResetEEPROM = false; // флаг сброса памяти
byte mi = 0;
byte minTimeOnOff = 5; // время задержки между переключениями реле холодильника, 5 мин
byte minTimeOnOffMin = 0;
byte minTimeOnOffMax = 20;

bool enabledRelayOnOff = true; //разрешено ли щелкать реле
bool isRelayOn = false; // включено ли реле

#ifdef DEBUGMODE
unsigned long timeLoop = 0; // время цикла, для понимания
#endif



byte symbolFull[8] =
{
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
};
byte symbolHalf[8] =
{
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
};


noDELAY readSensors; // таймер чтения сенсора
noDELAY minOnOff;    // таймер минимального щелкания реле
noDELAY exitMenu;    // таймер выхода из меню

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal lcd(8, 9, 4, 5, 6, 7 );//For LCD Keypad Shield
//int pushedButton; // нажатая кнопка
bool tempOut = false; // временная переменная для тестирвоания, убрать потом!



void setup () {
#if defined(__LGT8FX8E__) // 
  analogReadResolution(10);
#endif

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.setCursor(2, 0);
  lcd.print("Thermobox V1");
  lcd.setCursor(0, 1);
  lcd.print("Mode1 by default");
#ifdef DEBUGMODE
  Serial.begin(9600);
  Serial.println("Debug begin!");
#endif

   pinMode(pinRelay, OUTPUT); // Объявляем пин реле как выход
  // digitalWrite(pinRelay, HIGH); // Выключаем реле - посылаем высокий сигнал

  dht.begin();
  readSensors.start();
  minOnOff.start();
  // exitMenu.start()
  lcd.createChar(1, symbolFull);
  lcd.createChar(2, symbolHalf);

  if (EepromTestCRC()) { // если CRC норнмальное, заполняем данные из EEPROM
    EepromReadAll();
  }

  delay (2000);
  lcd.clear();
}

void loop() {

#ifdef DEBUGMODE
  //     timeLoop = millis();
#endif
  // проверяем нажатия кнопок и обрабатываем нажатия
  int newKeyValue = GetKeyValue(); // Получаем актуальное состояние кнопок с коррекцией дребезга
  exitMenu.read(timeToExitMenu * 1000); //
  if (exitMenu.tick && innerMenu) { // если таймаут
    // не забыть восстановить измененные данные
    if (flagAutoSave) { //если автосохранение включено
      EepromUpdateAll();
    }


    innerMenu = false;

    exitMenu.stop(); // останавливаем счетчик простоя
    lcd.clear();
  }

  if (keyValue != newKeyValue) {  // Если новое значение не совпадает со старым - реагируем на него
    keyValue = newKeyValue;       // Актуализируем переменную хранения состояния
    if (keyValue > 0) {           // Если значение больше 0, значит кнопка нажата
      if (!innerMenu) { // если мы не в меню, входим
        innerMenu = true;
        mi = 0;
        exitMenu.start(); // запускаем счетчик простоя

        // выводим строчки меню, вначале первую

        lcd.clear();
        lcd.setCursor((16 - strlen(menuName[mi])) / 2, 0);
        lcd.print(menuName[mi]);
        PrintSecondStringInMenu(mi);

      }
      else { // если уже в меню смотрим на нажатия

        exitMenu.restart(); // сброс счетчика простоя
        switch (keyValue) {
          case KEYUP:
            if (mi-- == 0 ) {
              mi = numMenu - 1;
            }
            lcd.clear();
            lcd.setCursor((16 - strlen(menuName[mi])) / 2, 0);

            lcd.print(menuName[mi]);
            PrintSecondStringInMenu(mi);
            break;

          case KEYDOWN:
            if (++mi >= numMenu) {
              mi = 0;
            }
            lcd.clear();
            lcd.setCursor((16 - strlen(menuName[mi])) / 2, 0);
            lcd.print(menuName[mi]);
            PrintSecondStringInMenu(mi);
            break;
          case KEYLEFT : // уменьшаем значения
            switch (mi) {
              case 0: // вращение нижнего вентилятора
                if (fanSpeedCurrent  > fanSpeedMin) {
                  fanSpeedCurrent--;
                }
                break;
              case 1: // вращение вентилятора испарителя
                if (coilSpeedCurrent  > coilSpeedMin) {
                  coilSpeedCurrent--;
                }
                break;
              case 2: // температура
                destTemp = destTemp - 0.1;
                if (destTemp  < destTempMin) {
                  destTemp  = destTempMin;
                }
                break;
              case 3: // влажность
                destHumi = destHumi - 0.1;
                if (destHumi  < destHumiMin) {
                  destHumi  = destHumiMin;
                }
                break;
              case 4: // температура hyst
                hystTemp = hystTemp - 0.1;
                if (hystTemp  < hystTempMin) {
                  hystTemp  = hystTempMin;
                }
                break;
              case 5: // влажность hyst
                hystHumi = hystHumi - 0.1;
                if (hystHumi  < hystHumiMin) {
                  hystHumi  = hystHumiMin;
                }
                break;
              case 6: // время задержки щелканья реле, мин
                minTimeOnOff = minTimeOnOff - 1;
                if (minTimeOnOff  < minTimeOnOffMin) {
                  minTimeOnOff = minTimeOnOffMin;
                }
                break;
              case 7: // время выхода из меню по таймауту, сек
                timeToExitMenu = timeToExitMenu - 1;
                if (timeToExitMenu  < timeToExitMenuMin) {
                  timeToExitMenu = timeToExitMenuMin;
                }
                break;
              case 8: // автосохранение меню по таймауту,  без SELECT
                flagAutoSave = !flagAutoSave ;

                break;
              case 9: // сброс EEPROM
                flagResetEEPROM = !flagResetEEPROM ;
                break;
            }
            PrintSecondStringInMenu(mi);
            break;
          case KEYRIGHT: // увеличиваем значения
            switch (mi) {
              case 0: // вращение нижнего вентилятора
                if (fanSpeedCurrent  < fanSpeedMax) {
                  fanSpeedCurrent++;
                }
                break;
              case 1: // вращение вентилятора испарителя
                if (coilSpeedCurrent  < coilSpeedMax) {
                  coilSpeedCurrent++;
                }
                break;
              case 2: // температура
                destTemp = destTemp + 0.1;
                if (destTemp  > destTempMax) {
                  destTemp  = destTempMax;
                }
                break;
              case 3: // влажность
                destHumi = destHumi + 0.1;
                if (destHumi  > destHumiMax) {
                  destHumi  = destHumiMax;
                }
                break;
              case 4: // температура - гистерезис
                hystTemp = hystTemp + 0.1;
                if (hystTemp  > hystTempMax) {
                  hystTemp  = hystTempMax;
                }
                break;
              case 5: // влажность - гистерезис
                hystHumi = hystHumi + 0.1;
                if (hystHumi  > hystHumiMax) {
                  hystHumi  = hystHumiMax;
                }
                break;
              case 6: // время задержки щелканья реле
                minTimeOnOff = minTimeOnOff + 1;
                if (minTimeOnOff  > minTimeOnOffMax) {
                  minTimeOnOff = minTimeOnOffMax;
                }
                break;
              case 7: // время выхода из меню по таймауту, сек
                timeToExitMenu = timeToExitMenu + 1;
                if (timeToExitMenu > timeToExitMenuMax) {
                  timeToExitMenu = timeToExitMenuMax;
                }
                break;
              case 8: // автосохранение меню по таймауту,  без SELECT
                flagAutoSave = !flagAutoSave ;
                break;
              case 9: // сброс EEPROM
                flagResetEEPROM = !flagResetEEPROM ;
                break;
            }
            PrintSecondStringInMenu(mi);
            break;
          case KEYSELECT: // запоминаем значение в EEPROM и выходим на главный экран
            if (mi == 9 && flagResetEEPROM ) { // если сброс памяти

              flagResetEEPROM = false;
              for (int i = 0; i < EEPROMLEN; i++) { // забиваем память рулями
                EEPROM_byte_write(i, 0);
              }
              resetFunc(); //вызываем reset
#ifdef DEBUGMODE
              Serial.println("EEPROM cleared");
#endif
            }
            else {
              EepromUpdateAll();
            }
            innerMenu = false;
            flagResetEEPROM = false;
            exitMenu.stop(); // останавливаем счетчик простоя
            lcd.clear();
            break;

        }
      }
#ifdef DEBUGMODE
      Serial.println("Key pressed: " + String(keyValue));
#endif


    }
    else if (keyValue < 0) {       // Если -1 - неизвестное состояние, незапрограммированное нажатие
#ifdef DEBUGMODE
      Serial.println("unknown pressed");
#endif
    }
    else {                        // Если 0, то состояние покоя
#ifdef DEBUGMODE
      Serial.println("all keys are not pressed");
#endif
    }
  }
  
  minOnOff.read((unsigned long) minTimeOnOff * 60000); // проверяем не часто ли щелкаем реле
  //minOnOff.read(minTimeOnOff * 2 * 1000); // проверяем не часто ли щелкаем реле 10 секунд тест
  if (minOnOff.tick) { // тикнуло 5 минут, можно щелкать реле
    enabledRelayOnOff = true;
    minOnOff.stop();
  }
  readSensors.read(2500); //каждые 2.5 секунды снимает показания датчиков и обновляем экран (на 2 секунды проверка в библиотеке DHT, поэтму чуть больше)

  if (readSensors.tick && !innerMenu) { // тикнуло 2,5 секунды и мы не в меню,  пора считывать и выводить данные
    float curHumidity = dht.readHumidity(); //считали влажность
    float  curTemp = dht.readTemperature(); //считали температуру

    lcd.setCursor(0, 0);
    if (isnan(curHumidity) || isnan(curTemp) ) {
#ifdef DEBUGMODE
      Serial.println("Failed to read from DHT sensor!");
#endif
      lcd.print (" Failed to read");
      lcd.setCursor(0, 1);
      lcd.print ("from DHT sensor!");
      return;
    }
    lcd.print ("Temp:"); //выводим все данные
    lcd.print ((float)curTemp, 1);
    lcd.print ("C");
    lcd.setCursor(0, 1);
    lcd.print ("Humi:");
    lcd.print (curHumidity, 1);
    lcd.print ("%");

    //включаем/выключаем вентиляторы
    // нижний (всегда работает
    if (fanSpeedCurrent == 0 || !fanSpeedOn) {
      digitalWrite(pinPWMFan, LOW); // выключен
      lcd.setCursor(11, 0);
      lcd.print("OFF  ");
    }
    else {
      analogWrite(pinPWMFan, map(fanSpeedCurrent, 0, 10, 20, 255));
      // выводим в правой части мощности работающих вентиляторов
      lcd.setCursor(11, 0);
      for (int i = 1; i <= fanSpeedCurrent / 2; i++) {
        lcd.print("\1");
      }
      if (fanSpeedCurrent % 2 != 0) { // напечатать половинку
        lcd.print("\2");
      }
      for (int i = 1; i <= (fanSpeedMax - fanSpeedCurrent) / 2; i++) {
        lcd.print(" ");
      }

    }
    // испарителя
    if ((float) curHumidity > (destHumi + hystHumi / 2.0)) { // если влажность повысилась выключаем койл
      coilSpeedOn = false;
    }
    if ((float) curHumidity < (destHumi - hystHumi / 2.0)) { // если влажность понизилась включаем койл
      coilSpeedOn = true;
    }
    if (coilSpeedCurrent == 0 || !coilSpeedOn) {
      digitalWrite(pinPWMCoil, LOW); // выключен
      lcd.setCursor(11, 1);
      lcd.print("OFF  ");
    }
    else {
      analogWrite(pinPWMCoil, map(coilSpeedCurrent, 0, 10, 0, 255));
      // выводим в правой части мощности работающих вентиляторов
      lcd.setCursor(11, 1);
      for (int i = 1; i <= coilSpeedCurrent / 2; i++) {
        lcd.print("\1");
      }
      if (fanSpeedCurrent % 2 != 0) { // напечатать половинку
        lcd.print("\2");
      }
      for (int i = 1; i <= (fanSpeedMax - fanSpeedCurrent) / 2; i++) {
        lcd.print(" ");
      }

    }

    // реле включения холодильника

       if (!enabledRelayOnOff) { // если задержка напечатаем решетку
      lcd.setCursor(10, 0);
         lcd.print("#");
       }
     if ( (float) curTemp > (destTemp + hystTemp / 2.0) && enabledRelayOnOff) { // если температура повысилась включаем реле
    //if ( (float) curTemp > (destTemp + hystTemp / 2.0) ) { // если температура повысилась включаем реле

      enabledRelayOnOff = false;
      isRelayOn = true;
      minOnOff.start();
      digitalWrite(pinRelay, HIGH); // включение в ???
      lcd.setCursor(10, 0);
      lcd.print("+");
    }
     if ((float) curTemp < (destTemp - hystTemp / 2.0) && enabledRelayOnOff) { // если температура низкая выключаем холодильник
    //if ((float) curTemp < (destTemp - hystTemp / 2.0)) { // если температура низкая выключаем холодильник
      enabledRelayOnOff = false;
      digitalWrite(pinRelay, LOW);
      isRelayOn = false;
      minOnOff.start();
      lcd.setCursor(10, 0);
      lcd.print("-");
    }
  }
}

int GetKeyValue() {         // Функция устраняющая дребезг
  static int   count;
  static int   oldKeyValue; // Переменная для хранения предыдущего значения состояния кнопок
  static int   innerKeyValue;
  uint16_t analogDataValue;

  analogDataValue = analogRead(pinIn);

#if defined(__LGT8FX8E__) // для WAVGAT при считывании А0 нужно корректировать на опорное, иначе не работает кнопка SELECT
  analogDataValue += analogRead(VCCM);

#endif

  // Здесь уже не можем использовать значение АЦП, так как оно постоянно меняется в силу погрешности
  int actualKeyValue = GetButtonNumberByValue(analogDataValue);  // Преобразовываем его в номер кнопки, тем самым убирая погрешность

  if (innerKeyValue != actualKeyValue) {  // Пришло значение отличное от предыдущего
    count = 0;                            // Все обнуляем и начинаем считать заново
    innerKeyValue = actualKeyValue;       // Запоминаем новое значение
  }
  else {
    count += 1;                           // Увеличиваем счетчик
  }

  if ((count >= 5) && (actualKeyValue != oldKeyValue)) {
    oldKeyValue = actualKeyValue;         // Запоминаем новое значение
  }
  return    oldKeyValue;
}

int GetButtonNumberByValue(int value) {   // Новая функция по преобразованию кода нажатой кнопки в её номер
#ifdef DEBUGMODE
  //Serial.println(value); // раскомментируйте для замера аналоговой величины кнопок
#endif
  // аналоговые соответствия кнопок, не нажато 0, RIGHT 1, UP 2, DOWN 3, LEFT 4, SELECT 5
  int values[6] = {1315, 0, 196, 454, 705, 992}; // для WAVGAT
  //  int values[6] = {1023, 0, 131, 306, 479, 721}; // для UNO, для каждой платы надо замерять данные по кнопкам
  int error     = 15;                     // Величина отклонения от значений - погрешность
  for (int i = 0; i <= 5; i++) {
    // Если значение в заданном диапазоне values[i]+/-error - считаем, что кнопка определена
    if (value <= values[i] + error && value >= values[i] - error) return i;
  }
  return -1;                              // Значение не принадлежит заданному диапазону
}

bool EepromTestCRC () {  // вычисление контрольной суммы

  // проверка контрольной суммы
  if ( EepromCheckCRC() == EEPROM.read(EEPROMLEN)) {
    // контрольна сумма правильная
#ifdef DEBUGMODE
    Serial.println("EEPROM CRC correct");
#endif
    return true;
  }
  else {
    // контрольная сумма неправильная
#ifdef DEBUGMODE
    Serial.println();
    Serial.print("EEPROM= CRC incorrect, data error");
#endif
    return false;
  }

}


byte EepromCheckCRC () {  // вычисление контрольной суммы
  byte sum = 0;
  for (byte i = 0; i < EEPROMLEN; i++) {
    sum += EEPROM.read(i);
  }
  // расчет контрольной суммы
  return (sum ^ 0xe5);

}

void EepromReadAll() { //считываем в переменные данные из EEPROM

  //eeprom_write_byte(1, 1);

#if defined(__LGT8FX8E__) // и с EEPROM WAVGAT работает по-своему и нет get|put|update, но куда-то платы надо использовать...

  fanSpeedCurrent = EEPROM_byte_read(0);
  coilSpeedCurrent = EEPROM_byte_read(1);
  destTemp = EEPROM_float_read(2);
  destHumi = EEPROM_float_read(6);
  hystTemp = EEPROM_float_read(10);
  hystHumi = EEPROM_float_read(14);
  minTimeOnOff = EEPROM_byte_read(18);
  flagAutoSave = EEPROM_byte_read(19);
  timeToExitMenu = EEPROM_byte_read(20);

#else // если родная аардуина уно
  EEPROM.get(0, fanSpeedCurrent);
  EEPROM.get(1, coilSpeedCurrent);
  EEPROM.get(2, destTemp);
  EEPROM.get(6, destHumi);
  EEPROM.get(10, hystTemp);
  EEPROM.get(14, hystHumi);
  EEPROM.get(18, minTimeOnOff);
  EEPROM.get(19, flagAutoSave);
  EEPROM.get(20, timeToExitMenu);

#endif
}

void EepromUpdateAll() { //записываем переменные в EEPROM, put работает как update, не записывает, если данные не изменились

#if defined(__LGT8FX8E__)

  EEPROM_byte_write(0, fanSpeedCurrent);
  EEPROM_byte_write(1, coilSpeedCurrent);
  EEPROM_float_write(2, destTemp);
  EEPROM_float_write(6, destHumi);
  EEPROM_float_write(10, hystTemp);
  EEPROM_float_write(14, hystHumi);
  EEPROM_byte_write(18, minTimeOnOff);
  EEPROM_byte_write(19, flagAutoSave);
  EEPROM_byte_write(20, timeToExitMenu);

  EEPROM_byte_write(EEPROMLEN, EepromCheckCRC());
#else
  EEPROM.put(0, fanSpeedCurrent);
  EEPROM.put(1, coilSpeedCurrent);
  EEPROM.put(2, destTemp);
  EEPROM.put(6, destHumi);
  EEPROM.put(10, hystTemp);
  EEPROM.put(14, hystHumi);
  EEPROM.put(18, minTimeOnOff);
  EEPROM.put(19, flagAutoSave);
  EEPROM.put(20, timeToExitMenu);

  EEPROM.update(EEPROMLEN, EepromCheckCRC());
#endif
}


void PrintSecondStringInMenu(byte value) { // функция вывода второй строки (значений в меню)
  switch (value) {
    case 0: // скорость вращения вентилятора
      lcd.setCursor(2, 1);

      lcd.print("[");
      if (fanSpeedCurrent == 0) {
        lcd.print("OFF       ");
      }
      else {
        for (int i = 1; i <= fanSpeedCurrent; i++) {
          lcd.print("\1");
        }
        for (int i = 1; i <= fanSpeedMax - fanSpeedCurrent; i++) {
          lcd.print(" ");
        }
      }
      lcd.print("]");
      break;
    case 1: // скорость вентилятора испарителя
      lcd.setCursor(2, 1);
      lcd.print("[");
      if (coilSpeedCurrent == 0) {
        lcd.print("OFF       ");
      }
      else {
        for (int i = 1; i <= coilSpeedCurrent; i++) {
          lcd.print("\1");
        }
        for (int i = 1; i <= (coilSpeedMax - coilSpeedCurrent); i++) {
          lcd.print(" ");
        }
      }
      lcd.print("]");
      break;
    case 2: // температурный режим
      lcd.setCursor(5, 1);    lcd.print ((float)destTemp, 1);
      lcd.print (" C ");
      break;
    case 3: // влажность
      lcd.setCursor(5, 1);    lcd.print ((float)destHumi, 1);
      lcd.print (" % ");
      break;
    case 4: // температурный гистерезис
      lcd.setCursor(5, 1);    lcd.print ((float)hystTemp, 1);
      lcd.print ("  ");
      break;
    case 5: // влажность гистерезис
      lcd.setCursor(5, 1);    lcd.print ((float)hystHumi, 1);
      lcd.print ("  ");
      break;
    case 6: // задержка реле
      lcd.setCursor(5, 1);    lcd.print (minTimeOnOff);
      lcd.print (" min ");
      break;
    case 7: //  время выхода из меню по таймауту, сек
      lcd.setCursor(5, 1);    lcd.print (timeToExitMenu);
      lcd.print (" sec ");
      break;
    case 8: //  автосохранение меню по таймауту,  без SELECT
      lcd.setCursor(6, 1);
      if (flagAutoSave) {
        lcd.print ("YES");
      }
      else {
        lcd.print (" NO");
      }
      break;
    case 9: // сброс EEPROM
      lcd.setCursor(6, 1);
      if (flagResetEEPROM) {
        lcd.print ("YES");
      }
      else {

        lcd.print (" NO");
      }
      break;
  }
}


// чтение
float EEPROM_float_read(int addr) {
  byte raw[4];
  for (byte i = 0; i < 4; i++) raw[i] = EEPROM.read(addr + i);
  float &num = (float&)raw;
  return num;
}

// запись
void EEPROM_float_write(int addr, float num) {
  if (EEPROM_float_read(addr) != num) { //если сохраняемое отличается
    byte raw[4];
    (float&)raw = num;
    for (byte i = 0; i < 4; i++) EEPROM.write(addr + i, raw[i]);
  }
  else {
#ifdef DEBUGMODE
    Serial.println("Not different, no save");
#endif
  }
}

// чтение
float EEPROM_byte_read(int addr) {
  return EEPROM.read(addr);
}

// запись
void EEPROM_byte_write(int addr, byte num) {
  if (EEPROM_byte_read(addr) != num) { //если сохраняемое отличается
    EEPROM.write(addr, num);
  }
  else {
#ifdef DEBUGMODE
    Serial.println("Not different, no save");
#endif
  }
}


