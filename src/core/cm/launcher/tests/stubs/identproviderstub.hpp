/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_TESTS_STUBS_IDENTPROVIDERSTUB_HPP_
#define AOS_CORE_CM_LAUNCHER_TESTS_STUBS_IDENTPROVIDERSTUB_HPP_

#include <core/common/iamclient/itf/identprovider.hpp>

#include <vector>

namespace aos::iamclient {

/**
 * Identification provider stub.
 */
class IdentProviderStub : public IdentProviderItf {
public:
    Error GetSystemInfo(SystemInfo& info) override
    {
        (void)info;

        return ErrorEnum::eNone;
    }

    Error GetSubjects(Array<StaticString<cIDLen>>& subjects) override
    {
        auto err = subjects.Assign(Array<StaticString<cIDLen>>(mSubjects.data(), mSubjects.size()));

        return AOS_ERROR_WRAP(err);
    }

    Error SubscribeListener(SubjectsListenerItf& subjectsListener) override
    {
        mSubjectsListener = &subjectsListener;

        return ErrorEnum::eNone;
    }

    Error UnsubscribeListener(SubjectsListenerItf& subjectsListener) override
    {
        if (mSubjectsListener != &subjectsListener) {
            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }

        mSubjectsListener = nullptr;

        return ErrorEnum::eNone;
    }

    Error SetSubjects(const std::vector<StaticString<cIDLen>>& subjects)
    {
        mSubjects = subjects;

        auto subjectsArray = Array<StaticString<cIDLen>>(mSubjects.data(), mSubjects.size());

        if (mSubjectsListener != nullptr) {
            mSubjectsListener->SubjectsChanged(subjectsArray);
        }

        return ErrorEnum::eNone;
    }

private:
    SubjectsListenerItf*              mSubjectsListener {};
    std::vector<StaticString<cIDLen>> mSubjects;
};

} // namespace aos::iamclient

#endif
