/**
 * @file trie.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-22
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <unordered_map>
#include <memory>
#include <ilias/task.hpp>

namespace mks::base
{
    template <typename T>
    class Trie {
    public:
        using value_type = T;

    public:
        Trie() : _root(std::make_unique<TrieNode>()) {}

        /**
         * @brief insert to trie
         *
         * @param str
         * @param value
         * @return ilias::Result<void>
         */
        auto insert(std::string_view str, const value_type &value) -> ilias::Result<void>;
        template <typename... Args>
        auto emplace(std::string_view str, Args &&...args) -> ilias::Result<void>;
        /**
         * @brief search in trie
         *
         * @param str
         * @return ilias::Result<T>
         */
        auto at(std::string_view str) const -> const value_type &;
        auto at(std::string_view str) -> value_type &;
        auto search(std::string_view str) -> ilias::Result<value_type>;
        /**
         * @brief clear the trie
         *
         */
        auto clear() -> void;

    private:
        struct TrieNode {
            value_type                                          value;
            bool                                                isEndOfWord = false;
            std::unordered_map<char, std::unique_ptr<TrieNode>> children;
        };
        std::unique_ptr<TrieNode> _root;
    };

    template <typename T>
    auto Trie<T>::insert(std::string_view str, const value_type &value) -> ilias::Result<void>
    {
        auto node = _root.get();
        for (auto ch : str) {
            if (node->children.find(ch) == node->children.end()) {
                node->children[ch] = std::make_unique<TrieNode>();
            }
            node = node->children[ch].get();
        }
        node->value       = value;
        node->isEndOfWord = true;
        return {};
    }

    template <typename T>
    template <typename... Args>
    auto Trie<T>::emplace(std::string_view str, Args &&...args) -> ilias::Result<void>
    {
        auto node = _root.get();
        for (int i = 0; i < (int)str.size(); ++i) {
            auto ch = str[i];
            if (node->children.find(ch) == node->children.end()) {
                if (i == (int)(str.size() - 1)) {
                    node->children[ch] = std::make_unique<TrieNode>(
                        value_type{std::forward<Args>(args)...}, true,
                        std::unordered_map<char, std::unique_ptr<TrieNode>>{});
                }
                else {
                    node->children[ch] = std::make_unique<TrieNode>();
                }
            }
        }
        return {};
    }

    template <typename T>
    auto Trie<T>::search(std::string_view str) -> ilias::Result<value_type>
    {
        auto node = _root.get();
        for (auto ch : str) {
            if (node->children.find(ch) == node->children.end()) {
                return ilias::Unexpected<ilias::Error>(ilias::Error::Unknown);
            }
            node = node->children.at(ch).get();
        }
        if (node->isEndOfWord) {
            return node->value;
        }
        return ilias::Unexpected<ilias::Error>(ilias::Error::Unknown);
    }

    template <typename T>
    auto Trie<T>::at(std::string_view str) -> value_type &
    {
        auto node = _root.get();
        for (auto ch : str) {
            if (node->children.find(ch) == node->children.end()) {
                throw std::out_of_range("Trie::at: string not found");
            }
            node = node->children.at(ch).get();
        }
        if (!node->isEndOfWord) {
            throw std::out_of_range("Trie::at: string not found");
        }
        return node->value;
    }

    template <typename T>
    auto Trie<T>::at(std::string_view str) const -> const value_type &
    {
        auto node = _root.get();
        for (auto ch : str) {
            if (node->children.find(ch) == node->children.end()) {
                throw std::out_of_range("Trie::at: string not found");
            }
            node = node->children.at(ch).get();
        }
        if (!node->isEndOfWord) {
            throw std::out_of_range("Trie::at: string not found");
        }
        return node->value;
    }

    template <typename T>
    auto Trie<T>::clear() -> void
    {
        _root = std::make_unique<TrieNode>();
    }
} // namespace mks::base