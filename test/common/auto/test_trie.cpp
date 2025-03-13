#include <gtest/gtest.h>

#include "mksync/base/trie.hpp"

TEST(Trie, Basic)
{
    mks::base::Trie<int> trie;
    trie.insert("abc", 1);
    trie.insert("ab", 2);
    trie.insert("abcd", 3);
    trie.insert("abce", 4);
    trie.insert("abcf", 5);
    trie.insert("abcefg", 6);
    trie.insert("abcfgh", 7);
    trie.insert("abcfghi", 8);
    trie.insert("abcfghij", 9);
    trie.insert("abcfghijk", 10);
    trie.insert("abcfghijkl", 11);
    EXPECT_EQ(trie.size(), 11);
    for (auto iter = trie.begin(); iter != trie.end(); ++iter) {
        std::cout << iter.key() << " = " << *iter << std::endl;
    }

    EXPECT_EQ(trie.search("abc").value(), 1);
    EXPECT_EQ(trie.search("ab").value(), 2);
    EXPECT_EQ(trie.search("abcd").value(), 3);
    EXPECT_EQ(trie.search("abce").value(), 4);
    EXPECT_EQ(trie.search("abcf").value(), 5);
    EXPECT_EQ(trie.search("abcefg").value(), 6);
    EXPECT_EQ(trie.search("abcfgh").value(), 7);
    EXPECT_EQ(trie.search("abcfghi").value(), 8);
    EXPECT_EQ(trie.search("abcfghij").value(), 9);
    EXPECT_EQ(trie.search("abcfghijk").value(), 10);
    EXPECT_EQ(trie.search("abcfghijkl").value(), 11);
    EXPECT_EQ(trie.search("abcfghijklm").error(), ilias::Error::Unknown);
    EXPECT_EQ(trie.search("abcfghijklmnopq").error(), ilias::Error::Unknown);

    EXPECT_FALSE(trie.remove("abcfghijklmnopq"));
    EXPECT_TRUE(trie.remove("abcd"));
    EXPECT_TRUE(trie.remove("abcfghijkl"));
    EXPECT_TRUE(trie.remove("abcfgh"));

    EXPECT_EQ(trie.search("abcd").error(), ilias::Error::Unknown);
    EXPECT_EQ(trie.search("abcfghijkl").error(), ilias::Error::Unknown);
    EXPECT_EQ(trie.search("abcfghijk").value(), 10);
    EXPECT_EQ(trie.search("abcfghij").value(), 9);
    EXPECT_EQ(trie.search("abcfgh").error(), ilias::Error::Unknown);
    EXPECT_EQ(trie.size(), 8);
    while (trie.size() != 0U) {
        auto it = trie.remove(trie.begin());
        if (it != trie.end()) {
            std::cout << it.key() << " = " << *it << std::endl;
        }
    }
    EXPECT_EQ(trie.size(), 0U);
    trie.clear();
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}