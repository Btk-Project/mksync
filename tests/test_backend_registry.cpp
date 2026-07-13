#include "platform/backend.hpp"
#include "support/mock_platform.hpp"

#include <gtest/gtest.h>
#include <ilias/testing.hpp>

namespace
{

    auto makePlatform() -> mks::Platform::Ptr
    {
        return std::make_shared<mks::test::MockPlatform>(std::vector<mks::ScreenInfo>{
            mks::ScreenInfo{
                            .width   = 1920,
                            .height  = 1080,
                            .name    = "test-output",
                            .primary = true,
                            },
        });
    }

    auto unavailableCaptureCheck() -> mks::Task<mks::BackendCheck>
    {
        co_return mks::BackendCheck{
            .available = true,
            .detail    = "partial backend",
            .screens   = {.supported = true,  .detail = "one output"         },
            .capture   = {.supported = false, .detail = "capture unavailable"},
            .injection = {.supported = true,  .detail = "injection available"},
        };
    }

    auto completeCheck() -> mks::Task<mks::BackendCheck>
    {
        co_return mks::BackendCheck{
            .available = true,
            .detail    = "complete backend",
            .screens   = {.supported = true, .detail = "one output"         },
            .capture   = {.supported = true, .detail = "capture available"  },
            .injection = {.supported = true, .detail = "injection available"},
        };
    }

    const mks::BackendRegistration kPartialRegistration{
        mks::BackendDescriptor{
                               .name        = "test-partial",
                               .displayName = "Test partial",
                               .order       = 10,
                               .check       = unavailableCaptureCheck,
                               .create      = makePlatform,
                               }
    };

    const mks::BackendRegistration kCompleteRegistration{
        mks::BackendDescriptor{
                               .name        = "test-complete",
                               .displayName = "Test complete",
                               .order       = 20,
                               .check       = completeCheck,
                               .create      = makePlatform,
                               }
    };

    TEST(BackendRegistry, ListsBackendsInDeclaredOrder)
    {
        const auto backends = mks::registeredBackends();
        ASSERT_EQ(backends.size(), 2U);
        EXPECT_EQ(backends[0].name, "test-partial");
        EXPECT_EQ(backends[1].name, "test-complete");
    }

    ILIAS_TEST(BackendRegistry, AutoSelectionSkipsBackendMissingRequiredCapability)
    {
        auto selected = co_await mks::selectBackend({}, mks::BackendRequirement::Screens |
                                                            mks::BackendRequirement::Capture);
        EXPECT_TRUE(selected.has_value());
        if (!selected) {
            co_return;
        }
        EXPECT_EQ(selected->descriptor.name, "test-complete");
    }

    ILIAS_TEST(BackendRegistry, ExplicitSelectionDoesNotFallBack)
    {
        auto selected = co_await mks::selectBackend(
            "test-partial", mks::BackendRequirement::Screens | mks::BackendRequirement::Capture);
        EXPECT_FALSE(selected.has_value());
        if (!selected) {
            EXPECT_EQ(selected.error(), std::make_error_code(std::errc::operation_not_supported));
        }
    }

} // namespace

auto main(int argc, char **argv) -> int
{
    ILIAS_TEST_SETUP_UTF8();
    auto context = ilias::PlatformContext{};
    context.install();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
