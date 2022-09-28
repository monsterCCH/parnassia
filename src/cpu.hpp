#ifndef PARNASSIA_TRINERVIS_CPU_HPP
#define PARNASSIA_TRINERVIS_CPU_HPP
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

namespace CPU {

enum CPUStates
{
    S_USER = 0,
    S_NICE,
    S_SYSTEM,
    S_IDLE,
    S_IOWAIT,
    S_IRQ,
    S_SOFTIRQ,
    S_STEAL,
    S_GUEST,
    S_GUEST_NICE,
    NUM_CPU_STATES
};

typedef struct CPUData
{
    std::string cpu;
    size_t times[NUM_CPU_STATES];
} CPUData;

void ReadStatsCPU(CPUData& entry);
size_t GetIdleTime(const CPUData & e);
size_t GetActiveTime(const CPUData & e);
std::string getCpuUsage();

void ReadStatsCPU(CPUData& entry)
{
    std::ifstream fileStat("/proc/stat");

    std::string line;

    const std::string STR_CPU("cpu");
    const std::size_t LEN_STR_CPU = STR_CPU.size();

    while(std::getline(fileStat, line))
    {
        if(!line.compare(0, LEN_STR_CPU, STR_CPU))
        {
            std::istringstream ss(line);
            ss >> entry.cpu;
            for(unsigned long & time : entry.times)
                ss >> time;
            break;
        }
    }
}

size_t GetIdleTime(const CPUData & e)
{
    return	e.times[S_IDLE] +
           e.times[S_IOWAIT];
}

size_t GetActiveTime(const CPUData & e)
{
    return	e.times[S_USER] +
           e.times[S_NICE] +
           e.times[S_SYSTEM] +
           e.times[S_IRQ] +
           e.times[S_SOFTIRQ] +
           e.times[S_STEAL] +
           e.times[S_GUEST] +
           e.times[S_GUEST_NICE];
}

std::string getCpuUsage()
{
    CPU::CPUData e1;
    CPU::CPUData e2;

    ReadStatsCPU(e1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ReadStatsCPU(e2);

    const float ACTIVE_TIME	= static_cast<float>(GetActiveTime(e2) - GetActiveTime(e1));
    const float IDLE_TIME	= static_cast<float>(GetIdleTime(e2) - GetIdleTime(e1));
    const float TOTAL_TIME	= ACTIVE_TIME + IDLE_TIME;
    std::stringstream ss;
    ss << (100.f * ACTIVE_TIME / TOTAL_TIME);
    return ss.str();
}
}


#endif   // PARNASSIA_TRINERVIS_CPU_HPP
