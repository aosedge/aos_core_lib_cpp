/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_TESTS_STUBS_CERTPROVIDER_HPP_
#define AOS_CORE_COMMON_CRYPTO_TESTS_STUBS_CERTPROVIDER_HPP_

#include <map>
#include <string>

#include <core/common/iamclient/itf/certprovider.hpp>

namespace aos::crypto {

/**
 * Stub implementation of CertProviderItf.
 */
class CertProviderStub : public iamclient::CertProviderItf {
public:
    Error GetCert(const String& storageID, const Array<uint8_t>& issuer, const Array<uint8_t>& serial,
        CertInfo& resCert) const override
    {
        (void)issuer;
        (void)serial;

        if (mCerts.count(storageID.CStr()) == 0) {
            return ErrorEnum::eNotFound;
        }

        resCert = mCerts.find(storageID.CStr())->second;

        return ErrorEnum::eNone;
    }

    Error SubscribeListener(const String& storageID, iamclient::CertListenerItf& certListener) override
    {
        (void)storageID;
        (void)certListener;

        return ErrorEnum::eNone;
    }

    Error UnsubscribeListener(iamclient::CertListenerItf& certListener) override
    {
        (void)certListener;

        return ErrorEnum::eNone;
    }

    void AddCert(const std::string& storageID, const std::string& certName)
    {
        CertInfo certInfo;

        certInfo.mCertURL = ("file://" + FullCertPath(certName)).c_str();
        certInfo.mKeyURL  = ("file://" + FullKeyPath(certName)).c_str();

        mCerts[storageID] = certInfo;
    }

private:
    static std::string FullCertPath(const std::string& name)
    {
        return std::string(CRYPTOHELPER_CERTS_DIR) + "/" + name + ".pem";
    }

    static std::string FullKeyPath(const std::string& name)
    {
        return std::string(CRYPTOHELPER_CERTS_DIR) + "/" + name + ".key";
    }

    std::map<std::string, CertInfo> mCerts;
};

} // namespace aos::crypto

#endif
