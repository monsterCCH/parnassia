#ifndef PARNASSIA_TRINERVIS_BASE_H
#define PARNASSIA_TRINERVIS_BASE_H
#include <string>

#define BEGIN_NAMESPACE(x)  namespace x {
#define END_NAMESPACE       }

typedef enum { FUNC_RET_OK, FUNC_RET_ERROR } FUNCTION_RETURN;
struct funcRes{
    bool result;
    std::string err_msg;
};

#endif   // PARNASSIA_TRINERVIS_BASE_H
