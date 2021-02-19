// Generated Code - DO NOT EDIT !!
// generated by 'emugen'
#ifndef __renderControl_server_proc_t_h
#define __renderControl_server_proc_t_h



#include "renderControl_types.h"
#ifdef _MSC_VER
#include <stdint.h>
#endif
#ifndef renderControl_APIENTRY
#define renderControl_APIENTRY 
#endif
typedef GLint (renderControl_APIENTRY *rcGetRendererVersion_server_proc_t) ();
typedef EGLint (renderControl_APIENTRY *rcGetEGLVersion_server_proc_t) (EGLint*, EGLint*);
typedef EGLint (renderControl_APIENTRY *rcQueryEGLString_server_proc_t) (EGLenum, void*, EGLint);
typedef EGLint (renderControl_APIENTRY *rcGetGLString_server_proc_t) (EGLenum, void*, EGLint);
typedef EGLint (renderControl_APIENTRY *rcGetNumConfigs_server_proc_t) (uint32_t*);
typedef EGLint (renderControl_APIENTRY *rcGetConfigs_server_proc_t) (uint32_t, GLuint*);
typedef EGLint (renderControl_APIENTRY *rcChooseConfig_server_proc_t) (EGLint*, uint32_t, uint32_t*, uint32_t);
typedef EGLint (renderControl_APIENTRY *rcGetFBParam_server_proc_t) (EGLint);
typedef uint32_t (renderControl_APIENTRY *rcCreateContext_server_proc_t) (uint32_t, uint32_t, uint32_t);
typedef void (renderControl_APIENTRY *rcDestroyContext_server_proc_t) (uint32_t);
typedef uint32_t (renderControl_APIENTRY *rcCreateWindowSurface_server_proc_t) (uint32_t, uint32_t, uint32_t);
typedef void (renderControl_APIENTRY *rcDestroyWindowSurface_server_proc_t) (uint32_t);
typedef uint32_t (renderControl_APIENTRY *rcCreateColorBuffer_server_proc_t) (uint32_t, uint32_t, GLenum);
typedef void (renderControl_APIENTRY *rcOpenColorBuffer_server_proc_t) (uint32_t);
typedef void (renderControl_APIENTRY *rcCloseColorBuffer_server_proc_t) (uint32_t);
typedef void (renderControl_APIENTRY *rcSetWindowColorBuffer_server_proc_t) (uint32_t, uint32_t);
typedef int (renderControl_APIENTRY *rcFlushWindowColorBuffer_server_proc_t) (uint32_t);
typedef EGLint (renderControl_APIENTRY *rcMakeCurrent_server_proc_t) (uint32_t, uint32_t, uint32_t);
typedef void (renderControl_APIENTRY *rcFBPost_server_proc_t) (uint32_t);
typedef void (renderControl_APIENTRY *rcFBSetSwapInterval_server_proc_t) (EGLint);
typedef void (renderControl_APIENTRY *rcBindTexture_server_proc_t) (uint32_t);
typedef void (renderControl_APIENTRY *rcBindRenderbuffer_server_proc_t) (uint32_t);
typedef EGLint (renderControl_APIENTRY *rcColorBufferCacheFlush_server_proc_t) (uint32_t, EGLint, int);
typedef void (renderControl_APIENTRY *rcReadColorBuffer_server_proc_t) (uint32_t, GLint, GLint, GLint, GLint, GLenum, GLenum, void*);
typedef int (renderControl_APIENTRY *rcUpdateColorBuffer_server_proc_t) (uint32_t, GLint, GLint, GLint, GLint, GLenum, GLenum, void*);
typedef int (renderControl_APIENTRY *rcOpenColorBuffer2_server_proc_t) (uint32_t);
typedef uint32_t (renderControl_APIENTRY *rcCreateClientImage_server_proc_t) (uint32_t, EGLenum, GLuint);
typedef int (renderControl_APIENTRY *rcDestroyClientImage_server_proc_t) (uint32_t);
typedef void (renderControl_APIENTRY *rcSelectChecksumHelper_server_proc_t) (uint32_t, uint32_t);
typedef void (renderControl_APIENTRY *rcCreateSyncKHR_server_proc_t) (EGLenum, EGLint*, uint32_t, int, uint64_t*, uint64_t*);
typedef EGLint (renderControl_APIENTRY *rcClientWaitSyncKHR_server_proc_t) (uint64_t, EGLint, uint64_t);
typedef void (renderControl_APIENTRY *rcFlushWindowColorBufferAsync_server_proc_t) (uint32_t);
typedef int (renderControl_APIENTRY *rcDestroySyncKHR_server_proc_t) (uint64_t);
typedef void (renderControl_APIENTRY *rcSetPuid_server_proc_t) (uint64_t);
typedef int (renderControl_APIENTRY *rcUpdateColorBufferDMA_server_proc_t) (uint32_t, GLint, GLint, GLint, GLint, GLenum, GLenum, void*, uint32_t);
typedef uint32_t (renderControl_APIENTRY *rcCreateColorBufferDMA_server_proc_t) (uint32_t, uint32_t, GLenum, int);
typedef void (renderControl_APIENTRY *rcWaitSyncKHR_server_proc_t) (uint64_t, EGLint);
typedef GLint (renderControl_APIENTRY *rcCompose_server_proc_t) (uint32_t, void*);
typedef int (renderControl_APIENTRY *rcCreateDisplay_server_proc_t) (uint32_t*);
typedef int (renderControl_APIENTRY *rcDestroyDisplay_server_proc_t) (uint32_t);
typedef int (renderControl_APIENTRY *rcSetDisplayColorBuffer_server_proc_t) (uint32_t, uint32_t);
typedef int (renderControl_APIENTRY *rcGetDisplayColorBuffer_server_proc_t) (uint32_t, uint32_t*);
typedef int (renderControl_APIENTRY *rcGetColorBufferDisplay_server_proc_t) (uint32_t, uint32_t*);
typedef int (renderControl_APIENTRY *rcGetDisplayPose_server_proc_t) (uint32_t, GLint*, GLint*, uint32_t*, uint32_t*);
typedef int (renderControl_APIENTRY *rcSetDisplayPose_server_proc_t) (uint32_t, GLint, GLint, uint32_t, uint32_t);
typedef GLint (renderControl_APIENTRY *rcSetColorBufferVulkanMode_server_proc_t) (uint32_t, uint32_t);
typedef void (renderControl_APIENTRY *rcReadColorBufferYUV_server_proc_t) (uint32_t, GLint, GLint, GLint, GLint, void*, uint32_t);
typedef int (renderControl_APIENTRY *rcIsSyncSignaled_server_proc_t) (uint64_t);
typedef void (renderControl_APIENTRY *rcCreateColorBufferWithHandle_server_proc_t) (uint32_t, uint32_t, GLenum, uint32_t);
typedef uint32_t (renderControl_APIENTRY *rcCreateBuffer_server_proc_t) (uint32_t);
typedef void (renderControl_APIENTRY *rcCloseBuffer_server_proc_t) (uint32_t);
typedef GLint (renderControl_APIENTRY *rcSetColorBufferVulkanMode2_server_proc_t) (uint32_t, uint32_t, uint32_t);
typedef int (renderControl_APIENTRY *rcMapGpaToBufferHandle_server_proc_t) (uint32_t, uint64_t);
typedef uint32_t (renderControl_APIENTRY *rcCreateBuffer2_server_proc_t) (uint64_t, uint32_t);
typedef int (renderControl_APIENTRY *rcMapGpaToBufferHandle2_server_proc_t) (uint32_t, uint64_t, uint64_t);
typedef void (renderControl_APIENTRY *rcFlushWindowColorBufferAsyncWithFrameNumber_server_proc_t) (uint32_t, uint32_t);
typedef void (renderControl_APIENTRY *rcSetTracingForPuid_server_proc_t) (uint64_t, uint32_t, uint64_t);
typedef void (renderControl_APIENTRY *rcMakeCurrentAsync_server_proc_t) (uint32_t, uint32_t, uint32_t);
typedef void (renderControl_APIENTRY *rcComposeAsync_server_proc_t) (uint32_t, void*);
typedef void (renderControl_APIENTRY *rcDestroySyncKHRAsync_server_proc_t) (uint64_t);
typedef GLint (renderControl_APIENTRY *rcComposeWithoutPost_server_proc_t) (uint32_t, void*);
typedef void (renderControl_APIENTRY *rcComposeAsyncWithoutPost_server_proc_t) (uint32_t, void*);


#endif
