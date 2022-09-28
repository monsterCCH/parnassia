#include "utils_file.h"

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
