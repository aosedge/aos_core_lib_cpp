/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_ALERTS_HPP_
#define AOS_ALERTS_HPP_

#include "aos/common/cloudprotocol/alerts.hpp"
#include "aos/common/types.hpp"

namespace aos::alerts {

/**
 * Storage interface.
 */
class StorageItf {
public:
    /**
     * Sets journal cursor.
     *
     * @param cursor journal cursor.
     * @return Error.
     */
    virtual Error SetJournalCursor(const String& cursor) = 0;

    /**
     * Gets journal cursor.
     *
     * @param cursor[out] journal cursor.
     * @return Error.
     */
    virtual Error GetJournalCursor(String& cursor) const = 0;

    /**
     * Destructor.
     */
    virtual ~StorageItf() = default;
};

/**
 * Sender interface.
 */
class SenderItf {
public:
    /**
     * Sends alert data.
     *
     * @param alert alert variant.
     * @return Error.
     */
    virtual Error SendAlert(const cloudprotocol::AlertVariant& alert) = 0;

    /**
     * Destructor.
     */
    virtual ~SenderItf() = default;
};

} // namespace aos::alerts

#endif
