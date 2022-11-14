#include "scp_file.h"
#include <libssh2.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ctime>
#include <arpa/inet.h>

#include <utility>
#include "logger.h"

using namespace SCP;

ScpFile::ScpFile(const hostInfo& src, const hostInfo& dst)
        : m_is_fin(false), m_is_local(false), m_init_ok(false)
{
    FUNCTION_RETURN s_ret = init(src, &m_si);
    FUNCTION_RETURN d_ret = init(dst, &m_di);
    if (s_ret != FUNC_RET_OK || d_ret != FUNC_RET_OK) {
        release(&m_si);
        release(&m_di);
        LOG->warn("ssh init fail");
    } else {
        m_init_ok = true;
    }

}

ScpFile::ScpFile(const hostInfo& src)
        : m_is_fin(false),  m_is_local(true), m_init_ok(false)
{
    FUNCTION_RETURN ret = init(src, &m_si);
    if (ret != FUNC_RET_OK) {
        release(&m_si);
        LOG->warn("ssh init fail");
    } else {
        m_init_ok = true;
    }

}

ScpFile::~ScpFile()
{
    release(&m_si);
    release(&m_di);
    libssh2_exit();
}

FUNCTION_RETURN ScpFile::init(const hostInfo& src, sshInfo* ssh_info)
{
    unsigned long host_addr;
    host_addr = inet_addr(src.hostIp.c_str());
    int i;
    struct sockaddr_in sin;
    const char *fingerprint;
    int rc;

    rc = libssh2_init(0);
    if (rc) {
        LOG->warn("libssh2 initialization failed {0:d}", rc);
        return FUNC_RET_ERROR;
    }

    ssh_info->sock = socket(AF_INET, SOCK_STREAM, 0);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(src.port);
    sin.sin_addr.s_addr = host_addr;
    if(connect(ssh_info->sock, (struct sockaddr*)(&sin),
                sizeof(struct sockaddr_in)) != 0) {
        LOG->warn("failed to connect: {}", src.hostIp);
        return FUNC_RET_ERROR;
    }

    ssh_info->session = libssh2_session_init();
    if (!ssh_info->session) {
        LOG->warn("session init fail: {}", src.hostIp);
        return FUNC_RET_ERROR;
    }

    rc = libssh2_session_handshake(ssh_info->session, ssh_info->sock);
    if (rc) {
        LOG->warn("Failure establishing SSH session: {} {0:d}", src.hostIp, rc);
        return FUNC_RET_ERROR;
    }

//    fingerprint = libssh2_hostkey_hash(ssh_info->session, LIBSSH2_HOSTKEY_HASH_SHA1);
    if(libssh2_userauth_password(ssh_info->session, src.user.c_str(), src.passwd.c_str())) {
        LOG->warn("Authentication by password failed: {}", src.hostIp);
        return FUNC_RET_ERROR;;
    }
    // libssh2 debug info print
    //libssh2_trace(ssh_info->session, LIBSSH2_TRACE_CONN|LIBSSH2_TRACE_SCP|LIBSSH2_TRACE_ERROR|LIBSSH2_TRACE_SOCKET);

    return FUNC_RET_OK;
}

void ScpFile::release(sshInfo* ssh_info)
{
    if (ssh_info->session != nullptr) {
        libssh2_session_disconnect(ssh_info->session,"");
        libssh2_session_free(ssh_info->session);
        ssh_info->session = nullptr;
        close(ssh_info->sock);
    }
}


std::vector<int> ScpFile::transFile(const std::vector<std::string>& src_file, const std::string& src_path, const std::string& dst_path)
{
    std::vector<int> res;

    // create remote path dir
    std::string command = "mkdir -p " + dst_path;
    if (!getIsLocal() && FUNC_RET_OK != execute(&m_di, command)) {
        LOG->warn("remote dir {} create fail", dst_path);
        return res;
    }

    for (const auto& iter : src_file) {
        int rc = 0;
        int sd = 0;
        libssh2_struct_stat_size got = 0;
        LIBSSH2_CHANNEL* channel_recv = nullptr;
        LIBSSH2_CHANNEL* channel_send = nullptr;
        libssh2_struct_stat fileinfo;
        memset(&fileinfo, 0, sizeof(libssh2_struct_stat));
        std::string src_abs_file;
        std::string dst_abs_file;
        src_abs_file.append(src_path).append("/").append(iter);
        dst_abs_file.append(dst_path).append("/").append(iter);

        channel_recv = libssh2_scp_recv2(m_si.session, src_abs_file.c_str(), &fileinfo);
        if (!channel_recv) {
            LOG->warn("Unable to open a session: {0:d}", libssh2_session_last_errno(m_si.session));
//            release(&m_si);
            res.push_back(FAIL);
            continue ;
        }
        int fd;
        if (getIsLocal()) {
            std::string cmd = "mkdir -p " + dst_path;
            system(cmd.c_str());
            fd = open(dst_abs_file.c_str(),
                          O_WRONLY | O_CREAT | O_TRUNC,
                          S_IRWXU);
            if (fd < 0) {
                LOG->warn("file open fail: {}", dst_abs_file);
                res.push_back(FAIL);
                continue;
            }
        } else {
            channel_send = libssh2_scp_send(m_di.session, dst_abs_file.c_str(), fileinfo.st_mode & 0777,
                                            (unsigned long)fileinfo.st_size);
            if (!channel_send) {
                LOG->warn("Unable to open a session: {0:d}", libssh2_session_last_errno(m_di.session));
//                release(&m_di);
                res.push_back(FAIL);
                continue ;
            }
        }

        while (got < fileinfo.st_size) {
            char mem[1024];
            int amount = sizeof(mem);

            if((fileinfo.st_size -got) < amount) {
                amount = (int)(fileinfo.st_size -got);
            }

            rc = libssh2_channel_read(channel_recv, mem, amount);
            if(rc > 0) {
                if (getIsLocal()) {
                    size_t ret = write(fd, mem, rc);
                    if (ret != rc) {
                        LOG->warn("local write failed: fd:{0:d}, ret{0:d}", fd, ret);
                        res.push_back(FAIL);
                        break;
                    }
                } else {
                    int write = rc;
                    bool fin = true;
                    char *ptr = mem;
                    do {
                        sd = libssh2_channel_write(channel_send, ptr, write);
                        if (sd < 0) {
                            fin = false;
                            break;
                        } else {
                            ptr += sd;
                            write -= sd;
                        }
                    } while (write);
                    if (!fin) {
                        LOG->warn("libssh2_channel_write() failed:", dst_abs_file);
                        res.push_back(FAIL);
                        break;
                    }
                }
            }
            else if(rc < 0) {
                LOG->warn("libssh2_channel_read() failed: {}", src_abs_file);
                res.push_back(FAIL);
                break;
            }
            got += rc;
        }
        if (got == fileinfo.st_size) {
            res.push_back(SUCCESS);
        }
        if (getIsLocal()) {
            close(fd);
        }
        if (channel_send) {
            libssh2_channel_send_eof(channel_send);
            libssh2_channel_wait_eof(channel_send);
            libssh2_channel_wait_closed(channel_send);
            libssh2_channel_free(channel_send);
            channel_send = nullptr;
        }

        libssh2_channel_free(channel_recv);
        channel_recv = nullptr;
    }

    return res;
}

FUNCTION_RETURN ScpFile::execute(sshInfo* ssh_info, const std::string& command)
{
    LIBSSH2_CHANNEL *channel;
    int rc;
    int record = libssh2_session_get_blocking(ssh_info->session);
    libssh2_session_set_blocking(ssh_info->session, 0);
    while ((channel = libssh2_channel_open_session(ssh_info->session)) == nullptr &&
           libssh2_session_last_error(ssh_info->session, NULL, NULL, 0) == LIBSSH2_ERROR_EAGAIN) {
        waitSocket(ssh_info->sock, ssh_info->session);
    }

    if (channel == nullptr) {
        LOG->warn("ssh execute init fail");
        return FUNC_RET_ERROR;
    }

    while((rc = libssh2_channel_exec(channel, command.c_str())) == LIBSSH2_ERROR_EAGAIN) {
        waitSocket(ssh_info->sock, ssh_info->session);
    }

    if (rc != 0) {
        LOG->warn("ssh execute fail");
        return FUNC_RET_ERROR;
    }

    while((rc = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN)
        waitSocket(ssh_info->sock, ssh_info->session);

    libssh2_channel_free(channel);
    channel = nullptr;

    libssh2_session_set_blocking(ssh_info->session, record);
    return FUNC_RET_OK;
}

int ScpFile::waitSocket(int socket_fd, LIBSSH2_SESSION *session)
{
    struct timeval timeout;
    int rc;
    fd_set fd;
    fd_set *writefd = nullptr;
    fd_set *readfd = nullptr;
    int dir;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    FD_ZERO(&fd);
    FD_SET(socket_fd, &fd);

    dir = libssh2_session_block_directions(session);
    if(dir & LIBSSH2_SESSION_BLOCK_INBOUND)
        readfd = &fd;

    if(dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
        writefd = &fd;

    rc = select(socket_fd + 1, readfd, writefd, NULL, &timeout);

    return rc;
}
