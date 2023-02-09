#include "utils_file.h"
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>

using namespace std;

string get_file_contents(const char *filename, size_t max_size) {
    string contents;
    ifstream in(filename, std::ios::binary);
    if (in) {
        size_t index = (size_t)in.seekg(0, ios::end).tellg();
        size_t limited_size = min(index, max_size);
        contents.resize(limited_size);
        in.seekg(0, ios::beg);
        in.read(&contents[0], limited_size);
        in.close();
    } else {
        throw runtime_error(std::strerror(errno));
    }
    return contents;
}

bool dir_exists(const std::string& dir) {
    struct stat info;
    if (stat(dir.c_str(), &info) == 0 && info.st_mode & S_IFDIR) {
        return true;
    }
    return false;
}

bool file_exists(const std::string& file) {
    struct stat info;
    return (stat (file.c_str(), &info) == 0);
}

bool create_dir(const char *dir) {
    char DirName[256];
    strcpy(DirName, dir);
    int i, len = strlen(DirName);
    if (DirName[len - 1] != '/') strcat(DirName, "/");

    len = strlen(DirName);

    for (i = 1; i < len; i++) {
        if (DirName[i] == '/') {
            DirName[i] = 0;
            if (access(DirName, NULL) != 0) {
                if (mkdir(DirName, 0755) == -1) {
                    return false;
                }
            }
            DirName[i] = '/';
        }
    }

    return true;
}

bool IsFile(const std::string &filePath)
{
    struct stat st;
    if (0 == stat(filePath.c_str(), &st)) {
        if (st.st_mode & S_IFDIR) {
            return false;   // dir
        }
        else if (st.st_mode & S_IFREG) {
            return true;   // file
        }
    }
    return false;
}

bool FastReadFile(const std::string &filePath, std::string &fileData, bool lock)
{
    static constexpr long bufSize = 4096;

    if (!IsFile(filePath)) {
        return false;
    }

    FILE* pFile;
    if ((pFile = fopen(filePath.c_str(), "r")) == NULL) {
        return false;
    }

    if (lock && flock(fileno(pFile), LOCK_SH | LOCK_NB) != 0) {
        fclose(pFile);
        return false;
    }

    // 计算文件大小
    fseek(pFile, 0, SEEK_SET);
    long begin = ftell(pFile);
    fseek(pFile, 0, SEEK_END);
    long end      = ftell(pFile);
    long fileSize = end - begin;
    fseek(pFile, 0, SEEK_SET);   // 重新指向文件头

    fileData.reserve(fileSize + 1);

    char readBuf[bufSize + 1];
    long readSize = 0;
    while (readSize < fileSize) {
        long minRead = std::min(fileSize - readSize, bufSize);
        long len     = fread(readBuf, 1, minRead, pFile);
        readSize += len;
        fileData.append(readBuf, len);
    }

    if (lock && flock(fileno(pFile), LOCK_UN) != 0) {
        fclose(pFile);
        return false;
    }

    fclose(pFile);
    return true;
}

bool WriteFile(const std::string &filePath, const std::string &fileData, bool lock)
{
    FILE *pFile;
    if ((pFile = fopen(filePath.c_str(), "w")) == NULL)
    {
        return false;
    }

    // 互斥锁/不阻塞
    if (lock && flock(fileno(pFile), LOCK_EX | LOCK_NB) != 0)
    {
        fclose(pFile);
        return false;
    }

    fwrite(fileData.c_str(), 1, fileData.length(), pFile);

    // 解锁
    if (lock && flock(fileno(pFile), LOCK_UN) != 0)
    {
        fclose(pFile);
        return false;
    }

    fclose(pFile);
    return true;
}

ReadSmallFile::ReadSmallFile(StringArg filename)
    : fd_(::open(filename.c_str(), O_RDONLY | O_CLOEXEC)),
      err_(0)
{
    buf_[0] = '\0';
    if (fd_ < 0)
    {
        err_ = errno;
    }
}

ReadSmallFile::~ReadSmallFile()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
    }
}

//template<typename String>
int ReadSmallFile::readToString(int maxSize,
                                          std::string* content,
                                          int64_t* fileSize,
                                          int64_t* modifyTime,
                                          int64_t* createTime)
{
    static_assert(sizeof(off_t) == 8, "_FILE_OFFSET_BITS = 64");
    int err = err_;
    if (fd_ >= 0)
    {
        content->clear();

        if (fileSize)
        {
            struct stat statbuf;
            if (::fstat(fd_, &statbuf) == 0)
            {
                if (S_ISREG(statbuf.st_mode))
                {
                    *fileSize = statbuf.st_size;
                    content->reserve(static_cast<int>(std::min(static_cast<int64_t>(maxSize), *fileSize)));
                }
                else if (S_ISDIR(statbuf.st_mode))
                {
                    err = EISDIR;
                }
                if (modifyTime)
                {
                    *modifyTime = statbuf.st_mtime;
                }
                if (createTime)
                {
                    *createTime = statbuf.st_ctime;
                }
            }
            else
            {
                err = errno;
            }
        }

        while (content->size() < static_cast<size_t>(maxSize))
        {
            size_t toRead = std::min(static_cast<size_t>(maxSize) - content->size(), sizeof(buf_));
            ssize_t n = ::read(fd_, buf_, toRead);
            if (n > 0)
            {
                content->append(buf_, n);
            }
            else
            {
                if (n < 0)
                {
                    err = errno;
                }
                break;
            }
        }
    }
    return err;
}

int ReadSmallFile::readFileStat(int64_t* fileSize, int64_t* modifyTime, int64_t* createTime)
{
    static_assert(sizeof(off_t) == 8, "_FILE_OFFSET_BITS = 64");
    int err = err_;
    if (fd_ >= 0)
    {
        if (fileSize)
        {
            struct stat statbuf;
            if (::fstat(fd_, &statbuf) == 0)
            {
                if (S_ISREG(statbuf.st_mode))
                {
                    *fileSize = statbuf.st_size;
                }
                else if (S_ISDIR(statbuf.st_mode))
                {
                    err = EISDIR;
                }
                if (modifyTime)
                {
                    *modifyTime = statbuf.st_mtime;
                }
                if (createTime)
                {
                    *createTime = statbuf.st_ctime;
                }
            }
            else
            {
                err = errno;
            }
        }
    }
    return err;
}
