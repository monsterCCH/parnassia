#ifndef JNILICENSE_UTILS_STRING_H_
#define JNILICENSE_UTILS_STRING_H_

#include <string>
#include <vector>

std::string trim_copy(const std::string& string_to_trim);

std::string toupper_copy(const std::string& lowercase);

size_t mstrnlen_s(const char* szptr, size_t maxsize);

size_t mstrlcpy(char* dst, const char* src, size_t n);

int hi_atoi(uint8_t *line, size_t n);

template<typename First, typename ... T>
inline bool is_in(const First& first, const T& ... t) {
    return ((first == t) or ...);
}


#endif // JNILICENSE_UTILS_STRING_H_
