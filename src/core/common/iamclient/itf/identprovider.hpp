/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IAMCLIENT_ITF_IDENTPROVIDER_HPP_
#define AOS_CORE_COMMON_IAMCLIENT_ITF_IDENTPROVIDER_HPP_

#include <core/common/types/common.hpp>

namespace aos::iamclient {

/**
 * Subjects listener interface.
 */
class SubjectsListenerItf {
public:
    /**
     * Destroys subjects changed listener interface.
     */
    virtual ~SubjectsListenerItf() = default;

    /**
     * Subjects listener interface.
     *
     * @param subjects subject changed messages.
     * @returns Error.
     */
    virtual Error SubjectsChanged(const Array<StaticString<cIDLen>>& subjects) = 0;
};

/**
 * Ident provider interface.
 */
class IdentProviderItf {
public:
    /**
     * Destroys ident provider interface.
     */
    virtual ~IdentProviderItf() = default;

    /**
     * Returns System ID.
     *
     * @returns RetWithError<StaticString>.
     */
    virtual RetWithError<StaticString<cIDLen>> GetSystemID() = 0;

    /**
     * Returns unit model.
     *
     * @returns RetWithError<StaticString>.
     */
    virtual RetWithError<StaticString<cIDLen>> GetUnitModel() = 0;

    /**
     * Returns subjects.
     *
     * @param[out] subjects result subjects.
     * @returns Error.
     */
    virtual Error GetSubjects(Array<StaticString<cIDLen>>& subjects) = 0;

    /**
     * Subscribes subjects listener.
     *
     * @param subjectsListener subjects listener.
     * @returns Error.
     */
    virtual Error SubscribeListener(SubjectsListenerItf& subjectsListener) = 0;

    /**
     * Unsubscribes subjects listener.
     *
     * @param subjectsListener subjects listener.
     * @returns Error.
     */
    virtual Error UnsubscribeListener(SubjectsListenerItf& subjectsListener) = 0;
};

} // namespace aos::iamclient

#endif
