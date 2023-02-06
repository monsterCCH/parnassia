#include "ini_parser.h"
#include "logger.h"

#define ASCIILINESZ         (1024)
#define INI_INVALID_KEY     ((char*)-1)

IniParser::IniParser(const std::string& ini_file) : ini_file(ini_file)
{
    iniparser_set_error_callback(err_callback);
    dict_ptr = iniparser_load(ini_file.c_str());
    parser_to_map(dict_ptr, ini_map);
}

IniParser::~IniParser()
{
    if (dict_ptr != nullptr) {
        iniparser_freedict(dict_ptr);
    }
    INI_MAP empty;
    ini_map.swap(empty);
}

void IniParser::reload_ini()
{
    if (dict_ptr != nullptr) {
        iniparser_freedict(dict_ptr);
    }
    dict_ptr = iniparser_load(ini_file.c_str());
    parser_to_map(dict_ptr, ini_map);
}

int IniParser::ini_dump()
{
    FILE *fp;
    if ((fp=fopen(ini_file.c_str(), "w"))==NULL) {
        LOG->warn("iniparser: cannot open {}.", ini_file);
        return -1 ;
    }
    dictionary_del(dict_ptr);
    dict_ptr = dictionary_new(0);

    for (const auto& iter : ini_map) {
        std::string section = iter.first;
        iniparser_set(dict_ptr, section.c_str(), NULL);
        for (const auto& it : iter.second) {
            std::string key = section + ":" + it.first;
            std::string value = it.second;
            iniparser_set(dict_ptr, key.c_str(), value.c_str());
        }
    }
    iniparser_dump_ini(dict_ptr, fp);
    fclose(fp);
}

int IniParser::parser_to_map(dictionary* dict, INI_MAP& ini_map)
{
    INI_MAP tmp_map;
    if (nullptr == dict) {
        return -1;
    }
    int nsec;
    const char *sec;
    nsec = iniparser_getnsec(dict);
    if (nsec < 1) {
        LOG->warn("Config manager: ini have no section, Invalid.");
        return -1;
    }

    for (int i = 0; i < nsec; i++) {
        sec = iniparser_getsecname(dict, i);
        std::string section = sec;
        if (!iniparser_find_entry(dict, sec)) {
            LOG->warn("Config manager: can not find section.");
            return -1;
        }
        char keym[ASCIILINESZ+1];
        int seclen;
        seclen = strlen(sec);
        sprintf(keym, "%s:", sec);
        for (int j = 0; j < dict->size; j++) {
            if (dict->key[j] == NULL) continue ;
            if (!strncmp(dict->key[j], keym, seclen+1)) {
                std::string key = dict->key[j]+seclen+1;
                std::string value = dict->val[j] ? dict->val[j] : "";
                tmp_map[section].insert(std::make_pair(key.c_str(), value.c_str()));
            }
        }
    }
    ini_map.swap(tmp_map);

    return 0;
}

int IniParser::err_callback(const char* format, ...)
{
    int ret = 0;
    va_list ap;
    va_start(ap, format);
    char* buf = nullptr;
    auto len = vasprintf(&buf, format, ap);
    if (len == -1){
        return -1;
    }
    va_end(ap);
    std::string errstr(buf, len);
    LOG->warn(errstr);
    free(buf);
    return ret;
}
