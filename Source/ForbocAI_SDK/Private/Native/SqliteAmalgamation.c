#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wpointer-bool-conversion"
#endif

#if WITH_FORBOC_SQLITE_VEC
#include "sqlite3.c"
#include "vec0.c"
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
