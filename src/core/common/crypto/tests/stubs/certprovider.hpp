/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_TESTS_STUBS_CERTPROVIDER_HPP_
#define AOS_CORE_COMMON_CRYPTO_TESTS_STUBS_CERTPROVIDER_HPP_

#include <map>
#include <string>

#include <core/iam/certhandler/certprovider.hpp>

namespace aos::crypto {

/**
 * Stub implementation of CertProviderItf.
 */
class CertProviderStub : public iam::certhandler::CertProviderItf {
public:
    Error GetCert(const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial,
        iam::certhandler::CertInfo& resCert) const override
    {
        (void)issuer;
        (void)serial;

        if (mCerts.count(certType.CStr()) == 0) {
            return ErrorEnum::eNotFound;
        }

        resCert = mCerts.find(certType.CStr())->second;

        return ErrorEnum::eNone;
    }

    Error SubscribeCertChanged(const String& certType, iam::certhandler::CertReceiverItf& certReceiver) override
    {
        (void)certType;
        (void)certReceiver;

        return ErrorEnum::eNone;
    }

    Error UnsubscribeCertChanged(iam::certhandler::CertReceiverItf& certReceiver) override
    {
        (void)certReceiver;

        return ErrorEnum::eNone;
    }

    void AddCert(const std::string& certType, const std::string& certName)
    {
        iam::certhandler::CertInfo certInfo;

        certInfo.mCertURL = ("file://" + FullCertPath(certName)).c_str();
        certInfo.mKeyURL  = ("file://" + FullKeyPath(certName)).c_str();

        mCerts[certType] = certInfo;
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

    std::map<std::string, iam::certhandler::CertInfo> mCerts;
};

} // namespace aos::crypto

#endif
