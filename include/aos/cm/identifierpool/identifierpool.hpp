/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_IDENTIFIER_POOL_HPP_
#define AOS_IDENTIFIER_POOL_HPP_

#include "aos/common/tools/noncopyable.hpp"
#include "aos/common/tools/thread.hpp"
#include "aos/common/types.hpp"

namespace aos::cm::identifierpool {

/**
 * Interface for identifier pool.
 */
class IdentifierPoolItf : public NonCopyable {
public:
    /**
     * Returns free identifier from pool.
     *
     * @return RetWithError<size_t>.
     */
    virtual RetWithError<size_t> GetFreeID() = 0;

    /**
     * Locks identifier in pool.
     *
     * @param id identifier to lock.
     * @return Error.
     */
    virtual Error LockID(size_t id) = 0;

    /**
     * Releases identifier in pool.
     *
     * @param id identifier to release.
     * @return Error.
     */
    virtual Error ReleaseID(size_t id) = 0;

    /**
     * Destructor.
     */
    virtual ~IdentifierPoolItf() = default;
};

/**
 * Identifier pool.
 */
class IdentifierPool : public IdentifierPoolItf {
public:
    /**
     * Identifier validator function type.
     */
    using IdValidator = bool (*)(size_t id);

    /**
     * Initializes identifier pool.
     *
     * @param idValidator identifier validator functor.
     * @return Error.
     */
    Error Init(IdValidator idValidator);

    /**
     * Returns free identifier from pool.
     *
     * @return RetWithError<size_t>.
     */
    RetWithError<size_t> GetFreeID() override;

    /**
     * Locks identifier in pool.
     *
     * @param id identifier to lock.
     * @return Error.
     */
    Error LockID(size_t id) override;

    /**
     * Releases identifier in pool.
     *
     * @param id identifier to release.
     * @return Error.
     */
    Error ReleaseID(size_t id) override;

private:
    static constexpr auto cIdsRangeBegin   = 5000;
    static constexpr auto cIdsRangeEnd     = 10000;
    static constexpr auto cMaxNumLockedIDs = cMaxNumInstances * cMaxNumServices;

    Mutex                                 mMutex;
    StaticArray<size_t, cMaxNumLockedIDs> mLockedIds;
    IdValidator                           mIdValidator {};
};

} // namespace aos::cm::identifierpool

#endif
