/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_STORAGESTATE_STORAGESTATE_HPP_
#define AOS_CORE_CM_STORAGESTATE_STORAGESTATE_HPP_

#include <core/common/crypto/itf/hash.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/thread.hpp>

#include "config.hpp"
#include "itf/sender.hpp"
#include "itf/state.hpp"
#include "itf/storage.hpp"
#include "itf/storagestate.hpp"

namespace aos::cm::storagestate {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Storage state.
 */
class StorageState : public StateItf, public StorageStateItf, private fs::FSEventSubscriberItf, private NonCopyable {
public:
    /**
     * Initializes storage state instance.
     *
     * @param config config object.
     * @param storage storage instance.
     * @param sender sender instance.
     * @param fsPlatform file system platform instance.
     * @param fsWatcher file system watcher instance.
     * @param cryptoProvider crypto provider instance.
     * @return Error.
     */
    Error Init(const Config& config, StorageItf& storage, SenderItf& sender, fs::FSPlatformItf& fsPlatform,
        fs::FSWatcherItf& fsWatcher, crypto::HasherItf& hasher);

    /**
     * Starts storage state instance.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops storage state instance.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Updates storage state with new state.
     *
     * @param instanceIdent instance ident.
     * @param state state content.
     * @param checksum state checksum.
     * @return Error.
     */
    Error UpdateState(const InstanceIdent& instanceIdent, const String& state, const String& checksum) override;

    /**
     * Accepts state from storage.
     *
     * @param instanceIdent instance ident.
     * @param checksum state checksum.
     * @param result state result.
     * @param reason reason of the result.
     * @return Error.
     */
    Error AcceptState(
        const InstanceIdent& instanceIdent, const String& checksum, StateResult result, const String& reason) override;

    /**
     * Setups storage state instance.
     *
     * @param instanceIdent instance ident.
     * @param setupParams setup parameters.
     * @param storagePath[out] storage path.
     * @param statePath[out] state path.
     * @return Error.
     */
    Error Setup(const InstanceIdent& instanceIdent, const SetupParams& setupParams, String& storagePath,
        String& statePath) override;

    /**
     * Clean-ups storage state instance.
     *
     * @param instanceIdent instance ident.
     * @return Error.
     */
    Error Cleanup(const InstanceIdent& instanceIdent) override;

    /**
     * Removes storage state instance.
     *
     * @param instanceIdent instance ident.
     * @return Error.
     */
    Error Remove(const InstanceIdent& instanceIdent) override;

    /**
     * Returns instance's checksum.
     *
     * @param instanceIdent instance ident.
     * @param checkSum[out] checksum.
     * @return Error
     */
    Error GetInstanceCheckSum(const InstanceIdent& instanceIdent, String& checkSum) override;

    /**
     * Returns total state size in bytes.
     *
     * @return RetWithError<size_t>.
     */
    RetWithError<size_t> GetTotalStateSize() const override;

    /**
     * Returns total storage size in bytes.
     *
     * @return RetWithError<size_t>.
     */
    RetWithError<size_t> GetTotalStorageSize() const override;

    /**
     * Checks if storage and state are on the same partition.
     *
     * @return bool.
     */
    bool IsSamePartition() const override;

    /**
     * Destructor.
     */
    ~StorageState() = default;

private:
    static constexpr auto cStateFilename          = "state.dat";
    static constexpr auto cStateChangeTimeout     = Time::cSeconds * 1;
    static constexpr auto cHashAlgorithm          = crypto::HashEnum::eSHA3_224;
    static constexpr auto cNumSendNewStateThreads = 1;
    static constexpr auto cInstanceStringLen      = 8;
    static constexpr auto cAllocatorSize          = sizeof(InstanceInfoArray) + sizeof(StaticString<cStateLen>);

    struct State {
        State(const InstanceIdent& instanceIdent, const String& filePath, size_t quota)
            : mInstanceIdent(instanceIdent)
            , mFilePath(filePath)
            , mQuota(quota)
        {
        }

        State(State&&)            = default;
        State& operator=(State&&) = default;

        InstanceIdent                         mInstanceIdent;
        StaticString<cFilePathLen>            mFilePath;
        size_t                                mQuota = {};
        StaticString<crypto::cSHA2DigestSize> mChecksum;

        friend Log& operator<<(Log& log, const State& state)
        {
            return log << Log::Field("instanceIdent", state.mInstanceIdent) << Log::Field("path", state.mFilePath)
                       << Log::Field("quota", state.mQuota);
        }
    };

    void  OnFSEvent(const String& path, const Array<fs::FSEvent>& events) override;
    Error InitStateWatching();
    Error PrepareState(
        const InstanceIdent& instanceIdent, const SetupParams& setupParams, const String& checksum, String& statePath);
    Error PrepareStorage(const InstanceIdent& instanceIdent, const SetupParams& setupParams, String& storagePath) const;
    Error CheckChecksumAndSendUpdateRequest(const State& state);
    Error CreateStateFileIfNotExist(const String& path, const SetupParams& params) const;
    Error StartStateWatching(const InstanceIdent& instanceIdent, const String& path, size_t quota);
    Error StopStateWatching(const InstanceIdent& instanceIdent);
    Error SetQuotas(const SetupParams& setupParams);
    Error SendNewStateIfFileChanged(State& state);
    Error RemoveFromSystem(const InstanceIdent& instanceIdent);
    bool  QuotasAreEqual(const InstanceInfo& lhs, const SetupParams& rhs) const;
    Error ValidateChecksum(const String& text, const String& checksum);
    StaticString<cFilePathLen> GetStateDir(const InstanceIdent& instanceIdent) const;
    StaticString<cFilePathLen> GetStateFilePath(const InstanceIdent& instanceIdent) const;
    StaticString<cFilePathLen> GetStoragePath(const InstanceIdent& instanceIdent) const;
    Error                      CalculateChecksum(const String& data, String& checksum);

    StaticAllocator<cAllocatorSize>                                         mAllocator;
    ThreadPool<cNumSendNewStateThreads, cMaxNumInstances, 2 * cFilePathLen> mThreadPool;
    Mutex                                                                   mMutex;
    Config                                                                  mConfig;
    StorageItf*                                                             mStorage                        = {};
    SenderItf*                                                              mMessageSender                  = {};
    fs::FSPlatformItf*                                                      mFSPlatform                     = {};
    fs::FSWatcherItf*                                                       mFSWatcher                      = {};
    crypto::HasherItf*                                                      mHasher                         = {};
    bool                                                                    mStateAndStorageOnSamePartition = {};
    StaticArray<State, cMaxNumInstances>                                    mStates;
};

/** @}*/

} // namespace aos::cm::storagestate

#endif
