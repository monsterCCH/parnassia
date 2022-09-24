#include <string>
#include <iostream>
#include <csignal>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
#include "logger.hpp"

const std::string PROC_NAME = "parnassia";
static std::string pid_file = "/var/run/" + PROC_NAME + ".pid";
volatile int g_program_run = 1;
static int isRunning();

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

    if (isRunning()) {
        exit(EXIT_FAILURE);
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

    while (g_program_run) {
        sleep(1);
        LOG->info("heart");
    }
    return 0;
}

int isRunning()
{
    char buf[16] = {0};

    int fd = open(pid_file.c_str(), O_CREAT | O_RDWR, S_IRWXU);
    if(fd < 0) {
        LOG->error("open file %s failed", pid_file);
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
        close(fd);
        return EXIT_FAILURE;
    } else {
        ftruncate(fd, 0);
        snprintf(buf, sizeof(buf), "%d", fl.l_pid);
        write(fd, buf, strlen(buf) + 1);
        return EXIT_SUCCESS;
    }
}
