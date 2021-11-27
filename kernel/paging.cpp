#include <cstdint>
#include <array>

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
        pdp_table[i_pdpt] = reinterpret_cast<uint64_t>(& page_directory[i_pdpt]) | 0x003;
    }
}
