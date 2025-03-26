/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/common/monitoring/average.hpp"

#include "log.hpp"

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

Error Average::Init(const PartitionInfoStaticArray& nodeDisks, size_t windowCount)
{
    mWindowCount = windowCount;
    if (mWindowCount == 0) {
        mWindowCount = 1;
    }

    mAverageNodeData.mMonitoringData.mPartitions = nodeDisks;
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

    for (auto& instance : data.mServiceInstances) {
        auto averageInstance = mAverageInstancesData.Find(instance.mInstanceIdent);
        if (averageInstance == mAverageInstancesData.end()) {
            LOG_ERR() << "Instance not found: instanceIdent=" << instance.mInstanceIdent;

            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }

        if (auto err = UpdateMonitoringData(averageInstance->mSecond.mMonitoringData, instance.mMonitoringData,
                averageInstance->mSecond.mIsInitialized);
            err.IsNone()) {
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

    data.mServiceInstances.Clear();

    // cppcheck-suppress unassignedVariable
    for (const auto& [instanceIdent, averageMonitoringData] : mAverageInstancesData) {
        if (auto err = data.mServiceInstances.EmplaceBack(InstanceMonitoringData {instanceIdent}); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err
            = GetMonitoringData(data.mServiceInstances.Back().mMonitoringData, averageMonitoringData.mMonitoringData);
            !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error Average::StartInstanceMonitoring(const InstanceMonitorParams& monitoringConfig)
{
    auto averageInstance = mAverageInstancesData.Find(monitoringConfig.mInstanceIdent);
    if (averageInstance != mAverageInstancesData.end()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eAlreadyExist, "instance monitoring already started"));
    }

    AverageData averageData {};

    for (const auto& partition : monitoringConfig.mPartitions) {
        if (auto err = averageData.mMonitoringData.mPartitions.PushBack(
                PartitionInfo {partition.mName, {}, partition.mPath, 0, 0});
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = mAverageInstancesData.Set(monitoringConfig.mInstanceIdent, averageData); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Average::StopInstanceMonitoring(const InstanceIdent& instanceIdent)
{
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

    if (data.mPartitions.Size() != newData.mPartitions.Size()) {
        return Error(ErrorEnum::eInvalidArgument, "service instances disk size mismatch");
    }

    for (size_t i = 0; i < data.mPartitions.Size(); ++i) {
        UpdateValue(data.mPartitions[i].mUsedSize, newData.mPartitions[i].mUsedSize, mWindowCount, isInitialized);
    }

    if (!isInitialized) {
        isInitialized = true;
    }

    return ErrorEnum::eNone;
}

Error Average::GetMonitoringData(MonitoringData& data, const MonitoringData& averageData) const
{
    data.mCPU = GetValue(averageData.mCPU, mWindowCount);
    data.mRAM = GetValue(averageData.mRAM, mWindowCount);
    data.mPartitions.Clear();

    for (const auto& disk : averageData.mPartitions) {
        data.mPartitions.EmplaceBack(PartitionInfo {
            disk.mName, disk.mTypes, disk.mPath, disk.mTotalSize, GetValue(disk.mUsedSize, mWindowCount)});
    }

    data.mDownload = GetValue(averageData.mDownload, mWindowCount);
    data.mUpload   = GetValue(averageData.mUpload, mWindowCount);

    return ErrorEnum::eNone;
}

} // namespace aos::monitoring
