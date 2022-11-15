#ifndef PARNASSIA_TRINERVIS_SCP_FILE_H
#define PARNASSIA_TRINERVIS_SCP_FILE_H
#include <string>
#include <vector>
#include <libssh2.h>
#include "nlohmann/json.hpp"
#include "base.h"

BEGIN_NAMESPACE(SCP)
#define SUCCESS 1
#define FAIL    0

class ScpFile
{
    typedef struct sshInfo{
        LIBSSH2_SESSION *session = nullptr;
        int sock;
    }sshInfo;
public:
    typedef struct hostInfo{
        std::string hostIp;
        std::string user;
        std::string passwd;
        int type;
        uint port = 22;
    }hostInfo;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(hostInfo, hostIp, user, passwd, type, port)



    ScpFile(const hostInfo& src, const hostInfo& dst);
    explicit ScpFile(const hostInfo& src);
    std::vector<int> transFile(const std::vector<std::string>& src_file, const std::string& src_path, const std::string& dst_path);

    [[nodiscard]] bool getInitRet() const { return m_init_ok; }
    [[nodiscard]] bool getIsLocal() const { return m_is_local; }

    ~ScpFile();
private:
    sshInfo m_si{};
    sshInfo m_di{};
    bool m_is_fin;
    bool m_init_ok;
    bool m_is_local;
    std::vector<std::string> m_src_file;
    std::string m_src_path;
    std::string m_dst_path;

    FUNCTION_RETURN init(const hostInfo& src, sshInfo *ssh_info);
    FUNCTION_RETURN execute(sshInfo *ssh_info, const std::string& command);
    void release(sshInfo *ssh_info);
    int waitSocket(int socket_fd, LIBSSH2_SESSION *session);
};
END_NAMESPACE

#endif   // PARNASSIA_TRINERVIS_SCP_FILE_H
