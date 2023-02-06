#ifndef TRINERVIS_INI_PARSER_H
#define TRINERVIS_INI_PARSER_H
#include <unordered_map>
#include <string>
#include "base.h"
#include "iniparser.h"
#include "dictionary.h"

using INI_MAP = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

class IniParser : noncopyable
{
public:
    explicit IniParser(const std::string& ini_file);
    virtual ~IniParser();

    int ini_dump();
    INI_MAP* get_ini_map() { return &ini_map; }
    static int err_callback(const char* format, ...);
    void reload_ini();

private:
    int parser_to_map(dictionary* dict, INI_MAP& ini_map);
    INI_MAP ini_map;
    dictionary *dict_ptr;
    std::string ini_file;

};

#endif   // TRINERVIS_INI_PARSER_H
