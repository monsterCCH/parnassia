#include <gtest/gtest.h>
#include "scp_file.h"

class ScpTest : public ::testing::Test {
protected:
    void SetUp() override {
        si.hostIp = "192.168.232.168";
        si.passwd = "1996xy21";
        si.user = "root";
        di.hostIp = "172.21.48.212";
        di.passwd = "1996xy21";
        di.user = "root";
        src_path = "/root/scp";
        dst_path = "/root/scp";
    }

    void TearDown() override {

    }

    SCP::ScpFile::hostInfo si;
    SCP::ScpFile::hostInfo di;
    std::string src_path;
    std::string dst_path;
};

TEST_F(ScpTest, scp_local) {
    SCP::ScpFile scp {si};
    if (!scp.getInitRet()) {
        std::cout << "init error" << std::endl;
    }
    std::vector<std::string> src_file;
    src_file.emplace_back("axeagent_ins.tar.gz");
//    src_file.emplace_back("wutong_ins.tar.gz");
    std::vector<int> res;
    res = scp.transFile(src_file, src_path, dst_path);
    auto file_name = src_file.begin();
    for (auto iter : res) {
        std::cout << *file_name << " " << (int)iter << std::endl;
        file_name++;
    }

}

TEST_F(ScpTest, scp_remote) {
    SCP::ScpFile scp {si,di};
    if (!scp.getInitRet()) {
        std::cout << "init error" << std::endl;
    }
    std::vector<std::string> src_file;
//    src_file.emplace_back("aaa");
    src_file.emplace_back("axeagent_ins.tar.gz");
    src_file.emplace_back("wutong_ins.tar.gz");
    std::vector<int> res;
    res = scp.transFile(src_file, src_path, dst_path);
    auto file_name = src_file.begin();
    for (auto iter : res) {
        std::cout << *file_name << " " << (int)iter << std::endl;
        file_name++;
    }

}
