#pragma once

#include <cstdint>

void InitializeLAPICTimer();
void StartLAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();

class Timer
{
public:
    Timer(unsigned long timout, int value);
    unsigned long Timeout() const { return timeout_; }
    int Value() const { return value_; }

private:
    unsigned long timeout_;
    int value_;
};

class TimerManager
{
public:
    void Tick();
    unsigned long CurrentTick() const { return tick_; }

private:
    volatile unsigned long tick_{0};
};
void LAPICTimerOnInterrupt();

extern TimerManager *timer_manager;