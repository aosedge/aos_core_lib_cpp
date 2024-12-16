/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/test/softhsmenv.hpp"

namespace aos::test {

Error SoftHSMEnv::Init(
    const String& pin, const String& label, const char* confFile, const char* tokensDir, const char* libPath)
{
    // Clear softhsm directory
    FS::ClearDir(tokensDir);
    setenv("SOFTHSM2_CONF", confFile, true);

    mLibrary = mManager.OpenLibrary(libPath);
    if (!mLibrary) {
        return ErrorEnum::eFailed;
    }

    return InitTestToken(pin, label);
}

RetWithError<SharedPtr<pkcs11::SessionContext>> SoftHSMEnv::OpenUserSession(const String& pin, bool login)
{
    Error                             err = ErrorEnum::eNone;
    SharedPtr<pkcs11::SessionContext> session;

    Tie(session, err) = mLibrary->OpenSession(mSlotID, CKF_RW_SESSION | CKF_SERIAL_SESSION);
    if (!err.IsNone() || !session) {
        return {nullptr, err};
    }

    if (login) {
        err = session->Login(CKU_USER, pin);
        if (!err.IsNone()) {
            return {nullptr, err};
        }
    }

    return session;
}

Error SoftHSMEnv::InitTestToken(const String& pin, const String& label)
{
    if (pin.IsEmpty()) {
        return ErrorEnum::eNone;
    }

    Error err         = ErrorEnum::eNone;
    Tie(mSlotID, err) = FindTestToken(label);

    if (err.Is(ErrorEnum::eNotFound)) {
        constexpr auto cDefaultSlotID = 0;

        err = mLibrary->InitToken(cDefaultSlotID, pin, label);
        if (!err.IsNone()) {
            return err;
        }

        Tie(mSlotID, err) = FindTestToken(label);
        if (!err.IsNone()) {
            return err;
        }

        SharedPtr<pkcs11::SessionContext> session;
        Tie(session, err) = mLibrary->OpenSession(mSlotID, CKF_RW_SESSION | CKF_SERIAL_SESSION);
        if (!err.IsNone()) {
            return err;
        }

        err = session->Login(CKU_SO, pin);
        if (!err.IsNone()) {
            return err;
        }

        err = session->InitPIN(pin);
        if (!err.IsNone()) {
            return err;
        }

        err = session->Logout();
    }

    return err;
}

RetWithError<pkcs11::SlotID> SoftHSMEnv::FindTestToken(const String& label)
{
    constexpr auto                                cMaxSlotListSize = 10;
    StaticArray<pkcs11::SlotID, cMaxSlotListSize> slotList;

    auto err = mLibrary->GetSlotList(true, slotList);
    if (!err.IsNone()) {
        return {0, err};
    }

    for (const auto id : slotList) {
        pkcs11::TokenInfo tokenInfo;
        err = mLibrary->GetTokenInfo(id, tokenInfo);
        if (!err.IsNone()) {
            return {0, err};
        }

        if (tokenInfo.mLabel == label) {
            return {id, ErrorEnum::eNone};
        }
    }
    return {0, ErrorEnum::eNotFound};
}

} // namespace aos::test
