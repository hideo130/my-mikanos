#pragma once

#include <deque>

#include "fat.hpp"
#include "layer.hpp"

class Terminal
{
    static const int kLineMax = 128;

private:
    std::shared_ptr<ToplevelWindow> window_;
    unsigned int layer_id_;
    uint64_t task_id_;

    Vector2D<int> cursor_{0, 0};
    bool cursor_visible_{false};
    void DrawCursor(bool visible);
    Vector2D<int> CalcCursorPos() const;

    int linebuf_index_{0};
    std::array<char, kLineMax> linebuf_{};
    void Scroll1();

    void Print(char c);
    void ExecuteLine();
    Error ExecuteFile(const fat::DirectoryEntry &file_entry, char *command, char *first_arg);

    std::deque<std::array<char, kLineMax>> cmd_history_{};
    int cmd_history_index_{0};
    Rectangle<int> HistoryUpDown(int direction);

public:
    static const int kRows = 15, kColumns = 60;

    Terminal(uint64_t task_id);
    ~Terminal();

    unsigned int LayerID() const { return layer_id_; }
    Rectangle<int> BlinkCursor();
    Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);
    void Print(const char *s, std::optional<size_t> len = std::nullopt);
};

void TaskTerminal(uint64_t task_id, int64_t data);
extern std::map<uint64_t, Terminal *> *terminals;
