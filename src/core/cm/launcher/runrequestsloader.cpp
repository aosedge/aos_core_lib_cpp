/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "runrequestsloader.hpp"

namespace aos::cm::launcher {

void RunRequestsLoader::Init(StorageItf& storage, InstanceManager& instanceManager)
{
    mStorage         = &storage;
    mInstanceManager = &instanceManager;
}

Error RunRequestsLoader::Save(const Array<RunInstanceRequest>& requests)
{
    if (auto err = mRunRequests.Assign(requests); !err.IsNone()) {
        return err;
    }

    mRunRequests.Sort([](const RunInstanceRequest& left, const RunInstanceRequest& right) {
        return left.mPriority > right.mPriority || (left.mPriority == right.mPriority && left.mItemID < right.mItemID);
    });

    return mStorage->SaveRunRequests(mRunRequests);
}

Error RunRequestsLoader::Load()
{
    mRunRequests.Clear();

    return mStorage->LoadRunRequests(mRunRequests);
}

void RunRequestsLoader::CreateInstances(const Array<Node>& nodes, Array<SharedPtr<Instance>>& instances)
{
    instances.Clear();

    for (const auto& request : mRunRequests) {
        if (request.mNumInstances == 0 && request.mUpdateItemType == UpdateItemTypeEnum::eComponent) {
            for (const auto& node : nodes) {
                InstanceIdent instanceIdent {
                    request.mItemID, request.mSubjectInfo.mSubjectID, 0, request.mUpdateItemType};

                auto [instance, createErr]
                    = mInstanceManager->CreateInstance(request, node.GetInfo().mNodeID, instances);
                if (!createErr.IsNone()) {
                    LOG_ERR() << "Can't create instance" << Log::Field("instance", instanceIdent)
                              << Log::Field(createErr);

                    continue;
                }

                if (!mInstanceManager->IsSubjectEnabled(*instance)) {
                    LOG_WRN() << "Subject is not enabled for instance" << Log::Field("instance", instanceIdent);

                    mInstanceManager->DisableInstance(instance);
                    continue;
                }

                if (auto err = instances.PushBack(instance); !err.IsNone()) {
                    LOG_ERR() << "Can't add instance to array" << Log::Field("instance", instanceIdent)
                              << Log::Field(AOS_ERROR_WRAP(err));

                    continue;
                }
            }

            continue;
        }

        for (size_t i = 0; i < request.mNumInstances; i++) {
            InstanceIdent instanceIdent {request.mItemID, request.mSubjectInfo.mSubjectID, i, request.mUpdateItemType};

            auto [instance, createErr] = mInstanceManager->CreateInstance(request, i);
            if (!createErr.IsNone()) {
                LOG_ERR() << "Can't create instance" << Log::Field("instance", instanceIdent) << Log::Field(createErr);

                continue;
            }

            if (!mInstanceManager->IsSubjectEnabled(*instance)) {
                LOG_WRN() << "Subject is not enabled for instance" << Log::Field("instance", instanceIdent);

                mInstanceManager->DisableInstance(instance);
                continue;
            }

            if (auto err = instances.PushBack(instance); !err.IsNone()) {
                LOG_ERR() << "Can't add instance to array" << Log::Field("instance", instanceIdent)
                          << Log::Field(AOS_ERROR_WRAP(err));

                continue;
            }
        }
    }
}

} // namespace aos::cm::launcher
