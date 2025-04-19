#include "Wire.h"

typedef struct {
    uint8_t hours;
    uint8_t minutes;
} Time;

Time getTime(void);
bool equalTime(Time t1, Time t2);


uint8_t getSeconds(void);
uint8_t getMinutes(void);
uint8_t getHours(void);
uint8_t getDay(void);
uint8_t getDate(void);
uint8_t getMonth(void);
uint16_t getYear(void);

void setTime(int8_t seconds, int8_t minutes, int8_t hours, int8_t date, int8_t month, int16_t year);