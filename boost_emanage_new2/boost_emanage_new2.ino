#include <LiquidCrystalRus.h>
#include <EEPROM.h>

// Параметры железа
LiquidCrystalRus lcd(2, 3, 4, 5, 6, 7);
const int Forsunka =  12;        // номер выхода, подключенного к форсунке
const int BoostPin =  11;        // номер выхода, подключенного к соленоиду
const int OrosheniePin =  13;        // номер выхода, подключенного к орошению
const int interval_LCD = 200;   // интервал между перерисовкой экрана в мс
const int interval_Button = 20;   // интервал между считыванием кнопок
const int SbrosMaxInj = 30000;  // пауза перед обнулением максимального времени впрыска
const int Button1Pin = 8;
const int Button2Pin = 9;
const int Button3Pin = 10;
const int MaxConfDisp = 5;

// Переменные хранения времени
volatile unsigned long previousMicros = 0;     // Время последнего открытия форсунки
unsigned long previousMillisLCD = 0;  // время последнего обновления экрана
unsigned long previousMillisButton = 0;  // время последнего обновления кнопок
volatile unsigned long pMObor = 0;                    // храним время последнего подсчета оборотов
volatile unsigned long pMMaxInj = 0;                  // храним время последнего подсчета максимального впрыска
unsigned long pButton1 = 0;           // храним время последнего переключения кнопки 1
unsigned long pButton2 = 0;           // храним время последнего переключения кнопки 2
unsigned long pButton3 = 0;           // храним время последнего переключения кнопки 3
unsigned long pConfTime = 0;          // Храним паузу для подсветки надписи Настройки       
unsigned long pMDuty=0;               // Время открытия соленоида
unsigned long sOpenFors=0;            // Время открытой форсунки
unsigned long pOroshenie=0;            // Время оршения
unsigned long pOroshenie2=0;            // Время оршения
unsigned long pOroshenie3=0;            // Время оршения

unsigned long pLoopTime = 0;          //тест скорости цикла
unsigned long pLoopTimeT = 0;

// Переменные прочие
unsigned long ForsOpen = 0; // Время открытия форсунки в мкc
int DutyW=0; // Рабочая скважность соленоида в %
int ApexBoost=0; // Пик давления
int Boost = 0; // Текущий буст
volatile unsigned long sFors = 0;
volatile unsigned long Rashod4 = 0;
volatile unsigned long RashodP = 0;
int BaseDuty = 0;
boolean Oroshenie = false;
boolean OroshenieOrd = true;

// Настройки
int MinBoostConf = 40;
int MaxBoostConf = 100;
int SetDutyMin=65; 
int SetDutyMax=95; 
int TryDuty = 90; 
int FrencSol = 20; // Частота соленоида в Гц
int Odlit = 0;
int Oprom = 0;

// Конфигурация
boolean ConfMode = false;
int ConfDisp = 0;

// глобальные переменные
volatile unsigned long MaxWMS = 0; // Максимальное время открытия форсунки в мкс
unsigned long PerTime = 0; // временной отрезок управления соленоидом в мкс 
volatile int Oboroti=0; // Обороты в минуту

void setup() {
  //Выставлем число столбцов и строк
  lcd.begin(16, 2);
  //Выводим текст
  lcd.setCursor(5, 0);
  lcd.print("SUBARU");
  attachInterrupt(1, blink2, CHANGE);
  pinMode(Button1Pin, INPUT);
  pinMode(Button2Pin, INPUT);
  pinMode(Button3Pin, INPUT);
  pinMode(Forsunka, INPUT);
  pinMode(BoostPin, OUTPUT);
  pinMode(OrosheniePin, OUTPUT);
  digitalWrite(14, HIGH); // включить резистор на выводе аналогового входа 0
  PerTime = 1000000/FrencSol;
  
  // Читаем настройки
    // Читаем настройки
  byte EEP[1024];
  for (int i=0; i<5; i++){
    EEP[i] = EEPROM.read(i);
  } 
  MinBoostConf = EEP[0];
  MaxBoostConf = EEP[1];
  SetDutyMax = EEP[2];
  SetDutyMin = EEP[3];
  OroshenieOrd = EEP[4];
  
  SetBaseDuty(MaxBoostConf);
  
  delay(1000);
}

void loop() {
  // ************ TEMP ******************
   pLoopTime = micros() - pLoopTimeT;
   pLoopTimeT = micros(); 
   //************************************
    if (micros()-pMDuty >= PerTime){
      digitalWrite(BoostPin, HIGH); // Откроем соленоид
      pMDuty = micros(); // запомним время открытия соленоида
    }
    
    // ********** Считываем буст и вычисляем Duty
    int BoostTemp1 = map(analogRead(0), 465 ,770 , 0 , 100); // Перевод напряжения в уровень наддува
    int BoostTemp2 = map(analogRead(0), 465 ,770 , 0 , 100); // Перевод напряжения в уровень наддува
    int BoostTemp3 = map(analogRead(0), 465 ,770 , 0 , 100); // Перевод напряжения в уровень наддува
    
    Boost = ( Boost + (BoostTemp1+BoostTemp2+BoostTemp3)/3 )/2;
    ApexBoost = max(ApexBoost,Boost);

    if (Boost > 10){ // Меньше 0.1 бар соленоид отключен
      if (Boost > MaxBoostConf-10){
        DutyW = map(Boost ,MaxBoostConf ,MaxBoostConf+15 , SetDutyMax, 40);
      }else
      {
        DutyW = BaseDuty+5;
      }  
      if (Boost < MinBoostConf) DutyW = 100;
    }else{
      DutyW = 0;
    }        
    
    if(ConfMode && ConfDisp==4) DutyW = TryDuty; // Тест Дьюти в режиме настроек
            
    if (DutyW > 100) DutyW=100;
    if (DutyW < 0) DutyW=0;
    
    ReadButton();
    Display();
    

  //  if(millis() - pOroshenie3 >= 180000){
 //     pOroshenie3 = millis();
 //     pOroshenie2 = millis();
 //     Odlit = 1000;
 //     Oprom = 1000;
 //     OroshenieFunc();  
 //   }  
    if (Boost > 20 && Boost < 60){
      pOroshenie2 = millis();
      Odlit = 10000;
      Oprom = 5000;
      OroshenieFunc();
    }
    if (Boost > 60 && Boost < 100){
      pOroshenie2 = millis();
      Odlit = 5000;
      Oprom = 1000;
      OroshenieFunc();
    }  
    if (Boost > 100){
      pOroshenie2 = millis();
      Odlit = 1000;
      Oprom = 1000;
      OroshenieFunc();
    }
    
    if (Oroshenie && (millis() - pOroshenie2 >= Odlit)){
      Oroshenie = false; 
      digitalWrite(OrosheniePin, LOW); 
    } 

    ResetMaxInjTime(); // Сбрасываем максимальное время впрыска
    
  if(micros()-pMDuty >= DutyW*PerTime/100) // Если прошел период открытия, закрываем соленоид
  {
    digitalWrite(BoostPin, LOW);
  }

}

void blink2() // Считываем время открытия форсунки
{
  unsigned long currentMicros = micros();
  if (previousMicros>=currentMicros) return;

  if (digitalRead(Forsunka)==LOW) // форсунка открылась
  { 
    previousMicros = currentMicros;
  }
  else // форсунка закрылась
  {
    ForsOpen = currentMicros-previousMicros;
    sFors += ForsOpen;
    
    if (currentMicros - sOpenFors >= 1000000){
      sOpenFors = currentMicros;
      Rashod4 = sFors*67200/1000000;
      sFors = 0;  
    }  
    Oboroti = (Oboroti + (60000/((currentMicros-pMObor)/1000)*2))/2;
    pMObor = currentMicros;

    if (ForsOpen > MaxWMS)
    {
      MaxWMS = ForsOpen; // Запомним максимальное время открытия форсунки
      pMMaxInj = currentMicros; // Время фиксации максимального врмени открытия
    }
  }
}

void ResetMaxInjTime()
{
  unsigned long TekMicros = micros(); // Текущее время 
  if((TekMicros - pMMaxInj)/1000 > SbrosMaxInj && pMMaxInj != 0) // Сбрасываем максимальное время впрыска
  {
    pMMaxInj = 0;
    MaxWMS = 0;
    ApexBoost = 0;
  }
}

void Display()
{
  unsigned long currentMillisLCD = millis();
  if(currentMillisLCD - previousMillisLCD > interval_LCD) {
    previousMillisLCD = currentMillisLCD; 

    lcd.clear();

    if (ConfMode) // Режим конфига
    {
      if (currentMillisLCD - pConfTime < 1500)
      {
        lcd.setCursor(0, 0);
        lcd.print("Настройки");  
      }else
      {
        // *********** Заголовок настрок
        lcd.setCursor(0, 0);
        switch (ConfDisp){
        case 0:
          lcd.print("Мин порог");
          break;
        case 1:
          lcd.print("Макс порог");
          break;
        case 2: 
          lcd.print("SetDutyMin");
          break;  
        case 3: 
          lcd.print("SetDutyMax");
          break;  
        case 4: 
          lcd.print("real TestDuty");
          break; 
        case 5: 
          lcd.print("Орошение разрешено?");
          break;   
        }  
        // ************************** Значение настроек
        lcd.setCursor(0, 1);
        switch (ConfDisp){
        case 0:
          lcd.print(MinBoostConf);
          break;
        case 1:
          lcd.print(MaxBoostConf);
          break;
        case 2: 
          lcd.print(SetDutyMin);
          break; 
        case 3: 
          lcd.print(SetDutyMax);
          break; 
        case 4: 
          lcd.setCursor(6, 1);
          lcd.print(TryDuty);
          lcd.setCursor(11, 1);
          lcd.print(Boost);
          break; 
        case 5: 
          if(OroshenieOrd){
            lcd.print("Да");
          }else{lcd.print("Нет");}  
          break;  
        }
      }
    }
    else // Режим ДИСПЛЕЯ
    {
      if(Boost<0){
        lcd.setCursor(0, 0);
        lcd.print('-');
      } 
      lcd.setCursor(1, 0);
      lcd.print(abs(Boost)/100);
      lcd.setCursor(2, 0);
      lcd.print('.');
      lcd.setCursor(3, 0);
      lcd.print(abs(Boost)/10 % 10);
      lcd.setCursor(4, 0);
      lcd.print(abs(Boost) % 10);

      lcd.setCursor(6, 0);
      lcd.print("max");
      
      lcd.setCursor(9, 0);
      lcd.print(ApexBoost/100);
      lcd.setCursor(10, 0);
      lcd.print('.');
      lcd.setCursor(11, 0);
      lcd.print(ApexBoost/10 % 10);
      lcd.setCursor(12, 0);
      lcd.print(ApexBoost % 10);
      
      if (Oroshenie){
        lcd.setCursor(15, 0);
        lcd.print('!');
      } 

      lcd.setCursor(3, 1);
      lcd.print("max");
      
      int t1 = MaxWMS/10000;
      if (t1 > 0){
        lcd.setCursor(6, 1);
        lcd.print(t1);   
      }
      lcd.setCursor(7, 1);
      lcd.print(MaxWMS/1000 % 10);
      lcd.setCursor(8, 1);
      lcd.print('.');
      lcd.setCursor(9, 1);
      lcd.print(MaxWMS/100 % 10);
      lcd.setCursor(10, 1);
      lcd.print(MaxWMS/10 % 10);
      
      lcd.setCursor(12, 1);
      lcd.print(Rashod4);
      
      lcd.setCursor(0,1);
      lcd.print(pLoopTime);
    }  
  }
}

void ReadButton()
{
  if(millis() - previousMillisButton > interval_Button) {
    previousMillisButton = millis(); 
    
    unsigned long CMB = millis();
  
    int B1state = digitalRead(Button1Pin);
    int B2state = digitalRead(Button2Pin);
    int B3state = digitalRead(Button3Pin);
  
    unsigned long Button1Time = 0;
    unsigned long Button2Time = 0;
    unsigned long Button3Time = 0;
    
    unsigned long bTimer = millis(); // Текущее время
    if(bTimer - pConfTime >= 20) 
  
    if (B1state == HIGH){
      if (pButton1==0) pButton1 = CMB;  
    }
    else{
      if (pButton1 != 0 && CMB-pButton1 > 10){
        Button1Time = CMB-pButton1;
        pButton1 = 0;
      }  
    }
  
    if (B2state == HIGH){
      if (pButton2==0) pButton2 = CMB;  
    }
    else{
      if (pButton2 != 0 && CMB-pButton2 > 10){
        Button2Time = CMB-pButton2;
        pButton2 = 0;
      }  
    }
  
    if (B3state == HIGH){
      if (pButton3==0) pButton3 = CMB;  
    }
    else{
      if (pButton3 != 0 && CMB-pButton3 > 10){
        Button3Time = CMB-pButton3;
        pButton3 = 0;
      }  
    }
  
    if (Button1Time > 1000) // Отпустили кнопку настроек (длинное нажатие)
    {
      ConfMode = !ConfMode; 
      if (!ConfMode){ // Вышли из настроек. Запись настроек в память
        EEPROM.write(0, MinBoostConf);
        EEPROM.write(1, MaxBoostConf);
        EEPROM.write(2, SetDutyMax);
        EEPROM.write(3, SetDutyMin);
        EEPROM.write(4, OroshenieOrd);
        SetBaseDuty(MaxBoostConf);
      }else{ // Зашли в настройки
        pConfTime = millis();
      }
    }
  
    if (Button1Time >= 50 && Button1Time < 1000) // Отпустили кнопку настроек (короткое нажатие)
    {
      if(ConfMode)
      {
        ConfDisp++;
        if (ConfDisp > MaxConfDisp) ConfDisp=0;
      }
    }
  
    if (Button2Time >= 50) // ************************************** Отпустили кнопку 2
    {
      if(ConfMode && ConfDisp==0)
      {
        MinBoostConf += 1;
        if (MinBoostConf > 140) MinBoostConf=0;
      }
  
      if(ConfMode && ConfDisp==1)
      {
        MaxBoostConf += 1;
        if (MaxBoostConf > 140) MaxBoostConf=0;
      }
  
      if(ConfMode && ConfDisp==2)
      {
        SetDutyMin++;
        if (SetDutyMin > 100) SetDutyMin=0;
      }
      
      if(ConfMode && ConfDisp==3)
      {
        SetDutyMax++;
        if (SetDutyMax > 100) SetDutyMax=0;
      }
      
      if(ConfMode && ConfDisp==4)
      {
        TryDuty += 1;
        if (TryDuty > 100) TryDuty=0;
      }
      
      if(ConfMode && ConfDisp==5)
      {
        OroshenieOrd = !OroshenieOrd;
      }
    }
  
    if (Button3Time >= 50) // Отпустили кнопку 3
    {
      if(ConfMode && ConfDisp==0)
      {
        MinBoostConf -= 1;
        if (MinBoostConf < 0) MinBoostConf=140;
      }
  
      if(ConfMode && ConfDisp==1)
      {
        MaxBoostConf -= 1;
        if (MaxBoostConf < 0) MaxBoostConf=140;
      }
  
      if(ConfMode && ConfDisp==2)
      {
        SetDutyMin--;
        if (SetDutyMin < 0) SetDutyMin=100;
      }
      
      if(ConfMode && ConfDisp==3)
      {
        SetDutyMax--;
        if (SetDutyMax < 0) SetDutyMax=100;
      }
      
      if(ConfMode && ConfDisp==4)
      {
        TryDuty -= 1;
        if (TryDuty < 0) TryDuty=100;
      }   
      
      if(ConfMode && ConfDisp==5)
      {
        OroshenieOrd = !OroshenieOrd;
      }
    }
  }
} 

void SetBaseDuty(int DDuty){
  BaseDuty = map(DDuty ,MinBoostConf ,MaxBoostConf , SetDutyMin, SetDutyMax);
}

void OroshenieFunc(){
  if (!Oroshenie && (millis() - pOroshenie >= Oprom) && OroshenieOrd){
    pOroshenie = millis();

    Oroshenie = true;
    digitalWrite(OrosheniePin, HIGH);
  }  
}  










