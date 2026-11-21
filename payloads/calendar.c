#include "calendar.h"
int ebrew_month_days(unsigned year, unsigned month)
{
    static const unsigned char days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year < 1900 || year > 9999 || month < 1 || month > 12)
        return 0;
    int leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    return days[month - 1] + (month == 2 && leap);
}
int ebrew_first_weekday(unsigned year, unsigned month)
{
    static const unsigned char offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (!ebrew_month_days(year, month))
        return -1;
    if (month < 3)
        year--;
    return (int)((year + year / 4 - year / 100 + year / 400 + offsets[month - 1] + 1) % 7);
}
