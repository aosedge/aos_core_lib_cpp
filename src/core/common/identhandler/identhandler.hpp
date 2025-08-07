/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IDENTHANDLER_IDENTHANDLER_HPP_
#define AOS_CORE_COMMON_IDENTHANDLER_IDENTHANDLER_HPP_

#include <core/common/types/types.hpp>

namespace aos::identhandler {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Subjects observer interface.
 */
class SubjectsObserverItf {
public:
    /**
     * Subjects observer interface.
     *
     * @param[in] messages subject changed messages.
     * @returns Error.
     */
    virtual Error SubjectsChanged(const Array<StaticString<cSubjectIDLen>>& messages) = 0;

    /**
     * Destroys subjects changed observer interface.
     */
    virtual ~SubjectsObserverItf() = default;
};

/**
 * Subjects publisher interface.
 */
class SubjectsPublisherItf {
public:
    /**
     * Destructor
     */
    virtual ~SubjectsPublisherItf() = default;

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
};

/**
 * Ident handler interface.
 */
class IdentHandlerItf {
public:
    /**
     * Starts ident handler.
     *
     * @returns Error.
     */
    virtual Error Start() { return ErrorEnum::eNone; }

    /**
     * Stops ident handler.
     *
     * @returns Error.
     */
    virtual Error Stop() { return ErrorEnum::eNone; }

    /**
     * Returns System ID.
     *
     * @returns RetWithError<StaticString>.
     */
    virtual RetWithError<StaticString<cSystemIDLen>> GetSystemID() = 0;

    /**
     * Returns unit model.
     *
     * @returns RetWithError<StaticString>.
     */
    virtual RetWithError<StaticString<cUnitModelLen>> GetUnitModel() = 0;

    /**
     * Returns subjects.
     *
     * @param[out] subjects result subjects.
     * @returns Error.
     */
    virtual Error GetSubjects(Array<StaticString<cSubjectIDLen>>& subjects) = 0;

    /**
     * Destroys ident handler interface.
     */
    virtual ~IdentHandlerItf() = default;
};

/** @}*/

} // namespace aos::identhandler

#endif
