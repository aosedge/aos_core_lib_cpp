/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_IDENTHANDLER_ITF_IDENTMODULE_HPP_
#define AOS_CORE_IAM_IDENTHANDLER_ITF_IDENTMODULE_HPP_

#include <core/common/iamclient/itf/identprovider.hpp>
#include <core/common/tools/thread.hpp>

namespace aos::iam::identhandler {

/**
 * Ident module interface.
 *
 * Provides unit model, version and subjects. System ID is provided by ident handler.
 */
class IdentModuleItf {
public:
    /**
     * Destructor.
     */
    virtual ~IdentModuleItf() = default;

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
     * Returns unit model info.
     *
     * @param[out] info result unit model info.
     * @returns Error.
     */
    virtual Error GetUnitModelInfo(UnitModelInfo& info) = 0;

    /**
     * Returns subjects.
     *
     * @param[out] subjects result subjects.
     * @returns Error.
     */
    virtual Error GetSubjects(Array<StaticString<cIDLen>>& subjects) = 0;

    /**
     * Subscribes subjects listener.
     *
     * @param subjectsListener subjects listener.
     * @returns Error.
     */
    virtual Error SubscribeListener(iamclient::SubjectsListenerItf& subjectsListener)
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
    virtual Error UnsubscribeListener(iamclient::SubjectsListenerItf& subjectsListener)
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
