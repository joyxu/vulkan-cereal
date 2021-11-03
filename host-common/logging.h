#pragma once

#include <cstdio>

#ifdef _MSC_VER
#define GFXSTREAM_LOG(file, fmt, ...) \
    fprintf(file, "%s:%d:%s: " fmt "\n", __FILE__, __LINE__, __func__, __VA_ARGS__)
#elif defined(__GNUC__) || defined(__clang__)
#define GFXSTREAM_LOG(file, fmt, ...) \
    fprintf(file, "%s:%d:%s: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define GFXSTREAM_LOG(...) ((void)0)
#endif

//#define ENABLE_GL_LOG 1
#if defined(ENABLE_GL_LOG)
#define GL_LOG(...) GFXSTREAM_LOG(stderr, __VA_ARGS__)
#else
#define GL_LOG(...) ((void)0)
#endif

#define ERR(...)                            \
    do {                                    \
        GFXSTREAM_LOG(stderr, __VA_ARGS__); \
        fflush(stderr);                     \
    } while (0)