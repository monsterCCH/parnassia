#ifndef _DMI_INFO_H
#define _DMI_INFO_H
#include <string>

class dmiInfo {
private:
    std::string m_sys_vendor;
    std::string m_bios_vendor;
    std::string m_bios_description;
    std::string m_sys_uuid;
public:
    dmiInfo();
    virtual ~dmiInfo()= default;
    static std::string get_dmi_value(const char* dmi_id);
    const std::string& sys_vendor() const { return m_sys_vendor; };
    const std::string& bios_vendor() const { return m_bios_vendor; }
    const std::string& bios_description() const { return m_bios_description; };
    const std::string& sys_uuid() const { return m_sys_uuid; };
};

#endif  // _DMI_INFO_H
