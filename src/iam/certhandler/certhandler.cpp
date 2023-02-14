/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "certhandler.hpp"
#include "log.hpp"

namespace aos {
namespace iam {
namespace certhandler {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error CertHandler::CreateKey()
{
    LOG_DBG() << "Create key";

    return ErrorEnum::eNone;
}

} // namespace certhandler
} // namespace iam
} // namespace aos
