set(egl-translator-common-sources
    ClientAPIExts.cpp
    EglConfig.cpp
    EglContext.cpp
    EglDisplay.cpp
    EglGlobalInfo.cpp
    EglImp.cpp
    EglPbufferSurface.cpp
    EglSurface.cpp
    EglThreadInfo.cpp
    EglValidate.cpp
    EglWindowSurface.cpp
    ThreadInfo.cpp)
set(egl-translator-windows-sources
    CoreProfileConfigs_windows.cpp EglOsApi_egl.cpp EglOsApi_wgl.cpp)
set(egl-translator-darwin-sources
    EglOsApi_darwin.cpp EglOsApi_egl.cpp MacNative.m
    MacPixelFormatsAttribs.m)
set(egl-translator-linux-sources
    CoreProfileConfigs_linux.cpp EglOsApi_egl.cpp EglOsApi_glx.cpp)

if (WIN32)
    add_library(
        EGL_translator_static
        ${egl-translator-common-sources}
        ${egl-translator-windows-sources})
elseif (APPLE)
    add_library(
        EGL_translator_static
        ${egl-translator-common-sources}
        ${egl-translator-darwin-sources})
else()
    add_library(
        EGL_translator_static
        ${egl-translator-common-sources}
        ${egl-translator-linux-sources})
endif()

target_link_libraries(EGL_translator_static PUBLIC GLcommon apigen-codec-common)
target_link_libraries(EGL_translator_static PRIVATE gfxstream-base astc-codec)
target_link_libraries(EGL_translator_static PUBLIC GLES_CM_translator_static GLES_V2_translator_static)
target_compile_options(
    EGL_translator_static
    PRIVATE -fvisibility=hidden -Wno-inconsistent-missing-override
    -Wno-macro-redefined)

if (APPLE)
    target_compile_options(EGL_translator_static PRIVATE -Wno-deprecated-declarations)
    target_compile_options(EGL_translator_static PRIVATE "-Wno-deprecated-declarations")
endif()

if (WIN32)
    target_link_libraries(EGL_translator_static PUBLIC "gdi32::gdi32")
elseif (APPLE)
    target_link_libraries(EGL_translator_static PUBLIC "-framework AppKit")
else()
    target_link_libraries(EGL_translator_static PUBLIC "-lGL -lX11 -ldl -lpthread")
endif()
