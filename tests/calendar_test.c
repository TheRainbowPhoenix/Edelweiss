#include "../payloads/calendar.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(ebrew_month_days(1900, 2) == 28);
    assert(ebrew_month_days(2000, 2) == 29);
    assert(ebrew_month_days(2024, 2) == 29);
    assert(ebrew_month_days(2100, 2) == 28);
    assert(ebrew_first_weekday(2000, 1) == 6);
    assert(ebrew_first_weekday(2020, 8) == 6);
    assert(ebrew_first_weekday(2026, 11) == 0);
    assert(ebrew_month_days(2026, 11) == 30);
    assert(!ebrew_month_days(2026, 0) && !ebrew_month_days(2026, 13));
    assert(ebrew_first_weekday(0, 1) < 0);
    puts("Calendar leap-year, century and six-row month checks passed");
}
