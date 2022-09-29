#include "disk.h"
#include <fstream>
#include <limits>
#include <sys/statvfs.h>
#include "utils_string.h"
#include "logger.h"
#include "utils_file.h"
using namespace std;

BEGIN_NAMESPACE(DISK)
const std::string PROC_FILESYSTEM = "/proc/filesystems";
const std::string MTAB = "/etc/mtab";
const std::string PROC_MOUNTS = "/proc/self/mounts";

constexpr auto SSmax = std::numeric_limits<std::streamsize>::max();

FUNCTION_RETURN getRealFileSystem(std::vector<std::string>& fs_types)
{
    try {
        ifstream disk_read;
        fs_types = {"zfs", "wslfs", "drvfs"};
        disk_read.open(PROC_FILESYSTEM);
        if (disk_read.good()) {
            for (string fs_type; disk_read >> fs_type;) {
                if (!is_in(fs_type, "nodev", "squashfs", "nullfs"))
                    fs_types.push_back(fs_type);
                disk_read.ignore(SSmax, '\n');
            }
        }
        else {
            LOG->warn("Failed to read /proc/filesystems");
        }
        disk_read.close();
        return FUNC_RET_OK;
    } catch (exception& e) {
        LOG->warn("Failed to get real filesystems : {}", e.what());
    }
    return FUNC_RET_ERROR;
}

FUNCTION_RETURN getMountPoints(std::vector<std::string>& mt_point)
{
    vector<string> ft;
    getRealFileSystem(ft);
    string path = file_exists(MTAB)? MTAB : PROC_MOUNTS;
    try {
        ifstream disk_read;
        disk_read.open(path);
        if (disk_read.good()) {
            string dev, mount_point, fs_type;
            while (!disk_read.eof()) {
                disk_read >> dev >> mount_point >> fs_type;
                disk_read.ignore(SSmax, '\n');
                auto iter = find(mt_point.begin(), mt_point.end(), mount_point);
                if ( iter != mt_point.end()) continue ;

                auto it = find(ft.begin(), ft.end(), fs_type);
                if (it == ft.end()) continue ;

                size_t zfs_dataset_name_start = 0;
                if (fs_type == "zfs" && (zfs_dataset_name_start = dev.find('/')) != std::string::npos) continue;
                mt_point.push_back(mount_point);
            }
        } else {
            LOG->warn("Failed to read {}", path);
        }
        disk_read.close();
        return FUNC_RET_OK;
    } catch (exception& e) {
        LOG->warn("Failed to get mount point : {}", e.what());
    }
    return FUNC_RET_ERROR;
}

std::vector<DiskInfo> getDiskInfos()
{
    vector<string> mt;
    vector<DiskInfo> di;
    getMountPoints(mt);

    for (auto& it : mt) {
        if (!dir_exists(it)) continue ;

        struct statvfs64 vfs;
        if (statvfs64(it.c_str(), &vfs) < 0) {
            LOG->warn("Failed to get disk/partition stats with statvfs() for: {}", it);
            continue;
        }
        DiskInfo disk{};
        disk.monunt_point = it;
        disk.total = vfs.f_blocks * vfs.f_frsize;
        disk.free = vfs.f_bavail * vfs.f_frsize;
        disk.used = disk.total - disk.free;
        disk.used_percent = round((double)disk.used * 100 / disk.total);
        disk.free_percent = 100 - disk.used_percent;
        di.emplace_back(disk);
    }
    return di;
}

END_NAMESPACE
