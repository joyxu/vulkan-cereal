// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>
#include <vector>

#include "virtio-gpu-gfxstream-renderer.h"
#include "host-common/testing/MockGraphicsAgentFactory.h"
#include "OSWindow.h"
#include "aemu/base/system/System.h"

class GfxStreamBackendTest : public ::testing::Test {
private:
    static void sWriteFence(void* cookie, uint32_t fence) {
        uint32_t current = *(uint32_t*)cookie;
        if (current < fence)
            *(uint32_t*)(cookie) = fence;
    }

protected:
    uint32_t cookie;
    static const bool useWindow;
    std::vector<stream_renderer_param> streamRendererParams;
    static constexpr uint32_t width = 256;
    static constexpr uint32_t height = 256;
    static std::unique_ptr<OSWindow> window;
    static constexpr int rendererFlags = GFXSTREAM_RENDERER_FLAGS_NO_VK_BIT;
    static constexpr int surfacelessFlags = GFXSTREAM_RENDERER_FLAGS_USE_SURFACELESS_BIT |
                                            GFXSTREAM_RENDERER_FLAGS_NO_VK_BIT;

    GfxStreamBackendTest()
        : cookie(0),
          streamRendererParams{
              {STREAM_RENDERER_PARAM_USER_DATA,
               static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&cookie))},
              {STREAM_RENDERER_PARAM_WRITE_FENCE_CALLBACK,
               static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&sWriteFence))},
              {STREAM_RENDERER_PARAM_RENDERER_FLAGS, surfacelessFlags},
              {STREAM_RENDERER_PARAM_WIN0_WIDTH, width},
              {STREAM_RENDERER_PARAM_WIN0_HEIGHT, height}
          } {}

    static void SetUpTestSuite() {
        android::emulation::injectGraphicsAgents(android::emulation::MockGraphicsAgentFactory());
        if (useWindow) {
            window.reset(CreateOSWindow());
        }
    }

    static void TearDownTestSuite() { window.reset(nullptr); }

    void SetUp() override {
        android::base::setEnvironmentVariable("ANDROID_GFXSTREAM_EGL", "1");
        if (useWindow) {
            window->initialize("GfxStreamBackendTestWindow", width, height);
            window->setVisible(true);
            window->messageLoop();
        }
    }

    void TearDown() override {
        if (useWindow) {
            window->destroy();
        }
        gfxstream_backend_teardown();
    }
};

std::unique_ptr<OSWindow> GfxStreamBackendTest::window = nullptr;

const bool GfxStreamBackendTest::useWindow =
        android::base::getEnvironmentVariable("ANDROID_EMU_TEST_WITH_WINDOW") == "1";

TEST_F(GfxStreamBackendTest, Init) {
    stream_renderer_init(streamRendererParams.data(), streamRendererParams.size());
}

TEST_F(GfxStreamBackendTest, InitOpenGLWindow) {
    if (!useWindow) {
        return;
    }

    std::vector<stream_renderer_param> glParams = streamRendererParams;
    for (auto& param: glParams) {
        if (param.key == STREAM_RENDERER_PARAM_RENDERER_FLAGS) {
            param.value = rendererFlags;
        }
    }
    stream_renderer_init(glParams.data(), glParams.size());
    gfxstream_backend_setup_window(window->getFramebufferNativeWindow(), 0, 0,
                                       width, height, width, height);
}

TEST_F(GfxStreamBackendTest, SimpleFlush) {
    stream_renderer_init(streamRendererParams.data(), streamRendererParams.size());

    const uint32_t res_id = 8;
    struct virgl_renderer_resource_create_args create_resource_args = {
            .handle = res_id,
            .target = 2,  // PIPE_TEXTURE_2D
            .format = VIRGL_FORMAT_R8G8B8A8_UNORM,
            .bind = VIRGL_BIND_SAMPLER_VIEW | VIRGL_BIND_SCANOUT |
                    VIRGL_BIND_SHARED,
            .width = width,
            .height = height,
            .depth = 1,
            .array_size = 1,
            .last_level = 0,
            .nr_samples = 0,
            .flags = 0,
    };
    EXPECT_EQ(
            pipe_virgl_renderer_resource_create(&create_resource_args, NULL, 0),
            0);
    // R8G8B8A8 is used, so 4 bytes per pixel
    auto fb = std::make_unique<uint32_t[]>(width * height);
    EXPECT_NE(fb, nullptr);
    stream_renderer_flush_resource_and_readback(res_id, 0, 0, width, height,
                                                fb.get(), width * height);
}

// Tests compile and link only.
TEST_F(GfxStreamBackendTest, DISABLED_ApiCallLinkTest) {
    stream_renderer_init(streamRendererParams.data(), streamRendererParams.size());

    const uint32_t res_id = 8;
    struct virgl_renderer_resource_create_args create_resource_args = {
        .handle = res_id,
        .target = 2,  // PIPE_TEXTURE_2D
        .format = VIRGL_FORMAT_R8G8B8A8_UNORM,
        .bind = VIRGL_BIND_SAMPLER_VIEW | VIRGL_BIND_SCANOUT |
            VIRGL_BIND_SHARED,
        .width = width,
        .height = height,
        .depth = 1,
        .array_size = 1,
        .last_level = 0,
        .nr_samples = 0,
        .flags = 0,
    };
    EXPECT_EQ(
            pipe_virgl_renderer_resource_create(&create_resource_args, NULL, 0),
            0);
    // R8G8B8A8 is used, so 4 bytes per pixel
    auto fb = std::make_unique<uint32_t[]>(width * height);
    EXPECT_NE(fb, nullptr);
    stream_renderer_flush_resource_and_readback(res_id, 0, 0, width, height,
            fb.get(), width * height);

    virtio_goldfish_pipe_reset(0, 0);
    pipe_virgl_renderer_init(0, 0, 0);
    pipe_virgl_renderer_poll();
    pipe_virgl_renderer_get_cursor_data(0, 0, 0);
    pipe_virgl_renderer_resource_unref(0);
    pipe_virgl_renderer_context_create(0, 0, 0);
    pipe_virgl_renderer_context_destroy(0);
    pipe_virgl_renderer_submit_cmd(0, 0, 0);
    pipe_virgl_renderer_transfer_read_iov(0, 0, 0, 0, 0, 0, 0, 0, 0);
    pipe_virgl_renderer_transfer_write_iov(0, 0, 0, 0, 0, 0, 0, 0, 0);

    pipe_virgl_renderer_get_cap_set(0, 0, 0);
    pipe_virgl_renderer_fill_caps(0, 0, 0);

    pipe_virgl_renderer_resource_attach_iov(0, 0, 0);
    pipe_virgl_renderer_resource_detach_iov(0, 0, 0);
    pipe_virgl_renderer_create_fence(0, 0);
    pipe_virgl_renderer_force_ctx_0();
    pipe_virgl_renderer_ctx_attach_resource(0, 0);
    pipe_virgl_renderer_ctx_detach_resource(0, 0);
    pipe_virgl_renderer_resource_get_info(0, 0);
}
