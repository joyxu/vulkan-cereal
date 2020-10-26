#include "stream-servers/FrameworkFormats.h"

#include <functional>

class FrameBuffer {
public:
    static FrameBuffer* getFB();

    bool importMemoryToColorBuffer(
#ifdef _WIN32
        void* handle,
#else
        int handle,
#endif
        uint64_t size,
        bool dedicated,
        bool linearTiling,
        bool vulkanOnly,
        uint32_t colorBufferHandle);
    bool getColorBufferInfo(uint32_t p_colorbuffer,
                            int* width,
                            int* height,
                            int* internalformat,
                            FrameworkFormat* frameworkFormat = nullptr);

    void setColorBufferInUse(uint32_t handle, bool inUse);
    // Reads back the raw color buffer to |pixels|
    // if |pixels| is not null.
    // Always returns in |numBytes| how many bytes were
    // planned to be transmitted.
    // |numBytes| is not an input parameter;
    // fewer or more bytes cannot be specified.
    // If the framework format is YUV, it will read
    // back as raw YUV data.
    bool readColorBufferContents(
        uint32_t p_colorbuffer,
        size_t* numBytes,
        void *pixels);
    void replaceColorBufferContents(uint32_t handle, void* ptr, size_t bytes);
    bool getBufferInfo(uint32_t p_buffer, int* size);

    void registerProcessCleanupCallback(void* key, std::function<void()> callback);
    void unregisterProcessCleanupCallback(void* key);

    void lock();
    void unlock();
};
