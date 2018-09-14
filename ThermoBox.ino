//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// проект термошкафа из холодильника. датчик AM2302, 2 вентилятора подключены через ШИМ, Холодильник питается через реле

#define EEPROMLEN 19 //количество байт, хранящихся в EEPROM, следующим хранится CRC
// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
// выберите тип подключенного дисплея
//#define LCD1602 1
#define LCD1602I2C 1

#define DEBUGMODE 1  // после окончания отладки закомментируем

#define KEYSELECT 5  //соответствия клавишей 
#define KEYLEFT 4
#define KEYUP 2
#define KEYDOWN 3
#define KEYRIGHT 1

#define DHTPIN      2     // what digital pin we're connected to
#define PININ      A0 // аналоговый порт кнопок
#define PINRELAY   12 //реле включения холодильника нормально включено
#define PINPWMFAN   3 //порт нижнего вентилятора
#define PINPWMCOIL 11 //порт вентилятора испарителя


#include <arduino.h>
#include <EEPROM.h>
#include "class_noDELAY.h"
#include "DHT.h"

#ifdef LCD1602
#include <LiquidCrystal.h> //посмотрим на англицкий пока
#endif

#ifdef LCD1602I2C
#include <LiquidCrystal_I2C.h> // подключение по I2C
#endif

//#include <LiquidCrystal_1602_RUS.h>

int numMenu = 7 ; //количество пунктов меню
char* menuName[]  = {"Fan Speed",
                     "Condenser Speed",
                     "Destination Temp",
                     "Destination Humi",
                     "Hysteresis Temp",
                     "Hysteresis Humi",
                     "Relay Delay"
                    };
byte fanSpeedCurrent = 5; //рабочая скорость
byte fanSpeedMin = 0; //максимальная скорость
byte fanSpeedMax = 10; //максимальная скорость
bool fanSpeedOn = true; //флаг включено вентилятора обдува

byte coilSpeedCurrent = 7; //рабочая скорость
byte coilSpeedMin = 1; //максимальная скорость
byte coilSpeedMax = 10; //максимальная скорость
bool coilSpeedOn = true; //флаг включено вентилятора испарителя
bool firstLoop = true; //флаг первого запуска

float destTemp = 12.0; // поддеживаемая температура
float destTempMin = 3.0;
float destTempMax = 30.0;

float hystTemp = 2.0; // температурный гистерезис
float hystTempMin = 0.3;
float hystTempMax = 4.0;

float destHumi = 76.0; // поддерживаемая влажность
float destHumiMin = 30.0;
float destHumiMax = 90.0;

float hystHumi = 2.0; // гистерезис влажности
float hystHumiMin = 0.5;
float hystHumiMax = 4.0;

int keyValue  =  0; // Состояние покоя
bool innerMenu = false; // признак нахождения в меню, не выводим основной экран
byte itemMenu = 1; //номер пункта меню при движении
int timeToExitMenu = 4 * 1000; //время по неактивности кнопок выход из меню в основной экран (10 сек)
byte mi = 0;
byte minTimeOnOff = 5; // время задержки между переключениями реле холодильника, 5 мин
byte minTimeOnOffMin = 1;
byte minTimeOnOffMax = 20;

bool enabledRelayOnOff = false ; //разрешено ли щелкать реле - при запуске задержка
bool isRelayOn = true; // включено ли реле - при запуске холодильник включен




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

// таймеры
noDELAY readSensors;
noDELAY minOnOff;
noDELAY exitMenu;

DHT dht(DHTPIN, DHTTYPE);

// инициализация дисплеев
#ifdef LCD1602
LiquidCrystal lcd(8, 9, 4, 5, 6, 7 );//For LCD Keypad Shield
#endif

#ifdef LCD1602I2C
LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display
#endif


//~~~~~~~~~~~~ S E T U P ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void setup () {
#if defined(__LGT8F__) // если wavgat
  analogReadResolution(10);
#endif
  analogReference(DEFAULT);
  pinMode(A0, INPUT_PULLUP); // подтягиваем, иначе часть кнопок на шилде не работают
  pinMode(DHTPIN, INPUT);
  pinMode(PINRELAY, OUTPUT);
  pinMode(PINPWMFAN, OUTPUT);
  pinMode(PINPWMCOIL, OUTPUT);
  digitalWrite(PINRELAY, LOW);


  TCCR2B = TCCR2B & 0b11111000 | 0x07; // устанавливаем частоту шим на 3 и 11 ноге в 4кГц
  // TCCR2B = TCCR2B & 0b11111000 | 0x02; // 32 кГц - не устанавливать

  // set up the LCD's number of columns and rows:
  lcd.init();                      // initialize the lcd
  lcd.begin(16, 2);
  lcd.backlight();
  // Print a message to the LCD.
  lcd.setCursor(2, 0);
  lcd.print("Thermobox V1");
  //  lcd.setCursor(0, 1);
  //  lcd.print("Mode1 by default");
#ifdef DEBUGMODE
  Serial.begin(9600);
  Serial.println("Debug begin!");
#endif


  dht.begin();
  readSensors.start(); // включаем таймер чтения сенсора, он читает регулярно без выключений
  minOnOff.start();   // включаем таймер задержки щелкания реле
  // nD_02.start();
  // exitMenu.start()
  lcd.createChar(1, symbolFull);
  lcd.createChar(2, symbolHalf);

  if (EepromTestCRC()) { // если CRC норнмальное, заполняем данные из EEPROM
    EepromReadAll();
  }

  delay (100);
  lcd.clear();
}

//~~~~~~~~~~~~ L O O P ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void loop() {

#ifdef DEBUGMODE
  //     timeLoop = millis();
#endif
  // проверяем нажатия кнопок и обрабатываем нажатия
  int newKeyValue = GetKeyValue(); // Получаем актуальное состояние кнопок с коррекцией дребезга
  exitMenu.read(timeToExitMenu); //
  if (exitMenu.tick && innerMenu) { // если таймаут
    // не забыть сделать пункт автосохранение
    innerMenu = false;
firstLoop = true;
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
                PrintSecondStringInMenu(mi);
                break;
              case 1: // вращение вентилятора испарителя
                if (coilSpeedCurrent  > coilSpeedMin) {
                  coilSpeedCurrent--;
                }
                PrintSecondStringInMenu(mi);
                break;
              case 2: // температура
                destTemp = destTemp - 0.1;
                if (destTemp  < destTempMin) {
                  destTemp  = destTempMin;
                }
                PrintSecondStringInMenu(mi);
                break;
              case 3: // влажность
                destHumi = destHumi - 0.1;
                if (destHumi  < destHumiMin) {
                  destHumi  = destHumiMin;
                }
                PrintSecondStringInMenu(mi);
                break;
              case 4: // температура hyst
                hystTemp = hystTemp - 0.1;
                if (hystTemp  < hystTempMin) {
                  hystTemp  = hystTempMin;
                }
                PrintSecondStringInMenu(mi);
                break;
              case 5: // влажность hyst
                hystHumi = hystHumi - 0.1;
                if (hystHumi  < hystHumiMin) {
                  hystHumi  = hystHumiMin;
                }
                PrintSecondStringInMenu(mi);
                break;
              case 6: // время задержки щелканья реле
                minTimeOnOff = minTimeOnOff - 1;
                if (minTimeOnOff  < minTimeOnOffMin) {
                  minTimeOnOff = minTimeOnOffMin;
                }
                PrintSecondStringInMenu(mi);
                break;
            }
            break;
          case KEYRIGHT: // увеличиваем значения
            switch (mi) {
              case 0: // вращение нижнего вентилятора
                if (fanSpeedCurrent  < fanSpeedMax) {
                  fanSpeedCurrent++;
                }
                PrintSecondStringInMenu(mi);
                break;
              case 1: // вращение вентилятора испарителя
                if (coilSpeedCurrent  < coilSpeedMax) {
                  coilSpeedCurrent++;
                }
                PrintSecondStringInMenu(mi);
                break;
              case 2: // температура
                destTemp = destTemp + 0.1;
                if (destTemp  > destTempMax) {
                  destTemp  = destTempMax;
                }
                PrintSecondStringInMenu(mi);
                break;
              case 3: // влажность
                destHumi = destHumi + 0.1;
                if (destHumi  > destHumiMax) {
                  destHumi  = destHumiMax;
                }
                PrintSecondStringInMenu(mi);
                break;
              case 4: // температура - гистерезис
                hystTemp = hystTemp + 0.1;
                if (hystTemp  > hystTempMax) {
                  hystTemp  = hystTempMax;
                }
                PrintSecondStringInMenu(mi);
                break;
              case 5: // влажность - гистерезис
                hystHumi = hystHumi + 0.1;
                if (hystHumi  > hystHumiMax) {
                  hystHumi  = hystHumiMax;
                }
                PrintSecondStringInMenu(mi);
                break;
              case 6: // время задержки щелканья реле
                minTimeOnOff = minTimeOnOff + 1;
                if (minTimeOnOff  > minTimeOnOffMax) {
                  minTimeOnOff = minTimeOnOffMax;
                }
                PrintSecondStringInMenu(mi);
                break;
            }
            break;
          case KEYSELECT: // запоминаем значение в EEPROM и выходим на главный экран
            EepromUpdateAll();
firstLoop = true;
            innerMenu = false;
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
  minOnOff.read(minTimeOnOff * 60000); // проверяем не часто ли щелкаем реле
  // minOnOff.read(minTimeOnOff * 2 * 1000); // проверяем не часто ли щелкаем реле 10 секунд тест
  if (minOnOff.tick) { // тикнуло 5 минут, можно щелкать реле
#ifdef DEBUGMODE
    Serial.println("minTimeOnOff tick, enabledRelayOnOff = true");
#endif
    enabledRelayOnOff = true;
    minOnOff.stop();   // выключаем таймер задержки щелкания реле, чтоб не тикал
  }
  readSensors.read(2500); //каждые 2.5 секунды снимает показания датчиков и обновляем экран (на 2 секунды проверка в библиотеке DHT, поэтму чуть больше)
  //nD_02.read(3200); // временно моргаем мощностями вентиляторов на экране

  if (readSensors.tick && !innerMenu) { // тикнуло 2,5 секунды и мы не в меню,  пора считывать и выводить данные
    float curHumidity = dht.readHumidity(); //считали влажность
    float  curTemp = dht.readTemperature(); //считали температуру

    lcd.setCursor(0, 0);
    if (isnan(curHumidity) || isnan(curTemp) ) {
#ifdef DEBUGMODE
      Serial.println("Failed to read from DHT sensor!");
#endif
      lcd.print ("Failed to read ");
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

    lcd.setCursor(10, 0);
    if (firstLoop) { // если первый проход по экарну вначале или после меню пририсовываем состояние реле
    if (enabledRelayOnOff) {
      
      if (isRelayOn)  lcd.print("+");
      else            lcd.print("-");
      firstLoop = false;
    }
    else              lcd.print("#");
    }



    //включаем/выключаем вентиляторы
    // нижний (всегда работает
    if (fanSpeedCurrent == 0 || !fanSpeedOn) {
      digitalWrite(PINPWMFAN, LOW); // выключен
      lcd.setCursor(11, 0);
      lcd.print("OFF  ");
    }
    else {
      analogWrite(PINPWMFAN, map(fanSpeedCurrent, 0, 10, 10, 255));
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
      digitalWrite(PINPWMCOIL, LOW); // выключен
      lcd.setCursor(11, 1);
      lcd.print("OFF  ");
    }
    else {
      analogWrite(PINPWMCOIL, map(coilSpeedCurrent, 0, 10, 10, 255));
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


    if ( curTemp > (destTemp + hystTemp / 2.0) && !isRelayOn) { // если температура повысилась и реле вsключено - включаем реле
      if (enabledRelayOnOff) {//если задержка прошла
        enabledRelayOnOff = false;
        isRelayOn = true;
        digitalWrite(PINRELAY, LOW); // включение в LOW
        minOnOff.start();   // включаем таймер задержки щелкания реле
        lcd.setCursor(10, 0);
        lcd.print("+");

#ifdef DEBUGMODE
        Serial.println("Temp is High, minTimeOnOff tick, isRelayOn = false");
#endif
      }
      else { // если задержка напечатаем решетку
        lcd.setCursor(10, 0);
        lcd.print("#");
#ifdef DEBUGMODE
        Serial.println("enabledRelayOnOff = false, Relay not work! HIGH"); // отладка
#endif
      }
    }

    if ( curTemp < (destTemp - hystTemp / 2.0) && isRelayOn) { // если температура низкая и реле включено выключаем холодильник
      if (enabledRelayOnOff) {//если задержка прошла
        enabledRelayOnOff = false;
        digitalWrite(PINRELAY, HIGH);
        isRelayOn = false;
        minOnOff.start();   // включаем таймер задержки щелкания реле
        lcd.setCursor(10, 0);
        lcd.print("-");
        Serial.println("Temp is Low, minTimeOnOff tick, enabledRelayOnOff = true, isRelayOn = true");
      }
      else { // если задержка напечатаем решетку
        lcd.setCursor(10, 0);
        lcd.print("#");
#ifdef DEBUGMODE
        Serial.println("enabledRelayOnOff = false, Relay not work! LOW");
#endif
      }
    }

  }

}



int GetKeyValue() {         // Функция устраняющая дребезг
  static int   count;
  static int   oldKeyValue; // Переменная для хранения предыдущего значения состояния кнопок
  static int   innerKeyValue;

  // Здесь уже не можем использовать значение АЦП, так как оно постоянно меняется в силу погрешности
  int actualKeyValue = GetButtonNumberByValue(analogRead(PININ));  // Преобразовываем его в номер кнопки, тем самым убирая погрешность

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
  //Serial.println( value);
#endif
  //  int values[6] = {1023, 0, 131, 306, 479, 721}; // для UNO
  int values[6] = {1023, 0, 131, 306, 479, 721}; // для Wavgat R3
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
    Serial.println();
    Serial.println("EEPROM= data error");
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

  // EEPROM.get(0, fanSpeedCurrent);
  //EEPROM.get(1, coilSpeedCurrent);
  // EEPROM.get(2, destTemp);
  // EEPROM.get(6, destHumi);
  // EEPROM.get(10, hystTemp);
  // EEPROM.get(14, hystHumi);
  // EEPROM.get(18, minTimeOnOff);
}

void EepromUpdateAll() { //записываем переменные в EEPROM, put работает как update, не записывает, если данные не изменились

  // EEPROM.put(0, fanSpeedCurrent);
  // EEPROM.put(1, coilSpeedCurrent);
  // EEPROM.put(2, destTemp);
  // EEPROM.put(6, destHumi);
  // EEPROM.put(10, hystTemp);
  // EEPROM.put(14, hystHumi);
  // EEPROM.put(18, minTimeOnOff);

  //  EEPROM.update(EEPROMLEN, EepromCheckCRC());
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
  }
}

