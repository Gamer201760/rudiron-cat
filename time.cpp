#include "time.h"

const uint8_t _addr = 0x68;
uint8_t _unpackRegister(uint8_t data);
uint8_t _encodeRegister(int8_t data);
static uint8_t DS_dim(uint8_t i);
static uint16_t getWeekDay(uint16_t y, uint8_t m, uint8_t d);
uint8_t _readRegister(uint8_t addr);
uint8_t _unpackHours(uint8_t data);

uint8_t _unpackRegister(uint8_t data) {
    return ((data >> 4) * 10 + (data & 0xF));
}

uint8_t _encodeRegister(int8_t data) {
    return (((data / 10) << 4) | (data % 10));
}

static uint8_t DS_dim(uint8_t i) {
    return (i < 7) ? ((i == 1) ? 28 : ((i & 1) ? 30 : 31)) : ((i & 1) ? 31 : 30);
}

static uint16_t getWeekDay(uint16_t y, uint8_t m, uint8_t d) {
    if (y >= 2000)
    y -= 2000;
    uint16_t days = d;
    for (uint8_t i = 1; i < m; ++i)
    //days += pgm_read_byte(_ds_daysInMonth + i - 1);
    days += DS_dim(i - 1);
    if (m > 2 && y % 4 == 0) days++;
    return (days + 365 * y + (y + 3) / 4 + 4) % 7 + 1;
}

uint8_t _readRegister(uint8_t addr) {
    Wire.beginTransmission(_addr);
    Wire.write(addr);
    if (Wire.endTransmission() != 0) return 0;
    Wire.requestFrom(_addr, (uint8_t)1);
    return Wire.read();
}

uint8_t _unpackHours(uint8_t data) {
    if (data & 0x20) return ((data & 0xF) + 20);
    else if (data & 0x10) return ((data & 0xF) + 10);
    else return (data & 0xF);
}

uint8_t getSeconds(void) {
    return (_unpackRegister(_readRegister(0x00)));
}

uint8_t getMinutes(void) {
    return (_unpackRegister(_readRegister(0x01)));
}

uint8_t getHours(void) {
    return (_unpackHours(_readRegister(0x02)));
}

uint8_t getDay(void) {
    return _readRegister(0x03);
}

uint8_t getDate(void) {
    return (_unpackRegister(_readRegister(0x04)));
}

uint8_t getMonth(void) {
    return (_unpackRegister(_readRegister(0x05)));
}

uint16_t getYear(void) {
    return (_unpackRegister(_readRegister(0x06)) + 2000);
}

void setTime(int8_t seconds, int8_t minutes, int8_t hours, int8_t date, int8_t month, int16_t year) {
    // защиты от дурака
    month = constrain(month, 1, 12);
    //date = constrain(date, 0, pgm_read_byte(_ds_daysInMonth + month - 1));
    date = constrain(date, 0, DS_dim(month - 1));
    seconds = constrain(seconds, 0, 59);
    minutes = constrain(minutes, 0, 59);
    hours = constrain(hours, 0, 23);
    
    // отправляем
    uint8_t day = getWeekDay(year, month, date);
    year -= 2000;
    Wire.beginTransmission(_addr);
    Wire.write(0x00);
    Wire.write(_encodeRegister(seconds));
    Wire.write(_encodeRegister(minutes));
    if (hours > 19) Wire.write((0x2 << 4) | (hours % 20));
    else if (hours > 9) Wire.write((0x1 << 4) | (hours % 10));
    else Wire.write(hours);	
    Wire.write(day);
    Wire.write(_encodeRegister(date));
    Wire.write(_encodeRegister(month));
    Wire.write(_encodeRegister(year));
    Wire.endTransmission();
}

Time getTime(void) {
    return Time{
        .hours=getHours(),
        .minutes=getMinutes()
    };
}

bool equalTime(Time t1, Time t2){
    return t1.minutes == t2.minutes && t1.hours == t2.hours;
}