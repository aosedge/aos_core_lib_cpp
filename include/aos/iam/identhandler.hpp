/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_IDENTHANDLER_HPP_
#define AOS_IDENTHANDLER_HPP_

#include "aos/common/types.hpp"
#include "aos/iam/config.hpp"

namespace aos {
namespace iam {
namespace identhandler {

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
 * Ident handler interface.
 */
class IdentHandlerItf {
public:
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

} // namespace identhandler
} // namespace iam
} // namespace aos

#endif
