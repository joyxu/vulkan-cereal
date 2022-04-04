/*
* Copyright (C) 2016 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "YUVConverter.h"

#include "DispatchTables.h"
#include "host-common/feature_control.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define FATAL(fmt,...) do { \
    fprintf(stderr, "%s: FATAL: " fmt "\n", __func__, ##__VA_ARGS__); \
    assert(false); \
} while(0)

#define YUV_CONVERTER_DEBUG 0

#if YUV_CONVERTER_DEBUG
#define YUV_DEBUG_LOG(fmt, ...)                                                     \
    fprintf(stderr, "yuv-converter: %s:%d " fmt "\n", __func__, __LINE__, \
            ##__VA_ARGS__);
#else
#define YUV_DEBUG_LOG(fmt, ...)
#endif

bool isInterleaved(FrameworkFormat format) {
    switch (format) {
    case FRAMEWORK_FORMAT_NV12:
        return true;
    case FRAMEWORK_FORMAT_YUV_420_888:
        return feature_is_enabled(kFeature_YUV420888toNV21);
    case FRAMEWORK_FORMAT_YV12:
        return false;
    default:
        FATAL("Invalid for format:%d", format);
        return false;
    }
}

enum class YUVInterleaveDirection {
    VU = 0,
    UV = 1,
};

YUVInterleaveDirection getInterleaveDirection(FrameworkFormat format) {
    if (!isInterleaved(format)) {
        FATAL("Format:%d not interleaved", format);
    }

    switch (format) {
    case FRAMEWORK_FORMAT_NV12:
        return YUVInterleaveDirection::UV;
    case FRAMEWORK_FORMAT_YUV_420_888:
        if (feature_is_enabled(kFeature_YUV420888toNV21)) {
            return YUVInterleaveDirection::VU;
        }
        FATAL("Format:%d not interleaved", format);
        return YUVInterleaveDirection::UV;
    case FRAMEWORK_FORMAT_YV12:
    default:
        FATAL("Format:%d not interleaved", format);
        return YUVInterleaveDirection::UV;
    }
}

GLint getGlTextureFormat(FrameworkFormat format, YUVPlane plane) {
    switch (format) {
    case FRAMEWORK_FORMAT_YV12:
        switch (plane) {
        case YUVPlane::Y:
        case YUVPlane::U:
        case YUVPlane::V:
            return GL_R8;
        case YUVPlane::UV:
            FATAL("Invalid plane:%d for format:%d", plane, format);
            return 0;
        }
    case FRAMEWORK_FORMAT_YUV_420_888:
        if (feature_is_enabled(kFeature_YUV420888toNV21)) {
            switch (plane) {
            case YUVPlane::Y:
                return GL_R8;
            case YUVPlane::UV:
                return GL_RG8;
            case YUVPlane::U:
            case YUVPlane::V:
                FATAL("Invalid plane:%d for format:%d", plane, format);
                return 0;
            }
        } else {
            switch (plane) {
            case YUVPlane::Y:
            case YUVPlane::U:
            case YUVPlane::V:
                return GL_R8;
            case YUVPlane::UV:
                FATAL("Invalid plane:%d for format:%d", plane, format);
                return 0;
            }
        }
    case FRAMEWORK_FORMAT_NV12:
        switch (plane) {
        case YUVPlane::Y:
            return GL_R8;
        case YUVPlane::UV:
            return GL_RG8;
        case YUVPlane::U:
        case YUVPlane::V:
            FATAL("Invalid plane:%d for format:%d", plane, format);
            return 0;
        }
    default:
        FATAL("Invalid format:%d", format);
        return 0;
    }
}

GLenum getGlPixelFormat(FrameworkFormat format, YUVPlane plane) {
    switch (format) {
    case FRAMEWORK_FORMAT_YV12:
        switch (plane) {
        case YUVPlane::Y:
        case YUVPlane::U:
        case YUVPlane::V:
            return GL_RED;
        case YUVPlane::UV:
            FATAL("Invalid plane:%d for format:%d", plane, format);
            return 0;
        }
    case FRAMEWORK_FORMAT_YUV_420_888:
        if (feature_is_enabled(kFeature_YUV420888toNV21)) {
            switch (plane) {
            case YUVPlane::Y:
                return GL_RED;
            case YUVPlane::UV:
                return GL_RG;
            case YUVPlane::U:
            case YUVPlane::V:
                FATAL("Invalid plane:%d for format:%d", plane, format);
                return 0;
            }
        } else {
            switch (plane) {
            case YUVPlane::Y:
            case YUVPlane::U:
            case YUVPlane::V:
                return GL_RED;
            case YUVPlane::UV:
                FATAL("Invalid plane:%d for format:%d", plane, format);
                return 0;
            }
        }
    case FRAMEWORK_FORMAT_NV12:
        switch (plane) {
        case YUVPlane::Y:
            return GL_RED;
        case YUVPlane::UV:
            return GL_RG;
        case YUVPlane::U:
        case YUVPlane::V:
            FATAL("Invalid plane:%d for format:%d", plane, format);
            return 0;
        }
    default:
        FATAL("Invalid format:%d", format);
        return 0;
    }
}

GLsizei getGlPixelType(FrameworkFormat format, YUVPlane plane) {
    switch (format) {
    case FRAMEWORK_FORMAT_YV12:
        switch (plane) {
        case YUVPlane::Y:
        case YUVPlane::U:
        case YUVPlane::V:
            return GL_UNSIGNED_BYTE;
        case YUVPlane::UV:
            FATAL("Invalid plane:%d for format:%d", plane, format);
            return 0;
        }
    case FRAMEWORK_FORMAT_YUV_420_888:
        if (feature_is_enabled(kFeature_YUV420888toNV21)) {
            switch (plane) {
            case YUVPlane::Y:
            case YUVPlane::UV:
                return GL_UNSIGNED_BYTE;
            case YUVPlane::U:
            case YUVPlane::V:
                FATAL("Invalid plane:%d for format:%d", plane, format);
                return 0;
            }
        } else {
            switch (plane) {
            case YUVPlane::Y:
            case YUVPlane::U:
            case YUVPlane::V:
                return GL_UNSIGNED_BYTE;
            case YUVPlane::UV:
                FATAL("Invalid plane:%d for format:%d", plane, format);
                return 0;
            }
        }
    case FRAMEWORK_FORMAT_NV12:
        switch (plane) {
        case YUVPlane::Y:
        case YUVPlane::UV:
            return GL_UNSIGNED_BYTE;
        case YUVPlane::U:
        case YUVPlane::V:
            FATAL("Invalid plane:%d for format:%d", plane, format);
            return 0;
        }
    default:
        FATAL("Invalid format:%d", format);
        return 0;
    }
}

// NV12 and YUV420 are all packed
static void NV12ToYUV420PlanarInPlaceConvert(int nWidth,
                                             int nHeight,
                                             uint8_t* pFrame,
                                             uint8_t* pQuad) {
    std::vector<uint8_t> tmp;
    if (pQuad == nullptr) {
        tmp.resize(nWidth * nHeight / 4);
        pQuad = tmp.data();
    }
    int nPitch = nWidth;
    uint8_t *puv = pFrame + nPitch * nHeight, *pu = puv,
            *pv = puv + nPitch * nHeight / 4;
    for (int y = 0; y < nHeight / 2; y++) {
        for (int x = 0; x < nWidth / 2; x++) {
            pu[y * nPitch / 2 + x] = puv[y * nPitch + x * 2];
            pQuad[y * nWidth / 2 + x] = puv[y * nPitch + x * 2 + 1];
        }
    }
    memcpy(pv, pQuad, nWidth * nHeight / 4);
}

inline uint32_t alignToPower2(uint32_t val, uint32_t align) {
    return (val + (align - 1)) & ~(align - 1);
}

// getYUVOffsets(), given a YUV-formatted buffer that is arranged
// according to the spec
// https://developer.android.com/reference/android/graphics/ImageFormat.html#YUV
// In particular, Android YUV widths are aligned to 16 pixels.
// Inputs:
// |yv12|: the YUV-formatted buffer
// Outputs:
// |yOffset|: offset into |yv12| of the start of the Y component
// |uOffset|: offset into |yv12| of the start of the U component
// |vOffset|: offset into |yv12| of the start of the V component
static void getYUVOffsets(int width,
                          int height,
                          FrameworkFormat format,
                          uint32_t* yOffset,
                          uint32_t* uOffset,
                          uint32_t* vOffset,
                          uint32_t* yWidth,
                          uint32_t* cWidth) {
    uint32_t yStride, cStride, cHeight, cSize;
    switch (format) {
    case FRAMEWORK_FORMAT_YV12:
        // Luma stride is 32 bytes aligned.
        yStride = alignToPower2(width, 32);
        // Chroma stride is 16 bytes aligned.
        cStride = alignToPower2(yStride, 16);
        cHeight = height / 2;
        cSize = cStride * cHeight;
        *yOffset = 0;
        *vOffset = yStride * height;
        *uOffset = (*vOffset) + cSize;
        *yWidth = yStride;
        *cWidth = cStride;
        break;
    case FRAMEWORK_FORMAT_YUV_420_888:
        if (feature_is_enabled(kFeature_YUV420888toNV21)) {
            yStride = width;
            cStride = yStride;
            cHeight = height / 2;
            *yOffset = 0;
            *vOffset = yStride * height;
            *uOffset = (*vOffset) + 1;
            *yWidth = yStride;
            *cWidth = cStride / 2;
        } else {
            yStride = width;
            cStride = yStride / 2;
            cHeight = height / 2;
            cSize = cStride * cHeight;
            *yOffset = 0;
            *uOffset = yStride * height;
            *vOffset = (*uOffset) + cSize;
            *yWidth = yStride;
            *cWidth = cStride;
        }
        break;
    case FRAMEWORK_FORMAT_NV12:
        yStride = width;
        cStride = yStride;
        cHeight = height / 2;
        cSize = cStride * cHeight;
        *yOffset = 0;
        *uOffset = yStride * height;
        *vOffset = (*uOffset) + 1;
        *yWidth = yStride;
        *cWidth = cStride / 2;
        break;
    case FRAMEWORK_FORMAT_GL_COMPATIBLE:
        FATAL("Input not a YUV format! (FRAMEWORK_FORMAT_GL_COMPATIBLE)");
    default:
        FATAL("Unknown format: 0x%x", format);
    }
}

// createYUVGLTex() allocates GPU memory that is enough
// to hold the raw data of the YV12 buffer.
// The memory is in the form of an OpenGL texture
// with one component (GL_LUMINANCE) and
// of type GL_UNSIGNED_BYTE.
// In order to process all Y, U, V components
// simultaneously in conversion, the simple thing to do
// is to use multiple texture units, hence
// the |textureUnit| argument.
// Returns a new OpenGL texture object in |outTextureName|
// that is to be cleaned up by the caller.
void YUVConverter::createYUVGLTex(GLenum textureUnit,
                                  GLsizei width,
                                  GLsizei height,
                                  GLuint* outTextureName,
                                  bool uvInterleaved) {
    assert(outTextureName);

    s_gles2.glActiveTexture(textureUnit);
    s_gles2.glGenTextures(1, outTextureName);
    s_gles2.glBindTexture(GL_TEXTURE_2D, *outTextureName);
    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GLint unprevAlignment = 0;
    s_gles2.glGetIntegerv(GL_UNPACK_ALIGNMENT, &unprevAlignment);
    s_gles2.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (uvInterleaved) {
        s_gles2.glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA,
                             width, height, 0,
                             GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,
                             NULL);
    } else {
        s_gles2.glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                             width, height, 0,
                             GL_LUMINANCE, GL_UNSIGNED_BYTE,
                             NULL);
    }
    s_gles2.glPixelStorei(GL_UNPACK_ALIGNMENT, unprevAlignment);
    s_gles2.glActiveTexture(GL_TEXTURE0);
}

static void readYUVTex(GLuint tex, void* pixels, bool uvInterleaved) {
    GLuint prevTexture = 0;
    s_gles2.glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&prevTexture);
    s_gles2.glBindTexture(GL_TEXTURE_2D, tex);
    GLint prevAlignment = 0;
    s_gles2.glGetIntegerv(GL_PACK_ALIGNMENT, &prevAlignment);
    s_gles2.glPixelStorei(GL_PACK_ALIGNMENT, 1);
    if (uvInterleaved) {
        if (s_gles2.glGetTexImage) {
            s_gles2.glGetTexImage(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA,
                                  GL_UNSIGNED_BYTE, pixels);
        } else {
            YUV_DEBUG_LOG("empty glGetTexImage");
        }
    } else {
        if (s_gles2.glGetTexImage) {
            s_gles2.glGetTexImage(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                                  GL_UNSIGNED_BYTE, pixels);
        } else {
            YUV_DEBUG_LOG("empty glGetTexImage");
        }
    }
    s_gles2.glPixelStorei(GL_PACK_ALIGNMENT, prevAlignment);
    s_gles2.glBindTexture(GL_TEXTURE_2D, prevTexture);
}

// subUpdateYUVGLTex() updates a given YUV texture
// at the coordinates (x, y, width, height),
// with the raw YUV data in |pixels|.
// We cannot view the result properly until
// after conversion; this is to be used only
// as input to the conversion shader.
static void subUpdateYUVGLTex(GLenum textureUnit,
                              GLuint tex,
                              int x, int y, int width, int height,
                              const void* pixels, bool uvInterleaved) {
    s_gles2.glActiveTexture(textureUnit);
    s_gles2.glBindTexture(GL_TEXTURE_2D, tex);
    GLint unprevAlignment = 0;
    s_gles2.glGetIntegerv(GL_UNPACK_ALIGNMENT, &unprevAlignment);
    s_gles2.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (uvInterleaved) {
        s_gles2.glTexSubImage2D(GL_TEXTURE_2D, 0,
                                x, y, width, height,
                                GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,
                                pixels);
    } else {
        s_gles2.glTexSubImage2D(GL_TEXTURE_2D, 0,
                                x, y, width, height,
                                GL_LUMINANCE, GL_UNSIGNED_BYTE,
                                pixels);
    }
    s_gles2.glPixelStorei(GL_UNPACK_ALIGNMENT, unprevAlignment);

    s_gles2.glActiveTexture(GL_TEXTURE0);
}

// createYUVGLShader() defines the vertex/fragment
// shader that does the actual work of converting
// YUV to RGB. The resulting program is stored in |outProgram|.
static void createYUVGLShader(GLuint* outProgram,
                              GLint* outUniformLocYWidthCutoff,
                              GLint* outUniformLocCWidthCutoff,
                              GLint* outUniformLocSamplerY,
                              GLint* outUniformLocSamplerU,
                              GLint* outUniformLocSamplerV,
                              GLint* outAttributeLocTexCoord,
                              GLint* outAttributeLocPos) {
    assert(outProgram);

    static const char kVShader[] = R"(
precision highp float;
attribute mediump vec4 position;
attribute highp vec2 inCoord;
varying highp vec2 outCoord;
void main(void) {
  gl_Position = position;
  outCoord = inCoord;
}
    )";
    const GLchar* const kVShaders =
        static_cast<const GLchar*>(kVShader);

    // Based on:
    // http://stackoverflow.com/questions/11093061/yv12-to-rgb-using-glsl-in-ios-result-image-attached
    // + account for 16-pixel alignment using |yWidthCutoff| / |cWidthCutoff|
    // + use conversion matrix in
    // frameworks/av/media/libstagefright/colorconversion/ColorConverter.cpp (YUV420p)
    // + more precision from
    // https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
    static const char kFShader[] = R"(
precision highp float;
varying highp vec2 outCoord;
uniform highp float yWidthCutoff;
uniform highp float cWidthCutoff;
uniform sampler2D ysampler;
uniform sampler2D usampler;
uniform sampler2D vsampler;
void main(void) {
    highp vec2 cutoffCoordsY;
    highp vec2 cutoffCoordsC;
    highp vec3 yuv;
    highp vec3 rgb;
    cutoffCoordsY.x = outCoord.x * yWidthCutoff;
    cutoffCoordsY.y = outCoord.y;
    cutoffCoordsC.x = outCoord.x * cWidthCutoff;
    cutoffCoordsC.y = outCoord.y;
    yuv[0] = texture2D(ysampler, cutoffCoordsY).r - 0.0625;
    yuv[1] = 0.96*(texture2D(usampler, cutoffCoordsC).r - 0.5);
    yuv[2] = texture2D(vsampler, cutoffCoordsC).r - 0.5;
    highp float yscale = 1.1643835616438356;
    rgb = mat3(yscale,                           yscale,            yscale,
               0,                  -0.39176229009491365, 2.017232142857143,
               1.5960267857142856, -0.8129676472377708,                  0) * yuv;
    gl_FragColor = vec4(rgb, 1);
}
    )";

    const GLchar* const kFShaders =
        static_cast<const GLchar*>(kFShader);

    GLuint vshader = s_gles2.glCreateShader(GL_VERTEX_SHADER);
    GLuint fshader = s_gles2.glCreateShader(GL_FRAGMENT_SHADER);

    const GLint vtextLen = strlen(kVShader);
    const GLint ftextLen = strlen(kFShaders);
    s_gles2.glShaderSource(vshader, 1, &kVShaders, &vtextLen);
    s_gles2.glShaderSource(fshader, 1, &kFShaders, &ftextLen);
    s_gles2.glCompileShader(vshader);
    s_gles2.glCompileShader(fshader);

    *outProgram = s_gles2.glCreateProgram();
    s_gles2.glAttachShader(*outProgram, vshader);
    s_gles2.glAttachShader(*outProgram, fshader);
    s_gles2.glLinkProgram(*outProgram);

    *outUniformLocYWidthCutoff = s_gles2.glGetUniformLocation(*outProgram, "yWidthCutoff");
    *outUniformLocCWidthCutoff = s_gles2.glGetUniformLocation(*outProgram, "cWidthCutoff");
    *outUniformLocSamplerY = s_gles2.glGetUniformLocation(*outProgram, "ysampler");
    *outUniformLocSamplerU = s_gles2.glGetUniformLocation(*outProgram, "usampler");
    *outUniformLocSamplerV = s_gles2.glGetUniformLocation(*outProgram, "vsampler");
    *outAttributeLocPos = s_gles2.glGetAttribLocation(*outProgram, "position");
    *outAttributeLocTexCoord = s_gles2.glGetAttribLocation(*outProgram, "inCoord");

    s_gles2.glDeleteShader(vshader);
    s_gles2.glDeleteShader(fshader);
}

static void createYUVInterleavedGLShader(GLuint* outProgram,
                                         GLint* outUniformLocYWidthCutoff,
                                         GLint* outUniformLocCWidthCutoff,
                                         GLint* outUniformLocSamplerY,
                                         GLint* outUniformLocSamplerVU,
                                         GLint* outAttributeLocTexCoord,
                                         GLint* outAttributeLocPos,
                                         YUVInterleaveDirection interleaveDir) {
    assert(outProgram);

    static const char kVShader[] = R"(
precision highp float;
attribute mediump vec4 position;
attribute highp vec2 inCoord;
varying highp vec2 outCoord;
void main(void) {
  gl_Position = position;
  outCoord = inCoord;
}
    )";
    const GLchar* const kVShaders =
        static_cast<const GLchar*>(kVShader);

    // Based on:
    // https://stackoverflow.com/questions/22456884/how-to-render-androids-yuv-nv21-camera-image-on-the-background-in-libgdx-with-o
    // + account for 16-pixel alignment using |yWidthCutoff| / |cWidthCutoff|
    // + use conversion matrix in
    // frameworks/av/media/libstagefright/colorconversion/ColorConverter.cpp (YUV420p)
    // + more precision from
    // https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
    // UV texture is (width/2*height/2) in size (downsampled by 2 in
    // both dimensions, each pixel corresponds to 4 pixels of the Y channel)
    // and each pixel is two bytes. By setting GL_LUMINANCE_ALPHA, OpenGL
    // puts first byte (V) into R,G and B components and of the texture
    // and the second byte (U) into the A component of the texture. That's
    // why we find U and V at A and R respectively in the fragment shader code.
    // Note that we could have also found V at G or B as well.
    static const char kFShaderVu[] = R"(
precision highp float;
varying highp vec2 outCoord;
uniform highp float yWidthCutoff;
uniform highp float cWidthCutoff;
uniform sampler2D ysampler;
uniform sampler2D vusampler;
void main(void) {
    highp vec2 cutoffCoordsY;
    highp vec2 cutoffCoordsC;
    highp vec3 yuv;
    highp vec3 rgb;
    cutoffCoordsY.x = outCoord.x * yWidthCutoff;
    cutoffCoordsY.y = outCoord.y;
    cutoffCoordsC.x = outCoord.x * cWidthCutoff;
    cutoffCoordsC.y = outCoord.y;
    yuv[0] = texture2D(ysampler, cutoffCoordsY).r - 0.0625;
    yuv[1] = 0.96 * (texture2D(vusampler, cutoffCoordsC).a - 0.5);
    yuv[2] = texture2D(vusampler, cutoffCoordsC).r - 0.5;
    highp float yscale = 1.1643835616438356;
    rgb = mat3(yscale,                           yscale,            yscale,
               0,                  -0.39176229009491365, 2.017232142857143,
               1.5960267857142856, -0.8129676472377708,                  0) * yuv;
    gl_FragColor = vec4(rgb, 1);
}
    )";

    static const char kFShaderUv[] = R"(
precision highp float;
varying highp vec2 outCoord;
uniform highp float yWidthCutoff;
uniform highp float cWidthCutoff;
uniform sampler2D ysampler;
uniform sampler2D uvsampler;
void main(void) {
    highp vec2 cutoffCoordsY;
    highp vec2 cutoffCoordsC;
    highp vec3 yuv;
    highp vec3 rgb;
    cutoffCoordsY.x = outCoord.x * yWidthCutoff;
    cutoffCoordsY.y = outCoord.y;
    cutoffCoordsC.x = outCoord.x * cWidthCutoff;
    cutoffCoordsC.y = outCoord.y;
    yuv[0] = texture2D(ysampler, cutoffCoordsY).r - 0.0625;
    yuv[1] = 0.96 * (texture2D(uvsampler, cutoffCoordsC).r - 0.5);
    yuv[2] = (texture2D(uvsampler, cutoffCoordsC).a - 0.5);
    highp float yscale = 1.1643835616438356;
    rgb = mat3(yscale,                           yscale,            yscale,
               0,                  -0.39176229009491365, 2.017232142857143,
               1.5960267857142856, -0.8129676472377708,                  0) * yuv;
    gl_FragColor = vec4(rgb, 1);
}
    )";

    const GLchar* const kFShaders =
        interleaveDir == YUVInterleaveDirection::VU ? kFShaderVu : kFShaderUv;

    GLuint vshader = s_gles2.glCreateShader(GL_VERTEX_SHADER);
    GLuint fshader = s_gles2.glCreateShader(GL_FRAGMENT_SHADER);

    const GLint vtextLen = strlen(kVShader);
    const GLint ftextLen = strlen(kFShaders);
    s_gles2.glShaderSource(vshader, 1, &kVShaders, &vtextLen);
    s_gles2.glShaderSource(fshader, 1, &kFShaders, &ftextLen);
    s_gles2.glCompileShader(vshader);
    s_gles2.glCompileShader(fshader);

    *outProgram = s_gles2.glCreateProgram();
    s_gles2.glAttachShader(*outProgram, vshader);
    s_gles2.glAttachShader(*outProgram, fshader);
    s_gles2.glLinkProgram(*outProgram);

    *outUniformLocYWidthCutoff = s_gles2.glGetUniformLocation(*outProgram, "yWidthCutoff");
    *outUniformLocCWidthCutoff = s_gles2.glGetUniformLocation(*outProgram, "cWidthCutoff");
    *outUniformLocSamplerY = s_gles2.glGetUniformLocation(*outProgram, "ysampler");
    *outUniformLocSamplerVU = s_gles2.glGetUniformLocation(
            *outProgram, interleaveDir == YUVInterleaveDirection::VU ? "vusampler" : "uvsampler");
    *outAttributeLocPos = s_gles2.glGetAttribLocation(*outProgram, "position");
    *outAttributeLocTexCoord = s_gles2.glGetAttribLocation(*outProgram, "inCoord");

    s_gles2.glDeleteShader(vshader);
    s_gles2.glDeleteShader(fshader);
}
// When converting YUV to RGB with shaders,
// we are using the OpenGL graphics pipeline to do compute,
// so we need to express the place to store the result
// with triangles and draw calls.
// createYUVGLFullscreenQuad() defines a fullscreen quad
// with position and texture coordinates.
// The quad will be textured with the resulting RGB colors,
// and we will read back the pixels from the framebuffer
// to retrieve our RGB result.
static void createYUVGLFullscreenQuad(GLuint* outVertexBuffer,
                                      GLuint* outIndexBuffer) {
    assert(outVertexBuffer);
    assert(outIndexBuffer);

    s_gles2.glGenBuffers(1, outVertexBuffer);
    s_gles2.glGenBuffers(1, outIndexBuffer);

    static const float kVertices[] = {
        +1, -1, +0, +1, +0,
        +1, +1, +0, +1, +1,
        -1, +1, +0, +0, +1,
        -1, -1, +0, +0, +0,
    };

    static const GLubyte kIndices[] = { 0, 1, 2, 2, 3, 0 };

    s_gles2.glBindBuffer(GL_ARRAY_BUFFER, *outVertexBuffer);
    s_gles2.glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices,
                         GL_STATIC_DRAW);
    s_gles2.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *outIndexBuffer);
    s_gles2.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices,
                         GL_STATIC_DRAW);
}

// doYUVConversionDraw() does the actual work of setting up
// and submitting draw commands to the GPU.
// It uses the textures, shaders, and fullscreen quad defined above
// and executes the pipeline on them.
// Note, however, that it is up to the caller to dig out
// the result of the draw.
static void doYUVConversionDraw(GLuint program,
                                GLint uniformLocYWidthCutoff,
                                GLint uniformLocCWidthCutoff,
                                GLint uniformLocYSampler,
                                GLint uniformLocUSampler,
                                GLint uniformLocVSampler,
                                GLint uniformLocVUSampler,
                                GLint attributeLocTexCoord,
                                GLint attributeLocPos,
                                GLuint quadVertexBuffer,
                                GLuint quadIndexBuffer,
                                int width, int ywidth,
                                int halfwidth, int cwidth,
                                float yWidthCutoff,
                                float cWidthCutoff,
                                bool uvInterleaved) {

    const GLsizei kVertexAttribStride = 5 * sizeof(GL_FLOAT);
    const GLvoid* kVertexAttribPosOffset = (GLvoid*)0;
    const GLvoid* kVertexAttribCoordOffset = (GLvoid*)(3 * sizeof(GL_FLOAT));

    s_gles2.glUseProgram(program);

    s_gles2.glUniform1f(uniformLocYWidthCutoff, yWidthCutoff);
    s_gles2.glUniform1f(uniformLocCWidthCutoff, cWidthCutoff);

    s_gles2.glUniform1i(uniformLocYSampler, 0);
    if (uvInterleaved) {
        s_gles2.glUniform1i(uniformLocVUSampler, 1);
    } else {
        s_gles2.glUniform1i(uniformLocUSampler, 1);
        s_gles2.glUniform1i(uniformLocVSampler, 2);
    }

    s_gles2.glBindBuffer(GL_ARRAY_BUFFER, quadVertexBuffer);
    s_gles2.glEnableVertexAttribArray(attributeLocPos);
    s_gles2.glEnableVertexAttribArray(attributeLocTexCoord);

    s_gles2.glVertexAttribPointer(attributeLocPos, 3, GL_FLOAT, false,
                                  kVertexAttribStride,
                                  kVertexAttribPosOffset);
    s_gles2.glVertexAttribPointer(attributeLocTexCoord, 2, GL_FLOAT, false,
                                  kVertexAttribStride,
                                  kVertexAttribCoordOffset);

    s_gles2.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIndexBuffer);
    s_gles2.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);

    s_gles2.glDisableVertexAttribArray(attributeLocPos);
    s_gles2.glDisableVertexAttribArray(attributeLocTexCoord);
}

// initialize(): allocate GPU memory for YUV components,
// and create shaders and vertex data.
YUVConverter::YUVConverter(int width, int height, FrameworkFormat format)
    : mWidth(width),
      mHeight(height),
      mFormat(format),
      mColorBufferFormat(format) {}

void YUVConverter::init(int width, int height, FrameworkFormat format) {
    uint32_t yOffset, uOffset, vOffset, ywidth, cwidth, cheight;
    getYUVOffsets(width, height, mFormat,
                  &yOffset, &uOffset, &vOffset,
                  &ywidth, &cwidth);
    cheight = height / 2;

    mWidth = width;
    mHeight = height;
    if (!mTextureY)
        createYUVGLTex(GL_TEXTURE0, ywidth, height, &mTextureY, false);
    switch (mFormat) {
        case FRAMEWORK_FORMAT_YV12:
            if (!mTextureU)
                createYUVGLTex(GL_TEXTURE1, cwidth, cheight, &mTextureU, false);
            if (!mTextureV)
                createYUVGLTex(GL_TEXTURE2, cwidth, cheight, &mTextureV, false);
            createYUVGLShader(&mProgram,
                              &mUniformLocYWidthCutoff,
                              &mUniformLocCWidthCutoff,
                              &mUniformLocSamplerY,
                              &mUniformLocSamplerU,
                              &mUniformLocSamplerV,
                              &mAttributeLocTexCoord,
                              &mAttributeLocPos);
            break;
        case FRAMEWORK_FORMAT_YUV_420_888:
            if (feature_is_enabled(
                    kFeature_YUV420888toNV21)) {
                if (!mTextureVU)
                    createYUVGLTex(GL_TEXTURE1, cwidth, cheight, &mTextureVU, true);
                createYUVInterleavedGLShader(&mProgram,
                                             &mUniformLocYWidthCutoff,
                                             &mUniformLocCWidthCutoff,
                                             &mUniformLocSamplerY,
                                             &mUniformLocSamplerVU,
                                             &mAttributeLocTexCoord,
                                             &mAttributeLocPos,
                                             YUVInterleaveDirection::VU);
            } else {
                if (!mTextureU)
                    createYUVGLTex(GL_TEXTURE1, cwidth, cheight, &mTextureU, false);
                if (!mTextureV)
                    createYUVGLTex(GL_TEXTURE2, cwidth, cheight, &mTextureV, false);
                createYUVGLShader(&mProgram,
                                  &mUniformLocYWidthCutoff,
                                  &mUniformLocCWidthCutoff,
                                  &mUniformLocSamplerY,
                                  &mUniformLocSamplerU,
                                  &mUniformLocSamplerV,
                                  &mAttributeLocTexCoord,
                                  &mAttributeLocPos);
            }
            break;
        case FRAMEWORK_FORMAT_NV12:
            if (!mTextureUV)
                createYUVGLTex(GL_TEXTURE1, cwidth, cheight, &mTextureUV, true);
            createYUVInterleavedGLShader(&mProgram,
                                         &mUniformLocYWidthCutoff,
                                         &mUniformLocCWidthCutoff,
                                         &mUniformLocSamplerY,
                                         &mUniformLocSamplerVU,
                                         &mAttributeLocTexCoord,
                                         &mAttributeLocPos,
                                         YUVInterleaveDirection::UV);
            break;
        default:
            FATAL("Unknown format: 0x%x", mFormat);
    }

    createYUVGLFullscreenQuad(&mQuadVertexBuffer, &mQuadIndexBuffer);
}

void YUVConverter::saveGLState() {
    s_gles2.glGetFloatv(GL_VIEWPORT, mCurrViewport);
    s_gles2.glGetIntegerv(GL_ACTIVE_TEXTURE, &mCurrTexUnit);
    s_gles2.glGetIntegerv(GL_TEXTURE_BINDING_2D, &mCurrTexBind);
    s_gles2.glGetIntegerv(GL_CURRENT_PROGRAM, &mCurrProgram);
    s_gles2.glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &mCurrVbo);
    s_gles2.glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &mCurrIbo);
}

void YUVConverter::restoreGLState() {
    s_gles2.glViewport(mCurrViewport[0], mCurrViewport[1],
                       mCurrViewport[2], mCurrViewport[3]);
    s_gles2.glActiveTexture(mCurrTexUnit);
    s_gles2.glUseProgram(mCurrProgram);
    s_gles2.glBindBuffer(GL_ARRAY_BUFFER, mCurrVbo);
    s_gles2.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mCurrIbo);
}

uint32_t YUVConverter::getDataSize() {
    uint32_t align = (mFormat == FRAMEWORK_FORMAT_YV12) ? 16 : 1;
    uint32_t yStride = (mWidth + (align - 1)) & ~(align - 1);
    uint32_t uvStride = (yStride / 2 + (align - 1)) & ~(align - 1);
    uint32_t uvHeight = mHeight / 2;
    uint32_t dataSize = yStride * mHeight + 2 * (uvHeight * uvStride);
    return dataSize;
}

void YUVConverter::readPixels(uint8_t* pixels, uint32_t pixels_size) {
    int width = mWidth;
    int height = mHeight;

    YUV_DEBUG_LOG("texture: width %d height %d\n", width, height);
    {
        uint32_t align = (mFormat == FRAMEWORK_FORMAT_YV12) ? 16 : 1;
        uint32_t yStride = (width + (align - 1)) & ~(align - 1);
        uint32_t uvStride = (yStride / 2 + (align - 1)) & ~(align - 1);
        uint32_t uvHeight = height / 2;
        uint32_t dataSize = yStride * height + 2 * (uvHeight * uvStride);
        if (pixels_size != dataSize) {
            YUV_DEBUG_LOG("failed\n");
            return;
        }
        YUV_DEBUG_LOG("reading %d bytes\n", (int)dataSize);
    }

    uint32_t yOffset, uOffset, vOffset, ywidth, cwidth;
    getYUVOffsets(width, height, mFormat, &yOffset, &uOffset, &vOffset, &ywidth,
                  &cwidth);

    if (mFormat == FRAMEWORK_FORMAT_YUV_420_888) {
        if (feature_is_enabled(
                kFeature_YUV420888toNV21)) {
            readYUVTex(mTextureVU, pixels + vOffset, true);
            YUV_DEBUG_LOG("done");
        } else {
            readYUVTex(mTextureU, pixels + uOffset, false);
            readYUVTex(mTextureV, pixels + vOffset, false);
            YUV_DEBUG_LOG("done");
        }
    } else if (mFormat == FRAMEWORK_FORMAT_NV12) {
        readYUVTex(mTextureUV, pixels + uOffset, true);
        if (mColorBufferFormat == FRAMEWORK_FORMAT_YUV_420_888) {
            // do a conversion here inplace: NV12 to YUV 420 888
            uint8_t* scrath_memory = pixels;
            NV12ToYUV420PlanarInPlaceConvert(width, height, pixels,
                                             scrath_memory);
            YUV_DEBUG_LOG("done");
        }
        YUV_DEBUG_LOG("done");
    } else if (mFormat == FRAMEWORK_FORMAT_YV12) {
            readYUVTex(mTextureU, pixels + uOffset, false);
            readYUVTex(mTextureV, pixels + vOffset, false);
            YUV_DEBUG_LOG("done");
    }
    // read Y the last, because we can might used it as a scratch space
    readYUVTex(mTextureY, pixels + yOffset, false);
    YUV_DEBUG_LOG("done");
}

void YUVConverter::swapTextures(uint32_t type, uint32_t* textures) {
    if (type == FRAMEWORK_FORMAT_NV12) {
        mFormat = FRAMEWORK_FORMAT_NV12;
        std::swap(textures[0], mTextureY);
        std::swap(textures[1], mTextureUV);
    } else if (type == FRAMEWORK_FORMAT_YUV_420_888) {
        mFormat = FRAMEWORK_FORMAT_YUV_420_888;
        std::swap(textures[0], mTextureY);
        std::swap(textures[1], mTextureU);
        std::swap(textures[2], mTextureV);
    } else {
        FATAL("Unknown format: 0x%x", type);
    }
}

// drawConvert: per-frame updates.
// Update YUV textures, then draw the fullscreen
// quad set up above, which results in a framebuffer
// with the RGB colors.
void YUVConverter::drawConvert(int x, int y, int width, int height, const char* pixels) {
    saveGLState();
    if (pixels && (width != mWidth || height != mHeight)) {
        reset();
    }

    if (mProgram == 0) {
        init(width, height, mFormat);
    }
    s_gles2.glViewport(x, y, width, height);
    uint32_t yOffset, uOffset, vOffset, ywidth, cwidth, cheight;
    getYUVOffsets(width, height, mFormat, &yOffset, &uOffset, &vOffset, &ywidth, &cwidth);
    cheight = height / 2;
    updateCutoffs(width, ywidth, width / 2, cwidth);

    if (!pixels) {
        // special case: draw from texture, only support NV12 for now
        // as cuvid's native format is NV12.
        // TODO: add more formats if there are such needs in the future.
        assert(mFormat == FRAMEWORK_FORMAT_NV12);
        s_gles2.glActiveTexture(GL_TEXTURE1);
        s_gles2.glBindTexture(GL_TEXTURE_2D, mTextureUV);
        s_gles2.glActiveTexture(GL_TEXTURE0);
        s_gles2.glBindTexture(GL_TEXTURE_2D, mTextureY);

        doYUVConversionDraw(mProgram,
                            mUniformLocYWidthCutoff,
                            mUniformLocCWidthCutoff,
                            mUniformLocSamplerY,
                            mUniformLocSamplerU,
                            mUniformLocSamplerV,
                            mUniformLocSamplerVU,
                            mAttributeLocTexCoord,
                            mAttributeLocPos,
                            mQuadVertexBuffer,
                            mQuadIndexBuffer,
                            width,
                            ywidth,
                            width / 2,
                            cwidth,
                            mYWidthCutoff,
                            mCWidthCutoff,
                            true);

        restoreGLState();
        return;
    }

    subUpdateYUVGLTex(GL_TEXTURE0, mTextureY,
                      x, y, ywidth, height,
                      pixels + yOffset, false);

    switch (mFormat) {
        case FRAMEWORK_FORMAT_YV12:
            subUpdateYUVGLTex(GL_TEXTURE1, mTextureU,
                              x, y, cwidth, cheight,
                              pixels + uOffset, false);
            subUpdateYUVGLTex(GL_TEXTURE2, mTextureV,
                              x, y, cwidth, cheight,
                              pixels + vOffset, false);
            doYUVConversionDraw(mProgram,
                                mUniformLocYWidthCutoff,
                                mUniformLocCWidthCutoff,
                                mUniformLocSamplerY,
                                mUniformLocSamplerU,
                                mUniformLocSamplerV,
                                mUniformLocSamplerVU,
                                mAttributeLocTexCoord,
                                mAttributeLocPos,
                                mQuadVertexBuffer,
                                mQuadIndexBuffer,
                                width, ywidth,
                                width / 2, cwidth,
                                mYWidthCutoff,
                                mCWidthCutoff,
                                false);
            break;
        case FRAMEWORK_FORMAT_YUV_420_888:
            if (feature_is_enabled(
                    kFeature_YUV420888toNV21)) {
                subUpdateYUVGLTex(GL_TEXTURE1, mTextureVU,
                                  x, y, cwidth, cheight,
                                  pixels + vOffset, true);
                doYUVConversionDraw(mProgram,
                                    mUniformLocYWidthCutoff,
                                    mUniformLocCWidthCutoff,
                                    mUniformLocSamplerY,
                                    mUniformLocSamplerU,
                                    mUniformLocSamplerV,
                                    mUniformLocSamplerVU,
                                    mAttributeLocTexCoord,
                                    mAttributeLocPos,
                                    mQuadVertexBuffer,
                                    mQuadIndexBuffer,
                                    width, ywidth,
                                    width / 2, cwidth,
                                    mYWidthCutoff,
                                    mCWidthCutoff,
                                    true);
            } else {
                subUpdateYUVGLTex(GL_TEXTURE1, mTextureU,
                                  x, y, cwidth, cheight,
                                  pixels + uOffset, false);
                subUpdateYUVGLTex(GL_TEXTURE2, mTextureV,
                                  x, y, cwidth, cheight,
                                  pixels + vOffset, false);
                doYUVConversionDraw(mProgram,
                                    mUniformLocYWidthCutoff,
                                    mUniformLocCWidthCutoff,
                                    mUniformLocSamplerY,
                                    mUniformLocSamplerU,
                                    mUniformLocSamplerV,
                                    mUniformLocSamplerVU,
                                    mAttributeLocTexCoord,
                                    mAttributeLocPos,
                                    mQuadVertexBuffer,
                                    mQuadIndexBuffer,
                                    width, ywidth,
                                    width / 2, cwidth,
                                    mYWidthCutoff,
                                    mCWidthCutoff,
                                    false);
            }
            break;
        case FRAMEWORK_FORMAT_NV12:
            subUpdateYUVGLTex(GL_TEXTURE1, mTextureUV,
                              x, y, cwidth, cheight,
                              pixels + uOffset, true);
            doYUVConversionDraw(mProgram,
                                mUniformLocYWidthCutoff,
                                mUniformLocCWidthCutoff,
                                mUniformLocSamplerY,
                                mUniformLocSamplerU,
                                mUniformLocSamplerV,
                                mUniformLocSamplerVU,
                                mAttributeLocTexCoord,
                                mAttributeLocPos,
                                mQuadVertexBuffer,
                                mQuadIndexBuffer,
                                width, ywidth,
                                width / 2, cwidth,
                                mYWidthCutoff,
                                mCWidthCutoff,
                                true);
            break;
        default:
            FATAL("Unknown format: 0x%x", mFormat);
    }

    restoreGLState();
}

void YUVConverter::updateCutoffs(float width, float ywidth,
                                 float halfwidth, float cwidth) {
    switch (mFormat) {
    case FRAMEWORK_FORMAT_YV12:
        mYWidthCutoff = ((float)width) / ((float)ywidth);
        mCWidthCutoff = ((float)halfwidth) / ((float)cwidth);
        break;
    case FRAMEWORK_FORMAT_YUV_420_888:
        mYWidthCutoff = 1.0f;
        mCWidthCutoff = 1.0f;
        break;
    case FRAMEWORK_FORMAT_NV12:
        mYWidthCutoff = 1.0f;
        mCWidthCutoff = 1.0f;
        break;
    case FRAMEWORK_FORMAT_GL_COMPATIBLE:
        FATAL("Input not a YUV format!");
    }
}

void YUVConverter::reset() {
    if (mQuadIndexBuffer) s_gles2.glDeleteBuffers(1, &mQuadIndexBuffer);
    if (mQuadVertexBuffer) s_gles2.glDeleteBuffers(1, &mQuadVertexBuffer);
    if (mProgram) s_gles2.glDeleteProgram(mProgram);
    if (mTextureY) s_gles2.glDeleteTextures(1, &mTextureY);
    if (mTextureU) s_gles2.glDeleteTextures(1, &mTextureU);
    if (mTextureV) s_gles2.glDeleteTextures(1, &mTextureV);
    if (mTextureVU) s_gles2.glDeleteTextures(1, &mTextureVU);
    if (mTextureUV) s_gles2.glDeleteTextures(1, &mTextureUV);
    mQuadIndexBuffer = 0;
    mQuadVertexBuffer = 0;
    mProgram = 0;
    mTextureY = 0;
    mTextureU = 0;
    mTextureV = 0;
    mTextureVU = 0;
    mTextureUV = 0;
}

YUVConverter::~YUVConverter() {
    reset();
}
