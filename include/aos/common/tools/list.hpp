/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_LIST_HPP_
#define AOS_LIST_HPP_

#include <assert.h>

#include "aos/common/tools/algorithm.hpp"
#include "aos/common/tools/buffer.hpp"
#include "aos/common/tools/new.hpp"
#include "aos/common/tools/utils.hpp"

namespace aos {

template <typename T>
class List;

/**
 * List implementation.
 * @tparam T list type.
 */
template <typename T>
class ListImpl {
protected:
    struct Node {
        bool  mAllocated = false;
        Node* mNext      = nullptr;
        Node* mPrev      = nullptr;
        alignas(T) uint8_t mBuffer[sizeof(T)];
    };

    /**
     * List iterator.
     */
    template <bool IsConst>
    class Iterator {
    public:
        using Pointer   = Conditional<IsConst, const T*, T*>;
        using Reference = Conditional<IsConst, const T&, T&>;
        using NodeType  = Conditional<IsConst, const Node, Node>;

        /**
         * Creates iterator.
         */
        Iterator() = default;

        /**
         * Creates iterator.
         *
         * @param node list node.
         */
        explicit Iterator(NodeType* node)
            : mCurrentNode(node)
        {
        }

        /**
         * Copy constructor.
         */
        // cppcheck-suppress noExplicitConstructor
        Iterator(const Iterator&) = default;

        /**
         * Copy constructor.
         */
        template <bool WasConst = IsConst, typename = EnableIf<IsConst || !WasConst>>
        // cppcheck-suppress noExplicitConstructor
        Iterator(const Iterator<WasConst>& it)
            : mCurrentNode(it.mCurrentNode)
        {
        }

        /**
         * Dereference operator.
         *
         * @return Reference.
         */
        Reference operator*() const { return *reinterpret_cast<Pointer>(mCurrentNode->mBuffer); }

        /**
         * Dereference operator.
         *
         * @return Pointer.
         */
        Pointer operator&() const { return reinterpret_cast<Pointer>(mCurrentNode->mBuffer); }

        /**
         * Dereference operator.
         *
         * @return Pointer.
         */
        Pointer operator->() const { return reinterpret_cast<Pointer>(mCurrentNode->mBuffer); }

        /**
         * Prefix increment operator.
         *
         * @return Iterator&.
         */
        Iterator& operator++()
        {
            mCurrentNode = mCurrentNode->mNext;

            return *this;
        }

        /**
         * Postfix increment operator.
         *
         * @return Iterator&.
         */
        Iterator operator++(int)
        {
            Iterator it(*this);

            operator++();

            return it;
        }

        /**
         * Prefix decrement operator.
         *
         * @return Iterator&.
         */
        Iterator& operator--()
        {
            mCurrentNode = mCurrentNode->mPrev;

            return *this;
        }

        /**
         * Postfix decrement operator.
         *
         * @return Iterator&.
         */
        Iterator operator--(int)
        {
            Iterator it(*this);

            operator--();

            return it;
        }

        /**
         * Checks if iterators are equal.
         *
         * @param other iterator to compare with.
         * @return bool.
         */
        bool operator==(const Iterator& other) const { return mCurrentNode == other.mCurrentNode; }

        /**
         * Checks if iterators are not equal.
         *
         * @param other iterator to compare with.
         * @return bool.
         */
        bool operator!=(const Iterator& other) const { return !operator==(other); }

    private:
        friend class List<T>;
        friend class Iterator<!IsConst>;

        NodeType* mCurrentNode = nullptr;
    };

    /**
     * Creates empty list instance.
     */
    ListImpl()
    {
        mTerminalNode.mNext = &mTerminalNode;
        mTerminalNode.mPrev = &mTerminalNode;
    }

    ListImpl(const ListImpl& list) = default;

    virtual Node* AllocateNode() = 0;

    template <typename... Args>
    Node* CreateNode(Args&&... args)
    {
        auto node = AllocateNode();
        if (!node) {
            return nullptr;
        }

        new (&node->mBuffer) T(args...);

        return node;
    }

    void InsertNode(Node& posNode, Node& newNode)
    {
        if (posNode.mPrev) {
            posNode.mPrev->mNext = &newNode;
        }

        newNode.mPrev = posNode.mPrev;
        newNode.mNext = &posNode;
        posNode.mPrev = &newNode;

        mSize++;
    }

    void ReleaseNode(Node& node)
    {
        assert(node.mAllocated);

        node.mAllocated = false;
        reinterpret_cast<T*>(node.mBuffer)->~T();
    }

    void RemoveNode(Node& node)
    {
        if (node.mPrev) {
            node.mPrev->mNext = node.mNext;
        }

        if (node.mNext) {
            node.mNext->mPrev = node.mPrev;
        }

        ReleaseNode(node);

        mSize--;
    }

    Node   mTerminalNode;
    size_t mSize = 0;
};

/**
 * List instance.
 * @tparam T list type.
 */
template <typename T>
class List : public AlgorithmItf<T, typename ListImpl<T>::Iterator<false>, typename ListImpl<T>::Iterator<true>>,
             protected ListImpl<T> {
public:
    using Iterator      = typename ListImpl<T>::Iterator<false>;
    using ConstIterator = typename ListImpl<T>::Iterator<true>;

    /**
     * Creates empty list instance.
     */
    List() = default;

    /**
     * Creates list instance over the buffer.
     *
     * @param buffer underlying buffer.
     * @param size current list size.
     */
    explicit List(const Buffer& buffer) { SetBuffer(buffer); }

    /**
     * Creates list instance from another list.
     *
     * @param list another list instance.
     */
    List(const List& list) = default;

    /**
     * Assigns existing list to the current one.
     *
     * @param list existing list.
     * @return List&.
     */
    List& operator=(const List& list)
    {
        assert(mItems && list.Size() <= this->MaxSize());

        this->Clear();

        for (const auto& item : list) {
            this->PushBack(item);
        }

        return *this;
    }

    /**
     * Returns current list sizeq.
     *
     * @return size_t.
     */
    size_t Size() const override { return this->mSize; }

    /**
     * Returns maximum available list size.
     *
     * @return size_t.
     */
    size_t MaxSize() const override { return mMaxSize; }

    /**
     * Clears list.
     */
    void Clear()
    {
        while (this->mSize > 0) {
            Remove(Iterator(this->mTerminalNode.mNext));
        }
    }

    /**
     * Pushes item at the end of list.
     *
     * @param item item to push.
     * @return Error.
     */
    Error PushBack(const T& item)
    {
        auto node = ListImpl<T>::CreateNode(item);
        if (!node) {
            return ErrorEnum::eNoMemory;
        }

        this->InsertNode(this->mTerminalNode, *node);

        return ErrorEnum::eNone;
    }

    /**
     * Creates item at the end of list.
     *
     * @param args args of item constructor.
     * @return Error.
     */
    template <typename... Args>
    Error EmplaceBack(Args&&... args)
    {
        auto node = ListImpl<T>::CreateNode(args...);
        if (!node) {
            return ErrorEnum::eNoMemory;
        }

        this->InsertNode(this->mTerminalNode, *node);

        return ErrorEnum::eNone;
    }

    /**
     * Pushes item at the beginning of list.
     *
     * @param item item to push.
     * @return Error.
     */
    Error PushFront(const T& item)
    {
        auto node = ListImpl<T>::CreateNode(item);
        if (!node) {
            return ErrorEnum::eNoMemory;
        }

        this->InsertNode(*this->mTerminalNode.mNext, *node);

        return ErrorEnum::eNone;
    }

    /**
     * Creates item at the beginning of list.
     *
     * @param args args of item constructor.
     * @return Error.
     */
    template <typename... Args>
    Error EmplaceFront(Args&&... args)
    {
        auto node = ListImpl<T>::CreateNode(args...);
        if (!node) {
            return ErrorEnum::eNoMemory;
        }

        InsertNode(*this->mTerminalNode.mNext, *node);

        return ErrorEnum::eNone;
    }

    /**
     * Creates item at the specific position of list.
     *
     * @param pos iterator to the position.
     * @param args args of item constructor.
     * @return Error.
     */
    template <typename... Args>
    Error Emplace(ConstIterator pos, Args&&... args)
    {
        auto node = ListImpl<T>::CreateNode(args...);
        if (!node) {
            return ErrorEnum::eNoMemory;
        }

        ListImpl<T>::InsertNode(*const_cast<Node*>(pos.mCurrentNode), *node);

        return ErrorEnum::eNone;
    }

    /**
     * Inserts item at the specific position of list.
     *
     * @param pos iterator to the position.
     * @param item item to insert.
     * @return Error.
     */
    Error Insert(ConstIterator pos, const T& item)
    {
        auto node = ListImpl<T>::CreateNode(item);
        if (!node) {
            return ErrorEnum::eNoMemory;
        }

        ListImpl<T>::InsertNode(*const_cast<Node*>(pos.mCurrentNode), *node);

        return ErrorEnum::eNone;
    }

    /**
     * Removes item from list.
     *
     * @param item item to remove.
     * @return RetWithError<T*> pointer to next after deleted item.
     */
    RetWithError<Iterator> Remove(Iterator it)
    {
        auto nextNode = it.mCurrentNode->mNext;

        ListImpl<T>::RemoveNode(*it.mCurrentNode);

        return Iterator(nextNode);
    }

    /**
     * Pops item from the end of list.
     *
     * @return Error.
     */
    Error PopBack()
    {
        if (this->mSize == 0) {
            return ErrorEnum::eNotFound;
        }

        RemoveNode(*this->mTerminalNode.mPrev);

        return ErrorEnum::eNone;
    }

    /**
     * Pops item from the beginning of list.
     *
     * @return Error.
     */
    Error PopFront()
    {
        if (this->mSize == 0) {
            return ErrorEnum::eNotFound;
        }

        RemoveNode(*this->mTerminalNode.mNext);

        return ErrorEnum::eNone;
    }

    /**
     * Erases items range from list.
     *
     * @param first first item to erase.
     * @param first last item to erase.
     * @return next after deleted item iterator.
     */
    Iterator Erase(ConstIterator first, ConstIterator last) override
    {
        for (auto it = first; it != last; ++it) {
            ListImpl<T>::RemoveNode(*const_cast<RemoveConstType<Node>*>(it.mCurrentNode));
        }

        return Iterator(const_cast<Node*>(last.mCurrentNode));
    }

    /**
     * Erases item from list.
     *
     * @param it item to erase.
     * @return next after deleted item iterator.
     */
    Iterator Erase(ConstIterator it) override
    {
        auto next = it;

        return Erase(it, ++next);
    }

    // Used for range based loop.
    Iterator      begin(void) override { return Iterator(this->mTerminalNode.mNext); }
    Iterator      end(void) override { return Iterator(&this->mTerminalNode); }
    ConstIterator begin(void) const override { return ConstIterator(this->mTerminalNode.mNext); }
    ConstIterator end(void) const override { return ConstIterator(&this->mTerminalNode); }

protected:
    using Node = typename ListImpl<T>::Node;

    void SetBuffer(const Buffer& buffer, size_t maxSize = 0)
    {
        if (maxSize == 0) {
            mMaxSize = buffer.Size() / sizeof(Node);
        } else {
            mMaxSize = maxSize;
        }

        assert(mMaxSize != 0);

        mItems = static_cast<Node*>(buffer.Get());

        for (size_t i = 0; i < mMaxSize; i++) {
            mItems[i].mAllocated = false;
        }
    }

private:
    Node* AllocateNode() override
    {
        for (size_t i = 0; i < mMaxSize; i++) {
            if (!mItems[i].mAllocated) {
                mItems[i].mAllocated = true;

                return &mItems[i];
            }
        }

        return nullptr;
    }

    Node*  mItems   = nullptr;
    size_t mMaxSize = 0;
};

/**
 * Static list instance.
 *
 * @tparam T type of items.
 * @tparam cMaxSize max size.
 */
template <typename T, size_t cMaxSize>
class StaticList : public List<T> {
public:
    /**
     * Creates static list.
     */
    StaticList() { List<T>::SetBuffer(mBuffer); }

    /**
     * Creates static list from another static list.
     *
     * @param list list to create from.
     */
    StaticList(const StaticList& list)
        : List<T>()
    {
        List<T>::SetBuffer(mBuffer);
        List<T>::operator=(list);
    }

    /**
     * Destroys static list.
     */
    ~StaticList() { List<T>::Clear(); }

    /**
     * Assigns static list from another static list.
     *
     * @param list list to create from.
     */
    StaticList& operator=(const StaticList& list)
    {
        List<T>::operator=(list);

        return *this;
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates static list from another list.
     *
     * @param list list to create from.
     */
    StaticList(const List<T>& list)
    {
        List<T>::SetBuffer(mBuffer);
        List<T>::operator=(list);
    }

    /**
     * Assigns static list from another list.
     *
     * @param list list to assign from.
     */
    StaticList& operator=(const List<T>& list)
    {
        List<T>::operator=(list);

        return *this;
    }

private:
    StaticBuffer<cMaxSize * sizeof(typename List<T>::Node)> mBuffer;
};

} // namespace aos

#endif
