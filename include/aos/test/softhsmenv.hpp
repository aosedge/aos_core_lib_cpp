/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_SOFTHSMENV_H_
#define AOS_SOFTHSMENV_H_

#include "aos/common/pkcs11/pkcs11.hpp"

namespace aos {
namespace test {

class SoftHSMEnv {
public:
    Error Init(const String& pin, const String& label, const char* confFile = SOFTHSM_BASE_DIR "/softhsm2.conf",
        const char* tokensDir = SOFTHSM_BASE_DIR "/tokens", const char* libPath = SOFTHSM2_LIB);

    RetWithError<SharedPtr<pkcs11::SessionContext>> OpenUserSession(const String& pin, bool login = true);

    pkcs11::PKCS11Manager&            GetManager() { return mManager; }
    SharedPtr<pkcs11::LibraryContext> GetLibrary() { return mLibrary; }
    pkcs11::SlotID                    GetSlotID() const { return mSlotID; }

private:
    Error                        InitTestToken(const String& pin, const String& label);
    RetWithError<pkcs11::SlotID> FindTestToken(const String& label);

    pkcs11::SlotID                    mSlotID = 0;
    pkcs11::PKCS11Manager             mManager;
    SharedPtr<pkcs11::LibraryContext> mLibrary;
};

} // namespace test
} // namespace aos

#endif
