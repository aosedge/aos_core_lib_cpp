/*
\ * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_IDENTHANDLER_IDENTMODULE_HPP_
#define AOS_CORE_IAM_IDENTHANDLER_IDENTMODULE_HPP_

#include <core/common/iamclient/itf/identprovider.hpp>

namespace aos::iam::identhandler {

/**
 * Ident module interface.
 */
class IdentModuleItf : public iamclient::IdentProviderItf {
public:
    /**
     * Destructor.
     */
    ~IdentModuleItf() override = default;

    /**
     * Starts ident module.
     *
     * @return Error.
     */
    virtual Error Start() = 0;

    /**
     * Stops ident module.
     *
     * @return Error.
     */
    virtual Error Stop() = 0;
};

} // namespace aos::iam::identhandler

#endif
