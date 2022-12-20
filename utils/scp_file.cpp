#include "scp_file.h"
#include <libssh2.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ctime>
#include <arpa/inet.h>
#include "libssh2_sftp.h"

#include <utility>
#include "logger.h"
#include "utils_file.h"

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
funcRes ScpFile::getDirFiles(LIBSSH2_SFTP* sftp_session, const std::string& src_path, std::vector<std::string>& files, std::vector<std::string>& deepest_dir)
{
    LIBSSH2_SFTP_HANDLE *sftp_handle = nullptr;

    sftp_handle = libssh2_sftp_opendir(sftp_session, src_path.c_str());
    if (!sftp_handle) {
        LOG->warn("Unable to open dir with SFTP");
        return {false, std::string("Unable to open dir with SFTP")};
    }

    std::vector<std::string> dirs;
    int rc;
    do {
        char mem[512];
        char longentry[512];
        LIBSSH2_SFTP_ATTRIBUTES attrs;

        rc = libssh2_sftp_readdir_ex(sftp_handle, mem, sizeof(mem),
                                     longentry, sizeof(longentry), &attrs);
        if (rc > 0) {
            if ((attrs.permissions & LIBSSH2_SFTP_S_IFMT) != LIBSSH2_SFTP_S_IFDIR) {
                std::string file = src_path + "/" + mem;
                files.emplace_back(file);
            } else {
                std::string dir_name = mem;
                if (dir_name == "." || dir_name == "..") continue ;
                std::string dir = src_path + "/" + mem;
                deepest_dir.emplace_back(dir);
                dirs.emplace_back(dir);
            }

        }else
            break;

    }while(1);

    libssh2_sftp_closedir(sftp_handle);


    for(const auto& iter : dirs) {
        funcRes ret = getDirFiles(sftp_session, iter, files, deepest_dir);
        if (!ret.result) {
            LOG->warn("get subdirectory files error");
            return {false, std::string("get subdirectory files error")};
        }
    }
    return {true, {}};
}

funcRes ScpFile::getAllFiles(const std::string& src_path, std::vector<std::string>& files, std::vector<std::string>& deepest_dir)
{
    LIBSSH2_SFTP *sftp_session = nullptr;
    LIBSSH2_SFTP_HANDLE *sftp_handle = nullptr;
    sftp_session = libssh2_sftp_init(m_si.session);
    if (!sftp_session) {
        LOG->warn("Unable to init SFTP session");
        return {false, std::string("Unable to init SFTP session")};
    }

    libssh2_session_set_blocking(m_si.session, 1);

    funcRes ret = getDirFiles(sftp_session, src_path, files, deepest_dir);

    libssh2_sftp_shutdown(sftp_session);

    return ret;
}

funcRes ScpFile::transFile(const std::string& src_path, const std::string& dst_path)
{
    try {
        if (!getIsLocal()) {
            return {false, std::string("don't support remote to remote")};
        }
        std::vector<std::string> files, dirs;
        funcRes ret = getAllFiles(src_path, files, dirs);
        if (!ret.result) {
            return ret;
        }

        LOG->info("start deliver directory {}", src_path);
        ret = scpFiles(files, src_path, dst_path, dirs);
        if (!ret.result) {
            return ret;
        }
        LOG->info("end deliver directory {}", src_path);
        return ret;
    }
    catch (std::exception& e) {
        LOG->warn("deliver directory fail, src:{} dst:{}, {}",src_path, dst_path, e.what());
        return {false, e.what()};
    }
}

funcRes ScpFile::scpFiles(std::vector<std::string>& files, const std::string& src_path, const std::string& dst_path, std::vector<std::string>& dirs)
{
    for (auto &it : dirs) {
        it = it.replace(it.find(src_path), src_path.length(), dst_path + "/");
    }
    if (getIsLocal()) {
        for (auto &it : dirs) {
            if (!create_dir(it.c_str())) {
                LOG->warn("can not mkdir in local");
                return {false, std::string("can not mkdir in local")};
            }
        }
    } else {
        for (auto &it : dirs) {
            std::string command = "mkdir -p " + it;
            std::string result;
            if (FUNC_RET_OK != execute(&m_di, command, result)) {
                LOG->warn("remote dir {} create fail", dst_path);
                return {false, std::string("can not mkdir in remote host")};
            }
        }
    }
    LIBSSH2_SFTP *sftp_session;
    sftp_session = libssh2_sftp_init(m_si.session);
    if (!sftp_session) {
        LOG->warn("Unable to init SFTP session");
        return {false, std::string("Unable to init SFTP session")};
    }

    libssh2_session_set_blocking(m_si.session, 1);

    for (const auto& iter : files) {

        LIBSSH2_SFTP_HANDLE* sftp_handle = nullptr;

        int rc = 0;
        std::string dst_abs_file = iter;
        dst_abs_file = dst_abs_file.replace( iter.find(src_path), src_path.length(), dst_path + "/");

        sftp_handle = libssh2_sftp_open(sftp_session, iter.c_str(), LIBSSH2_FXF_READ, 0);
        if (!sftp_handle) {
            LOG->warn("Unable to open file with SFTP");
            return {false, std::string("Unable to open file with SFTP")};
        }

        int fd;
        if (getIsLocal()) {
            fd = open(
                dst_abs_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
            if (fd < 0) {
                LOG->warn("file open fail: {}", dst_abs_file);
                return {false, std::string("file open fail")};
            }
        }
        do {
            char mem[1024];

            /* loop until we fail */
            rc = libssh2_sftp_read(sftp_handle, mem, sizeof(mem));
            if (rc > 0) {
                write(fd, mem, rc);
            }
            else {
                break;
            }
        } while(1);

        if (getIsLocal()) {
            if (fd > 2) {
                close(fd);
            }
        }
        libssh2_sftp_close(sftp_handle);
    }

    libssh2_sftp_shutdown(sftp_session);

    return {true, {}};
}

funcRes ScpFile::mkDir(sshInfo* ssh_info, std::vector<std::string>& dirs)
{
    LIBSSH2_SFTP *sftp_session = nullptr;
    int rc;
    sftp_session = libssh2_sftp_init(ssh_info->session);

    if(!sftp_session) {
        LOG->warn("mkdir: Unable to init SFTP session");
        return {false, std::string("mkdir: Unable to init SFTP session")};
    }

    libssh2_session_set_blocking(ssh_info->session, 1);

    for (auto &iter : dirs) {
        rc = libssh2_sftp_mkdir(sftp_session, iter.c_str(),
                                LIBSSH2_SFTP_S_IRWXU|
                                    LIBSSH2_SFTP_S_IRGRP|LIBSSH2_SFTP_S_IXGRP|
                                    LIBSSH2_SFTP_S_IROTH|LIBSSH2_SFTP_S_IXOTH);

        if(rc) {
            libssh2_sftp_shutdown(sftp_session);
            LOG->warn("libssh2_sftp_mkdir failed");
            return {false, std::string("libssh2_sftp_mkdir failed")};
        }
    }
    libssh2_sftp_shutdown(sftp_session);

    return {true, std::string("")};
}

std::vector<int> ScpFile::transFile(const std::vector<std::string>& src_file, const std::string& src_path, const std::string& dst_path)
{
    std::vector<int> res;

    // create remote path dir
    std::string command = "mkdir -p " + dst_path;
    std::string result;
    if (!getIsLocal() && FUNC_RET_OK != execute(&m_di, command, result)) {
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
                res.push_back(FAIL);
                continue ;
            }
        }
        LOG->info("start deliver {}", src_abs_file);

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
        LOG->info("end deliver {}", dst_abs_file);
    }

    return res;
}

FUNCTION_RETURN ScpFile::execute(sshInfo* ssh_info, const std::string& command,
                                 std::string& result)
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
    std::stringstream ss;
    for(;;) {
        /* loop until we block */
        int rc;
        do {
            char buffer[0x4000];
            rc = libssh2_channel_read(channel, buffer, sizeof(buffer) );
            if(rc > 0) {
                int i;
                for(i = 0; i < rc; ++i)
                    ss << buffer[i];
            }
            else {
                if(rc != LIBSSH2_ERROR_EAGAIN)
                    LOG->info("ScpFile::execute: libssh2_channel_read returned {0:d}", rc);
            }
        }
        while(rc > 0);

        /* this is due to blocking that would occur otherwise so we loop on
           this condition */
        if(rc == LIBSSH2_ERROR_EAGAIN) {
            waitSocket(ssh_info->sock, ssh_info->session);
        }
        else
            break;
    }
    result = ss.str();

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
