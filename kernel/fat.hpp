#pragma once

#include <cstddef>
#include <cstdint>
#include "error.hpp"
#include "file.hpp"

namespace fat
{
    struct BPB
    {
        uint8_t jump_boot[3];
        char oem_name[8];
        uint16_t bytes_per_sec;
        uint8_t sec_per_clus;
        uint16_t rsvd_sec_cnt;
        uint8_t num_fats;
        uint16_t root_ent_cnt;
        uint16_t tot_sec_16;
        uint8_t media;
        uint16_t fatsz16;
        uint16_t ser_per_trk;
        uint16_t num_heads;
        uint32_t hidd_sec;
        uint32_t tot_sec_32;
        uint32_t fat_sz_32;
        uint16_t ext_falgs;
        uint16_t fsver;
        uint32_t root_clus;
        uint16_t fs_info;
        uint16_t bk_boot_sec;
        uint8_t reserved[12];
        uint8_t drv_num;
        uint8_t reserved1;
        uint8_t boot_sig;
        uint32_t vol_ld;
        char vol_lab[11];
        char fil_sys_type[8];
    } __attribute__((packed));

    enum class Attribute : uint8_t
    {
        kReadOnly = 0x01,
        kHidden = 0x02,
        kSystem = 0x04,
        kVolumeId = 0x08,
        kDirectory = 0x10,
        kArchive = 0x20,
        kLongName = 0x0f
    };

    struct DirectoryEntry
    {
        unsigned char name[11];
        Attribute attr;
        uint8_t nt_resv;
        uint8_t crt_time_tenth;
        uint16_t crt_time;
        uint16_t crt_date;
        uint16_t lst_acc_date;
        uint16_t fst_clus_hi;
        uint16_t wrt_time;
        uint16_t wrt_date;
        uint16_t fst_clus_lo;
        uint32_t file_size;

        uint32_t FirstCluster() const
        {
            return fst_clus_lo |
                   (static_cast<uint32_t>(fst_clus_hi) << 16);
        }

    } __attribute__((packed));

    extern BPB *boot_volume_image;
    extern unsigned long bytes_per_cluster;
    void Initialize(void *volume_image);

    uintptr_t GetClusterAddr(unsigned long cluster);
    template <class T>
    T *GetSectorByCluster(unsigned long cluster)
    {
        return reinterpret_cast<T *>(GetClusterAddr(cluster));
    }

    void ReadName(const DirectoryEntry &entry, char *base, char *ext);
    void FormatName(const DirectoryEntry &entry, char *dest);

    unsigned long NextCluster(unsigned long cluster);
    static const unsigned long kEndOfClusterchain = 0x0ffffffflu;
    std::pair<DirectoryEntry *, bool> FindFile(const char *name, unsigned long directory_cluster = 0);
    std::pair<const char *, bool> NextPathElement(const char *path, char *path_elem);

    size_t LoadFile(void *buf, size_t len, const DirectoryEntry &entry);

    class FileDescriptor : public ::FileDescriptor
    {
    public:
        explicit FileDescriptor(DirectoryEntry &fat_entry);
        size_t Read(void *buf, size_t len) override;
        size_t Write(const void *buf, size_t len) override;

    private:
        DirectoryEntry &fat_entry_;
        size_t rd_off_ = 0;
        unsigned long rd_cluster_ = 0;
        size_t rd_cluster_off_ = 0;
        // Use the following 3 variables for writing.
        // off set from file head
        size_t wr_off_ = 0;
        // Write target cluster number
        unsigned long wr_cluster_ = 0;
        // Offset at cluster
        size_t wr_cluster_off_ = 0;
    };

    bool IsEndOfClusterchain(unsigned long cluster);

    void SetFileName(DirectoryEntry &dir, const char *filename);
    WithError<DirectoryEntry *> CreateFile(const char *path);
    DirectoryEntry *AllocateEntry(unsigned long dir_cluster);
    unsigned long AllocateClusterChain(size_t n);
    unsigned long ExtendCluster(unsigned long eoc_cluster, size_t n);
    uint32_t *GetFAT();
}
