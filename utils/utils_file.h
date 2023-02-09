#ifndef _UTILS_FILE_H
#define _UTILS_FILE_H

#include <string>
#include <fstream>
#include <cstring>
#include <sys/stat.h>
#include "base.h"

std::string get_file_contents(const char *filename,size_t max_size);
bool dir_exists(const std::string& dir);
bool file_exists(const std::string& file);
bool create_dir(const char *dir);
bool IsFile(const std::string& filePath);
bool FastReadFile(const std::string &filePath, std::string &fileData, bool lock);
bool WriteFile(const std::string &filePath, const std::string &fileData, bool lock);

class ReadSmallFile : noncopyable
{
public:
    ReadSmallFile(StringArg filename);
    ~ReadSmallFile();
//    template<typename String>
    int readToString(int maxSize,
                             std::string* content,
                             int64_t* fileSize = NULL,
                             int64_t* modifyTime = NULL,
                             int64_t* createTime = NULL);
    int readFileStat(int64_t* fileSize, int64_t* modifyTime = NULL, int64_t* createTime = NULL);
    const char* buffer() const { return buf_; }
    const int getErr() const { return err_;}

    static const int BufSize = 128 * 1024;
private:
    int fd_;
    int err_;
    char buf_[BufSize];
};

template<typename String>
int readFile(StringArg filename,
             int maxSize,
             String* content,
             int64_t* fileSize = NULL,
             int64_t* modifyTime = NULL,
             int64_t* createTime = NULL)
{
    ReadSmallFile file(filename);
    return file.readToString(maxSize, content, fileSize, modifyTime, createTime);
}

#endif  // _UTILS_FILE_H
