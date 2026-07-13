#include <gtest/gtest.h>

#if defined(__linux__)

    #include "platform/wayland_keymap.hpp"

    #include <linux/input-event-codes.h>

namespace
{

    TEST(WaylandKeymap, ConvertsPortableHidUsagesToEvdevCodes)
    {
        EXPECT_EQ(mks::wayland::evdevKeyCode(mks::Key::A), KEY_A);
        EXPECT_EQ(mks::wayland::evdevKeyCode(mks::Key::Digit1), KEY_1);
        EXPECT_EQ(mks::wayland::evdevKeyCode(mks::Key::F12), KEY_F12);
        EXPECT_EQ(mks::wayland::evdevKeyCode(mks::Key::RightCtrl), KEY_RIGHTCTRL);
        EXPECT_EQ(mks::wayland::evdevKeyCode(mks::Key::MediaPlayPause), KEY_PLAYPAUSE);
    }

    TEST(WaylandKeymap, RejectsHidUsagesWithoutAnEvdevEquivalent)
    {
        EXPECT_FALSE(mks::wayland::evdevKeyCode(mks::Key::None));
        EXPECT_FALSE(mks::wayland::evdevKeyCode(mks::Key::ErrorOverflow));
    }

    TEST(WaylandKeymap, ConvertsEvdevCodesBackToPortableHidUsages)
    {
        EXPECT_EQ(mks::wayland::keyFromEvdev(KEY_A), mks::Key::A);
        EXPECT_EQ(mks::wayland::keyFromEvdev(KEY_F12), mks::Key::F12);
        EXPECT_EQ(mks::wayland::keyFromEvdev(KEY_RIGHTCTRL), mks::Key::RightCtrl);
        EXPECT_EQ(mks::wayland::keyFromEvdev(KEY_PLAYPAUSE), mks::Key::MediaPlayPause);
        EXPECT_EQ(mks::wayland::keyFromEvdev(KEY_RESERVED), mks::Key::None);
    }

} // namespace

#endif
