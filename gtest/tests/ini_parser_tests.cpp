#include <gtest/gtest.h>
#include "iniparser.h"
#include "logger.h"

#define ASCIILINESZ         (1024)
#define INI_INVALID_KEY     ((char*)-1)

class IniParserTest : public ::testing::Test
{
protected:
    using INI_MAP = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
    void SetUp() override {
        config_file = "/etc/config_manager.ini";
        iniparser_set_error_callback(err_callback);
        dict_ptr = iniparser_load(config_file.c_str());


    }
    void TearDown() override {

    }
    int parser_to_map(dictionary* dict, INI_MAP& ini_map);
    static int err_callback(const char* format, ...);

    dictionary *dict_ptr;
    std::string config_file;
    INI_MAP ini_map;

};

int IniParserTest::parser_to_map(dictionary* dict, INI_MAP& ini_map)
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

int IniParserTest::err_callback(const char* format, ...)
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

TEST_F(IniParserTest, parser)
{
    if (dict_ptr == nullptr) {
        std::cout << "parser error" << std::endl;
    }
}

TEST_F(IniParserTest, parser_to_map)
{
    parser_to_map(dict_ptr, ini_map);
    std::cout << "finish" << std::endl;
}
