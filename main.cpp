#include "config.h"         // Подключение пользовательского конфигурационного файла

#include "Arduino.h"        // Основная библиотека Arduino
#include "buildTime.h"      // Файл с временем сборки (макросы BUILD_HOUR и т.д.)
#include "time.h"           // Работа с временем

// Структура для хранения задачи: время и факт выполнения
struct Task {
  Time t;                   // Время выполнения задачи
  bool executed;            // Флаг, была ли задача выполнена
};

// Структура пакета, получаемого по Serial (например, BLE)
struct Packet {
    uint8_t command;                       // Команда
    uint8_t payload[MAX_PAYLOAD_SIZE];    // Полезная нагрузка
    uint8_t len;                           // Длина данных
};

// Массив задач. Задано 3 задачи по умолчанию (8:00, 13:00, 20:00)
Task tasks[MAX_TASK] = {
    Task{Time{8, 0}, 0}, 
    Task{Time{13, 0}, 0},
    Task{Time{20, 0}, 0}
};

unsigned long tmrOrder = 0;    // Таймер для проверки задач
unsigned long tmrHandler = 0;  // Таймер для обработки пакетов
Packet incomingPacket;         // Входящий пакет

// Объявление функций
bool isRmTask(uint8_t i);              
void removeTask(uint8_t i);            
void addTask(uint8_t i, Time t);       

void readPacket();                     
void handleCommand(const Packet& pkt); 

void playSound();                     
void go(unsigned int speed, uint16_t iter, bool dir); 

void setupStepper();                  
void setupSound();                    
void setupTime();                     

void writeTasksToEEPROM(uint16_t eepromAddr, Task *tasks, size_t count); 
void readTasksFromEEPROM(uint16_t eepromAddr, Task *tasks, size_t count);

// Инициализация устройства
void setup() {
    Wire.begin();                          // Запуск I2C
    Serial.begin(115200);                 // Отладочный порт
    Serial1.begin(9600);                  // Вторичный порт, например BLE

    setupStepper();                       // Инициализация шагового двигателя
    setupSound();                         // Инициализация звука
    setupTime();                          // Установка времени

    Serial.println("Start");

    memset(tasks, 0, sizeof(tasks));      // Обнуление задач

    readTasksFromEEPROM(EEPROM_START_ADDR, tasks, MAX_TASK);  // Загрузка задач из EEPROM
}
  
// Главный цикл программы
void loop() {
    // Проверка задач каждую секунду
    if (millis() - tmrOrder > 1000){
        tmrOrder = millis();
    
        Time now = getTime();             // Текущее время
        for (int i = 0; i < MAX_TASK; i++){
            if (isRmTask(i)) continue;    // Пропустить "удалённые" задачи
            
            // Выполнить задачу, если время совпадает
            if (!tasks[i].executed && equalTime(now, tasks[i].t)) {
                playSound();
                go(SPEED, PARTITION, 1);
                tasks[i].executed = true;
            }

            // Сброс флага, если текущее время отличается
            if (!equalTime(now, tasks[i].t)){
                tasks[i].executed = false;
            }
        }
    }

    // Обработка входящих пакетов каждую секунду
    if (millis() - tmrHandler > 1000){
        tmrHandler = millis();
        readPacket();
    }
} 

// Чтение команды по Serial1
void readPacket() {
    if (Serial1.available() > 0) {
        incomingPacket.command = Serial1.read();   // Чтение команды
        incomingPacket.len = Serial1.read();       // Чтение длины данных

        Serial.print("cmd: ");
        Serial.print(incomingPacket.command, HEX);
        Serial.print(" | len: ");
        Serial.println(incomingPacket.len);

        if (incomingPacket.len > MAX_PAYLOAD_SIZE) {
            incomingPacket.len = MAX_PAYLOAD_SIZE;
        }

        size_t bytesRead = Serial1.readBytes(incomingPacket.payload, incomingPacket.len);  // Чтение данных
        Serial.print("Bytes read: ");
        Serial.println(bytesRead);

        // Вывод считанных байт
        for (int i = 0; i < MAX_PAYLOAD_SIZE; i++){
            Serial.print(incomingPacket.payload[i], HEX);
            Serial.print(' ');
        }
        Serial.println();

        // Обработка команды, если данные получены полностью
        if (bytesRead == incomingPacket.len) {
            handleCommand(incomingPacket);
        } else {
            Serial.println("Timeout while receiving payload");
        }
    }
}

// Обработка команды
void handleCommand(const Packet& pkt) {
    switch (pkt.command) {
        case 0x00: // Добавить задачу
            if (pkt.len >= 3) {
                uint8_t hour = pkt.payload[0];
                uint8_t minute = pkt.payload[1];
                uint8_t idx = pkt.payload[2];

                Serial.print(idx);
                Serial.print(" New task: ");
                Serial.print(hour);
                Serial.print(":");
                Serial.println(minute);

                addTask(idx, Time{hour, minute});
            }
            break;

        case 0x01: // Удалить задачу
            if (pkt.len >= 1) {
                uint8_t idx = pkt.payload[0];
                Serial.print(idx);
                Serial.print(" remove task: ");
                Serial.print(tasks[idx].t.hours);
                Serial.print(":");
                Serial.println(tasks[idx].t.minutes);

                removeTask(idx);
            }
            break;

        case 0x02: // Отправить список задач
            for (int i = 0; i < MAX_TASK; i++){
                Serial.print(i);
                Serial.print(" Task: ");
                Serial.print(tasks[i].t.hours);
                Serial.print(':');
                Serial.print(tasks[i].t.minutes);
                Serial.print(" exec: ");
                Serial.println(tasks[i].executed); 

                // Отправка по BLE
                Serial1.write(tasks[i].t.hours);
                Serial1.write(tasks[i].t.minutes);
            }   
            break;

        case 0x03: // Установка времени
            if (pkt.len >= 6) {
                setTime(pkt.payload[0], pkt.payload[1], pkt.payload[2], 
                        pkt.payload[3], pkt.payload[4], pkt.payload[5]+2000);
                
                // Отладочный вывод
                Serial.print(pkt.payload[2]); Serial.print(':');
                Serial.print(pkt.payload[1]); Serial.print(':');
                Serial.print(pkt.payload[0]); Serial.print(' ');
                Serial.print(pkt.payload[3]); Serial.print('/');
                Serial.print(pkt.payload[4]); Serial.print('/');
                Serial.println(pkt.payload[5]+2000);
            }
            break;

        case 0x04: // Принудительный запуск двигателя и звука
            playSound();
            go(SPEED, PARTITION, 1);
            break;

        default:
            Serial.print("Unknown command: ");
            Serial.println(pkt.command, HEX);
            break;
    }
}

// Добавление задачи
void addTask(uint8_t i, Time t){
    if (i >= MAX_TASK) return;

    tasks[i] = Task{t, 0};
    writeTasksToEEPROM(EEPROM_START_ADDR, tasks, MAX_TASK);  // Сохранение в EEPROM
}

// Удаление задачи
void removeTask(uint8_t i){
    if (i >= MAX_TASK) return;

    tasks[i] = Task{Time{100, 0}, 0};  // Некорректное время — считается удалённой
    writeTasksToEEPROM(EEPROM_START_ADDR, tasks, MAX_TASK);
}

// Проверка, является ли задача удалённой
bool isRmTask(uint8_t i){
    if (i >= MAX_TASK) return true;
    return tasks[i].t.hours > 23 || tasks[i].t.minutes > 59;
}

// Воспроизведение звука
void playSound(){
    digitalWrite(SOUND, 0);
    digitalWrite(SOUND, 1);
}

// Движение шагового двигателя
void go(unsigned int speed, uint16_t iter, bool dir){
    digitalWrite(EN, 0);            // Включить драйвер
    digitalWrite(DIR, dir);         // Установить направление

    for (uint16_t i = 0; i < iter; i++){
        digitalWrite(STEP, 1);
        delayMicroseconds(speed);
        digitalWrite(STEP, 0);
        delayMicroseconds(speed);
    }

    digitalWrite(EN, 1);            // Отключить драйвер
}
// Настройка шагового двигателя
void setupStepper(){
    pinMode(DIR, OUTPUT);    // Установка пина направления как выход
    pinMode(EN, OUTPUT);     // Установка пина включения как выход
    pinMode(STEP, OUTPUT);   // Установка пина шага как выход

    digitalWrite(EN, 1);     // Отключение драйвера по умолчанию (EN = HIGH)
    digitalWrite(DIR, 1);    // Установка направления по умолчанию
}

// Настройка пина для звука (пищалки)
void setupSound(){
    pinMode(SOUND, OUTPUT);  // Установка пина звука как выход
    digitalWrite(SOUND, 1);  // Установка высокого уровня (пищалка выключена)
}

// Установка времени при старте — используется время сборки прошивки
void setupTime(){
    setTime(BUILD_SEC, BUILD_MIN, BUILD_HOUR, BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
}

// Запись массива задач в EEPROM по I2C
void writeTasksToEEPROM(uint16_t eepromAddr, Task *tasks, size_t count) {
    uint8_t *ptr = (uint8_t *)tasks;                   // Указатель на данные как на массив байт
    size_t totalBytes = count * sizeof(Task);          // Общее количество байт для записи

    while (totalBytes > 0) {
        Wire.beginTransmission(EEPROM_ADDR);           // Начало передачи
        Wire.write((uint8_t)(eepromAddr >> 8));        // Старший байт адреса
        Wire.write((uint8_t)(eepromAddr & 0xFF));      // Младший байт адреса

        uint8_t pageLeft = 32 - (eepromAddr % 32);     // Вычисление оставшегося места на странице
        uint8_t toWrite = min(pageLeft, totalBytes);   // Определение, сколько байт можно записать

        Wire.write(ptr, toWrite);                      // Запись данных
        Wire.endTransmission();                        // Завершение передачи
        delay(5);                                      // Задержка для завершения записи

        eepromAddr += toWrite;                         // Смещение адреса
        ptr += toWrite;                                // Смещение указателя
        totalBytes -= toWrite;                         // Уменьшение количества оставшихся байт
    }
}

// Чтение массива задач из EEPROM по I2C
void readTasksFromEEPROM(uint16_t eepromAddr, Task *tasks, size_t count) {
    uint8_t *ptr = (uint8_t *)tasks;                   // Указатель на буфер
    size_t totalBytes = count * sizeof(Task);          // Общее количество байт для чтения

    while (totalBytes > 0) {
        Wire.beginTransmission(EEPROM_ADDR);           // Начало передачи
        Wire.write((uint8_t)(eepromAddr >> 8));        // Старший байт адреса
        Wire.write((uint8_t)(eepromAddr & 0xFF));      // Младший байт адреса
        Wire.endTransmission(false);                   // Повторный старт (не завершаем)

        uint8_t toRead = min(32, totalBytes);          // Не больше 32 байт за раз
        Wire.requestFrom(EEPROM_ADDR, toRead);         // Запрос на чтение
        for (uint8_t i = 0; i < toRead && Wire.available(); i++) {
            *ptr++ = Wire.read();                      // Чтение байта
        }

        eepromAddr += toRead;                          // Смещение адреса
        totalBytes -= toRead;                          // Уменьшение количества оставшихся байт
    }
}
