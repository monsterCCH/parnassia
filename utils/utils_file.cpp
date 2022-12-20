#include "utils_file.h"
#include <csignal>
#include <unistd.h>

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
