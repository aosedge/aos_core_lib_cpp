/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "runrequestsloader.hpp"

namespace aos::cm::launcher {

void RunRequestsLoader::Init(
    StorageItf& storage, InstanceManager& instanceManager, ImageInfoProvider& imageInfoProvider)
{
    mStorage           = &storage;
    mInstanceManager   = &instanceManager;
    mImageInfoProvider = &imageInfoProvider;
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
            GenerateInstances(request, nodes, instances);

            if (auto err = GenerateInstances(request, nodes, instances); !err.IsNone()) {
                LOG_ERR() << "Can't generate instances" << Log::Field("instance", instanceIdent) << Log::Field(err);
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

Error RunRequestsLoader::GenerateInstances(
    const RunInstanceRequest& request, const Array<Node>& nodes, Array<SharedPtr<Instance>>& instances)
{
    auto imageIndex = MakeUnique<oci::ImageIndex>(&mAllocator);

    if (auto err = mImageInfoProvider->GetImageIndex(request.mItemID, request.mVersion, *imageIndex); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto combinedRuntimes = MakeUnique<CombinedRuntimesArray>(&mAllocator);
    auto itemConfig       = MakeUnique<oci::ItemConfig>(&mAllocator);

    if (auto err = CombinedRuntimes(*imageIndex, *combinedRuntimes, *itemConfig); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    CreateGeneratedInstances(request, nodes, *combinedRuntimes, instances);

    return ErrorEnum::eNone;
}

Error RunRequestsLoader::CombinedRuntimes(
    const oci::ImageIndex& imageIndex, CombinedRuntimesArray& combinedRuntimes, oci::ItemConfig& itemConfig)
{
    for (const auto& manifest : imageIndex.mManifests) {
        if (auto err = mImageInfoProvider->GetItemConfig(manifest, itemConfig); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        for (const auto& runtime : itemConfig.mRuntimes) {
            if (combinedRuntimes.Contains(runtime)) {
                continue;
            }

            if (auto err = combinedRuntimes.PushBack(runtime); !err.IsNone()) {
                LOG_WRN() << "Can't add runtime to combined list" << Log::Field("runtime", runtime)
                          << Log::Field(AOS_ERROR_WRAP(err));

                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

void RunRequestsLoader::CreateGeneratedInstances(const RunInstanceRequest& request, const Array<Node>& nodes,
    const CombinedRuntimesArray& combinedRuntimes, Array<SharedPtr<Instance>>& instances)
{
    for (const auto& node : nodes) {
        for (const auto& runtimeInfo : node.GetInfo().mRuntimes) {
            if (!combinedRuntimes.Contains(runtimeInfo.mRuntimeType)) {
                continue;
            }

            InstanceIdent instanceIdent {request.mItemID, request.mSubjectInfo.mSubjectID, 0, request.mUpdateItemType};

            auto [instance, createErr]
                = mInstanceManager->CreateInstance(request, node.GetInfo().mNodeID, runtimeInfo.mRuntimeID, instances);
            if (!createErr.IsNone()) {
                LOG_ERR() << "Can't create instance" << Log::Field("instance", instanceIdent)
                          << Log::Field("runtime", runtimeInfo.mRuntimeType) << Log::Field(createErr);

                continue;
            }

            if (!mInstanceManager->IsSubjectEnabled(*instance)) {
                LOG_WRN() << "Subject is not enabled for instance" << Log::Field("instance", instanceIdent)
                          << Log::Field("runtime", runtimeInfo.mRuntimeType);

                mInstanceManager->DisableInstance(instance);
                continue;
            }

            if (auto err = instances.PushBack(instance); !err.IsNone()) {
                LOG_ERR() << "Can't add instance to array" << Log::Field("instance", instanceIdent)
                          << Log::Field("runtime", runtimeInfo.mRuntimeType) << Log::Field(AOS_ERROR_WRAP(err));

                continue;
            }
        }
    }
}

} // namespace aos::cm::launcher
