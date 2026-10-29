/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_CURRENTNODE_FILEHWINFOPROVIDER_HPP_
#define AOS_CORE_IAM_CURRENTNODE_FILEHWINFOPROVIDER_HPP_

#include <core/common/types/common.hpp>

#include "itf/hwinfoprovider.hpp"

namespace aos::iam::currentnode {

/**
 * File-based hardware information provider.
 */
class FileHwInfoProvider : public HwInfoProviderItf {
public:
    /**
     * Initializes the file-based hardware information provider.
     *
     * @param hwIDPath path to the hardware ID file.
     * @return Error.
     */
    Error Init(const String& hwIDPath);

    /**
     * Gets hardware ID.
     *
     * @param hwID hardware ID.
     * @return Error.
     */
    Error GetHardwareID(String& hwID) override;

private:
    StaticString<cHardwareIDLen> mHwID;
};

} // namespace aos::iam::currentnode

#endif
