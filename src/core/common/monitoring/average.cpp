/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "average.hpp"

namespace aos::monitoring {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

template <typename T>
constexpr T Round(double value)
{
    return static_cast<T>(value + 0.5);
}

template <typename T>
T GetValue(const T& value, size_t window)
{
    return Round<T>(static_cast<double>(value) / window);
}

template <>
double GetValue(const double& value, size_t window)
{
    return value / window;
}

template <typename T>
void UpdateValue(T& value, T newValue, size_t window, bool isInitialized)
{
    if (!isInitialized) {
        value = newValue * window;
    } else {
        value -= GetValue(value, window);
        value += newValue;
    }
}

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Average::Init(size_t windowCount)
{
    mWindowCount = windowCount;
    if (mWindowCount == 0) {
        mWindowCount = 1;
    }

    mAverageInstancesData.Clear();

    return ErrorEnum::eNone;
}

Error Average::Update(const NodeMonitoringData& data)
{
    if (auto err
        = UpdateMonitoringData(mAverageNodeData.mMonitoringData, data.mMonitoringData, mAverageNodeData.mIsInitialized);
        !err.IsNone()) {
        return err;
    }

    for (auto& instance : data.mInstances) {
        auto averageInstance = mAverageInstancesData.Find(instance.mInstanceIdent);
        if (averageInstance == mAverageInstancesData.end()) {
            LOG_ERR() << "Instance not found" << Log::Field("ident", instance.mInstanceIdent);

            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }

        if (auto err = UpdateMonitoringData(averageInstance->mSecond.mMonitoringData, instance.mMonitoringData,
                averageInstance->mSecond.mIsInitialized);
            !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error Average::GetData(NodeMonitoringData& data) const
{
    if (auto err = GetMonitoringData(data.mMonitoringData, mAverageNodeData.mMonitoringData); !err.IsNone()) {
        return err;
    }

    data.mInstances.Clear();

    for (const auto& [instanceIdent, averageMonitoringData] : mAverageInstancesData) {
        if (auto err = data.mInstances.EmplaceBack(instanceIdent); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = GetMonitoringData(data.mInstances.Back().mMonitoringData, averageMonitoringData.mMonitoringData);
            !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error Average::StartInstanceMonitoring(const InstanceIdent& instanceIdent)
{
    LOG_DBG() << "Start average instance monitoring" << Log::Field("ident", instanceIdent);

    auto averageInstance = mAverageInstancesData.Find(instanceIdent);
    if (averageInstance != mAverageInstancesData.end()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eAlreadyExist, "instance monitoring already started"));
    }

    auto averageData = MakeUnique<AverageData>(&mAllocator);

    if (auto err = mAverageInstancesData.Set(instanceIdent, *averageData); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Average::StopInstanceMonitoring(const InstanceIdent& instanceIdent)
{
    LOG_DBG() << "Stop average instance monitoring" << Log::Field("ident", instanceIdent);

    auto err = mAverageInstancesData.Remove(instanceIdent);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error Average::UpdateMonitoringData(MonitoringData& data, const MonitoringData& newData, bool& isInitialized)
{
    UpdateValue(data.mCPU, newData.mCPU, mWindowCount, isInitialized);
    UpdateValue(data.mRAM, newData.mRAM, mWindowCount, isInitialized);
    UpdateValue(data.mDownload, newData.mDownload, mWindowCount, isInitialized);
    UpdateValue(data.mUpload, newData.mUpload, mWindowCount, isInitialized);

    for (const auto& partition : newData.mPartitions) {
        auto it = data.mPartitions.FindIf([&partition](const PartitionUsage& p) { return p.mName == partition.mName; });

        if (it == data.mPartitions.end()) {
            if (auto err = data.mPartitions.EmplaceBack(partition); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            it = &data.mPartitions.Back();
        }

        UpdateValue(it->mUsedSize, partition.mUsedSize, mWindowCount, isInitialized);
    }

    isInitialized = true;

    return ErrorEnum::eNone;
}

Error Average::GetMonitoringData(MonitoringData& data, const MonitoringData& averageData) const
{
    data.mCPU      = GetValue(averageData.mCPU, mWindowCount);
    data.mRAM      = GetValue(averageData.mRAM, mWindowCount);
    data.mDownload = GetValue(averageData.mDownload, mWindowCount);
    data.mUpload   = GetValue(averageData.mUpload, mWindowCount);

    data.mPartitions.Clear();

    for (const auto& disk : averageData.mPartitions) {
        if (auto err = data.mPartitions.EmplaceBack(disk); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        data.mPartitions.Back().mUsedSize = GetValue(disk.mUsedSize, mWindowCount);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::monitoring
