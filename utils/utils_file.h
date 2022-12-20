#ifndef _UTILS_FILE_H
#define _UTILS_FILE_H

#include <string>
#include <fstream>
#include <cstring>
#include <sys/stat.h>

std::string get_file_contents(const char *filename,size_t max_size);
bool dir_exists(const std::string& dir);
bool file_exists(const std::string& file);
bool create_dir(const char *dir);


#endif  // _UTILS_FILE_H
