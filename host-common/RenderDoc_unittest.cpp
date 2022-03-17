#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "RenderDoc.h"

#include <type_traits>

#include "base/SharedLibrary.h"

namespace emugl {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::SetArgPointee;

using FunctionPtr = android::base::SharedLibrary::FunctionPtr;
using RenderDocApi = RenderDoc::RenderDocApi;

class MockSharedLibrary : public android::base::SharedLibrary {
   public:
    MockSharedLibrary() : SharedLibrary(NULL) {}
    MOCK_METHOD(FunctionPtr, findSymbol, (const char*), (const, override));
};

TEST(RenderDocTest, InitializeWithNullSharedLibrary) {
    EXPECT_EQ(RenderDoc::create(nullptr), nullptr);
}

TEST(RenderDocTest, CantFindRENDERDOC_GetAPI) {
    MockSharedLibrary sharedLibrary;
    EXPECT_CALL(sharedLibrary, findSymbol(_)).WillRepeatedly(Return(nullptr));
    EXPECT_EQ(RenderDoc::create(&sharedLibrary), nullptr);
}

TEST(RenderDocTest, RENDERDOC_GetAPIFails) {
    MockSharedLibrary sharedLibrary;
    static MockFunction<std::remove_pointer_t<pRENDERDOC_GetAPI>> mockRENDERDOC_GetAPI;
    static pRENDERDOC_GetAPI fpMockRENDERDOC_GetAPI = [](RENDERDOC_Version version,
                                                         void** outAPIPointers) {
        return mockRENDERDOC_GetAPI.Call(version, outAPIPointers);
    };
    RenderDocApi rdocApi;

    EXPECT_CALL(sharedLibrary, findSymbol(_)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(sharedLibrary, findSymbol("RENDERDOC_GetAPI"))
        .WillRepeatedly(Return(reinterpret_cast<FunctionPtr>(fpMockRENDERDOC_GetAPI)));

    EXPECT_CALL(mockRENDERDOC_GetAPI, Call(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(&rdocApi), Return(0)));
    EXPECT_EQ(RenderDoc::create(&sharedLibrary), nullptr);

    EXPECT_CALL(mockRENDERDOC_GetAPI, Call(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(nullptr), Return(1)));
    EXPECT_EQ(RenderDoc::create(&sharedLibrary), nullptr);
}

TEST(RenderDocTest, CreateSuccessfully) {
    MockSharedLibrary sharedLibrary;
    static MockFunction<std::remove_pointer_t<pRENDERDOC_GetAPI>> mockRENDERDOC_GetAPI;
    static pRENDERDOC_GetAPI fpMockRENDERDOC_GetAPI = [](RENDERDOC_Version version,
                                                         void** outAPIPointers) {
        return mockRENDERDOC_GetAPI.Call(version, outAPIPointers);
    };
    RenderDocApi rdocApiMock;
    static MockFunction<std::remove_pointer_t<pRENDERDOC_GetCaptureOptionU32>>
        getCaptureOptionU32Mock;
    rdocApiMock.GetCaptureOptionU32 = [](RENDERDOC_CaptureOption option) {
        return getCaptureOptionU32Mock.Call(option);
    };

    EXPECT_CALL(sharedLibrary, findSymbol(_)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(sharedLibrary, findSymbol("RENDERDOC_GetAPI"))
        .WillRepeatedly(Return(reinterpret_cast<FunctionPtr>(fpMockRENDERDOC_GetAPI)));
    EXPECT_CALL(mockRENDERDOC_GetAPI, Call(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(&rdocApiMock), Return(1)));
    std::unique_ptr<RenderDoc> renderDoc = RenderDoc::create(&sharedLibrary);
    EXPECT_NE(renderDoc, nullptr);

    EXPECT_CALL(getCaptureOptionU32Mock, Call(eRENDERDOC_Option_DebugOutputMute))
        .WillRepeatedly(Return(1));
    EXPECT_EQ(renderDoc->call(RenderDoc::kGetCaptureOptionU32, eRENDERDOC_Option_DebugOutputMute),
              1);
}
}  // namespace
}  // namespace emugl
