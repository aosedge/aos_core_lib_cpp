/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STUBS_SENDERSTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_SENDERSTUB_HPP_

#include <core/cm/launcher/itf/sender.hpp>

namespace aos::cm::launcher {

class SenderStub : public SenderItf {
public:
    Error SendOverrideEnvsStatuses(const OverrideEnvVarsStatuses& statuses) override
    {
        mStatuses = statuses;

        return ErrorEnum::eNone;
    }

    const OverrideEnvVarsStatuses& GetOverrideEnvVarsStatuses() const { return mStatuses; }

private:
    OverrideEnvVarsStatuses mStatuses;
};

} // namespace aos::cm::launcher

#endif
