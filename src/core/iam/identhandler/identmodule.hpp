/*
\ * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_IDENTHANDLER_IDENTMODULE_HPP_
#define AOS_CORE_IAM_IDENTHANDLER_IDENTMODULE_HPP_

#include <core/common/iamclient/itf/identprovider.hpp>
#include <core/common/tools/thread.hpp>

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

    /**
     * Subscribes subjects listener.
     *
     * @param subjectsListener subjects listener.
     * @returns Error.
     */
    Error SubscribeListener(iamclient::SubjectsListenerItf& subjectsListener) override
    {
        LockGuard lock {mMutex};

        if (const auto it = mSubscribers.Find(&subjectsListener); it != mSubscribers.end()) {
            return AOS_ERROR_WRAP(ErrorEnum::eAlreadyExist);
        }

        return mSubscribers.EmplaceBack(&subjectsListener);
    }

    /**
     * Unsubscribes subjects listener.
     *
     * @param subjectsListener subjects listener.
     * @returns Error.
     */
    Error UnsubscribeListener(iamclient::SubjectsListenerItf& subjectsListener) override
    {
        LockGuard lock {mMutex};

        if (mSubscribers.Remove(&subjectsListener) == 0) {
            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }

        return ErrorEnum::eNone;
    }

protected:
    void NotifySubjectsChanged(const Array<StaticString<cIDLen>>& subjects)
    {
        LockGuard lock {mMutex};

        for (auto* subscriber : mSubscribers) {
            if (subscriber) {
                subscriber->SubjectsChanged(subjects);
            }
        }
    }

private:
    static constexpr auto cSubscribersMaxNum = 4;

    Mutex                                                            mMutex;
    StaticArray<iamclient::SubjectsListenerItf*, cSubscribersMaxNum> mSubscribers;
};

} // namespace aos::iam::identhandler

#endif
