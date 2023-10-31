#ifndef TIME_FUNCTIONS_H
#define TIME_FUNCTIONS_H

time_t my_timegm(struct tm * t);
/* struct tm to seconds since Unix epoch */
std::tm str_to_tm(const char* datestr, int tflag);

#endif
