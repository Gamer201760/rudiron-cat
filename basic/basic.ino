#include "Arduino.h"

void setup()
{
    //конфигурация встроенных кнопок
    pinMode(BUTTON_BUILTIN_1, INPUT_PULLDOWN);
    pinMode(BUTTON_BUILTIN_2, INPUT_PULLDOWN);
    pinMode(BUTTON_BUILTIN_3, INPUT_PULLDOWN);

    //конфигурация встроенных светодиодов
    pinMode(LED_BUILTIN_1, OUTPUT);
    pinMode(LED_BUILTIN_2, OUTPUT);

    //включение встроенных светодиодов
    digitalWrite(LED_BUILTIN_1, true);
    digitalWrite(LED_BUILTIN_2, true);

    //конфигурация последовательного порта
    Serial.begin(115200);
    //отправка приветствия через последовательный порт
    Serial.println("Рудирон Бутерброд!");
}

void loop()
{
    //чтение встроенных кнопок, true = есть нажатие
    bool pressed1 = digitalRead(BUTTON_BUILTIN_1);
    bool pressed2 = digitalRead(BUTTON_BUILTIN_2);
    bool pressed3 = digitalRead(BUTTON_BUILTIN_3);
    
    //пауза программы на 1 секунду
    delay(1000);
}
