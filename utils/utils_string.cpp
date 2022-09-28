#include <cctype>
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <regex>
#include "utils_string.h"

using namespace std;

string trim_copy(const string &string_to_trim) {
   std::string::const_iterator it = string_to_trim.begin();
   while (it != string_to_trim.end() && isspace(*it)) {
       ++it;
   }
   std::string::const_reverse_iterator rit = string_to_trim.rbegin();
   while (rit.base() != it && (isspace(*rit) || *rit == 0)) {
       ++rit;
   }
   return std::string(it, rit.base());
}

string toupper_copy(const string &lowercase) {
   string cp(lowercase);
   std::transform(cp.begin(), cp.end(), cp.begin(), (int (*)(int))toupper);
   return cp;
}

size_t mstrnlen_s(const char *szptr, size_t maxsize) {
    if (szptr == nullptr) {
        return 0;
    }
    size_t count = 0;
    while (*szptr++ && maxsize--) {
        count++;
    }
    return count;
}

size_t mstrlcpy(char *dst, const char *src, size_t n) {
    size_t n_orig = n;
    if (n > 0) {
        char *pd;
        const char *ps;

        for (--n, pd = dst, ps = src; n > 0 && *ps != '\0'; --n, ++pd, ++ps) *pd = *ps;

        *pd = '\0';
    }

    return n_orig - n;
}

int hi_atoi(uint8_t *line, size_t n) {
    int value;

    if (n == 0) {
        return -1;
    }

    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return -1;
        }

        value = value * 10 + (*line - '0');
    }

    if (value < 0) {
        return -1;
    }

    return value;
}
