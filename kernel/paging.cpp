#include <cstdint>
#include <array>

#include "asmfunc.h"
#include "paging.hpp"
#include "task.hpp"
#include "memory_manager.hpp"

namespace
{
    const uint64_t kPageSize4k = 4096;
    const uint64_t kPageSize2M = 512 * kPageSize4k;
    const uint64_t kPageSize1G = 512 * kPageSize2M;

    alignas(kPageSize4k) std::array<uint64_t, 512> pml4_table;
    alignas(kPageSize4k) std::array<uint64_t, 512> pdp_table;
    alignas(kPageSize4k)
        std::array<std::array<uint64_t, 512>, kPageDirectoryCount> page_directory;
}

void SetupIdentityPageTable()
{
    pml4_table[0] = reinterpret_cast<uint64_t>(&pdp_table[0]) | 0x003;
    for (int i_pdpt = 0; i_pdpt < page_directory.size(); i_pdpt++)
    {
        pdp_table[i_pdpt] = reinterpret_cast<uint64_t>(&page_directory[i_pdpt]) | 0x003;
        for (int i_pd = 0; i_pd < 512; i_pd++)
        {
            page_directory[i_pdpt][i_pd] = i_pdpt * kPageSize1G + i_pd * kPageSize2M | 0x83;
        }
    }
    SetCR3(reinterpret_cast<uint64_t>(&pml4_table[0]));
}

void ResetCR3()
{
    SetCR3(reinterpret_cast<uint64_t>(&pml4_table[0]));
}

namespace
{

    WithError<PageMapEntry *> SetNewPageMapIfNotPresent(PageMapEntry &entry)
    {
        if (entry.bits.present)
        {
            return {entry.Pointer(), MAKE_ERROR(Error::kSuccess)};
        }

        auto [child_map, err] = NewPageMap();
        if (err)
        {
            return {nullptr, err};
        }

        entry.SetPointer(child_map);
        entry.bits.present = 1;
        return {child_map, MAKE_ERROR(Error::kSuccess)};
    }

    WithError<size_t> SetupPageMap(
        PageMapEntry *page_map, int page_map_level, LinearAddress4Level addr, size_t num_4kpages)
    {
        while (num_4kpages > 0)
        {
            const auto entry_index = addr.Part(page_map_level);

            auto [child_map, err] = SetNewPageMapIfNotPresent(page_map[entry_index]);
            if (err)
            {
                return {num_4kpages, err};
            }
            page_map[entry_index].bits.writable = 1;
            page_map[entry_index].bits.user = 1;
            if (page_map_level == 1)
            {
                num_4kpages--;
            }
            else
            {
                auto [num_remain_pages, err] =
                    SetupPageMap(child_map, page_map_level - 1, addr, num_4kpages);
                if (err)
                {
                    return {num_4kpages, err};
                }
                num_4kpages = num_remain_pages;
            }

            if (entry_index == 511)
            {
                break;
            }

            addr.SetPart(page_map_level, entry_index + 1);
            for (int level = page_map_level - 1; level >= 1; level--)
            {
                addr.SetPart(level, 0);
            }
        }
        // ret val is num that we could not allocate page
        return {num_4kpages, MAKE_ERROR(Error::kSuccess)};
    }

}

WithError<PageMapEntry *> NewPageMap()
{
    // allocate a frame, its size is indicated kBytesPerFrame
    // that is 4Kib bytes = 4 * 1024 bytes
    auto frame = memory_manager->Allocate(1);
    if (frame.error)
    {
        return {nullptr, frame.error};
    }

    auto e = reinterpret_cast<PageMapEntry *>(frame.value.Frame());

    // Why does we multiply 512?
    // Becasue a page map has 512 PageMapEntry.
    // I think rest of memory is 4*1024bytes - 512*8bytes = 0
    memset(e, 0, sizeof(uint64_t) * 512);
    return {e, MAKE_ERROR(Error::kSuccess)};
}


Error SetupPageMaps(LinearAddress4Level addr, size_t num_4kpages)
{
    auto pml4_table = reinterpret_cast<PageMapEntry *>(GetCR3());
    // SetupPageMap may not be able to allocate full size of num_4kpages.
    // the rest is retun value but we do not handle it.
    return SetupPageMap(pml4_table, 4, addr, num_4kpages).error;
}

Error HandlePageFault(uint64_t error_code, uint64_t causal_addr)
{
    auto &task = task_manager->CurrentTask();
    if (error_code & 1)
    {
        // Exception occured, P = 1 and violate page level privilage
        return MAKE_ERROR(Error::kAlreadyAllocated);
    }
    if (causal_addr < task.DPagingBegin() || task.DPagingEnd() <= causal_addr)
    {
        return MAKE_ERROR(Error::kIndexOutOfRange);
    }
    return SetupPageMaps(LinearAddress4Level{causal_addr}, 1);
}