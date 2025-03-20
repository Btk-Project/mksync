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
#include <algorithm>

namespace mks::base
{
    template <typename T>
    class TrieIterator;
    /**
     * @brief trie
     * 字典树。
     * @tparam T
     */
    template <typename T>
    class Trie {
    public:
        using value_type    = T;
        using iterator_type = TrieIterator<T>;
        struct TrieNode {
            value_type                                value;
            bool                                      isEndOfWord = false;
            std::map<char, std::unique_ptr<TrieNode>> children;
        };

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
        auto remove(std::string_view str) -> bool;
        auto size() -> std::size_t;
        /**
         * @brief clear the trie
         *
         */
        auto clear() -> void;

        iterator_type begin();
        iterator_type end();
        auto          remove(iterator_type it) -> iterator_type;

    private:
        auto _remove(std::string_view str, TrieNode *node) -> int;
        auto _leaf_count(TrieNode *node) -> int;

    private:
        std::unique_ptr<TrieNode> _root;
    };

    template <typename T>
    class TrieIterator {
    public:
        using TrieType   = Trie<T>;
        using value_type = typename TrieType::value_type;
        using pointer    = value_type *;
        using reference  = value_type &;
        using TrieNode   = TrieType::TrieNode;

        struct TrieNodeIterator {
            TrieNode *node;
            char      ch = 0;
        };

        TrieIterator() = default;
        TrieIterator(TrieNode *node);
        TrieIterator(const TrieNodeIterator &iterator) : _stack(iterator._stack) {}
        TrieIterator(TrieNodeIterator &&iterator) : _stack(std::move(iterator._stack)) {}

        auto operator=(const TrieNodeIterator &iterator) -> TrieIterator &;
        auto operator=(TrieNodeIterator &&iterator) -> TrieIterator &;
        bool operator==(const TrieIterator &other) const;
        auto operator*() const -> reference;
        auto operator->() const -> pointer;
        auto key() const -> const std::string &;
        auto operator++() -> TrieIterator &;
        auto operator++(int) -> TrieIterator;

    private:
        std::vector<TrieNodeIterator> _stack;
        std::string                   _key;
    };

    template <typename T>
    TrieIterator<T>::TrieIterator(TrieNode *node)
    {
        _stack.push_back({.node = node, .ch = '\0'});
        ++*this;
    }

    template <typename T>
    TrieIterator<T> &TrieIterator<T>::operator=(const TrieNodeIterator &iterator)
    {
        if (this != &iterator) {
            _stack = iterator._stack;
            return *this;
        }
    }

    template <typename T>
    TrieIterator<T> &TrieIterator<T>::operator=(TrieNodeIterator &&iterator)
    {
        if (this != &iterator) {
            _stack = std::move(iterator._stack);
            return *this;
        }
    }

    template <typename T>
    bool TrieIterator<T>::operator==(const TrieIterator &other) const
    {
        if (_stack.empty() ^ other._stack.empty()) {
            return false;
        }
        if (_stack.empty() && other._stack.empty()) {
            return true;
        }
        return _stack.back().node == other._stack.back().node;
    }

    template <typename T>
    auto TrieIterator<T>::operator*() const -> reference
    {
        return _stack.back().node->value;
    }

    template <typename T>
    auto TrieIterator<T>::operator->() const -> pointer
    {
        return &_stack.back().node->value;
    }

    template <typename T>
    auto TrieIterator<T>::key() const -> const std::string &
    {
        return _key;
    }

    template <typename T>
    auto TrieIterator<T>::operator++() -> TrieIterator &
    {
        while (!_stack.empty()) {
            // 获取当前正在遍历的层的父节点
            TrieNode *node = _stack.back().node;
            char     &ch   = _stack.back().ch;
            if (node->children.empty()) { // 如果没有儿子则该层无需遍历。回退到上一层
                _stack.pop_back();
                if (!_key.empty()) {
                    _key.pop_back();
                }
            }
            else { // 遍历该层，找到大于ch的第一个有值的节点
                auto item = std::upper_bound(node->children.begin(), node->children.end(), ch,
                                             [](char ch, auto &&pair) {
                                                 return ch < pair.first;
                                             });    // 找到第一个大于ch的儿子节点。
                if (item == node->children.end()) { // 没有则该层以遍历完成。
                    _stack.pop_back();
                    if (!_key.empty()) {
                        _key.pop_back();
                    }
                }
                else { // 有则继续遍历ch之后的儿子
                    ch = item->first;
                    _stack.emplace_back(item->second.get(), item->first);
                    _key.push_back(item->first);
                    if (item->second->isEndOfWord) { // 如果该儿子有值则找到了。
                        return *this;
                    }
                    // 否则以该儿子为新的层开始遍历。
                }
            }
        }
        return *this;
    }

    template <typename T>
    auto TrieIterator<T>::operator++(int) -> TrieIterator
    {
        auto result = *this;
        ++(*this);
        return result;
    }

    template <typename T>
    Trie<T>::iterator_type Trie<T>::begin()
    {
        return iterator_type(_root.get());
    }

    template <typename T>
    Trie<T>::iterator_type Trie<T>::end()
    {
        return iterator_type();
    }

    template <typename T>
    auto Trie<T>::remove(iterator_type it) -> iterator_type
    {
        if (it.key() == "") {
            return end();
        }
        auto key = it.key();
        ++it;
        _remove(key, _root.get());
        return it;
    }

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
    auto Trie<T>::size() -> std::size_t
    {
        return _leaf_count(_root.get());
    }

    template <typename T>
    auto Trie<T>::remove(std::string_view str) -> bool
    {
        return _remove(str, _root.get()) >= 0;
    }

    template <typename T>
    auto Trie<T>::_leaf_count(TrieNode *node) -> int
    {
        if (node->children.empty()) {
            return node->isEndOfWord ? 1 : 0;
        }
        int count = node->isEndOfWord ? 1 : 0;
        for (auto &item : node->children) {
            count += _leaf_count(item.second.get());
        }
        return count;
    }

    template <typename T>
    auto Trie<T>::_remove(std::string_view str, TrieNode *node) -> int
    {
        if (str.empty()) {
            if (node->isEndOfWord) {
                node->isEndOfWord = false;
                node->value.~T();
                new (&node->value) T{};
                return (int)node->children.size();
            }
            return -1;
        }
        auto ch = str[0];
        if (node->children.find(ch) == node->children.end()) {
            return -1;
        }
        auto ret = _remove(str.substr(1), node->children[ch].get());
        if (ret == -1) {
            return -1;
        }
        if (ret == 0) {
            node->children.erase(ch);
        }
        return node->isEndOfWord ? 1 : (int)node->children.size();
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