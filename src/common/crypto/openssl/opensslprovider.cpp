/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <openssl/core.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/provider.h>
#include <openssl/x509.h>

#include <aos/common/tools/error.hpp>
#include <aos/common/tools/logger.hpp>

#include "aos/common/crypto/openssl/opensslprovider.hpp"

namespace aos::crypto::openssl {

#define OSSL_DISPATCH_END                                                                                              \
    {                                                                                                                  \
        0, NULL                                                                                                        \
    }
#define OSSL_ALGORITHM_END                                                                                             \
    {                                                                                                                  \
        NULL, NULL, NULL, NULL                                                                                         \
    }

namespace {

/***********************************************************************************************************************
 * Key management functions
 **********************************************************************************************************************/

struct AosPrivKey {
    PrivateKeyItf* mPrivKey;
};

static void* KeyMgmtNew(void* provctx)
{
    (void)provctx;

    auto key = static_cast<AosPrivKey*>(OPENSSL_zalloc(sizeof(AosPrivKey)));
    if (!key) {
        LOG_ERR() << "OpenSSL allocation failed, err=" << OPENSSL_ERROR();

        return nullptr;
    }

    return key;
}

void KeyMgmtFree(void* keydata)
{
    OPENSSL_free(keydata);
}

int KeyMgmtHas(const void* key, int selection)
{
    const auto aosKey = static_cast<const AosPrivKey*>(key);

    if (!(selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY || selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY)
        && (aosKey == nullptr || aosKey->mPrivKey == nullptr)) {
        return 1;
    }

    // Public / Private key parameters supported only
    if (selection & OSSL_KEYMGMT_SELECT_DOMAIN_PARAMETERS) {
        return 1;
    }

    if (selection & OSSL_KEYMGMT_SELECT_OTHER_PARAMETERS) {
        return 1;
    }

    return 0;
}

const char* KeyMgmtQuery(int id)
{
    (void)id;

    return cAosAlgorithm;
}

int KeyMgmtImport(void* keydata, int selection, const OSSL_PARAM* p)
{
    if (selection != EVP_PKEY_KEYPAIR) {
        LOG_ERR() << "Not supported selection for AOS key: err=" << AOS_ERROR_WRAP(Error(ErrorEnum::eFailed));

        return 0;
    }

    auto aosKey = static_cast<AosPrivKey*>(keydata);

    const OSSL_PARAM* pKeyParam = OSSL_PARAM_locate_const(p, cPKeyParamAosKeyPair);
    if (pKeyParam != nullptr) {
        if (pKeyParam->data_type != OSSL_PARAM_OCTET_STRING) {
            LOG_ERR() << "Wrong data type for AOS key: err=" << AOS_ERROR_WRAP(Error(ErrorEnum::eFailed));

            return 0;
        }

        if (pKeyParam->data_size != sizeof(const PrivateKeyItf*)) {
            LOG_ERR() << "Wrong data size for AOS key: err=" << AOS_ERROR_WRAP(Error(ErrorEnum::eFailed));

            return 0;
        }

        aosKey->mPrivKey = static_cast<PrivateKeyItf*>(pKeyParam->data);
    }

    return 1;
}

const OSSL_PARAM* KeyMgmtImportTypes(int selection)
{
    (void)selection;

    static const OSSL_PARAM cAosImportTypes[] = {OSSL_PARAM_BN(cPKeyParamAosKeyPair, NULL, 0), OSSL_PARAM_END};

    return cAosImportTypes;
}

/***********************************************************************************************************************
 * Signing functions
 **********************************************************************************************************************/

struct AosSignCtx {
    AosPrivKey*   mAosKey;
    const EVP_MD* mEvpMd;
    Hash          mHash;
};

RetWithError<int> GetNidFromOID(const Array<uint8_t>& oid)
{
    auto [fullOID, err] = GetFullOID(oid);
    if (!err.IsNone()) {
        return {-1, OPENSSL_ERROR()};
    }

    const uint8_t* oidData = fullOID.Get();

    auto asn1obj = DeferRelease(d2i_ASN1_OBJECT(nullptr, &oidData, fullOID.Size()), ASN1_OBJECT_free);
    if (!asn1obj) {
        return {-1, OPENSSL_ERROR()};
    }

    int nid = OBJ_obj2nid(asn1obj.Get());
    if (nid == NID_undef) {
        return {-1, OPENSSL_ERROR()};
    }

    return {nid, ErrorEnum::eNone};
}

RetWithError<int> GetECCurveBitLen(int nid)
{
    switch (nid) {
    case NID_X9_62_prime192v1:
        return {192, ErrorEnum::eNone};

    case NID_secp224r1:
        return {224, ErrorEnum::eNone};

    case NID_X9_62_prime256v1:
        return {256, ErrorEnum::eNone};

    case NID_secp384r1:
        return {384, ErrorEnum::eNone};

    case NID_secp521r1:
        return {521, ErrorEnum::eNone};

    case NID_secp192k1:
        return {192, ErrorEnum::eNone};

    case NID_secp224k1:
        return {224, ErrorEnum::eNone};

    case NID_secp256k1:
        return {256, ErrorEnum::eNone};

    case NID_X25519:
        return {255, ErrorEnum::eNone};

    case NID_X448:
        return {448, ErrorEnum::eNone};

    case NID_brainpoolP256r1:
        return {256, ErrorEnum::eNone};

    case NID_brainpoolP384r1:
        return {384, ErrorEnum::eNone};

    case NID_brainpoolP512r1:
        return {512, ErrorEnum::eNone};

    default:
        return {0, AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument)};
    }
}

RetWithError<HashEnum> GetHashAlg(const RSAPublicKey& pubKey)
{
    auto bn = DeferRelease(BN_bin2bn(pubKey.GetN().Get(), pubKey.GetN().Size(), NULL), BN_free);
    if (!bn) {
        return {HashEnum::eNone, OPENSSL_ERROR()};
    }

    auto modulusBitlen = BN_num_bits(bn.Get());

    if (modulusBitlen < 2048) {
        return HashEnum::eSHA1;
    }

    if (modulusBitlen <= 3072) {
        return HashEnum::eSHA256;
    }

    if (modulusBitlen <= 7680) {
        return HashEnum::eSHA384;
    }

    return HashEnum::eSHA512;
}

RetWithError<HashEnum> GetHashAlg(const ECDSAPublicKey& pubKey)
{
    auto [nid, err] = GetNidFromOID(pubKey.GetECParamsOID());
    if (!err.IsNone()) {
        return {HashEnum::eNone, err};
    }

    int curveBitlen = 0;

    Tie(curveBitlen, err) = GetECCurveBitLen(nid);
    if (!err.IsNone()) {
        return {HashEnum::eNone, err};
    }

    if (curveBitlen <= 160) {
        return HashEnum::eSHA1;
    }

    if (curveBitlen <= 224) {
        return HashEnum::eSHA224;
    }

    if (curveBitlen <= 256) {
        return HashEnum::eSHA256;
    }

    if (curveBitlen <= 384) {
        return HashEnum::eSHA384;
    }

    return HashEnum::eSHA512;
}

RetWithError<HashEnum> GetHashAlg(const PublicKeyItf& pubKey)
{
    switch (pubKey.GetKeyType().GetValue()) {
    case KeyTypeEnum::eRSA:
        return GetHashAlg(static_cast<const RSAPublicKey&>(pubKey));

    case KeyTypeEnum::eECDSA:
        return GetHashAlg(static_cast<const ECDSAPublicKey&>(pubKey));

    default:
        return {HashEnum::eNone, AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument)};
    }
}

RetWithError<int> GetSignAlgNID(const RSAPublicKey& pubKey)
{
    auto [hashAlg, err] = GetHashAlg(pubKey);
    if (!err.IsNone()) {
        return {-1, err};
    }

    switch (hashAlg) {
    case HashEnum::eSHA1:
        return {NID_sha1WithRSAEncryption, ErrorEnum::eNone};

    case HashEnum::eSHA224:
        return {NID_sha224WithRSAEncryption, ErrorEnum::eNone};

    case HashEnum::eSHA256:
        return {NID_sha256WithRSAEncryption, ErrorEnum::eNone};

    case HashEnum::eSHA384:
        return {NID_sha384WithRSAEncryption, ErrorEnum::eNone};

    case HashEnum::eSHA512:
        return {NID_sha512WithRSAEncryption, ErrorEnum::eNone};

    case HashEnum::eSHA3_256:
        return {-1, ErrorEnum::eNotSupported};

    default:
        return {-1, ErrorEnum::eNotSupported};
    }
}

RetWithError<int> GetSignAlgNID(const ECDSAPublicKey& pubKey)
{
    auto [hashAlg, err] = GetHashAlg(pubKey);
    if (!err.IsNone()) {
        return {-1, err};
    }

    switch (hashAlg) {
    case HashEnum::eSHA1:
        return {NID_ecdsa_with_SHA1, ErrorEnum::eNone};

    case HashEnum::eSHA224:
        return {NID_ecdsa_with_SHA224, ErrorEnum::eNone};

    case HashEnum::eSHA256:
        return {NID_ecdsa_with_SHA256, ErrorEnum::eNone};

    case HashEnum::eSHA384:
        return {NID_ecdsa_with_SHA384, ErrorEnum::eNone};

    case HashEnum::eSHA512:
        return {NID_ecdsa_with_SHA512, ErrorEnum::eNone};

    case HashEnum::eSHA3_256:
        return {NID_ecdsa_with_SHA3_256, ErrorEnum::eNone};

    default:
        return {-1, ErrorEnum::eNotSupported};
    }
}

Error FormatSignature(const PrivateKeyItf& privKey, Array<uint8_t>& signature)
{
    if (privKey.GetPublic().GetKeyType().GetValue() == KeyTypeEnum::eECDSA) {
        size_t         halfSize = signature.Size() / 2;
        const uint8_t* rData    = signature.Get();
        const uint8_t* sData    = signature.Get() + halfSize;

        // Create BIGNUMs from r and s
        auto r = DeferRelease(BN_bin2bn(rData, halfSize, nullptr), BN_free);
        if (!r) {
            return OPENSSL_ERROR();
        }

        auto s = DeferRelease(BN_bin2bn(sData, halfSize, nullptr), BN_free);
        if (!s) {
            return OPENSSL_ERROR();
        }

        auto sig = DeferRelease(ECDSA_SIG_new(), ECDSA_SIG_free);
        if (!sig) {
            return OPENSSL_ERROR();
        }

        if (ECDSA_SIG_set0(sig.Get(), r.Get(), s.Get()) != 1) {
            return OPENSSL_ERROR();
        }

        // Ownership transferred to ECDSA_SIG object
        r.Release();
        s.Release();

        // Convert ECDSA_SIG to DER
        uint8_t* derSig = nullptr;
        int      derLen = i2d_ECDSA_SIG(sig.Get(), &derSig);
        if (derLen <= 0) {
            return OPENSSL_ERROR();
        }

        auto freeSig = DeferRelease(derSig, AOS_OPENSSL_free);

        signature.Clear();

        auto err = signature.Insert(signature.begin(), derSig, derSig + derLen);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    return ErrorEnum::eNone;
}

X509_ALGOR* GetSignAlg(const PublicKeyItf& pubKey)
{
    auto alg = DeferRelease(X509_ALGOR_new(), X509_ALGOR_free);
    if (!alg) {
        LOG_ERR() << "Allocation failed: err=" << OPENSSL_ERROR();

        return nullptr;
    }

    switch (pubKey.GetKeyType().GetValue()) {
    case KeyTypeEnum::eRSA: {
        auto [algNID, err] = GetSignAlgNID(static_cast<const RSAPublicKey&>(pubKey));
        if (!err.IsNone()) {
            LOG_ERR() << "Get sign algorithm failed: err=" << AOS_ERROR_WRAP(err);

            return nullptr;
        }

        auto algOID = OBJ_nid2obj(algNID);
        if (!algOID) {
            LOG_ERR() << "Conversion failed, err=" << OPENSSL_ERROR();

            return nullptr;
        }

        // According to ossl_DER_w_algorithmIdentifier_MDWithRSAEncryption
        // implementation: PARAMETERS, always NULL in current standards
        X509_ALGOR_set0(alg.Get(), algOID, V_ASN1_NULL, NULL);

        return alg.Release();
    }

    case KeyTypeEnum::eECDSA: {
        auto [algNID, err] = GetSignAlgNID(static_cast<const ECDSAPublicKey&>(pubKey));
        if (!err.IsNone()) {
            LOG_ERR() << "Get sign algorithm failed: err=" << AOS_ERROR_WRAP(err);

            return nullptr;
        }

        auto algOID = OBJ_nid2obj(algNID);
        if (!algOID) {
            LOG_ERR() << "Conversion failed, err=" << OPENSSL_ERROR();

            return nullptr;
        }

        // According to ossl_DER_w_algorithmIdentifier_ECDSA_with_MD implementation:
        // there is no PARAMETERS for ECDSA
        X509_ALGOR_set0(alg.Get(), algOID, V_ASN1_UNDEF, NULL);

        return alg.Release();
    }
    default:
        return nullptr;
    }
}

int ConvertHashAlgToNID(HashEnum hashAlg)
{
    switch (hashAlg) {
    case HashEnum::eSHA1:
        return NID_sha1;

    case HashEnum::eSHA224:
        return NID_sha224;

    case HashEnum::eSHA256:
        return NID_sha256;

    case HashEnum::eSHA384:
        return NID_sha384;

    case HashEnum::eSHA512:
        return NID_sha512;

    case HashEnum::eSHA3_256:
        return NID_sha3_256;

    default:
        return NID_undef;
    }
}

void* SignNewCtx(void* provctx, const char* propq)
{
    (void)provctx;
    (void)propq;

    auto ctx = static_cast<AosSignCtx*>(OPENSSL_zalloc(sizeof(AosSignCtx)));
    if (!ctx) {
        LOG_ERR() << "Allocation failed, err=" << OPENSSL_ERROR();

        return nullptr;
    }

    return ctx;
}

void SignFreeCtx(void* sigctx)
{
    OPENSSL_free(sigctx);
}

int DgstSignInit(void* ctx, const char* mdname, void* provkey, const OSSL_PARAM params[])
{
    (void)params;
    (void)mdname;

    auto aosCtx = static_cast<AosSignCtx*>(ctx);
    if (!aosCtx) {
        LOG_ERR() << "AOS context is not initialized: err=" << AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument));

        return 0;
    }

    aosCtx->mAosKey = static_cast<AosPrivKey*>(provkey);

    if (!aosCtx->mAosKey || !aosCtx->mAosKey->mPrivKey) {
        LOG_ERR() << "AOS context is not initialized: err=" << AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument));

        return 0;
    }

    auto privKey = aosCtx->mAosKey->mPrivKey;

    auto [hashAlg, err] = GetHashAlg(privKey->GetPublic());
    if (!err.IsNone()) {
        LOG_ERR() << "Get hash algorithm failed: err=" << AOS_ERROR_WRAP(err);

        return 0;
    }

    aosCtx->mHash = hashAlg;

    auto hashNID = ConvertHashAlgToNID(hashAlg);
    if (hashNID == NID_undef) {
        LOG_ERR() << "Convert hash algorithm to nid failed: err=" << AOS_ERROR_WRAP(ErrorEnum::eFailed);

        return 0;
    }

    aosCtx->mEvpMd = EVP_get_digestbynid(hashNID);

    return 1;
}

int DgstSign(void* ctx, unsigned char* sig, size_t* siglen, size_t sigsize, const unsigned char* tbs, size_t tbslen)
{
    if (!ctx || !siglen || !tbs) {
        LOG_ERR() << "Invalid arguments: err=" << AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument));

        return 0;
    }

    auto aosCtx = static_cast<AosSignCtx*>(ctx);
    if (!aosCtx->mAosKey || !aosCtx->mAosKey->mPrivKey || !aosCtx->mEvpMd) {
        LOG_ERR() << "Invalid arguments: err=" << AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument));

        return 0;
    }

    auto privKey = aosCtx->mAosKey->mPrivKey;

    // when result buffer is null, return maximum signature size.
    if (!sig) {
        *siglen = cSignatureSize;

        return 1;
    }

    // Compute the hash.
    const EVP_MD*                         evpMd = aosCtx->mEvpMd;
    StaticArray<uint8_t, EVP_MAX_MD_SIZE> digest;
    unsigned int                          digestLen = 0;

    digest.Resize(digest.MaxSize());

    if (EVP_Digest(tbs, tbslen, digest.Get(), &digestLen, evpMd, NULL) != 1) {
        LOG_ERR() << "Digest calculation failed: err=" << OPENSSL_ERROR();

        return 0;
    }

    digest.Resize(digestLen);

    // Sign
    Array<uint8_t> signature {sig, static_cast<size_t>(sigsize)};

    auto err = privKey->Sign(digest, {aosCtx->mHash}, signature);
    if (!err.IsNone()) {
        LOG_ERR() << "Aos sign failed: err=" << AOS_ERROR_WRAP(err);

        return 0;
    }

    err = FormatSignature(*privKey, signature);
    if (!err.IsNone()) {
        LOG_ERR() << "Aos signature format failed: err=" << AOS_ERROR_WRAP(err);

        return 0;
    }

    *siglen = signature.Size();

    return 1;
}

int SignatureGetCtxParams(void* ctx, OSSL_PARAM params[])
{
    AosSignCtx* aosCtx = static_cast<AosSignCtx*>(ctx);
    if (!aosCtx || !aosCtx->mAosKey || !aosCtx->mAosKey->mPrivKey) {
        LOG_ERR() << "Aos context is not initalized: err=" << AOS_ERROR_WRAP(ErrorEnum::eWrongState);

        return 0;
    }

    if (params == nullptr) {
        LOG_ERR() << "Params are not initalized: err=" << AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);

        return 0;
    }

    OSSL_PARAM* p = OSSL_PARAM_locate(params, OSSL_SIGNATURE_PARAM_ALGORITHM_ID);
    if (p != nullptr) {
        auto alg = DeferRelease(GetSignAlg(aosCtx->mAosKey->mPrivKey->GetPublic()), X509_ALGOR_free);
        if (!alg) {
            return 0;
        }

        // Convert alg to DER
        uint8_t* outDer = nullptr;

        int outLen = i2d_X509_ALGOR(alg.Get(), &outDer);
        if (outLen <= 0) {
            LOG_ERR() << "Alg to DER conversion failed: err=" << OPENSSL_ERROR();

            return 0;
        }

        if (OSSL_PARAM_set_octet_string(p, outDer, outLen) != 1) {
            OPENSSL_free(outDer);

            return 0;
        }
    }

    return 1;
}

const OSSL_PARAM* SignatureGettableCtxParams(void*)
{
    static const OSSL_PARAM cGettableCtxParams[]
        = {OSSL_PARAM_octet_string(OSSL_SIGNATURE_PARAM_ALGORITHM_ID, NULL, 0), OSSL_PARAM_END};

    return cGettableCtxParams;
}

/***********************************************************************************************************************
 * Provider functions
 **********************************************************************************************************************/

const OSSL_ALGORITHM* ProviderQuery(void* provctx, int operationID, int* noCache)
{
    (void)provctx;

    *noCache = 0;

    static const OSSL_DISPATCH cSignFunctions[] = {{OSSL_FUNC_SIGNATURE_NEWCTX, (void (*)(void))SignNewCtx},
        {OSSL_FUNC_SIGNATURE_FREECTX, (void (*)(void))SignFreeCtx},

        {OSSL_FUNC_SIGNATURE_DIGEST_SIGN_INIT, (void (*)(void))DgstSignInit},
        {OSSL_FUNC_SIGNATURE_DIGEST_SIGN, (void (*)(void))DgstSign},

        {OSSL_FUNC_SIGNATURE_GET_CTX_PARAMS, (void (*)(void))SignatureGetCtxParams},
        {OSSL_FUNC_SIGNATURE_GETTABLE_CTX_PARAMS, (void (*)(void))SignatureGettableCtxParams},

        OSSL_DISPATCH_END};

    static const OSSL_ALGORITHM cSignAlgorithms[]
        = {{cAosEncryption, cAosSignerProvider, cSignFunctions, "AOS Signature"}, OSSL_ALGORITHM_END};

    static const OSSL_DISPATCH cKeyMgmFunctions[]
        = {{OSSL_FUNC_KEYMGMT_NEW, (void (*)(void))KeyMgmtNew}, {OSSL_FUNC_KEYMGMT_FREE, (void (*)(void))KeyMgmtFree},

            {OSSL_FUNC_KEYMGMT_HAS, (void (*)(void))KeyMgmtHas},
            {OSSL_FUNC_KEYMGMT_QUERY_OPERATION_NAME, (void (*)(void))KeyMgmtQuery},

            {OSSL_FUNC_KEYMGMT_IMPORT, (void (*)(void))KeyMgmtImport},
            {OSSL_FUNC_KEYMGMT_IMPORT_TYPES, (void (*)(void))KeyMgmtImportTypes},

            OSSL_DISPATCH_END};

    static const OSSL_ALGORITHM cKeyMgmAlgorithms[]
        = {{cAosEncryption, cAosSignerProvider, cKeyMgmFunctions, "AOS Key Management"}, OSSL_ALGORITHM_END};

    switch (operationID) {
    case OSSL_OP_SIGNATURE:
        return cSignAlgorithms;

    case OSSL_OP_KEYMGMT:
        return cKeyMgmAlgorithms;

    case OSSL_OP_STORE:
        return NULL;
    }

    return NULL;
}

int ProviderInit(const OSSL_CORE_HANDLE* handle, const OSSL_DISPATCH* in, const OSSL_DISPATCH** out, void** provctx)
{
    (void)handle;
    (void)in;

    if (!provctx) {
        LOG_ERR() << "Params are not initalized: err=" << OPENSSL_ERROR();

        return 0;
    }

    *provctx = OSSL_LIB_CTX_new();

    static const OSSL_DISPATCH provfns[] = {{OSSL_FUNC_PROVIDER_QUERY_OPERATION, (void (*)(void))ProviderQuery},
        {OSSL_FUNC_PROVIDER_TEARDOWN, (void (*)(void))OSSL_LIB_CTX_free}, OSSL_DISPATCH_END};

    *out = provfns;

    return 1;
}

} // namespace

/***********************************************************************************************************************
 * OpenSSLProvider implementation
 **********************************************************************************************************************/

Error OpenSSLProvider::Load(OSSL_LIB_CTX* libctx)
{
    mDefaultProvider = OSSL_PROVIDER_load(libctx, "default");
    if (!mDefaultProvider) {
        return OPENSSL_ERROR();
    }

    if (OSSL_PROVIDER_add_builtin(libctx, cAosSigner, &ProviderInit) != 1) {
        return OPENSSL_ERROR();
    }

    mAOSProvider = OSSL_PROVIDER_load(libctx, cAosSigner);
    if (!mAOSProvider) {
        return OPENSSL_ERROR();
    }

    if (EVP_set_default_properties(libctx, "?provider!=aossigner") != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error OpenSSLProvider::Unload()
{
    if (mAOSProvider) {
        if (OSSL_PROVIDER_unload(mAOSProvider) != 1) {
            return OPENSSL_ERROR();
        }
        mAOSProvider = nullptr;
    }

    if (mDefaultProvider) {
        if (OSSL_PROVIDER_unload(mDefaultProvider) != 1) {
            return OPENSSL_ERROR();
        }
        mDefaultProvider = nullptr;
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Public helper functions
 **********************************************************************************************************************/

RetWithError<StaticArray<uint8_t, cECDSAParamsOIDSize>> GetFullOID(const Array<uint8_t>& rawOID)
{
    StaticArray<uint8_t, cECDSAParamsOIDSize> fullOID;

    auto size = ASN1_object_size(0, rawOID.Size(), V_ASN1_OBJECT);
    if (size <= 0) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eFailed)};
    }

    auto err = fullOID.Resize(size);
    if (!err.IsNone()) {
        return {{}, AOS_ERROR_WRAP(err)};
    }

    auto p = fullOID.Get();
    ASN1_put_object(&p, 0, rawOID.Size(), V_ASN1_OBJECT, V_ASN1_UNIVERSAL);
    memcpy(p, rawOID.Get(), rawOID.Size());

    return {fullOID, ErrorEnum::eNone};
}

void AOS_OPENSSL_free(void* ptr)
{
    OPENSSL_free(ptr);
}

} // namespace aos::crypto::openssl
