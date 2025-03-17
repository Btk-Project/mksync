#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <set>

#include "mksync/base/settings.hpp"
#include "mksync/proto/config_proto.hpp"

enum Mode
{
    eServer,
    eClient,
};

TEST(Settings, Default)
{
    mks::VirtualScreenConfig vsconfig{
        .name   = "test",
        .width  = 1920,
        .height = 1080,
        .left   = "",
        .top    = "",
        .right  = "",
        .bottom = "",
    };
    mks::base::Settings settings("./config.json");
    settings.set("appname", "mksync");
    settings.set("version", "0.0.1");
    settings.set("loglevel", "info");
    settings.set("logdir", "./logs");
    settings.set("logrotate", true);
    settings.set("logrotate_size", 100);
    settings.set("logrotate_count", 10);
    settings.set("logrotate_keep", 5);
    settings.set("logrotate_compress", true);
    settings.set("logrotate_compress_level", 6);
    settings.set("virtual_screen_config", std::vector<mks::VirtualScreenConfig>{vsconfig});
    std::vector<double> vs = {1.0, 2.0, 3.0};
    settings.set("vs", vs);
    std::array<uint64_t, 3> as = {1, 2, 3};
    settings.set("as", as);
    std::set<std::string> ss = {"a", "b", "c"};
    settings.set("ss", ss);
    settings.set("mode", eServer);
    EXPECT_TRUE(settings.save());
}

TEST(Settings, Load)
{
    mks::base::Settings settings("./config.json");
    EXPECT_EQ(settings.get<std::string>("appname", ""), "mksync");
    EXPECT_EQ(settings.get<std::string>("version", ""), "0.0.1");
    EXPECT_EQ(settings.get<std::string>("loglevel", ""), "info");
    EXPECT_EQ(settings.get<std::string>("logdir", ""), "./logs");
    EXPECT_EQ(settings.get<bool>("logrotate", false), true);
    EXPECT_EQ(settings.get<uint64_t>("logrotate_size", 0), 100);
    EXPECT_EQ(settings.get<uint64_t>("logrotate_count", 0), 10);
    EXPECT_EQ(settings.get<uint64_t>("logrotate_keep", 0), 5);
    EXPECT_EQ(settings.get<bool>("logrotate_compress", false), true);
    EXPECT_EQ(settings.get<uint64_t>("logrotate_compress_level", 0), 6);
    std::vector<double> vs;
    vs = settings.get("vs", vs);
    EXPECT_EQ(vs.size(), 3);
    EXPECT_EQ(vs[0], 1.0);
    EXPECT_EQ(vs[1], 2.0);
    EXPECT_EQ(vs[2], 3.0);
    std::vector<uint64_t> as;
    as = settings.get("as", as);
    EXPECT_EQ(as.size(), 3);
    EXPECT_EQ(as[0], 1);
    EXPECT_EQ(as[1], 2);
    EXPECT_EQ(as[2], 3);
    std::vector<std::string> ss;
    ss = settings.get("ss", ss);
    EXPECT_EQ(ss.size(), 3);
    EXPECT_EQ(ss[0], "a");
    EXPECT_EQ(ss[1], "b");
    EXPECT_EQ(ss[2], "c");
    EXPECT_EQ(settings.get<Mode>("mode", eClient), eServer);
    std::vector<mks::VirtualScreenConfig> vsconfigs;
    vsconfigs = settings.get("virtual_screen_config", vsconfigs);
    ASSERT_EQ(vsconfigs.size(), 1);
    EXPECT_EQ(vsconfigs[0].name, "test");
    EXPECT_EQ(vsconfigs[0].width, 1920);
    EXPECT_EQ(vsconfigs[0].height, 1080);
    EXPECT_EQ(vsconfigs[0].left, "");
    EXPECT_EQ(vsconfigs[0].top, "");
    EXPECT_EQ(vsconfigs[0].right, "");
    EXPECT_EQ(vsconfigs[0].bottom, "");
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}