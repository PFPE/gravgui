#include <ctime>
#include <sstream>

time_t my_timegm(struct tm * t)
/* struct tm to seconds since Unix epoch */
{
    long year;
    time_t result;
#define MONTHSPERYEAR   12      /* months per calendar year */
    static const int cumdays[MONTHSPERYEAR] =
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

    /*@ +matchanyintegral @*/
    year = 1900 + t->tm_year + t->tm_mon / MONTHSPERYEAR;
    result = (year - 1970) * 365 + cumdays[t->tm_mon % MONTHSPERYEAR];
    result += (year - 1968) / 4;
    result -= (year - 1900) / 100;
    result += (year - 1600) / 400;
    if ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0) &&
        (t->tm_mon % MONTHSPERYEAR) < 2)
        result--;
    result += t->tm_mday - 1;
    result *= 24;
    result += t->tm_hour;
    result *= 60;
    result += t->tm_min;
    result *= 60;
    result += t->tm_sec;
    if (t->tm_isdst == 1)
        result -= 3600;
    /*@ -matchanyintegral @*/
    return (result);
}

std::tm str_to_tm(const char* datestr, int tflag) {
    std::tm t = {};

    if (tflag == 1) {
        if (std::sscanf(datestr, "%d/%d/%d-%d:%d:%d", &t.tm_mon, &t.tm_mday, &t.tm_year, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6) {
            return t;  // TODO more consequences here?
        }
    }
    if (tflag == 2) {
        if (std::sscanf(datestr, "%d-%d-%dT%d:%d:%dZ", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6) {
            return t;  // TODO more consequences here?
        }
    }

    t.tm_year -= 1900;
    --t.tm_mon;
    t.tm_isdst = -1;
    return t;
}

