#include "asmfunc.h"
#include "segment.hpp"
#include "task.hpp"

alignas(16) TaskContext task_b_ctx, task_a_ctx;

namespace
{
    TaskContext *current_task;
}

Task::Task(uint64_t id) : id_{id} {};

Task &Task::InitContext(TaskFunc *f, int64_t data)
{
    const size_t stack_size = kDefaultStackBytes / sizeof(stack_[0]);
    stack_.resize(stack_size);
    uint64_t stack_end = reinterpret_cast<uint64_t>(&stack_[stack_size]);

    memset(&context_, 0, sizeof(context_));

    context_.cr3 = GetCR3();
    // What's 0x202?
    // If 1 and 9bit are 1, the others are 0, then the value is 0x202.
    // If 9 bit is 1, then intrruption is allowed.
    // 1 bit is fixed 1 on hardware, so it doesn't matter if bit is 1 or 0
    context_.rflags = 0x202;
    context_.cs = kKernelCS;
    context_.ss = kKernelSS;
    context_.rsp = (stack_end & ~0xflu) - 8;
    context_.rip = reinterpret_cast<uint64_t>(f);

    // set argument for new task to rdi and rsi.
    // register rdi and rsi is used for argument
    context_.rdi = id_;
    context_.rsi = data;

    // mask all exceptions in mxcsr
    *reinterpret_cast<uint32_t *>(&context_.fxsave_area[24]) = 0x1f80;

    return *this;
}

TaskContext &Task::Context()
{
    return context_;
}

TaskManager *task_manager;

TaskManager::TaskManager()
{
    NewTask();
}

Task &TaskManager::NewTask()
{
    ++latest_id_;
    return *tasks_.emplace_back(new Task{latest_id_});
}

void TaskManager::SwitchTask()
{
    size_t next_task_index = current_task_index_ + 1;
    if (next_task_index >= tasks_.size())
    {
        next_task_index = 0;
    }

    Task &current_task = *tasks_[current_task_index_];
    Task &next_task = *tasks_[next_task_index];
    current_task_index_ = next_task_index;

    SwitchContext(&next_task.Context(), &current_task.Context());
}

void SwitchTask()
{
    TaskContext *old_current_task = current_task;
    if (current_task == &task_a_ctx)
    {
        current_task = &task_b_ctx;
    }
    else
    {
        current_task = &task_a_ctx;
    }
    SwitchContext(current_task, old_current_task);
}

void InitializeTask()
{
    task_manager = new TaskManager;

    __asm__("cli");
    timer_manager->AddTimer(
        Timer{timer_manager->CurrentTick() + kTaskTimerPeriod, kTaskTimerValue});
    __asm__("sti");
}
