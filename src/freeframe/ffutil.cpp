#include "NosuchUtil.h"
#include "ffutil.h"

// Freeframe utilities
std::string CopyFFString16(const char *src)
{
    // src should be fixed-length, but it might be null-terminated instead
    char buff[17];
    memcpy(buff, src, 16);
    buff[16] = 0;
    return std::string(buff);
}
