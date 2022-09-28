#include <string>
#include <iostream>
#include <csignal>
#include <sys/stat.h>
#include <unistd.h>
#include "logger.h"
#include "sig_func.hpp"
#include "config.h"
#include "host_info.h"
#include "redis_cl_manager.h"
#include "redis_publish.h"

const std::string PROC_NAME = "parnassia";
static std::string pid_file = "/var/run/" + PROC_NAME + ".pid";
volatile int g_program_run = 1;
static int isRunning(pid_t& pid);
static void sigExit(int signo);
void redisInit();

enum class run_model{START = 0, STOP, DEBUG, MODEL_END };

void Usage()
{
    std::cout << "Usage:" << std::endl;
    std::cout << "    " << PROC_NAME << " -r [start|stop|debug] -p [pid_file]" << std::endl;
    std::cout << "Description:" << std::endl;
    std::cout << "    " << "-r start|stop|debug, program run modal" << std::endl;
    std::cout << "    " << "-p set the pid file,if not set -p, the default is " << pid_file << std::endl;
}

int main(int argc, char* argv[])
{
    pid_t opid;
    if (argc == 1) {
        Usage();
        exit(EXIT_FAILURE);
    }

    run_model model = run_model::MODEL_END;
    int opt;
    while (EOF != (opt = getopt(argc, argv, ":r:p:"))) {
        switch (opt) {
        case 'r':
            if (strcmp(optarg, "start") == 0) {
                model = run_model::START;
                break ;
            }
            if (strcmp(optarg, "stop") == 0) {
                model = run_model::STOP;
                break ;
            }
            if (strcmp(optarg, "debug") == 0) {
                model = run_model::DEBUG;
                break ;
            }
            Usage();
            exit(EXIT_FAILURE);
        case 'p':
            pid_file = optarg;
            break ;
        case '?':
            Usage();
            exit(EXIT_FAILURE);
        default:
            break ;
        }
    }

    if (model == run_model::STOP) {
        int i = 0;
        int ret = isRunning(opid);
        if (ret) {
            kill(opid, SIGTERM);
            do {
                sleep(2);
                ret = isRunning(opid);
                i++;
            } while (ret && i < 10);  //进程还存在并且最多检查10次

            if (ret) {
                kill(opid, SIGKILL); //防止没有关闭，强制关闭
            }
        } else {
            std::cout << "No running program" <<std::endl;
            exit(EXIT_SUCCESS);
        }
        std::cout << "Stop " << PROC_NAME << ", pid = " << opid << std::endl;
        exit(EXIT_SUCCESS);
    }

    if (model == run_model::MODEL_END) {
        Usage();
        exit(EXIT_FAILURE);
    }

    if (model == run_model::START) {
        pid_t pid, sid;
        pid = fork();
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }
        else if (pid < 0) {
            exit(EXIT_FAILURE);
        }
        sid = setsid();
        if (sid < 0) {
            LOG->error("Could not generate session ID for child process");
            exit(EXIT_FAILURE);
        }
    }

    if (isRunning(opid)) {
        exit(EXIT_FAILURE);
    }

    if (signal(SIGTERM, sigExit) == SIG_ERR) {
        LOG->error("Register signal exit func fail");
    }

    umask(0);

    LOG->info("Successfully started parnassia");

    if ((chdir("/")) < 0) {
        LOG->error("Could not change working directory to /");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    signal(SIGPIPE, SIG_IGN);

    try {
        for (const auto& iter : CONFIG::config::instance().get_redisCluster()){
            std::string redis_server = iter.redis_server;
        }
    } catch (exception& e) {
        LOG->error("{}", e.what());
    }

    hostInfo hi;
    std::shared_ptr<redisClManager> redis_ptr= std::make_shared<redisClManager>(CONFIG::config::instance().get_redisCluster());
//    redisInit();
    CRedisPublisher publisher;

    bool ret = publisher.init();
    if (!ret)
    {
        printf("Init failed.\n");
        return 0;
    }

    ret = publisher.connect();
    if (!ret)
    {
        printf("connect failed.");
        return 0;
    }

    while (g_program_run) {
        hi.flush();
        string res = hi.genHwInfoJson();
        stringstream ss;
        ss << "PUBLISH hw_info " << res ;
        LOG->info("{}", ss.str());
//        redis_ptr->redisClCommand("", res);
        publisher.publish("hw_info", res);
        sleep(30);
        LOG->info("heart");
    }
    return 0;
}

int isRunning(pid_t& pid)
{
    char buf[16] = {0};

    int fd = open(pid_file.c_str(), O_CREAT | O_RDWR, S_IRWXU);
    if(fd < 0) {
        LOG->error("open file {} failed", pid_file);
        sleep(3);
        return EXIT_FAILURE;
    }

    struct flock fl{};
    fl.l_type = F_WRLCK;
    fl.l_start = 0;
    fl.l_whence = SEEK_SET;
    fl.l_len = 0;
    fl.l_pid = getpid();

    if(-1 == fcntl(fd, F_SETLK, &fl)) {
        if(fcntl(fd, F_GETLK, &fl) == 0) {
            printf("%s has been run, pid = %d\n", PROC_NAME.c_str(), fl.l_pid);
        }
        pid = fl.l_pid;
        close(fd);
        return EXIT_FAILURE;
    } else {
        ftruncate(fd, 0);
        snprintf(buf, sizeof(buf), "%d", fl.l_pid);
        write(fd, buf, strlen(buf) + 1);
        return EXIT_SUCCESS;
    }
}

void sigExit(int signo)
{
    g_program_run = 0;
    LOG->warn("Caught terminate signal {0:d}, the program will exit", signo);
}

void redisInit()
{
    redisClManager instance{CONFIG::config::instance().get_redisCluster()};
}
