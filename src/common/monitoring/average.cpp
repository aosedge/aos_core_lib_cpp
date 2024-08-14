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

    mAverageNodeData.mMonitoringData.mDisk = nodeDisks;
    mAverageInstancesData.Clear();

    return ErrorEnum::eNone;
}

Error Average::Update(const NodeMonitoringData& data)
{
    auto err
        = UpdateMonitoringData(mAverageNodeData.mMonitoringData, data.mMonitoringData, mAverageNodeData.mIsInitialized);
    if (!err.IsNone()) {
        return err;
    }

    if (data.mServiceInstances.Size() != mAverageInstancesData.Size()) {
        return Error(ErrorEnum::eInvalidArgument, "service instances size mismatch");
    }

    for (size_t i = 0; i < data.mServiceInstances.Size(); ++i) {
        if (!(err = UpdateMonitoringData(mAverageInstancesData[i].mAverageData.mMonitoringData,
                  data.mServiceInstances[i].mMonitoringData, mAverageInstancesData[i].mAverageData.mIsInitialized))
                 .IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error Average::GetData(NodeMonitoringData& data) const
{
    auto err = GetMonitoringData(data.mMonitoringData, mAverageNodeData.mMonitoringData);
    if (!err.IsNone()) {
        return err;
    }

    data.mServiceInstances.Clear();

    for (const auto& instance : mAverageInstancesData) {
        if (!(err = data.mServiceInstances.EmplaceBack(
                  InstanceMonitoringData {instance.mInstanceID, instance.mInstanceIdent, {}}))
                 .IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (!(err = GetMonitoringData(
                  data.mServiceInstances.Back().mValue.mMonitoringData, instance.mAverageData.mMonitoringData))
                 .IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error Average::StartInstanceMonitoring(const String& instanceID, const InstanceMonitorParams& monitoringConfig)
{
    auto findInstance = mAverageInstancesData.Find(
        [&instanceID](const AverageInstanceData& instance) { return instance.mInstanceID == instanceID; });
    if (findInstance.mError.IsNone()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eAlreadyExist, "instance monitoring already started"));
    }

    auto err = mAverageInstancesData.EmplaceBack(AverageInstanceData {instanceID, monitoringConfig.mInstanceIdent,
        AverageData {false, MonitoringData {0, 0, monitoringConfig.mPartitions, 0, 0}}});
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Average::StopInstanceMonitoring(const String& instanceID)
{
    auto err
        = mAverageInstancesData
              .Remove([&instanceID](const AverageInstanceData& instance) { return instance.mInstanceID == instanceID; })
              .mError;
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

    if (data.mDisk.Size() != newData.mDisk.Size()) {
        return Error(ErrorEnum::eInvalidArgument, "service instances disk size mismatch");
    }

    for (size_t i = 0; i < data.mDisk.Size(); ++i) {
        UpdateValue(data.mDisk[i].mUsedSize, newData.mDisk[i].mUsedSize, mWindowCount, isInitialized);
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
    data.mDisk.Clear();

    for (const auto& disk : averageData.mDisk) {
        data.mDisk.EmplaceBack(PartitionInfo {
            disk.mName, disk.mTypes, disk.mPath, disk.mTotalSize, GetValue(disk.mUsedSize, mWindowCount)});
    }

    data.mDownload = GetValue(averageData.mDownload, mWindowCount);
    data.mUpload   = GetValue(averageData.mUpload, mWindowCount);

    return ErrorEnum::eNone;
}

} // namespace aos::monitoring
