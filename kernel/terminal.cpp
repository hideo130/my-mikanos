#include "asmfunc.h"
#include "elf.hpp"
#include "font.hpp"
#include "keyboard.hpp"
#include "logger.hpp"
#include "memory_manager.hpp"
#include "task.hpp"
#include "terminal.hpp"
#include "paging.hpp"
#include "pci.hpp"

namespace
{
    WithError<int> MakeArgVector(char *command, char *first_arg,
                                 char **argv, int argv_len, char *argbuf, int argbuf_len)
    {
        int argc = 0;
        int argbuf_index = 0;

        auto push_to_argv = [&](const char *s)
        {
            if (argc >= argv_len || argbuf_index >= argbuf_len)
            {
                return MAKE_ERROR(Error::kFull);
            }

            argv[argc] = &argbuf[argbuf_index];
            argc++;

            strcpy(&argbuf[argbuf_index], s);
            // add 1 for delimiter
            argbuf_index += strlen(s) + 1;
            return MAKE_ERROR(Error::kSuccess);
        };
        if (auto err = push_to_argv(command))
        {
            return {argc, err};
        }

        if (!first_arg)
        {
            return {argc, MAKE_ERROR(Error::kSuccess)};
        }

        char *p = first_arg;
        while (true)
        {
            while (isspace(p[0]))
            {
                p++;
            }
            if (p[0] == 0)
            {
                break;
            }
            const char *arg = p;

            while (p[0] != 0 && !isspace(p[0]))
            {
                p++;
            }
            // here: p[0] == 0 || isspace(p[0])
            const bool is_end = p[0] == 0;
            p[0] = 0;
            if (auto err = push_to_argv(arg))
            {
                return {argc, err};
            }
            if (is_end)
            {
                break;
            }
            p++;
        }

        return {argc, MAKE_ERROR(Error::kSuccess)};
    }

    Elf64_Phdr *GetProgramHeader(Elf64_Ehdr *ehdr)
    {
        return reinterpret_cast<Elf64_Phdr *>(reinterpret_cast<uintptr_t>(ehdr) + ehdr->e_phoff);
    }

    uintptr_t GetFirstLoadAddress(Elf64_Ehdr *ehdr)
    {
        auto phdr = GetProgramHeader(ehdr);
        for (int i = 0; i < ehdr->e_phnum; i++)
        {
            if (phdr[i].p_type != PT_LOAD)
                continue;
            return phdr[i].p_vaddr;
        }
        return 0;
    }

    uint64_t GetLastLoadAddress(Elf64_Ehdr *ehdr)
    {
        uint64_t last_addr = 0;
        auto phdr = GetProgramHeader(ehdr);
        for (int i = 0; i < ehdr->e_phnum; i++)
        {
            if (phdr[i].p_type != PT_LOAD)
                continue;
            uint64_t candidat_address = phdr[i].p_vaddr + phdr[i].p_memsz;
            last_addr = std::max(last_addr, candidat_address);
        }
        return last_addr;
    }

    Error CopyLoadSegments(Elf64_Ehdr *ehdr)
    {
        const auto phdr = GetProgramHeader(ehdr);
        for (int i = 0; i < ehdr->e_phnum; i++)
        {
            if (phdr[i].p_type != PT_LOAD)
                continue;

            LinearAddress4Level dest_addr;
            dest_addr.value = phdr[i].p_vaddr;
            const auto num_4kpages = (phdr[i].p_memsz + 4095) / 4096;

            if (auto err = SetupPageMaps(dest_addr, num_4kpages + 1))
            {
                return err;
            }

            // CheckPaging(dest_addr);
            const auto src = reinterpret_cast<uint8_t *>(ehdr) + phdr[i].p_offset;
            const auto dst = reinterpret_cast<uint8_t *>(phdr[i].p_vaddr);
            memcpy(dst, src, phdr[i].p_filesz);
            LinearAddress4Level dst2{static_cast<uint64_t>(*dst)};
            LinearAddress4Level dst3{static_cast<uint64_t>(phdr[i].p_vaddr)};
            LinearAddress4Level dst4{static_cast<uint64_t>(phdr[i].p_vaddr + phdr[i].p_filesz)};
            LinearAddress4Level dst5{static_cast<uint64_t>(phdr[i].p_vaddr + phdr[i].p_memsz)};
            LinearAddress4Level dst6{static_cast<uint64_t>(phdr[i].p_vaddr + 4096)};

            memset(dst + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
        }
        return MAKE_ERROR(Error::kSuccess);
    }

    WithError<uint64_t> LoadElf(Elf64_Ehdr *ehdr)
    {
        if (ehdr->e_type != ET_EXEC)
        {
            return {0, MAKE_ERROR(Error::kInvalidFormat)};
        }

        // get first load addr to wheather it's valid
        const auto addr_first = GetFirstLoadAddress(ehdr);
        // virtual address of application range is 0xffff'8000'0000'0000 or more
        if (addr_first < 0xffff'8000'0000'0000)
        {
            return {0, MAKE_ERROR(Error::kInvalidFormat)};
        }

        if (auto err = CopyLoadSegments(ehdr))
        {
            return {0, err};
        }

        uint64_t last_addr = GetLastLoadAddress(ehdr);
        return {last_addr, MAKE_ERROR(Error::kSuccess)};
    }

    static_assert(kBytesPerFrame >= 4096);

    Error CleanPageMap(PageMapEntry *page_map, int page_map_level)
    {
        for (int i = 0; i < 512; i++)
        {
            auto entry = page_map[i];
            if (!entry.bits.present)
            {
                continue;
            }
            if (page_map_level > 1)
            {
                if (auto err = CleanPageMap(entry.Pointer(), page_map_level - 1))
                {
                    return err;
                }
            }

            const auto entry_addr = reinterpret_cast<uintptr_t>(entry.Pointer());
            const FrameID map_frame{entry_addr / kBytesPerFrame};
            if (auto err = memory_manager->Free(map_frame, 1))
            {
                return err;
            }
            page_map[i].data = 0;
        }
        return MAKE_ERROR(Error::kSuccess);
    }

    Error CleanPageMaps(LinearAddress4Level addr)
    {
        auto pml4_table = reinterpret_cast<PageMapEntry *>(GetCR3());
        auto pdp_table = pml4_table[addr.parts.pml4].Pointer();
        pml4_table[addr.parts.pml4].data = 0;
        if (auto err = CleanPageMap(pdp_table, 3))
        {
            return err;
        }

        const auto pdp_addr = reinterpret_cast<uintptr_t>(pdp_table);
        const FrameID pdp_fram{pdp_addr / kBytesPerFrame};
        return memory_manager->Free(pdp_fram, 1);
    }
}

Terminal::Terminal(uint64_t task_id, bool show_window) : task_id_{task_id}, show_window_{show_window}
{
    if (show_window)
    {
        window_ = std::make_shared<ToplevelWindow>(
            kColumns * 8 + 8 + ToplevelWindow::kMarginX,
            kRows * 16 + 8 + ToplevelWindow::kMarginY,
            screen_config.pixel_format,
            "MikanTerm");
        DrawTerminal(*window_->InnerWriter(), {0, 0}, window_->InnerSize());

        layer_id_ = layer_manager->NewLayer()
                        .SetWindow(window_)
                        .SetDraggable(true)
                        .ID();
        Print(">");
    }
    cmd_history_.resize(8);
}

Rectangle<int> Terminal::BlinkCursor()
{
    cursor_visible_ = !cursor_visible_;
    DrawCursor(cursor_visible_);
    return {CalcCursorPos(), {7, 15}};
}

void Terminal::DrawCursor(bool visible)
{
    if (show_window_)
    {
        const auto color = visible ? ToColor(0xffffff) : ToColor(0);
        FillRectangle(*window_->Writer(), CalcCursorPos(), {7, 15}, color);
    }
}

Vector2D<int> Terminal::CalcCursorPos() const
{
    return ToplevelWindow::kTopLeftMargin + Vector2D<int>{4 + 8 * cursor_.x, 4 + 16 * cursor_.y};
}

Rectangle<int> Terminal::InputKey(uint8_t modifier, uint8_t keycode, char ascii)
{
    DrawCursor(false);
    Rectangle<int> draw_area{CalcCursorPos(), {8 * 2, 16}};

    if (ascii == '\n')
    {
        linebuf_[linebuf_index_] = 0;
        if (linebuf_index_ > 0)
        {
            cmd_history_.pop_back();
            cmd_history_.push_front(linebuf_);
        }
        cmd_history_index_ = -1;
        linebuf_index_ = 0;
        cursor_.x = 0;
        Log(kWarn, "line: %s\n", &linebuf_[0]);
        if (cursor_.y < kRows - 1)
        {
            cursor_.y++;
        }
        else
        {
            Scroll1();
        }
        ExecuteLine();
        Print(">");
        draw_area.pos = ToplevelWindow::kTopLeftMargin;
        draw_area.size = window_->InnerSize();
    }
    else if (ascii == '\b')
    {
        if (cursor_.x > 0)
        {
            cursor_.x--;
            FillRectangle(*window_->Writer(), CalcCursorPos(), {8, 16}, {0, 0, 0});
            draw_area.pos = CalcCursorPos();
            if (linebuf_index_ > 0)
            {
                linebuf_index_--;
            }
        }
    }
    else if (keycode == 0x51)
    {
        // down arrow
        draw_area = HistoryUpDown(-1);
    }
    else if (keycode == 0x52 || (ascii == '\0' && keycode == 'R'))
    {
        // up arrow
        draw_area = HistoryUpDown(1);
    }
    else if (ascii != 0)
    {
        if (cursor_.x < kColumns - 1 && linebuf_index_ < kLineMax - 1)
        {
            linebuf_[linebuf_index_] = ascii;
            linebuf_index_++;
            if (show_window_)
            {
                WriteAscii(*window_->Writer(), CalcCursorPos(), ascii, {255, 255, 255});
            }
            cursor_.x++;
        }
    }
    DrawCursor(true);
    return draw_area;
}

void Terminal::Scroll1()
{
    Rectangle<int> move_src{
        ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4 + 16},
        {8 * kColumns, 16 * (kRows - 1)}};
    window_->Move(ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4}, move_src);
    FillRectangle(*window_->InnerWriter(), {4, 4 + 16 * cursor_.y}, {8 * kColumns, 16}, {0, 0, 0});
}

void Terminal::Print(char c)
{

    auto newline = [this]()
    {
        cursor_.x = 0;
        if (cursor_.y < kRows - 1)
        {
            cursor_.y++;
        }
        else
        {
            Scroll1();
        }
    };

    if (c == '\n')
    {
        newline();
    }
    else
    {
        if (show_window_)
        {
            WriteAscii(*window_->Writer(), CalcCursorPos(), c, {255, 255, 255});
        }
        if (cursor_.x == kColumns - 1)
        {
            newline();
        }
        else
        {
            cursor_.x++;
        }
    }
}

void Terminal::Print(const char *s, std::optional<size_t> len)
{
    const auto cursor_before = CalcCursorPos();
    DrawCursor(false);
    if (len)
    {
        for (size_t i = 0; i < *len; i++)
        {
            Print(*s);
            s++;
        }
    }
    else
    {
        while (*s)
        {
            Print(*s);
            s++;
        }
    }

    DrawCursor(false);
    const auto cursor_after = CalcCursorPos();
    Vector2D<int> draw_pos{ToplevelWindow::kTopLeftMargin.x, cursor_before.y};
    Vector2D<int> draw_size{window_->InnerSize().x,
                            cursor_after.y - cursor_before.y + 16};
    Rectangle<int> draw_area{draw_pos, draw_size};
    Message msg = MakeLayerMessage(task_id_, LayerID(), LayerOperation::DrawArea, draw_area);
    __asm__("cli");
    task_manager->SendMessage(1, msg);
    __asm__("sti");
}

void Terminal::ExecuteLine()
{
    char *command = &linebuf_[0];
    char *first_arg = strchr(&linebuf_[0], ' ');

    if (first_arg)
    {
        *first_arg = 0;
        first_arg++;
    }
    if (strcmp(command, "echo") == 0)
    {
        if (first_arg)
        {
            Print(first_arg);
        }
        Print("\n");
    }
    else if (strcmp(command, "clear") == 0)
    {
        FillRectangle(*window_->InnerWriter(), {4, 4}, {8 * kColumns, 16 * kRows}, {0, 0, 0});
        cursor_.y = 0;
    }
    else if (strcmp(command, "lspci") == 0)
    {
        char s[64];
        for (int i = 0; i < pci::num_device; i++)
        {
            const auto &dev = pci::devices[i];
            auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
            sprintf(s, "%02x:%02x.%d vend=%04x head=%02x class=%02x.%02x.%02x\n",
                    dev.bus, dev.device, dev.function, vendor_id, dev.header_type,
                    dev.class_code.base, dev.class_code.sub, dev.class_code.interface);
            Print(s);
        }
    }
    else if (strcmp(command, "ls") == 0)
    {
        if (first_arg[0] == '\0')
        {
            ListAllEntries(this, fat::boot_volume_image->root_clus);
        }
        else
        {
            auto [dir, post_slash] = fat::FindFile(first_arg);
            if (dir == nullptr)
            {
                Print("No such file or directory: ");
                Print(first_arg);
                Print("\n");
            }
            else if (dir->attr == fat::Attribute::kDirectory)
            {
                ListAllEntries(this, dir->FirstCluster());
            }
            else
            {
                char name[13];
                fat::FormatName(*dir, name);
                if (post_slash)
                {
                    Print(name);
                    Print(" is not a directory \n");
                }
                else
                {
                    Print(name);
                    Print("\n");
                }
            }
        }
    }
    else if (strcmp(command, "cat") == 0)
    {
        char s[64];

        auto [file_entry, _post_slash] = fat::FindFile(first_arg);
        if (!file_entry)
        {
            sprintf(s, "no such file: %s\n", first_arg);
            Print(s);
        }
        else
        {
            auto cluster = file_entry->FirstCluster();
            auto remain_bytes = file_entry->file_size;

            DrawCursor(false);
            // why do we avoid cluster 0.
            while (cluster != 0 && cluster != fat::kEndOfClusterchain)
            {
                char *p = fat::GetSectorByCluster<char>(cluster);

                int i = 0;
                for (; i < fat::bytes_per_cluster && i < remain_bytes; i++)
                {
                    Print(*p);
                    p++;
                }
                remain_bytes -= i;
                cluster = fat::NextCluster(cluster);
            }
            DrawCursor(true);
        }
    }
    else if (strcmp(command, "noterm") == 0)
    {
        task_manager->NewTask()
            .InitContext(TaskTerminal, reinterpret_cast<int64_t>(first_arg))
            .Wakeup();
    }
    else if (command[0] != 0)
    {
        auto [file_entry, post_slash] = fat::FindFile(command);
        if (!file_entry)
        {
            Print("no such command: ");
            Print(command);
            Print("\n");
        }
        else if (file_entry->attr != fat::Attribute::kDirectory && post_slash)
        {
            char name[13];
            fat::FormatName(*file_entry, name);
            Print(name);
            Print(" is not a directory\n");
        }
        else if (auto err = ExecuteFile(*file_entry, command, first_arg))
        {
            Print("failed to exec file: ");
            Print(err.Name());
            Print("\n");
        }
    }
}

Error Terminal::ExecuteFile(const fat::DirectoryEntry &file_entry, char *command, char *first_arg)
{
    std::vector<uint8_t> file_buf(file_entry.file_size);
    fat::LoadFile(&file_buf[0], file_buf.size(), file_entry);

    auto elf_header = reinterpret_cast<Elf64_Ehdr *>(&file_buf[0]);
    if (memcmp(elf_header->e_ident, "\x7f"
                                    "ELF",
               4) != 0)
    {
        return MAKE_ERROR(Error::kInvalidFile);
    }

    __asm__("cli");
    auto &task = task_manager->CurrentTask();
    __asm__("sti");

    if (auto pml4 = SetupPML4(task); pml4.error)
    {
        return pml4.error;
    }
    const auto [elf_last_addr, elf_err] = LoadElf(elf_header);

    if (elf_err)
    {
        return elf_err;
    }

    LinearAddress4Level args_frame_addr{0xffff'ffff'ffff'f000};
    if (auto err = SetupPageMaps(args_frame_addr, 1))
    {
        return err;
    }
    auto argv = reinterpret_cast<char **>(args_frame_addr.value);
    int argv_len = 32; // argv = 8 x 32 = 256bytes
    auto argbuf = reinterpret_cast<char *>(args_frame_addr.value + sizeof(char **) * argv_len);
    int argbuf_len = 4096 - sizeof(char **) * argv_len;
    auto argc = MakeArgVector(command, first_arg, argv, argv_len, argbuf, argbuf_len);
    if (argc.error)
    {
        return argc.error;
    }

    LinearAddress4Level stack_frame_addr{0xffff'ffff'ffff'e000};
    if (auto err = SetupPageMaps(stack_frame_addr, 1))
    {
        return err;
    }
    for (int i = 0; i < 3; i++)
    {
        task.Files().push_back(
            std::make_unique<TerminalFileDescriptor>(task, *this));
    }

    const uint64_t elf_next_page = (elf_last_addr + kBytesPerFrame +  4095) & 0xffff'ffff'ffff'f000;
    task.SetDPagingBegin(elf_next_page);
    task.SetDPagingEnd(elf_last_addr);

    auto entry_addr = elf_header->e_entry;
    // Expression 3<<3 | 3 is same for table 20.3
    uint16_t user_cpl = 3;
    uint16_t gdt_index_of_ss = 3;
    uint16_t ss_value = gdt_index_of_ss << 3 | user_cpl;
    // 4096 = 0x1000
    // why do we subtract 8?
    int ret = CallApp(argc.value, argv, ss_value, entry_addr,
                      stack_frame_addr.value + 4096 - 8,
                      &task.OSStackPointer());
    task.Files().clear();
    char s[64];
    sprintf(s, "app exited. ret = %d\n", ret);
    Print(s);

    const auto addr_first = GetFirstLoadAddress(elf_header);
    if (auto err = CleanPageMaps(LinearAddress4Level{addr_first}))
    {
        return err;
    }

    return FreePML4(task);
}

WithError<PageMapEntry *> SetupPML4(Task &current_task)
{
    auto pml4 = NewPageMap();
    if (pml4.error)
    {
        return pml4;
    }

    const auto current_pml4 = reinterpret_cast<PageMapEntry *>(GetCR3());
    memcpy(pml4.value, current_pml4, 256 * sizeof(uint64_t));

    const auto cr3 = reinterpret_cast<uint64_t>(pml4.value);
    SetCR3(cr3);
    current_task.Context().cr3 = cr3;
    return pml4;
}

Error FreePML4(Task &current_task)
{
    const auto cr3 = current_task.Context().cr3;
    current_task.Context().cr3 = 0;
    ResetCR3();

    const FrameID frame{cr3 / kBytesPerFrame};
    return memory_manager->Free(frame, 1);
}

Rectangle<int> Terminal::HistoryUpDown(int direction)
{
    if (direction == -1 && cmd_history_index_ >= 0)
    {
        cmd_history_index_--;
    }
    else if (direction == 1 && cmd_history_index_ + 1 < cmd_history_.size())
    {
        cmd_history_index_++;
    }
    cursor_.x = 1;
    const auto first_pos = CalcCursorPos();

    Rectangle<int> draw_area{first_pos, {8 * (kColumns - 1), 16}};
    FillRectangle(*window_->Writer(), draw_area.pos, draw_area.size, {0, 0, 0});

    const char *history = "";
    if (cmd_history_index_ >= 0)
    {
        history = &cmd_history_[cmd_history_index_][0];
    }

    strcpy(&linebuf_[0], history);
    linebuf_index_ = strlen(history);
    WriteString(*window_->Writer(), first_pos, history, {255, 255, 255});
    cursor_.x = linebuf_index_ + 1;
    return draw_area;
}

void ListAllEntries(Terminal *term, uint32_t dir_cluster)
{

    auto dir_entries = fat::GetSectorByCluster<fat::DirectoryEntry>(
        dir_cluster);
    auto entries_per_cluster = fat::boot_volume_image->bytes_per_sec / sizeof(fat::DirectoryEntry) *
                               fat::boot_volume_image->sec_per_clus;
    char base[9], ext[4];
    char s[64];
    for (int i = 0; i < entries_per_cluster; i++)
    {
        ReadName(dir_entries[i], base, ext);
        if (base[0] == 0x00)
        {
            break;
        }
        else if (static_cast<uint8_t>(base[0]) == 0xe5)
        {
            continue;
        }
        else if (dir_entries[i].attr == fat::Attribute::kLongName)
        {
            continue;
        }

        if (ext[0])
        {
            sprintf(s, "%s.%s\n", base, ext);
        }
        else
        {
            sprintf(s, "%s\n", base);
        }

        term->Print(s);
    }
}

void TaskTerminal(uint64_t task_id, int64_t data)
{
    const char *command_line = reinterpret_cast<char *>(data);
    const bool show_window = command_line == nullptr;

    __asm__("cli");
    Task &task = task_manager->CurrentTask();
    Terminal *terminal = new Terminal{task_id, show_window};

    if (show_window)
    {
        layer_manager->Move(terminal->LayerID(), {100, 200});
        layer_task_map->insert(std::make_pair(terminal->LayerID(), task_id));
        active_layer->Activate(terminal->LayerID());
    }
    __asm__("sti");
    if (command_line)
    {
        for (int i = 0; command_line[i] != '\0'; i++)
        {
            terminal->InputKey(0, 0, command_line[i]);
        }
        terminal->InputKey(0, 0, '\n');
    }
    auto add_blink_timer = [task_id](unsigned long t)
    {
        timer_manager->AddTimer(Timer{t + static_cast<int>(kTimerFreq * 0.5), 1, task_id});
    };
    add_blink_timer(timer_manager->CurrentTick());

    bool window_isactive = false;

    while (true)
    {
        __asm__("cli");
        auto msg = task.ReceiveMessage();
        if (!msg)
        {
            task.Sleep();
            __asm__("sti");
            continue;
        }
        __asm__("sti");

        switch (msg->type)
        {
        case Message::kTimerTimeout:
        {
            add_blink_timer(msg->arg.timer.timeout);
            if (show_window && window_isactive)
            {
                const auto area = terminal->BlinkCursor();
                Message msg = MakeLayerMessage(task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
                __asm__("cli");
                task_manager->SendMessage(1, msg);
                __asm__("sti");
            }
        }
        break;
        case Message::kKeyPush:
        {
            if (msg->arg.keyboard.press)
            {
                const auto area = terminal->InputKey(msg->arg.keyboard.modifier,
                                                     msg->arg.keyboard.keycode,
                                                     msg->arg.keyboard.ascii);
                if (show_window)
                {
                    Message msg = MakeLayerMessage(task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
                    __asm__("cli");
                    task_manager->SendMessage(1, msg);
                    __asm__("sti");
                }
            }
        }
        break;
        case Message::kWindowActive:
        {
            window_isactive = msg->arg.window_active.activate;
        }
        break;
        default:
            break;
        }
    }
}

TerminalFileDescriptor::TerminalFileDescriptor(Task &task, Terminal &term)
    : task_{task}, term_{term}
{
}

size_t TerminalFileDescriptor::Read(void *buf, size_t len)
{
    char *bufc = reinterpret_cast<char *>(buf);
    while (true)
    {
        __asm__("cli");
        auto msg = task_.ReceiveMessage();
        if (!msg)
        {
            __asm__("sti");
            task_.Sleep();
            continue;
        }
        __asm__("sti");
        if (msg->type != Message::kKeyPush || !msg->arg.keyboard.press)
        {
            continue;
        }
        if (msg->arg.keyboard.modifier & (kLControlBitMask | kRControlBitMask))
        {
            char s[3] = "^ ";
            s[1] = toupper(msg->arg.keyboard.ascii);
            term_.Print(s);
            if (msg->arg.keyboard.keycode == 7)
            {
                // EOT
                return 0;
            }
            continue;
        }
        bufc[0] = msg->arg.keyboard.ascii;
        term_.Print(bufc, 1);
        return 1;
    }
}

size_t TerminalFileDescriptor::Write(const void *buf, size_t len)
{
    term_.Print(reinterpret_cast<const char *>(buf), len);
    return len;
}