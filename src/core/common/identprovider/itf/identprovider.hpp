/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IDENTPROVIDER_ITF_IDENTPROVIDER_HPP_
#define AOS_CORE_COMMON_IDENTPROVIDER_ITF_IDENTPROVIDER_HPP_

#include <core/common/types/types.hpp>

namespace aos::identprovider {

/**
 * Subjects observer interface.
 */
class SubjectsObserverItf {
public:
    /**
     * On subjects changed event.
     *
     * @param messages changed messages.
     * @returns Error.
     */
    virtual Error SubjectsChanged(const Array<StaticString<cIDLen>>& messages) = 0;

    /**
     * Destroys subjects changed observer interface.
     */
    virtual ~SubjectsObserverItf() = default;
};

/**
 * Ident provider interface.
 */
class IdentProviderItf {
public:
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
     * Subscribes to subjects changed events.
     *
     * @param observer subjects observer.
     * @returns Error.
     */
    virtual Error SubscribeSubjectsChanged(SubjectsObserverItf& observer) = 0;

    /**
     * Unsubscribes from subjects changed events.
     *
     * @param observer subjects observer.
     */
    virtual void UnsubscribeSubjectsChanged(SubjectsObserverItf& observer) = 0;

    /**
     * Destructor.
     */
    virtual ~IdentProviderItf() = default;
};

/** @}*/

} // namespace aos::identprovider

#endif
