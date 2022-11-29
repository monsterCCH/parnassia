#include "dmi_info.h"
#include "logger.h"
#include "utils_string.h"
#include "utils_file.h"

dmiInfo::dmiInfo() {
    m_bios_description = get_dmi_value("/sys/class/dmi/id/modalias");
    m_sys_vendor = get_dmi_value("/sys/class/dmi/id/sys_vendor");
    m_sys_uuid = get_dmi_value("/sys/class/dmi/id/product_uuid");
}

std::string dmiInfo::get_dmi_value(const char* dmi_id)
{
    std::string res;
    try {
        res = toupper_copy(trim_copy(get_file_contents(dmi_id, 256)));
        char last_char = res[res.length() - 1];
        if (last_char == '\r' || last_char == '\n') {
            res = res.erase(res.length() - 1);
        }
    } catch (const std::exception& e) {
        LOG->warn("Can not read dmi_info : {}", dmi_id);
    }
    return res;
}
