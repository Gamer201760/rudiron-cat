#include "config.h"

#include "Arduino.h"
#include "buildTime.h"
#include "time.h"

struct Task {
  Time t;
  bool executed;
};

struct Packet {
    uint8_t command;
    uint8_t payload[MAX_PAYLOAD_SIZE];
    uint8_t len;
};

Task tasks[MAX_TASK] = {
    Task{Time{8, 0}, 0}, 
    Task{Time{13, 0}, 0},
    Task{Time{20, 0}, 0}
};

unsigned long tmrOrder = 0;
unsigned long tmrHandler = 0;
Packet incomingPacket;

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

void setup() {
    Wire.begin();
    Serial.begin(115200);
    Serial1.begin(9600); // BLE
    // Serial1.setTimeout(5000);

    setupStepper();
    setupSound();
    setupTime();

    Serial.println("Start");

    memset(tasks, 0, sizeof(tasks));

    // Чтение
    readTasksFromEEPROM(EEPROM_START_ADDR, tasks, MAX_TASK);
}
  
void loop() {
    if (millis() - tmrOrder > 1000){
        tmrOrder = millis();
    
        Time now = getTime();
        for (int i = 0; i < MAX_TASK; i++){
            if (isRmTask(i)){
                continue;
            }
            
            if (!tasks[i].executed && equalTime(now, tasks[i].t)) {
                playSound();
                go(SPEED, PARTITION, 1);
                tasks[i].executed = true;
            }
            if (!equalTime(now, tasks[i].t)){
                tasks[i].executed = false;
            }
        }
    }

    if (millis() - tmrHandler > 1000){
        tmrHandler = millis();
        readPacket();
    }
} 

void readPacket() {
    if (Serial1.available() > 0) {
        incomingPacket.command = Serial1.read();
        incomingPacket.len = Serial1.read();

        Serial.print("cmd: ");
        Serial.print(incomingPacket.command, HEX);
        Serial.print(" | len: ");
        Serial.println(incomingPacket.len);

        if (incomingPacket.len > MAX_PAYLOAD_SIZE) {
            incomingPacket.len = MAX_PAYLOAD_SIZE;
        }

        size_t bytesRead = Serial1.readBytes(incomingPacket.payload, incomingPacket.len);
        Serial.print("Bytes read: ");
        Serial.println(bytesRead);
        for (int i = 0; i < MAX_PAYLOAD_SIZE; i++){
            Serial.print(incomingPacket.payload[i], HEX);
            Serial.print(' ');
        }
        Serial.println();
        if (bytesRead == incomingPacket.len) {
            handleCommand(incomingPacket);
        } else {
            Serial.println("Timeout while receiving payload");
        }
    }
}

void handleCommand(const Packet& pkt) {
    switch (pkt.command) {
        case 0x00:
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
        case 0x01:
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
        case 0x02:
            for (int i = 0; i < MAX_TASK; i++){
                Serial.print(i);
                Serial.print(" Task: ");
                Serial.print(tasks[i].t.hours);
                Serial.print(':');
                Serial.print(tasks[i].t.minutes);
                Serial.print(" exec: ");
                Serial.println(tasks[i].executed); 
                Serial1.write(tasks[i].t.hours);
                Serial1.write(tasks[i].t.minutes);
            }   
            break;
        case 0x03:
            if (pkt.len >= 6) {
                setTime(pkt.payload[0], pkt.payload[1], pkt.payload[2], 
                        pkt.payload[3], pkt.payload[4], pkt.payload[5]+2000);
                Serial.print(pkt.payload[2]);
                Serial.print(':');
                
                Serial.print(pkt.payload[1]);
                Serial.print(':');
                
                Serial.print(pkt.payload[0]);
                Serial.print(' ');

                Serial.print(pkt.payload[3]);
                Serial.print('/');
                Serial.print(pkt.payload[4]);
                Serial.print('/');
                Serial.print(pkt.payload[5]+2000);
                Serial.println();
            }
            break;
        case 0x04:
            playSound();
            go(SPEED, PARTITION, 1);
            break;
        default:
            Serial.print("Unknown command: ");
            Serial.println(pkt.command, HEX);
            break;
    }
}

void addTask(uint8_t i, Time t){
    if (i >= MAX_TASK){
        return;
    }
    tasks[i] = Task{t, 0};
    writeTasksToEEPROM(EEPROM_START_ADDR, tasks, MAX_TASK);
}

void removeTask(uint8_t i){
    if (i >= MAX_TASK){
        return;
    }
    tasks[i] = Task{Time{100, 0}, 0};
    writeTasksToEEPROM(EEPROM_START_ADDR, tasks, MAX_TASK);
}

bool isRmTask(uint8_t i){
    if (i >= MAX_TASK){
        return true;
    }
    return tasks[i].t.hours > 23 || tasks[i].t.minutes > 59;
}

void playSound(){
    digitalWrite(SOUND, 0);
    digitalWrite(SOUND, 1);
}

void go(unsigned int speed, uint16_t iter, bool dir){
    digitalWrite(EN, 0);
    digitalWrite(DIR, dir);
    for (uint16_t i = 0; i < iter; i++){
        digitalWrite(STEP, 1);
        delayMicroseconds(speed);
        digitalWrite(STEP, 0);
        delayMicroseconds(speed);
    }
    digitalWrite(EN, 1);
}

void setupStepper(){
    pinMode(DIR, OUTPUT);
    pinMode(EN, OUTPUT);
    pinMode(STEP, OUTPUT);

    digitalWrite(EN, 1);
    digitalWrite(DIR, 1);
}

void setupSound(){
    pinMode(SOUND, OUTPUT);
    digitalWrite(SOUND, 1);
}

void setupTime(){
    setTime(BUILD_SEC, BUILD_MIN, BUILD_HOUR, BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
}

void writeTasksToEEPROM(uint16_t eepromAddr, Task *tasks, size_t count) {
    uint8_t *ptr = (uint8_t *)tasks;
    size_t totalBytes = count * sizeof(Task);

    while (totalBytes > 0) {
        Wire.beginTransmission(EEPROM_ADDR);
        Wire.write((uint8_t)(eepromAddr >> 8));
        Wire.write((uint8_t)(eepromAddr & 0xFF));

        uint8_t pageLeft = 32 - (eepromAddr % 32);
        uint8_t toWrite = min(pageLeft, totalBytes);

        Wire.write(ptr, toWrite);
        Wire.endTransmission();
        delay(5);

        eepromAddr += toWrite;
        ptr += toWrite;
        totalBytes -= toWrite;
    }
}

void readTasksFromEEPROM(uint16_t eepromAddr, Task *tasks, size_t count) {
    uint8_t *ptr = (uint8_t *)tasks;
    size_t totalBytes = count * sizeof(Task);

    while (totalBytes > 0) {
        Wire.beginTransmission(EEPROM_ADDR);
        Wire.write((uint8_t)(eepromAddr >> 8));
        Wire.write((uint8_t)(eepromAddr & 0xFF));
        Wire.endTransmission(false);

        uint8_t toRead = min(32, totalBytes);
        Wire.requestFrom(EEPROM_ADDR, toRead);
        for (uint8_t i = 0; i < toRead && Wire.available(); i++) {
            *ptr++ = Wire.read();
        }

        eepromAddr += toRead;
        totalBytes -= toRead;
    }
}
