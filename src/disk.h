#ifndef PARNASSIA_TRINERVIS_DISK_H
#define PARNASSIA_TRINERVIS_DISK_H
#include "base.h"
#include <string>
#include <vector>

BEGIN_NAMESPACE(DISK)

#define MB (1024 * 1024)
#define GB (1024 * 1024 * 1024)
typedef struct {
    std::string monunt_point;
    int64_t total = 0,
            used = 0,
            free = 0;
    int used_percent = 0,
        free_percent = 0;
}DiskInfo;

FUNCTION_RETURN getRealFileSystem(std::vector<std::string>& fs_types);

FUNCTION_RETURN getMountPoints(std::vector<std::string>& mt_point);

std::vector<DiskInfo> getDiskInfos();


END_NAMESPACE

#endif   // PARNASSIA_TRINERVIS_DISK_H
