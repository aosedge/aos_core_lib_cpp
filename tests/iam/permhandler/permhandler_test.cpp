/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>

#include <gtest/gtest.h>

#include "aos/common/tools/buffer.hpp"
#include "aos/iam/permhandler.hpp"
#include "log.hpp"
#include "mocks/subjectsobservermock.hpp"

using namespace aos;
using namespace aos::iam::permhandler;
using namespace testing;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class PermHandlerTest : public Test {
protected:
    void SetUp() override { InitLogs(); }

    PermHandler mPermHandler;
};

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(PermHandlerTest, RegisterInstanceSucceeds)
{
    InstanceIdent                                instanceIdent;
    StaticArray<FunctionalServicePermissions, 1> perms;

    const auto err = mPermHandler.RegisterInstance(instanceIdent, perms);
    EXPECT_TRUE(err.mError.IsNone()) << err.mError.Message();
    ASSERT_FALSE(err.mValue.IsEmpty());
}

TEST_F(PermHandlerTest, RegisterInstanceReturnsSecretFromCache)
{
    InstanceIdent                                instanceIdent;
    StaticArray<FunctionalServicePermissions, 1> perms;
    Error                                        err;
    StaticString<uuid::cUUIDLen>                 secret1;
    StaticString<uuid::cUUIDLen>                 secret2;

    Tie(secret1, err) = mPermHandler.RegisterInstance(instanceIdent, perms);
    ASSERT_TRUE(err.IsNone()) << err.Message();
    ASSERT_FALSE(secret1.IsEmpty());

    Tie(secret2, err) = mPermHandler.RegisterInstance(instanceIdent, perms);
    ASSERT_TRUE(err.IsNone()) << err.Message();
    EXPECT_EQ(secret1, secret2);
}

TEST_F(PermHandlerTest, RegisterInstanceReachedMaxSize)
{
    const StaticArray<FunctionalServicePermissions, 1> perms;
    InstanceIdent                                      instanceIdent {"", "", 0};
    Error                                              err;
    StaticString<uuid::cUUIDLen>                       secret;

    for (size_t i = 0; i < cMaxNumInstances; ++i) {
        instanceIdent.mInstance = i;

        Tie(secret, err) = mPermHandler.RegisterInstance(instanceIdent, perms);
        ASSERT_TRUE(err.IsNone()) << err.Message();
        ASSERT_FALSE(secret.IsEmpty());
    }

    instanceIdent.mInstance = cMaxNumInstances;

    Tie(secret, err) = mPermHandler.RegisterInstance(instanceIdent, perms);
    ASSERT_TRUE(err.Is(ErrorEnum::eNoMemory)) << err.Message();
    ASSERT_TRUE(secret.IsEmpty());
}

TEST_F(PermHandlerTest, GetPermissionsNotFound)
{
    InstanceIdent                instanceIdent;
    StaticArray<PermKeyValue, 1> perms;

    auto err = mPermHandler.GetPermissions("uknownSecretUUID", "unknownServerId", instanceIdent, perms);
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << err.Message();
}

TEST_F(PermHandlerTest, GetPermissionsNoMemoryForPerms)
{
    const InstanceIdent instanceIdent1 {"serviceID1", "subjectID1", 1};

    FunctionalServicePermissions testServicePermissions;
    testServicePermissions.mName = "testService";
    testServicePermissions.mPermissions.PushBack({"key1", "value1"});
    testServicePermissions.mPermissions.PushBack({"key2", "value2"});

    StaticArray<FunctionalServicePermissions, 2> funcServerPermissions;
    funcServerPermissions.PushBack(testServicePermissions);

    InstanceIdent                resInstanceIdent;
    StaticArray<PermKeyValue, 1> resServicePerms;
    Error                        err;
    StaticString<uuid::cUUIDLen> secret;

    Tie(secret, err) = mPermHandler.RegisterInstance(instanceIdent1, funcServerPermissions);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    err = mPermHandler.GetPermissions(secret, "testService", resInstanceIdent, resServicePerms);
    ASSERT_TRUE(err.Is(ErrorEnum::eNoMemory)) << err.Message();
    ASSERT_TRUE(resServicePerms.IsEmpty());
}

TEST_F(PermHandlerTest, UnregisterInstance)
{
    InstanceIdent instanceIdent;
    instanceIdent.mServiceID = "test-service-id";

    StaticArray<FunctionalServicePermissions, 1> perms;
    StaticString<uuid::cUUIDLen>                 secret;

    auto err = mPermHandler.UnregisterInstance(instanceIdent);
    EXPECT_FALSE(err.IsNone()) << err.Message();

    Tie(secret, err) = mPermHandler.RegisterInstance(instanceIdent, perms);
    EXPECT_TRUE(err.IsNone()) << err.Message();
    EXPECT_FALSE(secret.IsEmpty());

    err = mPermHandler.UnregisterInstance(instanceIdent);
    EXPECT_TRUE(err.IsNone()) << err.Message();

    err = mPermHandler.UnregisterInstance(instanceIdent);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << err.Message();
}

TEST_F(PermHandlerTest, TestInstancePermissions)
{
    const InstanceIdent instanceIdent1 {"serviceID1", "subjectID1", 1};
    const InstanceIdent instanceIdent2 {"serviceID2", "subjectID2", 2};
    InstanceIdent       instance;

    FunctionalServicePermissions visServicePermissions;
    visServicePermissions.mName = "vis";
    visServicePermissions.mPermissions.PushBack({"*", "rw"});
    visServicePermissions.mPermissions.PushBack({"test", "r"});

    FunctionalServicePermissions systemCoreServicePermissions;
    systemCoreServicePermissions.mName = "systemCore";
    systemCoreServicePermissions.mPermissions.PushBack({"test1.*", "rw"});
    systemCoreServicePermissions.mPermissions.PushBack({"test2", "r"});

    StaticArray<FunctionalServicePermissions, 2> funcServerPermissions;
    funcServerPermissions.PushBack(visServicePermissions);
    funcServerPermissions.PushBack(systemCoreServicePermissions);

    StaticString<uuid::cUUIDLen> secret1;
    StaticString<uuid::cUUIDLen> secret2;
    Error                        err;
    StaticArray<PermKeyValue, 2> permsResult;

    Tie(secret1, err) = mPermHandler.RegisterInstance(instanceIdent1, funcServerPermissions);
    ASSERT_TRUE(err.IsNone()) << err.Message();
    ASSERT_FALSE(secret1.IsEmpty());

    err = mPermHandler.GetPermissions(secret1, "vis", instance, permsResult);
    ASSERT_TRUE(err.IsNone()) << err.Message();
    ASSERT_EQ(instance, instanceIdent1) << "Wrong instance";
    ASSERT_EQ(permsResult, visServicePermissions.mPermissions);

    Tie(secret2, err) = mPermHandler.RegisterInstance(instanceIdent2, funcServerPermissions);
    ASSERT_TRUE(err.IsNone()) << err.Message();
    ASSERT_FALSE(secret2.IsEmpty());

    ASSERT_NE(secret1, secret2) << "Duplicated secret for second registration";

    permsResult.Clear();
    err = mPermHandler.GetPermissions(secret2, "systemCore", instance, permsResult);
    ASSERT_TRUE(err.IsNone()) << err.Message();
    ASSERT_EQ(instance, instanceIdent2) << "Wrong instance";
    ASSERT_EQ(permsResult, systemCoreServicePermissions.mPermissions);

    permsResult.Clear();
    err = mPermHandler.GetPermissions(secret1, "nill", instance, permsResult);
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << err.Message();

    err = mPermHandler.UnregisterInstance(instanceIdent2);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    err = mPermHandler.UnregisterInstance(instanceIdent2);
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << err.Message();

    err = mPermHandler.UnregisterInstance(instanceIdent1);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    err = mPermHandler.UnregisterInstance(instanceIdent1);
    ASSERT_TRUE(err.Is(ErrorEnum::eNotFound)) << err.Message();
}
