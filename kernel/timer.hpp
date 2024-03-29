#pragma once

#include <cstdint>
#include <queue>

#include "message.hpp"

void InitializeLAPICTimer();
void StartLAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();

class Timer
{
public:
    Timer(unsigned long timout, int value, uint64_t taks_id);
    unsigned long Timeout() const { return timeout_; }
    int Value() const { return value_; }
    uint64_t TaskID() const { return task_id_; }

private:
    unsigned long timeout_;
    int value_;
    uint64_t task_id_;
};

/**  compare timer priority. The father the timeout,  the lower priority */
inline bool operator<(const Timer &lhs, const Timer &rhs)
{
    return lhs.Timeout() > rhs.Timeout();
}

class TimerManager
{
public:
    TimerManager();
    void AddTimer(const Timer &timer);
    bool Tick();
    unsigned long CurrentTick() const { return tick_; }

private:
    volatile unsigned long tick_{0};
    std::priority_queue<Timer> timers_{};
};
void LAPICTimerOnInterrupt();

extern TimerManager *timer_manager;
extern unsigned long lapic_timer_freq;
const int kTimerFreq = 100;
const int kTaskTimerPeriod = static_cast<int>(kTimerFreq * 0.02);
const int kTaskTimerValue = std::numeric_limits<int>::max();
