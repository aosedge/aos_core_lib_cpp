/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_CURRENTNODE_HWINFOMODULES_FILEHWINFOPROVIDER_HPP_
#define AOS_CORE_IAM_CURRENTNODE_HWINFOMODULES_FILEHWINFOPROVIDER_HPP_

#include <core/common/types/common.hpp>
#include <core/iam/currentnode/itf/hwinfoprovider.hpp>

#include "config.hpp"

namespace aos::iam::currentnode {

/**
 * File-based hardware information provider.
 */
class FileHWInfoProvider : public HWInfoProviderItf {
public:
    /**
     * Initializes the file-based hardware information provider.
     *
     * @param config file hardware info provider config.
     * @return Error.
     */
    Error Init(const FileHWInfoProviderConfig& config);

    /**
     * Gets hardware ID.
     *
     * @param hwID hardware ID.
     * @return Error.
     */
    Error GetHWID(String& hwID) override;

private:
    StaticString<cIDLen> mHwID;
};

} // namespace aos::iam::currentnode

#endif
