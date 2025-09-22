/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <queue>

#include <gtest/gtest.h>

#include <core/cm/storagestate/storagestate.hpp>
#include <core/cm/tests/mocks/communicationmock.hpp>
#include <core/common/crypto/cryptoprovider.hpp>
#include <core/common/tests/mocks/fsmock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>

using namespace testing;

namespace aos::cm::storagestate {

namespace {

/***********************************************************************************************************************
 * Constants
 **********************************************************************************************************************/

const auto                 cTestDir    = std::filesystem::path("storage_state");
const auto                 cStorageDir = cTestDir / "storage";
const auto                 cStateDir   = cTestDir / "state";
static const InstanceIdent cInstanceIdent {{"itemID"}, {"subjectID"}, 1};

/***********************************************************************************************************************
 * Stubs
 **********************************************************************************************************************/

class StorageStub : public aos::cm::storagestate::StorageItf {
public:
    Error AddStorageStateInfo(const InstanceInfo& storageStateInfo) override
    {
        std::lock_guard lock {mMutex};

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
    MOCK_METHOD(Error, SendStateRequest, (const InstanceIdent& instanceIdent, bool isDefault), (override));
    MOCK_METHOD(Error, SendNewState, (const InstanceIdent& instanceIdent, const String& state, const String& checksum),
        (override));
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

        EXPECT_TRUE(mCryptoProvider.Init().IsNone()) << "Failed to initialize crypto provider";

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

    Error CalculateChecksum(const std::string& text, String& result)
    {
        StaticArray<uint8_t, crypto::cSHA2DigestSize> array;

        if (auto err = CalculateChecksum(std::string(text), array); !err.IsNone()) {
            return err;
        }

        return result.ByteArrayToHex(array);
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

    crypto::DefaultCryptoProvider mCryptoProvider;
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
    auto err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
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

    EXPECT_CALL(mFSPlatformMock, SetUserQuota(_, setupParams.mStateQuota, setupParams.mUID)).Times(1);

    auto err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
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

    auto err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSPlatformMock, SetUserQuota(_, setupParams.mStorageQuota, setupParams.mUID)).Times(1);
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

    auto err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
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

    auto err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(
        mFSPlatformMock, SetUserQuota(String(cStorageDir.c_str()), cSetupParams.mStorageQuota, cSetupParams.mUID))
        .WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mFSPlatformMock, SetUserQuota(String(cStateDir.c_str()), cSetupParams.mStateQuota, cSetupParams.mUID))
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

    auto err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
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
    auto err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
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
                SetUserQuota(_, testParam.mSetupParams.mStateQuota + testParam.mSetupParams.mStorageQuota,
                    testParam.mSetupParams.mUID))
                .WillOnce(Return(ErrorEnum::eNone));
        }

        if (testParam.mExpectStateRequest) {
            EXPECT_CALL(mSenderMock, SendStateRequest(cInstanceIdent, false)).WillOnce(Return(ErrorEnum::eNone));
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

    err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    StaticString<crypto::cSHA2DigestSize> storedChecksumStr;

    err = mStorageState.GetInstanceCheckSum(cInstanceIdent, storedChecksumStr);
    EXPECT_TRUE(err.IsNone()) << "Failed to get instance checksum: " << tests::utils::ErrorToStr(err);

    err = mStorageState.GetInstanceCheckSum(InstanceIdent {{}, {}, 111}, storedChecksumStr);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << "Expected not found error, got: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, Cleanup)
{
    auto err = AddInstanceIdent(cInstanceIdent, "cleanup-content");

    err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Cleanup(cInstanceIdent);
    EXPECT_TRUE(err.IsNone());

    err = mStorageState.Cleanup(cInstanceIdent);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));

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

    err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
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

    err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    StaticString<crypto::cSHA2DigestSize> checksum;

    err = CalculateChecksum(newStateContent, checksum);
    EXPECT_TRUE(err.IsNone()) << "Failed to calculate checksum: " << tests::utils::ErrorToStr(err);

    err = mStorageState.UpdateState(cInstanceIdent, newStateContent, checksum);
    EXPECT_TRUE(err.IsNone()) << "Failed to update state: " << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(mStorageStub.Contains([&checksum](const InstanceInfo& info) {
        return info.mInstanceIdent == cInstanceIdent && info.mStateChecksum == checksum;
    })) << "Storage state info should be updated";

    err = mStorageState.UpdateState(InstanceIdent {{}, {}, 111}, newStateContent, checksum);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, AcceptStateUnknownInstance)
{
    auto err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.AcceptState(
        InstanceIdent {{}, {}, 111}, "some-checksum", StateResultEnum::eAccepted, "accepted");
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, AcceptStateChecksumMismatch)
{
    auto err = AddInstanceIdent(cInstanceIdent, "initial state content");

    err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.AcceptState(cInstanceIdent, "invalid checksum", StateResultEnum::eAccepted, "accepted");
    EXPECT_TRUE(err.Is(ErrorEnum::eInvalidChecksum))
        << "Accepting state with invalid checksum should fail: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, AcceptStateWithRejectedStatus)
{
    auto err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
    EXPECT_TRUE(err.IsNone()) << "Failed to initialize storage state: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start storage state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Subscribe).WillOnce(Return(ErrorEnum::eNone));

    StaticString<cFilePathLen> storagePath, statePath;

    EXPECT_CALL(mSenderMock, SendStateRequest(cInstanceIdent, false)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mFSPlatformMock, SetUserQuota).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Setup(cInstanceIdent, SetupParams {getuid(), getgid(), 2000, 1000}, storagePath, statePath);
    EXPECT_TRUE(err.IsNone()) << "Failed to setup storage state: " << tests::utils::ErrorToStr(err);

    InstanceInfo storageData;

    err = mStorageStub.GetStorageStateInfo(cInstanceIdent, storageData);
    EXPECT_TRUE(err.IsNone()) << "Failed to get storage state info: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mSenderMock, SendStateRequest(cInstanceIdent, false)).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.AcceptState(cInstanceIdent, storageData.mStateChecksum, StateResultEnum::eRejected, "rejected");
    EXPECT_TRUE(err.IsNone()) << "Failed to accept state: " << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

TEST_F(StorageStateTests, UpdateAndAcceptStateFlow)
{
    const auto                            cSetupParams        = SetupParams {getuid(), getgid(), 2000, 1000};
    constexpr auto                        cStateContent       = "initial state content";
    constexpr auto                        cUpdateStateContent = "updated state content";
    StaticString<cFilePathLen>            storagePath;
    StaticString<cFilePathLen>            statePath;
    StaticString<crypto::cSHA2DigestSize> cStateContentChecksum;
    StaticString<crypto::cSHA2DigestSize> cUpdateStateContentChecksum;

    auto err = CalculateChecksum(cStateContent, cStateContentChecksum);
    EXPECT_TRUE(err.IsNone()) << "Failed to calculate checksum: " << tests::utils::ErrorToStr(err);

    err = CalculateChecksum(cUpdateStateContent, cUpdateStateContentChecksum);
    EXPECT_TRUE(err.IsNone()) << "Failed to calculate checksum: " << tests::utils::ErrorToStr(err);

    err = mStorageState.Init(mConfig, mStorageStub, mSenderMock, mFSPlatformMock, mFSWatcherMock, mCryptoProvider);
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
    EXPECT_CALL(mSenderMock, SendStateRequest(cInstanceIdent, false)).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Setup(cInstanceIdent, cSetupParams, storagePath, statePath);
    EXPECT_TRUE(err.IsNone()) << "Failed to setup storage state: " << tests::utils::ErrorToStr(err);

    LOG_DBG() << "State path: " << statePath << ", storage path: " << storagePath;

    // Update state with initial content

    err = mStorageState.UpdateState(cInstanceIdent, cStateContent, cStateContentChecksum);
    EXPECT_TRUE(err.IsNone()) << "Failed to update state: " << tests::utils::ErrorToStr(err);

    // Emulate service mutates its state file

    err = fs::WriteStringToFile(ToStatePath(cInstanceIdent).c_str(), cUpdateStateContent, 0600);
    EXPECT_TRUE(err.IsNone()) << "Failed to write state file: " << tests::utils::ErrorToStr(err);

    std::promise<void> stateSentPromise;

    EXPECT_CALL(
        mSenderMock, SendNewState(cInstanceIdent, String(cUpdateStateContent), String(cUpdateStateContentChecksum)))
        .WillOnce(Invoke([&stateSentPromise](const InstanceIdent&, const String&, const String&) {
            stateSentPromise.set_value();
            return ErrorEnum::eNone;
        }));

    fsEventSubscriber->OnFSEvent(ToStatePath(cInstanceIdent).c_str(), {});

    EXPECT_EQ(stateSentPromise.get_future().wait_for(std::chrono::seconds(10)), std::future_status::ready)
        << "State was not sent in time";

    // New state is accepted

    err = mStorageState.AcceptState(
        cInstanceIdent, cUpdateStateContentChecksum, StateResultEnum::eAccepted, "accepted");
    EXPECT_TRUE(err.IsNone()) << "Failed to accept state: " << tests::utils::ErrorToStr(err);

    // And the storage stub is updated

    EXPECT_TRUE(mStorageStub.Contains([&cUpdateStateContentChecksum](const InstanceInfo& info) {
        return info.mInstanceIdent == cInstanceIdent && info.mStateChecksum == cUpdateStateContentChecksum;
    })) << "Storage state info should be updated with new state checksum";

    EXPECT_CALL(mFSWatcherMock, Unsubscribe).WillOnce(Return(ErrorEnum::eNone));

    err = mStorageState.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop storage state: " << tests::utils::ErrorToStr(err);
}

} // namespace aos::cm::storagestate
