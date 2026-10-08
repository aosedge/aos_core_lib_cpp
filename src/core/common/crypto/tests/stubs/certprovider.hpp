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
    Error GetCert(const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial,
        CertInfo& resCert) const override
    {
        (void)issuer;
        (void)serial;

        if (mCerts.count(certType.CStr()) == 0) {
            return ErrorEnum::eNotFound;
        }

        resCert = mCerts.find(certType.CStr())->second;

        return ErrorEnum::eNone;
    }

    Error GetAllCerts(const String& certType, Array<CertInfo>& infos) const override
    {
        if (const auto it = mAllCerts.find(certType.CStr()); it != mAllCerts.end()) {
            for (const auto& cert : it->second) {
                if (auto err = infos.PushBack(cert); !err.IsNone()) {
                    return err;
                }
            }

            return ErrorEnum::eNone;
        }

        if (mCerts.count(certType.CStr()) == 0) {
            return ErrorEnum::eNotFound;
        }

        return infos.PushBack(mCerts.find(certType.CStr())->second);
    }

    Error GetRootCertType(String& certType) const override
    {
        if (mRootCertType.empty()) {
            return ErrorEnum::eNotFound;
        }

        certType = mRootCertType.c_str();

        return ErrorEnum::eNone;
    }

    Error SubscribeListener(const String& certType, iamclient::CertListenerItf& certListener) override
    {
        (void)certType;
        (void)certListener;

        return ErrorEnum::eNone;
    }

    Error UnsubscribeListener(iamclient::CertListenerItf& certListener) override
    {
        (void)certListener;

        return ErrorEnum::eNone;
    }

    void AddCert(const std::string& certType, const std::string& certName)
    {
        CertInfo certInfo;

        certInfo.mCertURL = ("file://" + FullCertPath(certName)).c_str();
        certInfo.mKeyURL  = ("file://" + FullKeyPath(certName)).c_str();

        mCerts[certType] = certInfo;
        mAllCerts.erase(certType);
        RememberRootCertType(certType);
    }

    void AddEmptyCertType(const std::string& certType)
    {
        mCerts.erase(certType);
        mAllCerts[certType] = {};
        RememberRootCertType(certType);
    }

    void AddCertURL(const std::string& certType, const std::string& url)
    {
        CertInfo certInfo;

        certInfo.mCertURL = url.c_str();

        mAllCerts[certType].push_back(certInfo);
        mCerts.erase(certType);
        RememberRootCertType(certType);
    }

    void AddCertCopies(const std::string& certType, const std::string& certName, size_t count)
    {
        mCerts.erase(certType);
        mAllCerts[certType].clear();
        RememberRootCertType(certType);

        for (size_t i = 0; i < count; ++i) {
            CertInfo certInfo;

            certInfo.mCertURL = ("file://" + FullCertPath(certName)).c_str();
            mAllCerts[certType].push_back(certInfo);
        }
    }

private:
    void RememberRootCertType(const std::string& certType)
    {
        if (certType == "rootcerts") {
            mRootCertType = certType;
        }
    }

    static std::string FullCertPath(const std::string& name)
    {
        return std::string(CRYPTOHELPER_CERTS_DIR) + "/" + name + ".pem";
    }

    static std::string FullKeyPath(const std::string& name)
    {
        return std::string(CRYPTOHELPER_CERTS_DIR) + "/" + name + ".key";
    }

    std::map<std::string, CertInfo>              mCerts;
    std::map<std::string, std::vector<CertInfo>> mAllCerts;
    std::string                                  mRootCertType;
};

} // namespace aos::crypto

#endif
