#include <gtest/gtest.h>
#include "iniparser.h"
#include "logger.h"
#include "config_manager.h"

class ConfigManagerTest : public ::testing::Test
{
protected:
    using INI_MAP = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
    void SetUp() override {
        config_file = "/etc/config_manager.ini";

    }
    void TearDown() override {

    }


    std::string config_file;

};

TEST_F(ConfigManagerTest, check)
{
    CONFIG_MANAGER::ConfigManager cm{config_file};
    cm.check_config(cm.get_ini_map());
    std::cout << "finish" << std::endl;
    
}

TEST_F(ConfigManagerTest, check_module)
{
    CONFIG_MANAGER::ConfigManager cm{config_file};
    cm.check_config(cm.get_ini_map(), "redis");
    std::cout << "finish" << std::endl;

}

TEST_F(ConfigManagerTest, task)
{
    CONFIG_MANAGER::ConfigManager cm{config_file};
    cm.run();
    std::cout << "finish" << std::endl;
}

TEST_F(ConfigManagerTest, set_config)
{
    ::INI_MAP new_ini;
    new_ini["openvas"].insert(std::make_pair("config", "/opt/openvas.conf"));
    new_ini["openvas"].insert(std::make_pair("start", "ddsad"));

    CONFIG_MANAGER::ConfigManager cm{config_file};
    cm.set_config(&new_ini);
    std::cout << "finish" << std::endl;

}

TEST_F(ConfigManagerTest, del_config)
{
    std::vector<std::string> sec{"openvas", "nginx"};
    CONFIG_MANAGER::ConfigManager cm{config_file};
    cm.del_section(sec);
    std::cout << "finish" << std::endl;
}
