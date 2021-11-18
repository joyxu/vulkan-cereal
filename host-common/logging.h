#pragma once

#include <cstdio>

#ifdef _MSC_VER
#define GFXSTREAM_LOG(file, prefix, fmt, ...) \
    fprintf(file, "%s %s:%d:%s: " fmt "\n", prefix, __FILE__, __LINE__, __func__, __VA_ARGS__)
#elif defined(__GNUC__) || defined(__clang__)
#define GFXSTREAM_LOG(file, prefix, fmt, ...) \
    fprintf(file, "%s %s:%d:%s: " fmt "\n", prefix, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define GFXSTREAM_LOG(...) ((void)0)
#endif

//#define ENABLE_GL_LOG 1
#if defined(ENABLE_GL_LOG)
#define GL_LOG(...) GFXSTREAM_LOG(stderr, "I", __VA_ARGS__)
#else
#define GL_LOG(...) ((void)0)
#endif

//#define ENABLE_DECODER_LOG 1
#if defined(ENABLE_DECODER_LOG)
#define DECODER_DEBUG_LOG(...) GFXSTREAM_LOG(stderr, "I", __VA_ARGS__)
#else
#define DECODER_DEBUG_LOG(...) ((void)0)
#endif

#define ENABLE_DISPATCH_LOG 1
#if defined(ENABLE_DISPATCH_LOG)
#define DISPATCH_DEBUG_LOG(...) GFXSTREAM_LOG(stderr, "I", __VA_ARGS__)
#else
#define DISPATCH_DEBUG_LOG(...) ((void)0)
#endif

#define ERR(...)                                 \
    do {                                         \
        GFXSTREAM_LOG(stderr, "E", __VA_ARGS__); \
        fflush(stderr);                          \
    } while (0)

#define INFO(...)                                \
    do {                                         \
        GFXSTREAM_LOG(stdout, "I", __VA_ARGS__); \
        fflush(stdout);                          \
    } while (0)

#ifndef GFXSTREAM_FATAL
#define GFXSTREAM_FATAL() abort();
#endif
