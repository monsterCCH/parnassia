#ifndef PARNASSIA_TRINERVIS_BASE_H
#define PARNASSIA_TRINERVIS_BASE_H
#include <string>
#include <map>

#define BEGIN_NAMESPACE(x)  namespace x {
#define END_NAMESPACE       }

typedef enum { FUNC_RET_OK, FUNC_RET_ERROR } FUNCTION_RETURN;
struct funcRes{
    bool result;
    std::string err_msg;
};

enum BaseMacro {
    BM_SYS_ID = 0,
    BM_OS_TYPE,
    BM_NET_NAME,
    BM_NET_ADDR,
    BM_NET_MAC,
    BM_MSG_ID
};

static std::map<int, std::string> BaseMacroMap = {
    {BM_SYS_ID, "sysId"},
    {BM_OS_TYPE, "osType"},
    {BM_NET_NAME, "netName"},
    {BM_NET_ADDR, "netAddr"},
    {BM_NET_MAC, "MAC"},
    {BM_MSG_ID, "msgId"}
};

template<typename T>
class Singleton
{
public:
    static T& GetInstance() {
        static T instance;
        return instance;
    }

    Singleton(const T&) = delete;
    Singleton(T&&) = delete;
    void operator= (const T&) = delete;
    void operator= (T &&) = delete;
protected:
    Singleton() = default;
    virtual ~Singleton() = default;
};

class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;
    void operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};

class StringArg
{
public:
    StringArg(const char* str)
        : str_(str)
    { }

    StringArg(const std::string& str)
        : str_(str.c_str())
    { }

    const char* c_str() const { return str_; }

private:
    const char* str_;
};

#endif   // PARNASSIA_TRINERVIS_BASE_H
