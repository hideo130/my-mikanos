#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "error.hpp"
#include "timer.hpp"
#include "fat.hpp"
#include "file.hpp"

struct TaskContext
{
    uint64_t cr3, rip, rflags, reserved1;            // offset 0x00
    uint64_t cs, ss, fs, gs;                         // offset 0x20
    uint64_t rax, rbx, rcx, rdx, rdi, rsi, rsp, rbp; // offset 0x40
    // 8 registers
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15; // offset 0x80
    std::array<uint8_t, 512> fxsave_area;          // offset 0xc0
} __attribute__((packed));

// extern TaskContext task_b_ctx, task_a_ctx;

using TaskFunc = void(uint64_t, int64_t);

class TaskManager;

class Task
{
public:
    static const int kDefaultLevel = 1;
    static const size_t kDefaultStackBytes = 4096;

    Task(uint64_t id);
    Task &InitContext(TaskFunc *f, int64_t data);
    TaskContext &Context();
    uint64_t ID() const;
    Task &Sleep();
    Task &Wakeup();
    Task &Wakeup(int level);
    void SendMessage(const Message &msg);
    std::optional<Message> ReceiveMessage();

    int Level() const { return level_; }
    bool Running() const { return running_; }

    uint64_t &OSStackPointer();
    std::vector<std::unique_ptr<::FileDescriptor>> &Files();
    uint64_t DPagingBegin() const;
    void SetDPagingBegin(uint64_t v);
    uint64_t DPagingEnd() const;
    void SetDPagingEnd(uint64_t v);

private:
    uint64_t id_;
    std::vector<uint64_t> stack_;
    alignas(16) TaskContext context_;
    // this value is assigned in CallApp
    uint64_t os_stack_ptr_;
    std::deque<Message> msgs_;
    unsigned int level_{kDefaultLevel};
    bool running_{false};
    std::vector<std::unique_ptr<::FileDescriptor>> files_{};
    uint64_t dpaging_begin_{0}, dpaging_end_{0};
    Task &SetLevel(int level)
    {
        level_ = level;
        return *this;
    }
    Task &SetRunning(bool running)
    {
        running_ = running;
        return *this;
    }

    friend TaskManager;
};

class TaskManager
{
public:
    // level: 0 = lowest, kMaxLevel = highest
    static const int kMaxLevel = 3;

    TaskManager();
    Task &NewTask();
    void SwitchTask(const TaskContext &current_ctx);

    void Sleep(Task *task);
    Error Sleep(uint64_t id);
    // specified -1 then task wakeup at the same level before sleeping
    void Wakeup(Task *task, int level = -1);
    Error Wakeup(uint64_t id, int level = -1);

    Task &CurrentTask();
    Error SendMessage(uint64_t id, const Message &msg);
    void ChangeLevelRunning(Task *task, int level);
    Task *RotateCurrentRunQueue(bool current_sleep);

private:
    std::vector<std::unique_ptr<Task>> tasks_{};
    uint64_t latest_id_{0};
    std::array<std::deque<Task *>, kMaxLevel + 1> running_{};
    int current_level_{kMaxLevel};
    bool level_changed_{false};
};

extern TaskManager *task_manager;

void InitializeTask();
