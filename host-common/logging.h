#pragma once

#include <cstdio>

#define ENABLE_GL_LOG 1
#if defined(ENABLE_GL_LOG)
#define GL_LOG(fmt, args ...) fprintf(stderr, "%s:%d:%s: " fmt "\n", __FILE__, __LINE__, __func__, ## args)
#else
#define GL_LOG(...)
#endif

#define ERR(fmt, args ...) fprintf(stderr, "%s:%d:%s: " fmt "\n", __FILE__, __LINE__, __func__, ## args)
