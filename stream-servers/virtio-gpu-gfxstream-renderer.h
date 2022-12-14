#ifndef VIRTIO_GOLDFISH_PIPE
#define VIRTIO_GOLDFISH_PIPE

/* An override of virtio-gpu-3d (virgl) that runs goldfish pipe.  One could
 * implement an actual virtio goldfish pipe, but this hijacking of virgl  is
 * done in order to avoid any guest kernel changes. */

#include <assert.h>
#include <stddef.h>

#include "virgl_hw.h"
#include "virglrenderer.h"


#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t VirtioGpuCtxId;
typedef uint8_t VirtioGpuRingIdx;
struct virgl_renderer_virtio_interface*
    get_goldfish_pipe_virgl_renderer_virtio_interface(void);

/* Needed for goldfish pipe */
void virgl_write_fence(void *opaque, uint32_t fence);

#ifdef _WIN32
#define VG_EXPORT __declspec(dllexport)
#else
#define VG_EXPORT __attribute__((visibility("default")))
#endif

VG_EXPORT void virtio_goldfish_pipe_reset(void* hwpipe, void* hostpipe);

#define VIRTIO_GOLDFISH_EXPORT_API
#ifdef VIRTIO_GOLDFISH_EXPORT_API

VG_EXPORT int pipe_virgl_renderer_init(void *cookie,
                                       int flags,
                                       struct virgl_renderer_callbacks *cb);
VG_EXPORT void pipe_virgl_renderer_poll(void);
VG_EXPORT void* pipe_virgl_renderer_get_cursor_data(
    uint32_t resource_id, uint32_t *width, uint32_t *height);
VG_EXPORT int pipe_virgl_renderer_resource_create(
    struct virgl_renderer_resource_create_args *args,
    struct iovec *iov, uint32_t num_iovs);
VG_EXPORT void pipe_virgl_renderer_resource_unref(uint32_t res_handle);
VG_EXPORT int pipe_virgl_renderer_context_create(
    uint32_t handle, uint32_t nlen, const char *name);
VG_EXPORT void pipe_virgl_renderer_context_destroy(uint32_t handle);
VG_EXPORT int pipe_virgl_renderer_submit_cmd(void *buffer,
                                          int ctx_id,
                                          int bytes);
VG_EXPORT int pipe_virgl_renderer_transfer_read_iov(
    uint32_t handle, uint32_t ctx_id,
    uint32_t level, uint32_t stride,
    uint32_t layer_stride,
    struct virgl_box *box,
    uint64_t offset, struct iovec *iov,
    int iovec_cnt);
VG_EXPORT int pipe_virgl_renderer_transfer_write_iov(
    uint32_t handle,
    uint32_t ctx_id,
    int level,
    uint32_t stride,
    uint32_t layer_stride,
    struct virgl_box *box,
    uint64_t offset,
    struct iovec *iovec,
    unsigned int iovec_cnt);
VG_EXPORT void pipe_virgl_renderer_get_cap_set(uint32_t, uint32_t*, uint32_t*);
VG_EXPORT void pipe_virgl_renderer_fill_caps(uint32_t, uint32_t, void *caps);

VG_EXPORT int pipe_virgl_renderer_resource_attach_iov(
    int res_handle, struct iovec *iov,
    int num_iovs);
VG_EXPORT void pipe_virgl_renderer_resource_detach_iov(
    int res_handle, struct iovec **iov, int *num_iovs);
VG_EXPORT int pipe_virgl_renderer_create_fence(
    int client_fence_id, uint32_t cmd_type);
VG_EXPORT void pipe_virgl_renderer_force_ctx_0(void);
VG_EXPORT void pipe_virgl_renderer_ctx_attach_resource(
    int ctx_id, int res_handle);
VG_EXPORT void pipe_virgl_renderer_ctx_detach_resource(
    int ctx_id, int res_handle);
VG_EXPORT int pipe_virgl_renderer_resource_get_info(
    int res_handle,
    struct virgl_renderer_resource_info *info);

VG_EXPORT void stream_renderer_flush_resource_and_readback(
    uint32_t res_handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
    void* pixels, uint32_t max_bytes);

#define STREAM_MEM_HANDLE_TYPE_OPAQUE_FD 0x1
#define STREAM_MEM_HANDLE_TYPE_DMABUF 0x2
#define STREAM_MEM_HANDLE_TYPE_OPAQUE_WIN32 0x3
#define STREAM_MEM_HANDLE_TYPE_SHM 0x4
#define STREAM_FENCE_HANDLE_TYPE_OPAQUE_FD 0x10
#define STREAM_FENCE_HANDLE_TYPE_SYNC_FD 0x11
#define STREAM_FENCE_HANDLE_TYPE_OPAQUE_WIN32 0x12
struct stream_renderer_handle {
    int64_t os_handle;
    uint32_t handle_type;
};

struct stream_renderer_create_blob {
    uint32_t blob_mem;
    uint32_t blob_flags;
    uint64_t blob_id;
    uint64_t size;
};

#define STREAM_BLOB_MEM_GUEST 1
#define STREAM_BLOB_MEM_HOST3D 2
#define STREAM_BLOB_MEM_HOST3D_GUEST 3

#define STREAM_BLOB_FLAG_USE_MAPPABLE 1
#define STREAM_BLOB_FLAG_USE_SHAREABLE 2
#define STREAM_BLOB_FLAG_USE_CROSS_DEVICE 4
#define STREAM_BLOB_FLAG_CREATE_GUEST_HANDLE 8

VG_EXPORT int stream_renderer_create_blob(uint32_t ctx_id, uint32_t res_handle,
                                          const struct stream_renderer_create_blob *create_blob,
                                          const struct iovec *iovecs, uint32_t num_iovs,
                                          const struct stream_renderer_handle *handle);

VG_EXPORT int stream_renderer_export_blob(uint32_t res_handle,
                                          struct stream_renderer_handle *handle);

VG_EXPORT int stream_renderer_resource_map(uint32_t res_handle, void** hvaOut, uint64_t* sizeOut);
VG_EXPORT int stream_renderer_resource_unmap(uint32_t res_handle);

VG_EXPORT int stream_renderer_context_create(uint32_t ctx_id, uint32_t nlen, const char *name,
                                             uint32_t context_init);

VG_EXPORT int stream_renderer_context_create_fence(
    uint64_t fence_id, uint32_t ctx_id, uint8_t ring_idx);

// Platform resources and contexts support
#define STREAM_RENDERER_PLATFORM_RESOURCE_USE_MASK  0xF0
#define STREAM_RENDERER_PLATFORM_RESOURCE_TYPE_MASK 0x0F

// types
#define STREAM_RENDERER_PLATFORM_RESOURCE_TYPE_EGL_NATIVE_PIXMAP 0x01
#define STREAM_RENDERER_PLATFORM_RESOURCE_TYPE_EGL_IMAGE 0x02

// uses
#define STREAM_RENDERER_PLATFORM_RESOURCE_USE_PRESERVE 0x10

VG_EXPORT int stream_renderer_platform_import_resource(int res_handle, int res_info, void* resource);
VG_EXPORT int stream_renderer_platform_resource_info(int res_handle, int* width, int*  height, int* internal_format);
VG_EXPORT void* stream_renderer_platform_create_shared_egl_context(void);
VG_EXPORT int stream_renderer_platform_destroy_shared_egl_context(void*);

#define STREAM_RENDERER_MAP_CACHE_MASK      0x0f
#define STREAM_RENDERER_MAP_CACHE_NONE      0x00
#define STREAM_RENDERER_MAP_CACHE_CACHED    0x01
#define STREAM_RENDERER_MAP_CACHE_UNCACHED  0x02
#define STREAM_RENDERER_MAP_CACHE_WC        0x03
VG_EXPORT int stream_renderer_resource_map_info(uint32_t res_handle, uint32_t *map_info);

struct stream_renderer_vulkan_info {
    uint32_t memory_index;
    uint8_t device_uuid[16];
    uint8_t driver_uuid[16];
};

VG_EXPORT int stream_renderer_vulkan_info(uint32_t res_handle,
                                          struct stream_renderer_vulkan_info *vulkan_info);

// Parameters - data passed to initialize the renderer, with the goal of avoiding FFI breakages.
// To change the data a parameter is passing safely, you should create a new parameter and
// deprecate the old one. The old parameter may be removed after sufficient time.

// Reserved.
#define STREAM_RENDERER_PARAM_NULL 0

// User data, for custom use by renderer. An example is VirglCookie which includes a fence
// handler and render server descriptor.
#define STREAM_RENDERER_PARAM_USER_DATA 1

// Bitwise flags for the renderer.
#define STREAM_RENDERER_PARAM_RENDERER_FLAGS 2

// Reserved to replace write_fence / write_context_fence.
#define STREAM_RENDERER_PARAM_FENCE_CALLBACK 3

// Callback for writing a fence.
#define STREAM_RENDERER_PARAM_WRITE_FENCE_CALLBACK 4
typedef void (*stream_renderer_param_write_fence_callback)(void* user_data, uint32_t fence);

// Callback for writing a fence with context.
#define STREAM_RENDERER_PARAM_WRITE_CONTEXT_FENCE_CALLBACK 5
typedef void (*stream_renderer_param_write_context_fence_callback)(void* user_data, uint64_t fence,
                                                                   uint32_t ctx_id,
                                                                   uint8_t ring_idx);

// Window 0's width.
#define STREAM_RENDERER_PARAM_WIN0_WIDTH 6

// Window 0's height.
#define STREAM_RENDERER_PARAM_WIN0_HEIGHT 7

// External callbacks for tracking metrics.
// Separating each function to a parameter allows new functions to be added later.
#define STREAM_RENDERER_PARAM_METRICS_CALLBACK_ADD_INSTANT_EVENT 1024
typedef void (*stream_renderer_param_metrics_callback_add_instant_event)(int64_t event_code);

#define STREAM_RENDERER_PARAM_METRICS_CALLBACK_ADD_INSTANT_EVENT_WITH_DESCRIPTOR 1025
typedef void (*stream_renderer_param_metrics_callback_add_instant_event_with_descriptor)(
    int64_t event_code, int64_t descriptor);

#define STREAM_RENDERER_PARAM_METRICS_CALLBACK_ADD_INSTANT_EVENT_WITH_METRIC 1026
typedef void (*stream_renderer_param_metrics_callback_add_instant_event_with_metric)(
    int64_t event_code, int64_t metric_value);

#define STREAM_RENDERER_PARAM_METRICS_CALLBACK_ADD_VULKAN_OUT_OF_MEMORY_EVENT 1027
typedef void (*stream_renderer_param_metrics_callback_add_vulkan_out_of_memory_event)(
    int64_t result_code, uint32_t op_code, const char* function, uint32_t line,
    uint64_t allocation_size, bool is_host_side_result, bool is_allocation);

#define STREAM_RENDERER_PARAM_METRICS_CALLBACK_SET_ANNOTATION 1028
typedef void (*stream_renderer_param_metrics_callback_set_annotation)(const char* key,
                                                                      const char* value);

#define STREAM_RENDERER_PARAM_METRICS_CALLBACK_ABORT 1029
typedef void (*stream_renderer_param_metrics_callback_abort)();

// An entry in the stream renderer parameters list.
struct stream_renderer_param {
    // The key should be one of STREAM_RENDERER_PARAM_*
    uint64_t key;
    // The value can be either a uint64_t or cast to a pointer to a struct, depending on if the
    // parameter needs to pass data bigger than a single uint64_t.
    uint64_t value;
};

static_assert(sizeof(stream_renderer_param) == 16, "stream_renderer_param must be 16 bytes");
static_assert(offsetof(stream_renderer_param, key) == 0,
              "stream_renderer_param.key must be at offset 0");
static_assert(offsetof(stream_renderer_param, value) == 8,
              "stream_renderer_param.value must be at offset 8");

// Entry point for the stream renderer.
// Pass a list of parameters to configure the renderer. The available ones are listed above. If a
// parameter is not supported, the renderer will ignore it and warn in stderr.
// Return value of 0 indicates success, otherwise an error code is returned.
VG_EXPORT int stream_renderer_init(
    struct stream_renderer_param* stream_renderer_params,
    uint64_t num_params);

struct gfxstream_callbacks {
    /* Metrics callbacks */
    void (*add_instant_event)(int64_t event_code);
    void (*add_instant_event_with_descriptor)(int64_t event_code, int64_t descriptor);
    void (*add_instant_event_with_metric)(int64_t event_code, int64_t metric_value);
    void (*add_vulkan_out_of_memory_event)(int64_t result_code, uint32_t op_code,
                                           const char* function, uint32_t line,
                                           uint64_t allocation_size, bool is_host_side_result,
                                           bool is_allocation);
    void (*set_annotation)(const char* key, const char* value);
    void (*abort)();
};

// Deprecated, use stream_renderer_init instead.
VG_EXPORT void gfxstream_backend_init(
    uint32_t display_width,
    uint32_t display_height,
    uint32_t display_type,
    void* renderer_cookie,
    int renderer_flags,
    struct virgl_renderer_callbacks* virglrenderer_callbacks,
    struct gfxstream_callbacks* gfxstreamcallbacks);

VG_EXPORT void gfxstream_backend_setup_window(
    void* native_window_handle,
    int32_t window_x,
    int32_t window_y,
    int32_t window_width,
    int32_t window_height,
    int32_t fb_width,
    int32_t fb_height);

VG_EXPORT void gfxstream_backend_teardown(void);

// Get the gfxstream backend render information.
// example:
//      /* Get the render string size */
//      size_t size = 0
//      gfxstream_backend_getrender(nullptr, 0, &size);
//
//      /* add extra space for '\0' */
//      char * buf = malloc(size + 1);
//
//      /* Get the result render string */
//      gfxstream_backend_getrender(buf, size+1, nullptr);
//
// if bufSize is less or equal the render string length, only bufSize-1 char copied.
VG_EXPORT void gfxstream_backend_getrender(
    char* buf,
    size_t bufSize,
    size_t* size);

// A customization point that allows the downstream to call their own functions when
// gfxstream_backend_init is called.
void gfxstream_backend_init_product_override();

#else

#define VG_EXPORT

#endif // !VIRTIO_GOLDFISH_EXPORT_API

#ifdef __cplusplus
} // extern "C"
#endif

// based on VIRGL_RENDERER_USE* and friends
enum RendererFlags {
    GFXSTREAM_RENDERER_FLAGS_USE_EGL_BIT = 1 << 0,
    GFXSTREAM_RENDERER_FLAGS_THREAD_SYNC = 1 << 1,
    GFXSTREAM_RENDERER_FLAGS_USE_GLX_BIT = 1 << 2,
    GFXSTREAM_RENDERER_FLAGS_USE_SURFACELESS_BIT = 1 << 3,
    GFXSTREAM_RENDERER_FLAGS_USE_GLES_BIT = 1 << 4,
    GFXSTREAM_RENDERER_FLAGS_NO_VK_BIT = 1 << 5,  // for disabling vk
    GFXSTREAM_RENDERER_FLAGS_ENABLE_GLES31_BIT =
        1 << 9,  // disables the PlayStoreImage flag
    GFXSTREAM_RENDERER_FLAGS_GUEST_USES_ANGLE = 1 << 21,
    GFXSTREAM_RENDERER_FLAGS_VULKAN_NATIVE_SWAPCHAIN_BIT = 1 << 22,
    GFXSTREAM_RENDERER_FLAGS_ASYNC_FENCE_CB = 1 << 23,
};

#endif
