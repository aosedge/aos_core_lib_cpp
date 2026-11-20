/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <mutex>
#include <queue>

#include <gtest/gtest.h>

#include <core/cm/storagestate/storagestate.hpp>
#include <core/common/crypto/cryptoprovider.hpp>
#include <core/common/tests/mocks/fsmock.hpp>
#include <core/common/tests/stubs/testallocator.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>

using namespace testing;

namespace aos::cm::storagestate {
namespace {
/***********************************************************************************************************************
 * Constants
 **********************************************************************************************************************/

const auto                 cTestDir    = std::filesystem::path("storage_state");
const auto                 cStorageDir = cTestDir / "storage";
const auto                 cStateDir   = cTestDir / "state";
static const InstanceIdent cInstanceIdent {{"itemID"}, "1.0.0", {"subjectID"}, 1, UpdateItemTypeEnum::eService};

/***********************************************************************************************************************
 * Stubs
 **********************************************************************************************************************/

class StorageStub : public aos::cm::storagestate::StorageItf {
public:
    Error AddStorageStateInfo(const InstanceInfo& storageStateInfo) override
    {
        std::lock_guard lock {mMutex};

        if (!mAddErr.IsNone()) {
            return mAddErr;
        }

        LOG_DBG() << "Add storage state info" << Log::Field("instanceIdent", storageStateInfo.mInstanceIdent);

        if (auto it = mStorageStateInfos.find(storageStateInfo.mInstanceIdent); it != mStorageStateInfos.end()) {
            return ErrorEnum::eAlreadyExist;
        }

        mStorageStateInfos[storageStateInfo.mInstanceIdent] = storageStateInfo;

        return ErrorEnum::eNone;
    }

    Error RemoveStorageStateInfo(const InstanceIdent& instanceIdent) override
    {
        std::lock_guard lock {mMutex};

        if (!mRemoveErr.IsNone()) {
            return mRemoveErr;
        }

        LOG_DBG() << "Remove storage state info" << Log::Field("instanceIdent", instanceIdent);

        auto it = mStorageStateInfos.find(instanceIdent);
        if (it == mStorageStateInfos.end()) {
            return ErrorEnum::eNotFound;
        }

        mStorageStateInfos.erase(it);

        return ErrorEnum::eNone;
    }

    Error GetAllStorageStateInfo(Array<InstanceInfo>& storageStateInfos) override
    {
        std::lock_guard lock {mMutex};

        if (!mGetAllErr.IsNone()) {
            return mGetAllErr;
        }

        LOG_DBG() << "Get all storage state infos";

        for (const auto& [instanceIdent, storageStateInfo] : mStorageStateInfos) {
            if (auto err = storageStateInfos.PushBack(storageStateInfo); !err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    Error GetStorageStateInfo(const InstanceIdent& instanceIdent, InstanceInfo& storageStateInfo) override
    {
        std::lock_guard lock {mMutex};

        if (!mGetErr.IsNone()) {
            return mGetErr;
        }

        LOG_DBG() << "Get storage state info" << Log::Field("instanceIdent", instanceIdent);

        auto it = mStorageStateInfos.find(instanceIdent);
        if (it == mStorageStateInfos.end()) {
            return ErrorEnum::eNotFound;
        }

        storageStateInfo = it->second;

        return ErrorEnum::eNone;
    }

    Error UpdateStorageStateInfo(const InstanceInfo& storageStateInfo) override
    {
        std::lock_guard lock {mMutex};

        if (!mUpdateErr.IsNone()) {
            return mUpdateErr;
        }

        LOG_DBG() << "Update storage state info" << Log::Field("instanceIdent", storageStateInfo.mInstanceIdent);

        auto it = mStorageStateInfos.find(storageStateInfo.mInstanceIdent);
        if (it == mStorageStateInfos.end()) {
            return ErrorEnum::eNotFound;
        }

        it->second = storageStateInfo;

        return ErrorEnum::eNone;
    }

    bool Contains(const std::function<bool(const InstanceInfo&)>& predicate)
    {
        std::lock_guard lock {mMutex};

        LOG_DBG() << "Check if storage state info contains";

        return std::find_if(mStorageStateInfos.begin(), mStorageStateInfos.end(), [predicate](const auto& pair) {
            return predicate(pair.second);
        }) != mStorageStateInfos.end();
    }

    Error mAddErr;
    Error mRemoveErr;
    Error mGetAllErr;
    Error mGetErr;
    Error mUpdateErr;

private:
    std::mutex                            mMutex;
    std::map<InstanceIdent, InstanceInfo> mStorageStateInfos;
};

template <class T>
class MessageVisitor : public StaticVisitor<bool> {
public:
    MessageVisitor(T& message, std::function<bool(const T&)> comparator)
        : mMessage(message)
        , mComparator(comparator)
    {
    }

    Res Visit(const T& variantMsg) const
    {
        if (mComparator && !mComparator(variantMsg)) {
            return false;
        }

        mMessage = variantMsg;

        return true;
    }

    template <class U>
    Res Visit(const U&) const
    {
        return false;
    }

private:
    T&                            mMessage;
    std::function<bool(const T&)> mComparator;
};

class SenderMock : public SenderItf {
public:
    MOCK_METHOD(Error, SendStateRequest, (const StateRequest& request), (override));
    MOCK_METHOD(Error, SendNewState, (const NewState& state), (override));
};

class HashStub : public crypto::HashItf {
public:
    HashStub(const Error& updateErr, const Error& finalizeErr)
        : mUpdateErr(updateErr)
        , mFinalizeErr(finalizeErr)
    {
    }

    Error Update(const Array<uint8_t>& data) override
    {
        (void)data;

        return mUpdateErr;
    }

    Error Finalize(Array<uint8_t>& hash) override
    {
        (void)hash;

        return mFinalizeErr;
    }

private:
    Error mUpdateErr;
    Error mFinalizeErr;
};

class HasherStub : public crypto::HasherItf {
public:
    HasherStub(crypto::HasherItf& hasher, AllocatorItf& allocator)
        : mHasher(hasher)
        , mAllocator(allocator)
    {
    }

    RetWithError<UniquePtr<crypto::HashItf>> CreateHash(const crypto::Hash& algorithm) override
    {
        std::unique_lock lock {mMutex};

        mCalls++;
        mCondVar.notify_all();

        if (!mCreateErr.IsNone()) {
            return {{}, mCreateErr};
        }

        if (mUpdateErr.IsNone() && mFinalizeErr.IsNone()) {
            return mHasher.CreateHash(algorithm);
        }

        auto hash = MakeUnique<HashStub>(&mAllocator, mUpdateErr, mFinalizeErr);

        return {UniquePtr<crypto::HashItf>(Move(hash)), ErrorEnum::eNone};
    }

    void SetErrors(
        const Error& createErr, const Error& updateErr = ErrorEnum::eNone, const Error& finalizeErr = ErrorEnum::eNone)
    {
        std::lock_guard lock {mMutex};

        mCreateErr   = createErr;
        mUpdateErr   = updateErr;
        mFinalizeErr = finalizeErr;
        mCalls       = 0;
    }

    bool WaitCalls(size_t count)
    {
        std::unique_lock lock {mMutex};

        return mCondVar.wait_for(lock, std::chrono::seconds(10), [&] { return mCalls >= count; });
    }

private:
    crypto::HasherItf&      mHasher;
    AllocatorItf&           mAllocator;
    std::mutex              mMutex;
    std::condition_variable mCondVar;
    size_t                  mCalls = 0;
    Error                   mCreateErr;
    Error                   mUpdateErr;
    Error                   mFinalizeErr;
};

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

std::filesystem::path ToStatePath(const InstanceIdent& instanceIdent)
{
    return cStateDir / instanceIdent.mItemID.CStr() / instanceIdent.mSubjectID.CStr()
        / std::to_string(instanceIdent.mInstance) / "state.dat";
}

} // namespace

using namespace testing;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class StorageStateTests : public Test {
protected:
    void SetUp() override
    {
        std::filesystem::remove_all(cTestDir);

        std::filesystem::create_directories(cTestDir);
        std::filesystem::create_directories(cStorageDir);
        std::filesystem::create_directories(cStateDir);

        mConfig.mStorageDir = cStorageDir.c_str();
        mConfig.mStateDir   = cStateDir.c_str();

        tests::utils::InitLog();

        EXPECT_TRUE(mCryptoProvider.Init(mAllocator).IsNone()) << "Failed to initialize crypto provider";

        EXPECT_CALL(mFSPlatformMock, GetMountPoint)
            .WillRepeatedly(Return(RetWithError<StaticString<cFilePathLen>>(cTestDir.c_str())));
        EXPECT_CALL(mFSPlatformMock, ChangeOwner).WillRepeatedly(Return(ErrorEnum::eNone));
    }

    Error CalculateChecksum(const std::string& text, Array<uint8_t>& result)
    {
        auto [hasher, err] = mCryptoProvider.CreateHash(crypto::HashEnum::eSHA3_224);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        err = hasher->Update(Array<uint8_t>(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        err = hasher->Finalize(result);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    Error AddInstanceIdent(const InstanceIdent& ident, const std::string& stateContent = "test state")
    {
        auto path = ToStatePath(ident);
        std::filesystem::create_directories(path.parent_path());

        if (auto err = fs::WriteStringToFile(path.c_str(), stateContent.c_str(), 0600); !err.IsNone()) {
            return err;
        }

        auto storageItem            = std::make_unique<InstanceInfo>();
        storageItem->mInstanceIdent = ident;
        storageItem->mStateQuota    = 2000;

        if (auto err = CalculateChecksum(stateContent, storageItem->mStateChecksum); !err.IsNone()) {
            return err;
        }

        if (auto err = mStorageStub.AddStorageStateInfo(*storageItem); !err.IsNone()) {
            return err;
        }

        return ErrorEnum::eNone;
    }

    Error InitWithStubs()
    {
        return mStorageState.Init(
            mTestAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mHasherStub);
    }

    std::unique_ptr<UpdateState> CreateUpdateState(const std::string& content)
    {
        auto state = std::make_unique<UpdateState>();

        static_cast<InstanceIdent&>(*state) = cInstanceIdent;
        state->mState                       = content.c_str();

        EXPECT_TRUE(CalculateChecksum(content, state->mChecksum).IsNone());

        return state;
    }

    // mAllocator must be declared (and therefore destroyed) after any member that allocates from it, since
    // members are destroyed in reverse declaration order.
    HeapAllocator        mAllocator;
    tests::TestAllocator mTestAllocator;

    crypto::DefaultCryptoProvider mCryptoProvider;
    HasherStub                    mHasherStub {mCryptoProvider, mAllocator};
    StorageStub                   mStorageStub;
    StrictMock<FSPlatformMock>    mFSPlatformMock;
    StrictMock<FSWatcherMock>     mFSWatcherMock;
    StrictMock<SenderMock>        mSenderMock;
    Config                        mConfig;
    StorageState                  mStorageState;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(StorageStateTests, StartStop)
{
    auto err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, StorageQuotaNotSet)
{
    SetupParams setupParams;
    setupParams.mUID          = getuid();
    setupParams.mGID          = getgid();
    setupParams.mStateQuota   = 2000;
    setupParams.mStorageQuota = 0;

    StaticString<cFilePathLen> storagePath, statePath;

    EXPECT_CALL(mFSPlatformMock, SetUserQuota(_, setupParams.mUID, setupParams.mStateQuota)).Times(1);

    auto err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mSenderMock, SendStateRequest).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Setup(cInstanceIdent, setupParams, storagePath, statePath);
    EXPECT_TRUE(err.IsNone()) << "Failed to setup storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(mStorageStub.Contains([](const InstanceInfo& info) { return info.mInstanceIdent == cInstanceIdent; }))
        << "Storage state info should be added";

    EXPECT_TRUE(storagePath.IsEmpty()) << "Storage path should be empty when storage quota is not set";
    EXPECT_FALSE(statePath.IsEmpty()) << "State path should not be empty when state quota is set";
}

TEST_F(StorageStateTests, StateQuotaNotSet)
{
    SetupParams setupParams;
    setupParams.mUID          = getuid();
    setupParams.mGID          = getgid();
    setupParams.mStateQuota   = 0;
    setupParams.mStorageQuota = 2000;

    StaticString<cFilePathLen> storagePath, statePath;

    auto err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSPlatformMock, SetUserQuota(_, setupParams.mUID, setupParams.mStorageQuota)).Times(1);
    EXPECT_CALL(mSenderMock, SendStateRequest).Times(0);
    EXPECT_CALL(mFSWatcherMock, Subscribe).Times(0);

    err = mStorageState.Setup(cInstanceIdent, setupParams, storagePath, statePath);
    EXPECT_TRUE(err.IsNone()) << "Failed to setup storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(mStorageStub.Contains([](const InstanceInfo& info) { return info.mInstanceIdent == cInstanceIdent; }))
        << "Storage state info should be added";

    EXPECT_FALSE(storagePath.IsEmpty()) << "Storage path should not be empty when storage quota is set";
    EXPECT_TRUE(statePath.IsEmpty()) << "State path should be empty when state quota is not set";

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).Times(0);

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, StorageAndStateQuotaNotSet)
{
    SetupParams setupParams;
    setupParams.mUID          = getuid();
    setupParams.mGID          = getgid();
    setupParams.mStateQuota   = 0;
    setupParams.mStorageQuota = 0;

    StaticString<cFilePathLen> storagePath, statePath;

    auto err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSPlatformMock, SetUserQuota).Times(0);

    err = mStorageState.Setup(cInstanceIdent, setupParams, storagePath, statePath);
    EXPECT_TRUE(err.IsNone()) << "Failed to setup storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(mStorageStub.Contains([](const InstanceInfo& info) { return info.mInstanceIdent == cInstanceIdent; }))
        << "Storage state info should be added";

    EXPECT_TRUE(storagePath.IsEmpty()) << "Storage path should  be empty when storage quota is set";
    EXPECT_TRUE(statePath.IsEmpty()) << "State path should be empty when state quota is not set";
}

TEST_F(StorageStateTests, SetupOnDifferentPartitions)
{
    const auto cSetupParams = SetupParams {getuid(), getgid(), 2000, 1000};

    EXPECT_CALL(mFSPlatformMock, GetMountPoint)
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>("partition1")))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>("partition2")));

    auto err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(
        mFSPlatformMock, SetUserQuota(String(cStorageDir.c_str()), cSetupParams.mUID, cSetupParams.mStorageQuota))
        .WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mFSPlatformMock, SetUserQuota(String(cStateDir.c_str()), cSetupParams.mUID, cSetupParams.mStateQuota))
        .WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mSenderMock, SendStateRequest).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    StaticString<cFilePathLen> storagePath, statePath;

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.IsNone()) << "Setup should succeed: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, SetupFailsOnSetUserQuotaError)
{
    constexpr auto cSetQuotaError = ErrorEnum::eOutOfRange;

    auto err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSPlatformMock, SetUserQuota).WillOnce(Return(cSetQuotaError));

    StaticString<cFilePathLen> storagePath, statePath;

    err = mStorageState.Setup(cInstanceIdent, SetupParams {getuid(), getgid(), 2000, 1000}, storagePath, statePath);
    EXPECT_TRUE(err.Is(cSetQuotaError)) << "Setup should fail with SetUserQuota error: "
                                        << tests::utils::ErrorToStr(err);

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, SetupSameInstance)
{
    auto err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    const struct TestParams {
        SetupParams                mSetupParams;
        std::optional<std::string> mNewState;
        bool                       mExpectSetQuota           = false;
        bool                       mExpectStateRequest       = false;
        bool                       mExpectFSWatchUnsubscribe = false;
        bool                       mExpectFSWatchSubscribe   = false;
    } params[] = {
        {{getuid(), getgid(), 2000, 1000}, {"state"}, true, true, false, true},
        {{getuid(), getgid(), 2000, 1000}, {"state 1"}, false, true, true, true},
        {{getuid(), getgid(), 2000, 1000}, {"state 2"}, false, true, true, true},
        {{getuid(), getgid(), 2000, 2000}, {""}, true, true, true, true},
    };

    size_t testIndex = 0;

    for (const auto& testParam : params) {
        LOG_DBG() << "Running test case" << Log::Field("index", testIndex++);

        StaticString<cFilePathLen> storagePath, statePath;

        if (testParam.mExpectSetQuota) {
            EXPECT_CALL(mFSPlatformMock,
                SetUserQuota(_, testParam.mSetupParams.mUID,
                    testParam.mSetupParams.mStateQuota + testParam.mSetupParams.mStorageQuota))
                .WillOnce(Return(ErrorEnum::eNone));
        }

        if (testParam.mExpectStateRequest) {
            EXPECT_CALL(mSenderMock, SendStateRequest(StateRequest {"", cInstanceIdent, false}))
                .WillOnce(Return(ErrorEnum::eNone));
        }

        if (testParam.mNewState.has_value()) {
            std::ofstream stateFile(ToStatePath(cInstanceIdent).string());

            stateFile << *testParam.mNewState << std::flush;
        }

        if (testParam.mExpectFSWatchUnsubscribe) {
            EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));
        }

        if (testParam.mExpectFSWatchSubscribe) {
            EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));
        }

        err = mStorageState.Setup(cInstanceIdent, testParam.mSetupParams, storagePath, statePath);
        EXPECT_TRUE(err.IsNone()) << "Can't setup storage state: " << tests::utils::ErrorToStr(err);
    }

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, GetInstanceCheckSum)
{
    auto err = AddInstanceIdent(cInstanceIdent, "getchecksum-content");
    EXPECT_TRUE(err.IsNone()) << "Failed to add instance ident: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    StaticArray<uint8_t, crypto::cSHA256Size> storedChecksumBytes;

    err = mStorageState.GetInstanceCheckSum(cInstanceIdent, storedChecksumBytes);
    EXPECT_TRUE(err.IsNone()) << "Failed to get instance checksum: " << tests::utils::ErrorToStr(err);

    err = mStorageState.GetInstanceCheckSum(
        InstanceIdent {{}, "", {}, 111, UpdateItemTypeEnum::eService}, storedChecksumBytes);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << "Expected not found error, got: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, Cleanup)
{
    auto err = AddInstanceIdent(cInstanceIdent, "cleanup-content");

    err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Cleanup(cInstanceIdent);
    EXPECT_TRUE(err.IsNone());

    err = mStorageState.Cleanup(cInstanceIdent);
    EXPECT_TRUE(err.IsNone());

    InstanceInfo storageData;

    err = mStorageStub.GetStorageStateInfo(cInstanceIdent, storageData);
    EXPECT_TRUE(err.IsNone()) << "Failed to get storage state info: " << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(std::filesystem::exists(ToStatePath(storageData.mInstanceIdent)))
        << "State file should exist after cleanup";

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, Remove)
{
    auto err = AddInstanceIdent(cInstanceIdent, "remove-content");

    err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Remove(cInstanceIdent);
    EXPECT_TRUE(err.IsNone());

    InstanceInfo storageData;

    err = mStorageStub.GetStorageStateInfo(cInstanceIdent, storageData);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound))
        << "Storage data should not exists after remove: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Remove(cInstanceIdent);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, UpdateState)
{
    constexpr auto newStateContent = "updated state content";

    auto err = AddInstanceIdent(cInstanceIdent, "outdated state content");

    err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    StaticArray<uint8_t, crypto::cSHA256Size> checksumBytes;

    err = CalculateChecksum(newStateContent, checksumBytes);
    EXPECT_TRUE(err.IsNone()) << "Failed to calculate checksum: " << tests::utils::ErrorToStr(err);

    auto stateUpdate = std::make_unique<UpdateState>();

    static_cast<InstanceIdent&>(*stateUpdate) = cInstanceIdent;
    stateUpdate->mState                       = newStateContent;
    stateUpdate->mChecksum                    = checksumBytes;

    // Keep state file opened as the running instance holds it through bind mount.
    std::ifstream stateFile(ToStatePath(cInstanceIdent));

    err = mStorageState.UpdateState(*stateUpdate);
    EXPECT_TRUE(err.IsNone()) << "Failed to update state: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(stateFile), {}), newStateContent)
        << "State file should be updated in place";

    EXPECT_TRUE(mStorageStub.Contains([&checksumBytes](const InstanceInfo& info) {
        return info.mInstanceIdent == cInstanceIdent && info.mStateChecksum == checksumBytes;
    })) << "Storage state info should be updated";

    stateUpdate->mInstance += 1;

    err = mStorageState.UpdateState(*stateUpdate);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, AcceptStateUnknownInstance)
{
    auto err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    auto stateAccept = std::make_unique<StateAcceptance>();

    static_cast<InstanceIdent&>(*stateAccept) = cInstanceIdent;
    stateAccept->mResult                      = StateResultEnum::eAccepted;
    stateAccept->mReason                      = "accepted";

    err = mStorageState.AcceptState(*stateAccept);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, AcceptStateChecksumMismatch)
{
    auto err = AddInstanceIdent(cInstanceIdent, "initial state content");

    err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    auto stateAccept = std::make_unique<StateAcceptance>();

    static_cast<InstanceIdent&>(*stateAccept) = cInstanceIdent;
    stateAccept->mResult                      = StateResultEnum::eAccepted;
    stateAccept->mReason                      = "accepted";
    stateAccept->mChecksum.EmplaceBack(0x00); // Invalid checksum

    err = mStorageState.AcceptState(*stateAccept);
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidChecksum))
        << "Accepting state with invalid checksum should fail: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, AcceptStateWithRejectedStatus)
{
    auto err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    StaticString<cFilePathLen> storagePath, statePath;

    EXPECT_CALL(mSenderMock, SendStateRequest(StateRequest {"", cInstanceIdent, false}))
        .WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mFSPlatformMock, SetUserQuota).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Setup(cInstanceIdent, SetupParams {getuid(), getgid(), 2000, 1000}, storagePath, statePath);
    EXPECT_TRUE(err.IsNone()) << "Failed to setup storage state: " << tests::utils::ErrorToStr(err);

    InstanceInfo storageData;

    err = mStorageStub.GetStorageStateInfo(cInstanceIdent, storageData);
    EXPECT_TRUE(err.IsNone()) << "Failed to get storage state info: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mSenderMock, SendStateRequest(StateRequest {"", cInstanceIdent, false}))
        .WillOnce(Return(ErrorEnum::eNone));

    auto stateAccept = std::make_unique<StateAcceptance>();

    static_cast<InstanceIdent&>(*stateAccept) = cInstanceIdent;
    stateAccept->mResult                      = StateResultEnum::eRejected;
    stateAccept->mReason                      = "rejected";
    stateAccept->mChecksum                    = storageData.mStateChecksum;

    err = mStorageState.AcceptState(*stateAccept);
    EXPECT_TRUE(err.IsNone()) << "Failed to accept state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, UpdateAndAcceptStateFlow)
{
    const auto                 cSetupParams        = SetupParams {getuid(), getgid(), 2000, 1000};
    constexpr auto             cStateContent       = "initial state content";
    constexpr auto             cUpdateStateContent = "updated state content";
    StaticString<cFilePathLen> storagePath;
    StaticString<cFilePathLen> statePath;

    StaticArray<uint8_t, crypto::cSHA256Size> stateContentChecksum;
    StaticArray<uint8_t, crypto::cSHA256Size> updateStateContentChecksum;

    auto err = CalculateChecksum(cStateContent, stateContentChecksum);
    EXPECT_TRUE(err.IsNone()) << "Failed to calculate checksum: " << tests::utils::ErrorToStr(err);

    err = CalculateChecksum(cUpdateStateContent, updateStateContentChecksum);
    EXPECT_TRUE(err.IsNone()) << "Failed to calculate checksum: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    // Setup storage state

    fs::FSEventSubscriberItf* fsEventSubscriber = nullptr;

    EXPECT_CALL(mFSWatcherMock, Subscribe)
        .WillOnce(Invoke([&fsEventSubscriber](const String&, fs::FSEventSubscriberItf& subscriber) {
            fsEventSubscriber = &subscriber;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mFSPlatformMock, SetUserQuota).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mSenderMock, SendStateRequest(StateRequest {"", cInstanceIdent, false}))
        .WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.IsNone()) << "Failed to setup storage state: " << tests::utils::ErrorToStr(err);

    LOG_DBG() << "State path: " << statePath << ", storage path: " << storagePath;

    // Update state with initial content

    auto updateState = std::make_unique<UpdateState>();

    static_cast<InstanceIdent&>(*updateState) = cInstanceIdent;
    updateState->mState                       = cStateContent;
    updateState->mChecksum                    = stateContentChecksum;

    err = mStorageState.UpdateState(*updateState);
    EXPECT_TRUE(err.IsNone()) << "Failed to update state: " << tests::utils::ErrorToStr(err);

    // Emulate service mutates its state file

    err = fs::WriteStringToFile(ToStatePath(cInstanceIdent).c_str(), cUpdateStateContent, 0600);
    EXPECT_TRUE(err.IsNone()) << "Failed to write state file: " << tests::utils::ErrorToStr(err);

    std::promise<void> stateSentPromise;

    auto expectedNewState = std::make_unique<NewState>();

    static_cast<InstanceIdent&>(*expectedNewState) = cInstanceIdent;
    expectedNewState->mState                       = String(cUpdateStateContent);
    expectedNewState->mChecksum                    = Array<uint8_t>(updateStateContentChecksum);

    EXPECT_CALL(mSenderMock, SendNewState(*expectedNewState)).WillOnce(Invoke([&stateSentPromise](const auto&) {
        stateSentPromise.set_value();
        return ErrorEnum::eNone;
    }));

    fsEventSubscriber->OnFSEvent(ToStatePath(cInstanceIdent).c_str(), {});

    EXPECT_EQ(stateSentPromise.get_future().wait_for(std::chrono::seconds(10)), std::future_status::ready)
        << "State was not sent in time";

    // New state is accepted

    auto acceptState = std::make_unique<StateAcceptance>();

    static_cast<InstanceIdent&>(*acceptState) = cInstanceIdent;
    acceptState->mResult                      = StateResultEnum::eAccepted;
    acceptState->mReason                      = "accepted";
    acceptState->mChecksum                    = updateStateContentChecksum;

    err = mStorageState.AcceptState(*acceptState);
    EXPECT_TRUE(err.IsNone()) << "Failed to accept state: " << tests::utils::ErrorToStr(err);

    // And the storage stub is updated

    EXPECT_TRUE(mStorageStub.Contains([&updateStateContentChecksum](const InstanceInfo& info) {
        return info.mInstanceIdent == cInstanceIdent && info.mStateChecksum == updateStateContentChecksum;
    })) << "Storage state info should be updated with new state checksum";

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, InitFails)
{
    std::filesystem::create_directories(cTestDir);
    std::ofstream(cTestDir / "file") << "not a dir";

    auto config      = mConfig;
    config.mStateDir = (cTestDir / "file" / "state").c_str();

    auto err = mStorageState.Init(
        mAllocator, config, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_FALSE(err.IsNone()) << "Init should fail on state dir creation";

    config             = mConfig;
    config.mStorageDir = (cTestDir / "file" / "storage").c_str();

    err = mStorageState.Init(
        mAllocator, config, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_FALSE(err.IsNone()) << "Init should fail on storage dir creation";

    EXPECT_CALL(mFSPlatformMock, GetMountPoint)
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>("", ErrorEnum::eFailed)));

    err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSPlatformMock, GetMountPoint)
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>("partition")))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>("", ErrorEnum::eNotFound)));

    err = mStorageState.Init(
        mAllocator, mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, StartFails)
{
    auto err = InitWithStubs();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    mTestAllocator.FailAfter(0);

    err = mStorageState.Start();
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    mTestAllocator.Reset();
    mStorageStub.mGetAllErr = ErrorEnum::eFailed;

    err = mStorageState.Start();
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, StartIgnoresSubscribeErrorAndStopIgnoresUnsubscribeError)
{
    auto err = AddInstanceIdent(cInstanceIdent);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = AddInstanceIdent(InstanceIdent {{"itemID"}, "1.0.0", {"subjectID"}, 2, UpdateItemTypeEnum::eService});
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = InitWithStubs();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eFailed)).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StaticArray<uint8_t, crypto::cSHA256Size> checksum;

    EXPECT_EQ(mStorageState.GetInstanceCheckSum(cInstanceIdent, checksum).IsNone()
            + mStorageState
                  .GetInstanceCheckSum(
                      InstanceIdent {{"itemID"}, "1.0.0", {"subjectID"}, 2, UpdateItemTypeEnum::eService}, checksum)
                  .IsNone(),
        1)
        << "Only one instance should be watched";

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eFailed));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, TotalSizesAndPartition)
{
    auto err = InitWithStubs();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSPlatformMock, GetTotalSize(String(cStateDir.c_str()))).WillOnce(Return(RetWithError<size_t>(10)));
    EXPECT_CALL(mFSPlatformMock, GetTotalSize(String(cStorageDir.c_str()))).WillOnce(Return(RetWithError<size_t>(20)));

    auto stateSize = mStorageState.GetTotalStateSize();
    EXPECT_TRUE(stateSize.mError.IsNone());
    EXPECT_EQ(stateSize.mValue, 10);

    auto storageSize = mStorageState.GetTotalStorageSize();
    EXPECT_TRUE(storageSize.mError.IsNone());
    EXPECT_EQ(storageSize.mValue, 20);

    EXPECT_TRUE(mStorageState.IsSamePartition());
}

TEST_F(StorageStateTests, UpdateStateErrors)
{
    auto err = AddInstanceIdent(cInstanceIdent, "state");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = InitWithStubs();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mStorageState.UpdateState(*CreateUpdateState(std::string(2001, 'a')));
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidArgument)) << tests::utils::ErrorToStr(err);

    auto updateState = CreateUpdateState("new state");
    updateState->mChecksum.Clear();

    err = mStorageState.UpdateState(*updateState);
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidChecksum)) << tests::utils::ErrorToStr(err);

    mHasherStub.SetErrors(ErrorEnum::eFailed);

    err = mStorageState.UpdateState(*CreateUpdateState("new state"));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    mHasherStub.SetErrors(ErrorEnum::eNone);

    mTestAllocator.FailAfter(0);

    err = mStorageState.UpdateState(*CreateUpdateState("new state"));
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    mTestAllocator.Reset();

    mStorageStub.mGetErr = ErrorEnum::eFailed;

    err = mStorageState.UpdateState(*CreateUpdateState("new state"));
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    mStorageStub.mGetErr = ErrorEnum::eNone;

    mStorageStub.mUpdateErr = ErrorEnum::eWrongState;

    err = mStorageState.UpdateState(*CreateUpdateState("new state"));
    EXPECT_TRUE(err.Is(ErrorEnum::eWrongState)) << tests::utils::ErrorToStr(err);

    mStorageStub.mUpdateErr = ErrorEnum::eNone;

    std::filesystem::remove(ToStatePath(cInstanceIdent));
    std::filesystem::create_directories(ToStatePath(cInstanceIdent));

    err = mStorageState.UpdateState(*CreateUpdateState("new state"));
    EXPECT_FALSE(err.IsNone()) << "Writing state to a directory should fail";

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, AcceptStateErrors)
{
    auto err = AddInstanceIdent(cInstanceIdent, "state");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = InitWithStubs();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // State checksum is empty after start, so acceptance with empty checksum matches it.
    auto acceptState = std::make_unique<StateAcceptance>();

    static_cast<InstanceIdent&>(*acceptState) = cInstanceIdent;
    acceptState->mResult                      = StateResultEnum::eAccepted;

    mTestAllocator.FailAfter(0);

    err = mStorageState.AcceptState(*acceptState);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    mTestAllocator.Reset();
    mStorageStub.mGetErr = ErrorEnum::eFailed;

    err = mStorageState.AcceptState(*acceptState);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    mStorageStub.mGetErr    = ErrorEnum::eNone;
    mStorageStub.mUpdateErr = ErrorEnum::eWrongState;

    err = mStorageState.AcceptState(*acceptState);
    EXPECT_TRUE(err.Is(ErrorEnum::eWrongState)) << tests::utils::ErrorToStr(err);

    mStorageStub.mUpdateErr = ErrorEnum::eNone;

    err = mStorageState.AcceptState(*acceptState);
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, SetupStorageInfoErrors)
{
    const auto                 cSetupParams = SetupParams {getuid(), getgid(), 2000, 1000};
    StaticString<cFilePathLen> storagePath, statePath;

    auto err = InitWithStubs();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    mTestAllocator.FailAfter(0);

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    mTestAllocator.FailAfter(1);

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    mTestAllocator.Reset();
    mStorageStub.mGetErr = ErrorEnum::eFailed;

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    mStorageStub.mGetErr = ErrorEnum::eNone;
    mStorageStub.mAddErr = ErrorEnum::eAlreadyExist;

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.Is(ErrorEnum::eAlreadyExist)) << tests::utils::ErrorToStr(err);

    mStorageStub.mAddErr    = ErrorEnum::eNone;
    mStorageStub.mUpdateErr = ErrorEnum::eWrongState;

    EXPECT_CALL(mFSPlatformMock, SetUserQuota).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.Is(ErrorEnum::eWrongState)) << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, SetupPrepareStorageErrors)
{
    StaticString<cFilePathLen> storagePath, statePath;

    auto err = InitWithStubs();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSPlatformMock, ChangeOwner).WillOnce(Return(ErrorEnum::eFailed));

    err = mStorageState.Setup(cInstanceIdent, SetupParams {getuid(), getgid(), 2000, 1000}, storagePath, statePath);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    std::ofstream(cStorageDir / "itemID2") << "not a dir";

    err = mStorageState.Setup(InstanceIdent {{"itemID2"}, "1.0.0", {"subjectID"}, 1, UpdateItemTypeEnum::eService},
        SetupParams {getuid(), getgid(), 2000, 1000}, storagePath, statePath);
    EXPECT_FALSE(err.IsNone()) << "Storage dir creation should fail";
}

TEST_F(StorageStateTests, SetupPrepareStateErrors)
{
    const auto                 cSetupParams = SetupParams {getuid(), getgid(), 1000, 0};
    StaticString<cFilePathLen> storagePath, statePath;

    auto err = InitWithStubs();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSPlatformMock, SetUserQuota).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mFSPlatformMock, ChangeOwner).WillOnce(Return(ErrorEnum::eFailed));

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eWrongState));

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.Is(ErrorEnum::eWrongState)) << tests::utils::ErrorToStr(err);

    // Send state request error, state watching is stopped with unsubscribe error

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mSenderMock, SendStateRequest).WillOnce(Return(ErrorEnum::eTimeout));
    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eFailed));

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.Is(ErrorEnum::eTimeout)) << tests::utils::ErrorToStr(err);

    StaticArray<uint8_t, crypto::cSHA256Size> checksum;

    EXPECT_TRUE(mStorageState.GetInstanceCheckSum(cInstanceIdent, checksum).Is(ErrorEnum::eNotFound))
        << "State watching should be stopped on error";

    for (const auto& [createErr, updateErr, finalizeErr] :
        std::vector<std::tuple<Error, Error, Error>> {{ErrorEnum::eFailed, ErrorEnum::eNone, ErrorEnum::eNone},
            {ErrorEnum::eNone, ErrorEnum::eFailed, ErrorEnum::eNone},
            {ErrorEnum::eNone, ErrorEnum::eNone, ErrorEnum::eFailed}}) {
        mHasherStub.SetErrors(createErr, updateErr, finalizeErr);

        EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));
        EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

        err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
        EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);
    }

    mHasherStub.SetErrors(ErrorEnum::eNone);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    mTestAllocator.FailAfter(1);

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);

    mTestAllocator.Reset();

    std::filesystem::remove(ToStatePath(cInstanceIdent));
    std::filesystem::create_directories(ToStatePath(cInstanceIdent));

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_FALSE(err.IsNone()) << "Reading state from a directory should fail";

    std::filesystem::remove_all(cStateDir / "itemID");
    std::ofstream(cStateDir / "itemID") << "not a dir";

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_FALSE(err.IsNone()) << "State dir creation should fail";
}

TEST_F(StorageStateTests, SetupStopWatchingErrorIsIgnored)
{
    const auto                 cSetupParams = SetupParams {getuid(), getgid(), 1000, 0};
    StaticString<cFilePathLen> storagePath, statePath;

    auto err = InitWithStubs();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSPlatformMock, SetUserQuota).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mFSWatcherMock, Subscribe).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mSenderMock, SendStateRequest).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eFailed));

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eFailed));

    err = mStorageState.Remove(cInstanceIdent);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, SetQuotasOnDifferentPartitionsErrors)
{
    const auto                 cSetupParams = SetupParams {getuid(), getgid(), 2000, 1000};
    StaticString<cFilePathLen> storagePath, statePath;

    EXPECT_CALL(mFSPlatformMock, GetMountPoint)
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>("partition1")))
        .WillOnce(Return(RetWithError<StaticString<cFilePathLen>>("partition2")));

    auto err = InitWithStubs();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_FALSE(mStorageState.IsSamePartition());

    EXPECT_CALL(mFSPlatformMock, SetUserQuota(String(cStateDir.c_str()), _, _)).WillOnce(Return(ErrorEnum::eFailed));

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSPlatformMock, SetUserQuota(String(cStateDir.c_str()), _, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mFSPlatformMock, SetUserQuota(String(cStorageDir.c_str()), _, _))
        .WillOnce(Return(ErrorEnum::eNoMemory));

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory)) << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, FSEventErrors)
{
    const InstanceIdent cInstanceIdent2 {{"itemID"}, "1.0.0", {"subjectID"}, 2, UpdateItemTypeEnum::eService};

    auto err = AddInstanceIdent(cInstanceIdent, "state 1");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = AddInstanceIdent(cInstanceIdent2, "state 2");
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = InitWithStubs();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    fs::FSEventSubscriberItf* fsEventSubscriber = nullptr;

    EXPECT_CALL(mFSWatcherMock, Subscribe)
        .Times(2)
        .WillRepeatedly(Invoke([&fsEventSubscriber](const String&, fs::FSEventSubscriberItf& subscriber) {
            fsEventSubscriber = &subscriber;

            return ErrorEnum::eNone;
        }));

    err = mStorageState.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    ASSERT_NE(fsEventSubscriber, nullptr);

    auto& subscriber = *fsEventSubscriber;

    // Unknown path and unreadable state file: no new state is sent. Event for second instance is used as a
    // barrier as the events are processed in order by a single thread.

    std::filesystem::remove(ToStatePath(cInstanceIdent));

    subscriber.OnFSEvent("unknown/path", {});
    subscriber.OnFSEvent(ToStatePath(cInstanceIdent).c_str(), {});

    std::promise<void> stateSentPromise;

    EXPECT_CALL(mSenderMock, SendNewState).WillOnce(Invoke([&stateSentPromise](const NewState& state) {
        EXPECT_EQ(state.mInstance, 2);

        stateSentPromise.set_value();

        return ErrorEnum::eFailed;
    }));

    subscriber.OnFSEvent(ToStatePath(cInstanceIdent2).c_str(), {});

    EXPECT_EQ(stateSentPromise.get_future().wait_for(std::chrono::seconds(10)), std::future_status::ready)
        << "State was not sent in time";

    mHasherStub.SetErrors(ErrorEnum::eFailed);

    subscriber.OnFSEvent(ToStatePath(cInstanceIdent2).c_str(), {});

    EXPECT_TRUE(mHasherStub.WaitCalls(1)) << "Checksum was not calculated in time";

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    mHasherStub.SetErrors(ErrorEnum::eNone);

    subscriber.OnFSEvent(ToStatePath(cInstanceIdent2).c_str(), {});
}

} // namespace aos::cm::storagestate
