/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_TESTS_STUBS_CERTPROVIDER_HPP_
#define AOS_CORE_COMMON_CRYPTO_TESTS_STUBS_CERTPROVIDER_HPP_

#include <map>
#include <string>
#include <vector>

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

    Error GetRootCerts(Array<CertInfo>& infos) const override
    {
        if (!mHasRootCerts) {
            return ErrorEnum::eNotFound;
        }

        for (const auto& cert : mRootCerts) {
            if (auto err = infos.PushBack(cert); !err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    Error SubscribeListener(const String& storageID, iamclient::CertListenerItf& certListener) override
    {
        (void)storageID;
        (void)certListener;

        return ErrorEnum::eNone;
    }

    Error SubscribeRootCertsListener(iamclient::CertListenerItf& certListener) override
    {
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

    void AddRootCert(const std::string& certName)
    {
        mHasRootCerts = true;
        mRootCerts    = {MakeCertInfo("file://" + FullCertPath(certName))};
    }

    void AddEmptyRootCerts()
    {
        mHasRootCerts = true;
        mRootCerts.clear();
    }

    void AddRootCertURL(const std::string& url)
    {
        mHasRootCerts = true;
        mRootCerts    = {MakeCertInfo(url)};
    }

    void AddRootCertCopies(const std::string& certName, size_t count)
    {
        mHasRootCerts = true;
        mRootCerts.clear();
        mRootCerts.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            mRootCerts.push_back(MakeCertInfo("file://" + FullCertPath(certName)));
        }
    }

private:
    static CertInfo MakeCertInfo(const std::string& certURL)
    {
        CertInfo certInfo;

        certInfo.mCertURL = certURL.c_str();

        return certInfo;
    }

    static std::string FullCertPath(const std::string& name)
    {
        return std::string(CRYPTOHELPER_CERTS_DIR) + "/" + name + ".pem";
    }

    static std::string FullKeyPath(const std::string& name)
    {
        return std::string(CRYPTOHELPER_CERTS_DIR) + "/" + name + ".key";
    }

    std::map<std::string, CertInfo> mCerts;
    std::vector<CertInfo>           mRootCerts;
    bool                            mHasRootCerts {};
};

} // namespace aos::crypto

#endif
