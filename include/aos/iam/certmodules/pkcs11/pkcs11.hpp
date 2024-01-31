/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_IAM_MODULES_PKCS11_HPP_
#define AOS_IAM_MODULES_PKCS11_HPP_

#include "aos/common/pkcs11/pkcs11.hpp"
#include "aos/common/pkcs11/privatekey.hpp"
#include "aos/common/tools/optional.hpp"
#include "aos/common/uuid.hpp"
#include "aos/iam/certmodules/hsm.hpp"
#include "aos/iam/config.hpp"

namespace aos {
namespace iam {
namespace certhandler {

/**
 * PKCS11 configuration
 */
struct PKCS11ModuleConfig {
    /**
     * Library name.
     */
    StaticString<cFilePathLen> mLibrary;

    /**
     * Slot ID.
     */
    Optional<uint32_t> mSlotID;

    /**
     * Slot index.
     */
    Optional<int> mSlotIndex;

    /**
     * Token label.
     */
    StaticString<pkcs11::cLabelLen> mTokenLabel;

    /**
     * Path to User PIN.
     */
    StaticString<cFilePathLen> mUserPINPath;

    /**
     * Module
     */
    bool mModulePathInURL;
};

/**
 * PKCS11 implementation of HSM interface.
 */
class PKCS11Module : public HSMItf {
public:
    /**
     * Initializes module.
     *
     * @param certType certificate type.
     * @param config module configuration.
     * @param pkcs11 reference to pkcs11 library context.
     * @param x509Provider reference to x509 crypto interface.
     * @return Error.
     */
    Error Init(const String& certType, const PKCS11ModuleConfig& config, pkcs11::PKCS11Manager& pkcs11,
        crypto::x509::ProviderItf& x509Provider);

    /**
     * Owns the module.
     *
     * @param password certificate password.
     * @return Error.
     */
    Error SetOwner(const String& password) override;

    /**
     * Removes all module certificates.
     *
     * @return Error.
     */
    Error Clear() override;

    /**
     * Generates private key.
     *
     * @param password owner password.
     * @param keyType key type.
     * @return RetWithError<SharedPtr<crypto::PrivateKeyItf>>.
     */
    RetWithError<SharedPtr<crypto::PrivateKeyItf>> CreateKey(const String& password, crypto::KeyType keyType) override;

    /**
     * Applies certificate chain to a module.
     *
     * @param certChain certificate chain.
     * @param[out] certInfo info about a top certificate in a chain.
     * @param[out] password owner password.
     * @return Error.
     */
    Error ApplyCert(const Array<crypto::x509::Certificate>& certChain, CertInfo& certInfo, String& password) override;

    /**
     * Removes certificate chain using top level certificate URL and password.
     *
     * @param certURL top level certificate URL.
     * @param password owner password.
     * @return Error.
     */
    Error RemoveCert(const String& certURL, const String& password) override;

    /**
     * Removes private key from a module.
     *
     * @param keyURL private key URL.
     * @param password owner password.
     * @return Error.
     */
    Error RemoveKey(const String& keyURL, const String& password) override;

    /**
     * Returns valid/invalid certificates.
     *
     * @param[out] invalidCerts invalid certificate URLs.
     * @param[out] invalidKeys invalid key URLs.
     * @param[out] validCerts information about valid certificates.
     * @return Error.
     */
    Error ValidateCertificates(Array<StaticString<cURLLen>>& invalidCerts, Array<StaticString<cURLLen>>& invalidKeys,
        Array<CertInfo>& validCerts) override;

private:
    static constexpr auto cEnvLoginType      = "CKTEEC_LOGIN_TYPE";
    static constexpr auto cDefaultTokenLabel = "aos";
    static constexpr auto cTeeLoginTypeLen   = AOS_CONFIG_CERTMODULE_PKCS11_TEE_LOGIN_TYPE_LEN;
    static constexpr auto cUUIDStringLen     = AOS_CONFIG_UUID_STR_LEN;
    static constexpr auto cRSAKeyLength      = 2048;
    static constexpr auto cECSDACurveID      = pkcs11::EllipticCurve::eP384;
    static constexpr auto cPKCS11Scheme      = "pkcs11";

    static constexpr auto cLoginTypePublic = "public";
    static constexpr auto cLoginTypeUser   = "user";
    static constexpr auto cLoginTypeGroup  = "group";

    using DERCert = StaticArray<uint8_t, crypto::cCertDERSize>;

    struct PendingKey {
        uuid::UUID         mUUID;
        pkcs11::PrivateKey mKey;
    };

    struct SearchObject;

    String                       GetTokenLabel() const;
    RetWithError<pkcs11::SlotID> GetSlotID();
    RetWithError<bool>           IsOwned() const;

    Error PrintInfo(pkcs11::SlotID slotId) const;

    Error GetTeeUserPIN(const String& loginType, String& userPIN);
    Error GeneratePIN(const String& loginType, String& userPIN);
    Error GetUserPin(String& pin) const;

    RetWithError<pkcs11::SessionContext*> CreateSession(bool userLogin, const String& pin);

    Error FindObject(pkcs11::SessionContext& session, const SearchObject& filter, Array<SearchObject>& dst);

    Error TokenMemInfo() const;

    bool CheckCertificate(const crypto::x509::Certificate& cert, const crypto::PrivateKeyItf& key) const;

    Error CreateCertificateChain(pkcs11::SessionContext& session, const Array<uint8_t>& id, const String& label,
        const Array<crypto::x509::Certificate>& chain);

    Error CreateURL(const String& label, const Array<uint8_t>& id, String& url);
    Error ParseURL(const String& url, String& label, Array<uint8_t>& id);

    Error GetValidInfo(pkcs11::SessionContext& session, Array<SearchObject>& certs, Array<SearchObject>& privKeys,
        Array<SearchObject>& pubKeys, Array<CertInfo>& resCerts);
    SearchObject* FindObjectByID(Array<SearchObject>& array, const Array<uint8_t>& id);
    Error GetX509Cert(pkcs11::SessionContext& session, pkcs11::ObjectHandle object, crypto::x509::Certificate& cert);
    Error CreateCertInfo(const crypto::x509::Certificate& cert, const Array<uint8_t>& keyID,
        const Array<uint8_t>& certID, CertInfo& certInfo);
    Error CreateInvalidURLs(const Array<SearchObject>& objects, Array<StaticString<cURLLen>>& urls);

    StaticString<cCertTypeLen> mCertType;
    PKCS11ModuleConfig         mConfig {};

    SharedPtr<pkcs11::LibraryContext> mPKCS11;
    crypto::x509::ProviderItf*        mX509Provider {};

    uint32_t                         mSlotID = 0;
    StaticString<cTeeLoginTypeLen>   mTeeLoginType;
    StaticString<pkcs11::cPINLength> mUserPIN;

    StaticAllocator<sizeof(crypto::x509::Certificate) + sizeof(DERCert)> mTmpObjAllocator;
    StaticAllocator<pkcs11::cPrivateKeyMaxSize * cCertsPerModule>        mLocalCacheAllocator;

    StaticArray<PendingKey, cCertsPerModule> mPendingKeys;
    UniquePtr<pkcs11::SessionContext>        mSession;
};

} // namespace certhandler
} // namespace iam
} // namespace aos

#endif
