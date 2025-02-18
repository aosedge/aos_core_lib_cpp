/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/ossl_typ.h>
#include <openssl/param_build.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "aos/common/crypto/openssl/cryptoprovider.hpp"
#include "aos/common/tools/logger.hpp"
#include "seqoid.hpp"

namespace aos::crypto {

/***********************************************************************************************************************
 * Statics
 **********************************************************************************************************************/

namespace {

constexpr auto cOSSLMaxNameSize = 50; // taken from openssl sources: include/internal/sizes.h
constexpr auto cRNGStrength     = 256;

Error AddDNSNames(const Array<StaticString<cDNSNameLen>>& dnsNames, STACK_OF(X509_EXTENSION) & extensions)
{
    if (dnsNames.IsEmpty()) {
        return ErrorEnum::eNone;
    }

    auto generalNames = DeferRelease(
        GENERAL_NAMES_new(), [](GENERAL_NAMES* names) { return sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free); });

    if (!generalNames) {
        return OPENSSL_ERROR();
    }

    for (const auto& dns : dnsNames) {
        auto generalName = a2i_GENERAL_NAME(NULL, NULL, NULL, GEN_DNS, dns.CStr(), 0);
        if (generalName == nullptr) {
            return OPENSSL_ERROR();
        }

        if (sk_GENERAL_NAME_push(generalNames.Get(), generalName) == 0) {
            GENERAL_NAME_free(generalName);

            return OPENSSL_ERROR();
        }
    }

    auto gnExt = X509V3_EXT_i2d(NID_subject_alt_name, 0, generalNames.Get());
    if (!gnExt) {
        return OPENSSL_ERROR();
    }

    if (sk_X509_EXTENSION_push(&extensions, gnExt) == 0) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error AddExtraExtensions(const Array<asn1::Extension>& extra, STACK_OF(X509_EXTENSION) & extensions)
{
    for (const auto& ext : extra) {
        auto nid = OBJ_txt2nid(ext.mID.CStr());
        if (nid == NID_undef) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidArgument, "bad OID"));
        }

        if (nid != NID_ext_key_usage) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotSupported, "not supported extension"));
        }

        // Decode the ASN.1 sequence into STACK_OF(ASN1_OBJECT)
        const unsigned char* p = ext.mValue.Get();
        auto eku = DeferRelease((SEQ_OID*)ASN1_item_d2i(nullptr, &p, ext.mValue.Size(), ASN1_ITEM_rptr(SEQ_OID)),
            [](SEQ_OID* oids) { return sk_ASN1_OBJECT_pop_free(oids, ASN1_OBJECT_free); });

        if (!eku) {
            return OPENSSL_ERROR();
        }

        // Create the X509 extension for EKU and push
        X509_EXTENSION* x509ext = X509V3_EXT_i2d(NID_ext_key_usage, 0, eku.Get());
        if (!x509ext) {
            return OPENSSL_ERROR();
        }

        if (sk_X509_EXTENSION_push(&extensions, x509ext) == 0) {
            X509_EXTENSION_free(x509ext);
            return OPENSSL_ERROR();
        }
    }

    return ErrorEnum::eNone;
}

Error ConvertX509NameToDer(const X509_NAME* src, Array<uint8_t>& dst)
{
    if (!src) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    int derSize = i2d_X509_NAME(src, nullptr);
    if (derSize <= 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    auto err = dst.Resize(derSize);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    auto dstBuf = dst.Get();
    if (i2d_X509_NAME(src, &dstBuf) <= 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    return ErrorEnum::eNone;
}

Error ConvertASN1IntToBN(const ASN1_INTEGER* src, Array<uint8_t>& dst)
{
    if (!src) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    auto bn = DeferRelease(ASN1_INTEGER_to_BN(src, nullptr), BN_free);
    if (!bn) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    int size = BN_num_bytes(bn.Get());

    auto err = dst.Resize(size);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (BN_bn2bin(bn.Get(), dst.Get()) <= 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    return ErrorEnum::eNone;
}

Error GetSubjectKeyID(X509* cert, Array<uint8_t>& skid)
{
    auto ext = X509_get_ext_d2i(cert, NID_subject_key_identifier, nullptr, nullptr);

    auto rawSkid = DeferRelease(static_cast<ASN1_OCTET_STRING*>(ext), ASN1_OCTET_STRING_free);
    if (!rawSkid) {
        LOG_DBG() << "Empty SKID";

        return ErrorEnum::eNone;
    }

    skid.Clear();

    auto data = ASN1_STRING_get0_data(rawSkid.Get());
    auto len  = ASN1_STRING_length(rawSkid.Get());

    if (auto err = skid.Insert(skid.begin(), data, data + len); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error GetAuthorityKeyID(X509* cert, Array<uint8_t>& akid)
{
    auto ext = X509_get_ext_d2i(cert, NID_authority_key_identifier, nullptr, nullptr);

    auto rawAkid = DeferRelease(static_cast<AUTHORITY_KEYID*>(ext), AUTHORITY_KEYID_free);
    if (!rawAkid) {
        LOG_DBG() << "Empty AKID";

        return ErrorEnum::eNone;
    }

    if (!rawAkid.Get() || !rawAkid.Get()->keyid) {
        return OPENSSL_ERROR();
    }

    akid.Clear();

    auto data = ASN1_STRING_get0_data(rawAkid.Get()->keyid);
    auto len  = ASN1_STRING_length(rawAkid.Get()->keyid);

    if (auto err = akid.Insert(akid.begin(), data, data + len); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ConvertASN1Time(const ASN1_TIME* src, Time& dst)
{
    if (!src) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    tm tmp = {};

    if (ASN1_TIME_to_tm(src, &tmp) != 1) {
        return OPENSSL_ERROR();
    }

    auto seconds = timegm(&tmp);
    if (seconds < 0) {
        return AOS_ERROR_WRAP(errno);
    }

    dst = Time::Unix(seconds, 0);

    return ErrorEnum::eNone;
}

Error SetRSAPubKey(const EVP_PKEY* src, Variant<ECDSAPublicKey, RSAPublicKey>& dst)
{
    BIGNUM *n = nullptr, *e = nullptr;

    if (EVP_PKEY_get_bn_param(src, OSSL_PKEY_PARAM_RSA_N, &n) <= 0) {
        return OPENSSL_ERROR();
    }

    if (EVP_PKEY_get_bn_param(src, OSSL_PKEY_PARAM_RSA_E, &e) <= 0) {
        return OPENSSL_ERROR();
    }

    auto modSize = BN_num_bytes(n);
    auto expSize = BN_num_bytes(e);

    auto modulus = DeferRelease(static_cast<uint8_t*>(OPENSSL_zalloc(modSize)), openssl::AOS_OPENSSL_free);
    if (BN_bn2bin(n, modulus.Get()) < 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    auto exponent = DeferRelease(static_cast<uint8_t*>(OPENSSL_zalloc(expSize)), openssl::AOS_OPENSSL_free);
    if (BN_bn2bin(e, exponent.Get()) < 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    dst.SetValue<RSAPublicKey>(Array<uint8_t>(modulus.Get(), modSize), Array<uint8_t>(exponent.Get(), expSize));

    return ErrorEnum::eNone;
}

Error SetECDSAPubKey(const EVP_PKEY* src, Variant<ECDSAPublicKey, RSAPublicKey>& dst)
{
    // get ecp point
    size_t                                   ecPointSize = 0;
    StaticArray<uint8_t, cECDSAPointDERSize> ecPoint;

    ecPoint.Resize(ecPoint.MaxSize());

    if (EVP_PKEY_get_octet_string_param(src, OSSL_PKEY_PARAM_PUB_KEY, ecPoint.Get(), ecPoint.Size(), &ecPointSize)
        != 1) {
        return OPENSSL_ERROR();
    }

    ecPoint.Resize(ecPointSize);

    // get curve name
    char curveName[cOSSLMaxNameSize] = {};

    if (EVP_PKEY_get_utf8_string_param(src, OSSL_PKEY_PARAM_GROUP_NAME, curveName, cOSSLMaxNameSize, nullptr) <= 0) {
        return OPENSSL_ERROR();
    }

    auto obj = DeferRelease(OBJ_txt2obj(curveName, 0), ASN1_OBJECT_free);
    if (!obj) {
        return OPENSSL_ERROR();
    }

    auto objData    = OBJ_get0_data(obj.Get());
    auto objDataLen = OBJ_length(obj.Get());

    StaticArray<uint8_t, cECDSAParamsOIDSize> groupOID;

    auto err = groupOID.Insert(groupOID.begin(), objData, objData + objDataLen);
    if (!err.IsNone()) {
        return err;
    }

    dst.SetValue<ECDSAPublicKey>(groupOID, ecPoint);

    return ErrorEnum::eNone;
}

Error ConvertEvpPKey(const EVP_PKEY* src, Variant<ECDSAPublicKey, RSAPublicKey>& dst)
{
    switch (EVP_PKEY_base_id(src)) {
    case EVP_PKEY_RSA:
        return SetRSAPubKey(src, dst);

    case EVP_PKEY_EC:
        return SetECDSAPubKey(src, dst);
    }

    return ErrorEnum::eNotSupported;
}

Error ConvertX509ToDER(const X509* cert, Array<uint8_t>& derBlob)
{
    if (!cert) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    int derLen = i2d_X509(cert, nullptr);
    if (derLen <= 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    if (auto err = derBlob.Resize(derLen); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    unsigned char* derBuff = derBlob.Get();

    derLen = i2d_X509(cert, &derBuff);
    if (derLen <= 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    if (auto err = derBlob.Resize(derLen); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ConvertX509ToAos(X509* cert, x509::Certificate& resultCert)
{
    auto err = ConvertX509NameToDer(X509_get_subject_name(cert), resultCert.mSubject);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = ConvertX509NameToDer(X509_get_issuer_name(cert), resultCert.mIssuer);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = ConvertASN1IntToBN(X509_get_serialNumber(cert), resultCert.mSerial);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = ConvertEvpPKey(X509_get_pubkey(cert), resultCert.mPublicKey);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = GetSubjectKeyID(cert, resultCert.mSubjectKeyId);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    err = GetAuthorityKeyID(cert, resultCert.mAuthorityKeyId);
    if (!err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    err = ConvertASN1Time(X509_get_notBefore(cert), resultCert.mNotBefore);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = ConvertASN1Time(X509_get_notAfter(cert), resultCert.mNotAfter);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = ConvertX509ToDER(cert, resultCert.mRaw);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ConvertX509ToPEM(X509* cer, OSSL_LIB_CTX* libCtx, String& pem)
{
    auto bio = DeferRelease(BIO_new_ex(libCtx, BIO_s_mem()), BIO_free);
    if (!bio) {
        return OPENSSL_ERROR();
    }

    if (PEM_write_bio_X509(bio.Get(), cer) != 1) {
        return OPENSSL_ERROR();
    }

    BUF_MEM* mem = nullptr;
    BIO_get_mem_ptr(bio.Get(), &mem);

    if (mem == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    pem.Clear();

    auto err = pem.Insert(pem.begin(), mem->data, mem->data + mem->length);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<EVP_PKEY*> GetEvpPublicKey(const RSAPublicKey& pubKey, OSSL_LIB_CTX* libCtx)
{
    auto ctx = DeferRelease(EVP_PKEY_CTX_new_from_name(libCtx, "RSA", nullptr), EVP_PKEY_CTX_free);
    if (!ctx) {
        return {nullptr, OPENSSL_ERROR()};
    }

    EVP_PKEY* pkey = nullptr;

    if (EVP_PKEY_fromdata_init(ctx.Get()) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    auto bld = DeferRelease(OSSL_PARAM_BLD_new(), OSSL_PARAM_BLD_free);
    if (!bld) {
        return {nullptr, OPENSSL_ERROR()};
    }

    auto n = DeferRelease(BN_bin2bn(pubKey.GetN().Get(), pubKey.GetN().Size(), nullptr), BN_free);
    if (!n) {
        return {nullptr, OPENSSL_ERROR()};
    }

    auto e = DeferRelease(BN_bin2bn(pubKey.GetE().Get(), pubKey.GetE().Size(), nullptr), BN_free);
    if (!e) {
        return {nullptr, OPENSSL_ERROR()};
    }

    if (OSSL_PARAM_BLD_push_BN(bld.Get(), OSSL_PKEY_PARAM_RSA_N, n.Get()) != 1
        || OSSL_PARAM_BLD_push_BN(bld.Get(), OSSL_PKEY_PARAM_RSA_E, e.Get()) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    auto params = DeferRelease(OSSL_PARAM_BLD_to_param(bld.Get()), openssl::AOS_OPENSSL_free);
    if (!params) {
        return {nullptr, OPENSSL_ERROR()};
    }

    if (EVP_PKEY_fromdata(ctx.Get(), &pkey, EVP_PKEY_PUBLIC_KEY, params.Get()) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    return {pkey, ErrorEnum::eNone};
}

// Takes OID with stripped tag & length
RetWithError<const char*> GetCurveName(const Array<uint8_t>& rawOID)
{
    auto [fullOID, err] = openssl::GetFullOID(rawOID);
    if (!err.IsNone()) {
        return {nullptr, AOS_ERROR_WRAP(err)};
    }

    const uint8_t* oidPtr = fullOID.Get();

    auto asn1oid = DeferRelease(d2i_ASN1_OBJECT(NULL, &oidPtr, fullOID.Size()), ASN1_OBJECT_free);
    if (!asn1oid) {
        return {"", OPENSSL_ERROR()};
    }

    int nid = OBJ_obj2nid(asn1oid.Get());
    if (nid == NID_undef) {
        return {"", OPENSSL_ERROR()};
    }

    const char* curveName = OBJ_nid2sn(nid);
    if (!curveName) {
        return {"", OPENSSL_ERROR()};
    }

    return {curveName, ErrorEnum::eNone};
}

RetWithError<EVP_PKEY*> GetEvpPublicKey(const ECDSAPublicKey& pubKey, OSSL_LIB_CTX* libCtx)
{
    auto [curveName, err] = GetCurveName(pubKey.GetECParamsOID());
    if (!err.IsNone()) {
        return {nullptr, err};
    }

    auto octetStr = DeferRelease(ASN1_OCTET_STRING_new(), ASN1_OCTET_STRING_free);
    if (ASN1_OCTET_STRING_set(octetStr.Get(), pubKey.GetECPoint().Get(), pubKey.GetECPoint().Size()) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    auto octetStrBuffer = ASN1_STRING_get0_data(octetStr.Get());
    auto octetStrLen    = ASN1_STRING_length(octetStr.Get());

    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, const_cast<char*>(curveName), 0),
        OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY, const_cast<uint8_t*>(octetStrBuffer), octetStrLen),
        OSSL_PARAM_construct_end()};

    auto ctx = DeferRelease(EVP_PKEY_CTX_new_from_name(libCtx, "EC", nullptr), EVP_PKEY_CTX_free);
    if (!ctx) {
        return {nullptr, OPENSSL_ERROR()};
    }

    EVP_PKEY* pkey = nullptr;

    if (EVP_PKEY_fromdata_init(ctx.Get()) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    if (EVP_PKEY_fromdata(ctx.Get(), &pkey, EVP_PKEY_PUBLIC_KEY, params) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    return {pkey, ErrorEnum::eNone};
}

Error SetPublicKey(const PublicKeyItf& pubKey, X509_REQ* csr, OSSL_LIB_CTX* libCtx)
{
    Error     err;
    EVP_PKEY* pkey = nullptr;

    switch (pubKey.GetKeyType().GetValue()) {
    case KeyTypeEnum::eRSA:
        Tie(pkey, err) = GetEvpPublicKey(static_cast<const RSAPublicKey&>(pubKey), libCtx);
        break;

    case KeyTypeEnum::eECDSA:
        Tie(pkey, err) = GetEvpPublicKey(static_cast<const ECDSAPublicKey&>(pubKey), libCtx);
        break;

    default:
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto freeEvpPKey = DeferRelease(pkey, EVP_PKEY_free);

    if (X509_REQ_set_pubkey(csr, pkey) != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error SetPublicKey(const PublicKeyItf& pubKey, X509* cer, OSSL_LIB_CTX* libCtx)
{
    Error     err;
    EVP_PKEY* pkey = nullptr;

    switch (pubKey.GetKeyType().GetValue()) {
    case KeyTypeEnum::eRSA:
        Tie(pkey, err) = GetEvpPublicKey(static_cast<const RSAPublicKey&>(pubKey), libCtx);
        break;

    case KeyTypeEnum::eECDSA:
        Tie(pkey, err) = GetEvpPublicKey(static_cast<const ECDSAPublicKey&>(pubKey), libCtx);
        break;

    default:
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto freeEvpPKey = DeferRelease(pkey, EVP_PKEY_free);

    if (X509_set_pubkey(cer, pkey) != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

UniquePtr<X509_NAME, void (&)(X509_NAME* a)> ConvertDER2X509Name(const Array<uint8_t>& der)
{
    const uint8_t* buf = der.Get();

    return DeferRelease(d2i_X509_NAME(nullptr, &buf, der.Size()), X509_NAME_free);
}

Error SetTime(const Time& src, ASN1_TIME* dst)
{
    auto [timeStr, err] = asn1::ConvertTimeToASN1Str(src);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (ASN1_TIME_set_string_X509(dst, timeStr.CStr()) != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error SetIssuer(const Array<uint8_t>& derIssuer, X509* cert)
{
    auto issuer = ConvertDER2X509Name(derIssuer);
    if (!issuer) {
        return OPENSSL_ERROR();
    }

    if (X509_set_issuer_name(cert, issuer.Get()) != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error SetSubject(const Array<uint8_t>& derSubject, X509* cert)
{
    auto subject = ConvertDER2X509Name(derSubject);
    if (!subject) {
        return OPENSSL_ERROR();
    }

    if (X509_set_subject_name(cert, subject.Get()) != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error SetSerial(const Array<uint8_t>& serial, X509* cert)
{
    ASN1_INTEGER* asn1Serial = nullptr;

    if (serial.IsEmpty()) {
        auto bnSerial = DeferRelease(BN_new(), BN_free);
        if (!bnSerial) {
            return OPENSSL_ERROR();
        }

        if (!BN_rand(bnSerial.Get(), 64, 0, 0)) {
            return OPENSSL_ERROR();
        }

        asn1Serial = BN_to_ASN1_INTEGER(bnSerial.Get(), nullptr);
        if (!asn1Serial) {
            return OPENSSL_ERROR();
        }
    } else {
        auto buf = serial.Get();

        auto bnSerialRaw = DeferRelease(BN_bin2bn(buf, serial.Size(), nullptr), BN_free);
        if (!bnSerialRaw) {
            return OPENSSL_ERROR();
        }

        asn1Serial = BN_to_ASN1_INTEGER(bnSerialRaw.Get(), nullptr);
        if (!asn1Serial) {
            return OPENSSL_ERROR();
        }
    }

    auto freeSerial = DeferRelease(asn1Serial, ASN1_INTEGER_free);

    if (X509_set_serialNumber(cert, asn1Serial) != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error SetNotBefore(const Time& notBefore, X509* cert)
{
    auto notBeforeASN1 = DeferRelease(ASN1_UTCTIME_new(), ASN1_TIME_free);

    auto err = SetTime(notBefore, notBeforeASN1.Get());
    if (!err.IsNone()) {
        return err;
    }

    if (X509_set_notBefore(cert, notBeforeASN1.Get()) != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error SetNotAfter(const Time& notAfter, X509* cert)
{
    auto notAfterASN1 = DeferRelease(ASN1_UTCTIME_new(), ASN1_TIME_free);

    auto err = SetTime(notAfter, notAfterASN1.Get());
    if (!err.IsNone()) {
        return err;
    }

    if (X509_set_notAfter(cert, notAfterASN1.Get()) != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error SetSKID(const Array<uint8_t>& derSKID, X509* cert)
{
    auto skid = DeferRelease<ASN1_OCTET_STRING>(nullptr, ASN1_OCTET_STRING_free);

    if (!derSKID.IsEmpty()) {
        const uint8_t* skidBuff = derSKID.Get();

        skid.Reset(d2i_ASN1_OCTET_STRING(nullptr, &skidBuff, derSKID.Size()));
        if (!skid) {
            return OPENSSL_ERROR();
        }
    } else {
        uint8_t      md[EVP_MAX_MD_SIZE];
        unsigned int mdLen = 0;

        if (X509_pubkey_digest(cert, EVP_sha1(), md, &mdLen) != 1) {
            return OPENSSL_ERROR();
        }

        skid.Reset(ASN1_OCTET_STRING_new());
        if (!skid) {
            return OPENSSL_ERROR();
        }

        if (ASN1_OCTET_STRING_set(skid.Get(), md, mdLen) != 1) {
            return OPENSSL_ERROR();
        }
    }

    auto skidExt = DeferRelease(X509V3_EXT_i2d(NID_subject_key_identifier, 0, skid.Get()), X509_EXTENSION_free);
    if (!skidExt) {
        return OPENSSL_ERROR();
    }

    if (X509_add_ext(cert, skidExt.Get(), -1) != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error SetAKID(const Array<uint8_t>& derAKID, X509* cert, const x509::Certificate& parent)
{
    auto akid = DeferRelease<ASN1_OCTET_STRING>(nullptr, ASN1_OCTET_STRING_free);

    if (!derAKID.IsEmpty()) {
        const uint8_t* akidBuff = derAKID.Get();

        akid.Reset(d2i_ASN1_OCTET_STRING(nullptr, &akidBuff, derAKID.Size()));
        if (!akid) {
            return OPENSSL_ERROR();
        }
    } else {
        auto parentCert = DeferRelease<X509>(nullptr, X509_free);

        if (!parent.mRaw.IsEmpty()) {
            auto derBuf = parent.mRaw.Get();

            parentCert.Reset(d2i_X509(nullptr, &derBuf, parent.mRaw.Size()));
            if (!parentCert) {
                return OPENSSL_ERROR();
            }
        }

        uint8_t      md[EVP_MAX_MD_SIZE];
        unsigned int mdLen = 0;

        auto issuerCert = parentCert ? parentCert.Get() : cert;
        if (X509_pubkey_digest(issuerCert, EVP_sha1(), md, &mdLen) != 1) {
            return OPENSSL_ERROR();
        }

        akid.Reset(ASN1_OCTET_STRING_new());
        if (!akid) {
            return OPENSSL_ERROR();
        }

        if (ASN1_OCTET_STRING_set(akid.Get(), md, mdLen) != 1) {
            return OPENSSL_ERROR();
        }
    }

    auto ans1akid   = DeferRelease(AUTHORITY_KEYID_new(), AUTHORITY_KEYID_free);
    ans1akid->keyid = akid.Release(); // release akid as AUTHORITY_KEYID takes its ownership

    auto akidExt = DeferRelease(X509V3_EXT_i2d(NID_authority_key_identifier, 0, ans1akid.Get()), X509_EXTENSION_free);
    if (!akidExt) {
        return OPENSSL_ERROR();
    }

    if (X509_add_ext(cert, akidExt.Get(), -1) != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

RetWithError<EVP_MD_CTX*> CreateSignCtx(const PrivateKeyItf& privKey, OSSL_LIB_CTX* libCtx)
{
    const char* cAosProps = openssl::cAosSignerProvider;

    // Create EVP_PKEY
    auto pKeyCtx
        = DeferRelease(EVP_PKEY_CTX_new_from_name(libCtx, openssl::cAosAlgorithm, cAosProps), EVP_PKEY_CTX_free);
    if (!pKeyCtx) {
        return {nullptr, OPENSSL_ERROR()};
    }

    EVP_PKEY* evpKey = NULL;

    if (EVP_PKEY_fromdata_init(pKeyCtx.Get()) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    PrivateKeyItf* privKeyPtr      = const_cast<PrivateKeyItf*>(&privKey);
    OSSL_PARAM     privKeyParams[] = {
        OSSL_PARAM_octet_string(openssl::cPKeyParamAosKeyPair, reinterpret_cast<void*>(privKeyPtr), sizeof(privKeyPtr)),
        OSSL_PARAM_END};

    if (EVP_PKEY_fromdata(pKeyCtx.Get(), &evpKey, EVP_PKEY_KEYPAIR, privKeyParams) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    auto freePKey = DeferRelease(evpKey, EVP_PKEY_free);

    // Sign
    auto mdCtx = DeferRelease(EVP_MD_CTX_new(), EVP_MD_CTX_free);

    // Use default MD algorithm based on the specified private key.
    if (EVP_DigestSignInit_ex(mdCtx.Get(), nullptr, nullptr, libCtx, cAosProps, evpKey, nullptr) != 1) {
        return {nullptr, OPENSSL_ERROR()};
    }

    return {mdCtx.Release(), ErrorEnum::eNone};
}

Error Sign(const PrivateKeyItf& privKey, X509* cer, OSSL_LIB_CTX* libCtx)
{
    auto [signCtx, err] = CreateSignCtx(privKey, libCtx);
    if (!err.IsNone()) {
        return err;
    }

    auto freeCtx = DeferRelease(signCtx, EVP_MD_CTX_free);

    if (X509_sign_ctx(cer, signCtx) <= 0) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error CreateClientCert(X509_REQ* csr, EVP_PKEY* caKey, X509* caCert, OSSL_LIB_CTX* libCtx, const Array<uint8_t>& serial,
    String& clientCertPEM)
{
    auto clientCert = DeferRelease(X509_new_ex(libCtx, nullptr), X509_free);
    if (!clientCert) {
        return OPENSSL_ERROR();
    }

    // Set subject params
    if (X509_set_subject_name(clientCert.Get(), X509_REQ_get_subject_name(csr)) != 1) {
        return OPENSSL_ERROR();
    }

    auto csrPublicKey = DeferRelease(X509_REQ_get_pubkey(csr), EVP_PKEY_free);
    if (!csrPublicKey) {
        return OPENSSL_ERROR();
    }

    if (X509_set_pubkey(clientCert.Get(), csrPublicKey.Get()) != 1) {
        return OPENSSL_ERROR();
    }

    // Set serial
    if (auto err = SetSerial(serial, clientCert.Get()); !err.IsNone()) {
        return err;
    }

    // Set validity period to 1 year
    auto notBefore = DeferRelease(ASN1_TIME_set(nullptr, time(nullptr)), ASN1_TIME_free);
    auto notAfter  = DeferRelease(ASN1_TIME_adj(nullptr, time(nullptr), 365, 0), ASN1_TIME_free);

    if (X509_set_notBefore(clientCert.Get(), notBefore.Get()) != 1
        || X509_set_notAfter(clientCert.Get(), notAfter.Get()) != 1) {
        return OPENSSL_ERROR();
    }

    // Set CA params
    if (X509_set_issuer_name(clientCert.Get(), X509_get_subject_name(caCert)) != 1) {
        return OPENSSL_ERROR();
    }

    if (X509_sign(clientCert.Get(), caKey, EVP_sha256()) == 0) {
        return OPENSSL_ERROR();
    }

    return ConvertX509ToPEM(clientCert.Get(), libCtx, clientCertPEM);
}

Error ConvertToPEM(X509_REQ* csr, String& pem)
{
    auto bio = DeferRelease(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio) {
        return OPENSSL_ERROR();
    }

    if (PEM_write_bio_X509_REQ(bio.Get(), csr) != 1) {
        return OPENSSL_ERROR();
    }

    BUF_MEM* mem = nullptr;
    BIO_get_mem_ptr(bio.Get(), &mem);

    if (mem == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    pem.Clear();

    auto err = pem.Insert(pem.begin(), mem->data, mem->data + mem->length);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error AddTemplParams(const x509::CSR& templ, X509_REQ* csr)
{
    auto subject = ConvertDER2X509Name(templ.mSubject);
    if (!subject) {
        return OPENSSL_ERROR();
    }

    if (X509_REQ_set_subject_name(csr, subject.Get()) != 1) {
        return OPENSSL_ERROR();
    }

    auto extensions = DeferRelease(sk_X509_EXTENSION_new_null(),
        [](X509_EXTENSIONS* extensions) { return sk_X509_EXTENSION_pop_free(extensions, X509_EXTENSION_free); });
    if (!extensions) {
        return OPENSSL_ERROR();
    }

    if (auto err = AddDNSNames(templ.mDNSNames, *extensions); !err.IsNone()) {
        return err;
    }

    if (auto err = AddExtraExtensions(templ.mExtraExtensions, *extensions); !err.IsNone()) {
        return err;
    }

    if (X509_REQ_add_extensions(csr, extensions.Get()) != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error Sign(const PrivateKeyItf& privKey, X509_REQ* req, OSSL_LIB_CTX* libCtx)
{
    auto [signCtx, err] = CreateSignCtx(privKey, libCtx);
    if (!err.IsNone()) {
        return err;
    }

    auto freeCtx = DeferRelease(signCtx, EVP_MD_CTX_free);

    if (X509_REQ_sign_ctx(req, signCtx) <= 0) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

OpenSSLCryptoProvider::~OpenSSLCryptoProvider()
{
    mOpenSSLProvider.Unload();
    OSSL_LIB_CTX_free(mLibCtx);
}

Error OpenSSLCryptoProvider::Init()
{
    LOG_DBG() << "Init OpenSSL crypto provider";

    if (mLibCtx = OSSL_LIB_CTX_new(); !mLibCtx) {
        return OPENSSL_ERROR();
    }

    if (auto err = mOpenSSLProvider.Load(mLibCtx); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error OpenSSLCryptoProvider::CreateCertificate(
    const x509::Certificate& templ, const x509::Certificate& parent, const PrivateKeyItf& privKey, String& pemCert)
{
    LOG_DBG() << "Create certificate";

    auto cert = DeferRelease(X509_new_ex(mLibCtx, nullptr), X509_free);
    if (!cert) {
        return OPENSSL_ERROR();
    }

    if (auto err = SetPublicKey(privKey.GetPublic(), cert.Get(), mLibCtx); !err.IsNone()) {
        return err;
    }

    auto& issuer = !parent.mSubject.IsEmpty() ? parent.mSubject : templ.mIssuer;
    if (auto err = SetIssuer(issuer, cert.Get()); !err.IsNone()) {
        return err;
    }

    if (auto err = SetSubject(templ.mSubject, cert.Get()); !err.IsNone()) {
        return err;
    }

    if (auto err = SetSerial(templ.mSerial, cert.Get()); !err.IsNone()) {
        return err;
    }

    if (auto err = SetNotBefore(templ.mNotBefore, cert.Get()); !err.IsNone()) {
        return err;
    }

    if (auto err = SetNotAfter(templ.mNotAfter, cert.Get()); !err.IsNone()) {
        return err;
    }

    if (auto err = SetSKID(templ.mSubjectKeyId, cert.Get()); !err.IsNone()) {
        return err;
    }

    const auto& akid = !parent.mSubjectKeyId.IsEmpty() ? parent.mSubjectKeyId : templ.mAuthorityKeyId;
    if (auto err = SetAKID(akid, cert.Get(), parent); !err.IsNone()) {
        return err;
    }

    if (auto err = Sign(privKey, cert.Get(), mLibCtx); !err.IsNone()) {
        return err;
    }

    if (auto err = ConvertX509ToPEM(cert.Get(), mLibCtx, pemCert); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error OpenSSLCryptoProvider::CreateClientCert(const String& csrPEM, const String& caKeyPEM, const String& caCertPEM,
    const Array<uint8_t>& serial, String& clientCert)
{
    LOG_DBG() << "Create client certificate";

    // Parse CSR
    auto bioCsr = DeferRelease(BIO_new_mem_buf(csrPEM.CStr(), csrPEM.Size()), BIO_free);
    if (!bioCsr) {
        return ErrorEnum::eFailed;
    }

    auto csr = DeferRelease(PEM_read_bio_X509_REQ(bioCsr.Get(), nullptr, nullptr, nullptr), X509_REQ_free);
    if (!csr) {
        return ErrorEnum::eFailed;
    }

    // Parse CA Private Key
    auto bioCaKey = DeferRelease(BIO_new_mem_buf(caKeyPEM.CStr(), caKeyPEM.Size()), BIO_free);
    if (!bioCaKey) {
        return ErrorEnum::eFailed;
    }

    auto caKey = DeferRelease(
        PEM_read_bio_PrivateKey_ex(bioCaKey.Get(), nullptr, nullptr, nullptr, mLibCtx, nullptr), EVP_PKEY_free);
    if (!caKey) {
        return ErrorEnum::eFailed;
    }

    // Parse CA Certificate
    auto bioCaCert = DeferRelease(BIO_new_mem_buf(caCertPEM.CStr(), caCertPEM.Size()), BIO_free);
    if (!bioCaCert) {
        return ErrorEnum::eFailed;
    }

    auto caCert = DeferRelease(PEM_read_bio_X509(bioCaCert.Get(), nullptr, nullptr, nullptr), X509_free);
    if (!caCert) {
        return ErrorEnum::eFailed;
    }

    return aos::crypto::CreateClientCert(csr.Get(), caKey.Get(), caCert.Get(), mLibCtx, serial, clientCert);
}

Error OpenSSLCryptoProvider::PEMToX509Certs(const String& pemBlob, Array<x509::Certificate>& resultCerts)
{
    LOG_DBG() << "Convert certs from PEM to x509";

    auto bio = DeferRelease(BIO_new_mem_buf(pemBlob.CStr(), pemBlob.Size()), BIO_free);
    if (!bio) {
        return ErrorEnum::eFailed;
    }

    X509* cert = nullptr;
    while ((cert = PEM_read_bio_X509(bio.Get(), nullptr, nullptr, nullptr)) != nullptr) {
        auto freeCert = DeferRelease(cert, X509_free);

        auto err = resultCerts.EmplaceBack();
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto& resultCert = resultCerts.Back();

        err = ConvertX509ToAos(cert, resultCert);
        if (!err.IsNone()) {
            return err;
        }
    }

    const char* cPEMHeader = "-----BEGIN CERTIFICATE-----";

    size_t certCount = 0;
    int    i         = 0;

    for (;;) {
        auto [certStart, err] = pemBlob.FindSubstr(i, cPEMHeader);
        if (!err.IsNone()) {
            break;
        }

        certCount++;
        i = certStart + 1;
    }

    if (certCount != resultCerts.Size()) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    return ErrorEnum::eNone;
}

Error OpenSSLCryptoProvider::X509CertToPEM(const x509::Certificate& certificate, String& pemCert)
{
    LOG_DBG() << "Convert certs from x509 to PEM";

    auto derBuf = certificate.mRaw.Get();

    auto cert = DeferRelease(d2i_X509(nullptr, &derBuf, certificate.mRaw.Size()), X509_free);
    if (!cert) {
        return OPENSSL_ERROR();
    }

    if (auto err = ConvertX509ToPEM(cert.Get(), mLibCtx, pemCert); !err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error OpenSSLCryptoProvider::DERToX509Cert(const Array<uint8_t>& derBlob, x509::Certificate& resultCert)
{
    LOG_DBG() << "Convert certs from DER to x509";

    auto derBuf = derBlob.Get();

    auto cert = DeferRelease(d2i_X509(nullptr, &derBuf, derBlob.Size()), X509_free);
    if (!cert) {
        return OPENSSL_ERROR();
    }

    return ConvertX509ToAos(cert.Get(), resultCert);
}

Error OpenSSLCryptoProvider::CreateCSR(const x509::CSR& templ, const PrivateKeyItf& privKey, String& pemCSR)
{
    LOG_DBG() << "Create CSR";

    auto csr = DeferRelease(X509_REQ_new_ex(mLibCtx, nullptr), X509_REQ_free);
    if (!csr) {
        return OPENSSL_ERROR();
    }

    auto err = AddTemplParams(templ, csr.Get());
    if (!err.IsNone()) {
        return err;
    }

    err = SetPublicKey(privKey.GetPublic(), csr.Get(), mLibCtx);
    if (!err.IsNone()) {
        return err;
    }

    err = Sign(privKey, csr.Get(), mLibCtx);
    if (!err.IsNone()) {
        return err;
    }

    return ConvertToPEM(csr.Get(), pemCSR);
}

RetWithError<SharedPtr<PrivateKeyItf>> OpenSSLCryptoProvider::PEMToX509PrivKey(const String& pemBlob)
{
    (void)pemBlob;

    LOG_ERR() << "Create private key from PEM not supported";

    return {{}, ErrorEnum::eNotSupported};
}

Error OpenSSLCryptoProvider::ASN1EncodeDN(const String& commonName, Array<uint8_t>& result)
{
    auto name = DeferRelease(X509_NAME_new(), X509_NAME_free);
    if (!name) {
        return ErrorEnum::eNone;
    }

    static constexpr auto cDelims = ",/";

    // Split input string (e.g., "CN=Aos Core/C=UA")
    for (size_t i = 0, j = 0; i < commonName.Size(); i = j + 1) {
        Error err;

        // Find next cn entry
        Tie(j, err) = commonName.FindAny(i, cDelims);
        if (!err.IsNone()) {
            j = commonName.Size();
        }

        StaticString<cOSSLMaxNameSize> entry;

        err = entry.Insert(entry.begin(), commonName.Get() + i, commonName.Get() + j);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(ErrorEnum::eFailed);
        }

        if (entry.Trim(" ").IsEmpty()) {
            continue;
        }

        // Split cn kev/value
        int pos       = 0;
        Tie(pos, err) = entry.FindAny(0, "=");
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
        }

        StaticString<cOSSLMaxNameSize> key, value;

        err = key.Insert(key.begin(), entry.begin(), entry.begin() + pos);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(ErrorEnum::eFailed);
        }

        err = value.Insert(value.begin(), entry.begin() + pos + 1, entry.end());
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(ErrorEnum::eFailed);
        }

        key.Trim(" ");
        value.Trim(" ");

        // Add entry to X509_NAME
        auto nid = OBJ_txt2nid(key.CStr());
        if (nid == NID_undef) {
            return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
        }

        auto res = X509_NAME_add_entry_by_NID(name.Get(), nid, MBSTRING_UTF8, (unsigned char*)value.CStr(), -1, -1, 0);
        if (res != 1) {
            return OPENSSL_ERROR();
        }
    }

    const uint8_t* der    = nullptr;
    size_t         derLen = 0;

    if (X509_NAME_get0_der(name.Get(), &der, &derLen) != 1) {
        return OPENSSL_ERROR();
    }

    result.Clear();

    auto err = result.Insert(result.begin(), der, der + derLen);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error OpenSSLCryptoProvider::ASN1DecodeDN(const Array<uint8_t>& dn, String& result)
{
    const uint8_t* dnBuff = dn.Get();

    auto name = DeferRelease(d2i_X509_NAME(nullptr, &dnBuff, dn.Size()), X509_NAME_free);
    if (!name) {
        return ErrorEnum::eNone;
    }

    auto buf = DeferRelease(X509_NAME_oneline(name.Get(), 0, 0), openssl::AOS_OPENSSL_free);

    result.Clear();

    auto err = result.Insert(result.begin(), buf.Get(), buf.Get() + strlen(buf.Get()));
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // To be compatible with MBedTLS implementation replace separators with ", "
    // and remove leading "/"
    err = result.LeftTrim("/").Replace("/", ", ");
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error OpenSSLCryptoProvider::ASN1EncodeObjectIds(const Array<asn1::ObjectIdentifier>& src, Array<uint8_t>& asn1Value)
{
    auto oids
        = DeferRelease(SEQ_OID_new(), [](SEQ_OID* oids) { return sk_ASN1_OBJECT_pop_free(oids, ASN1_OBJECT_free); });

    for (const auto& oid : src) {
        auto obj = OBJ_txt2obj(oid.CStr(), 0);

        if (sk_ASN1_OBJECT_push(oids.Get(), obj) == 0) {
            return AOS_ERROR_WRAP(ErrorEnum::eFailed);
        }
    }

    uint8_t* buf = nullptr;
    auto     len = i2d_SEQ_OID(oids.Get(), &buf);

    if (len <= 0 || buf == nullptr) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    asn1Value.Clear();

    auto err = asn1Value.Insert(asn1Value.begin(), buf, buf + len);
    OPENSSL_free(buf);

    if (!err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error OpenSSLCryptoProvider::ASN1EncodeBigInt(const Array<uint8_t>& number, Array<uint8_t>& asn1Value)
{
    auto bn = DeferRelease(BN_signed_bin2bn(number.Get(), number.Size(), nullptr), BN_free);
    if (!bn) {
        return OPENSSL_ERROR();
    }

    auto asn1Int = DeferRelease(BN_to_ASN1_INTEGER(bn.Get(), nullptr), ASN1_INTEGER_free);
    if (!asn1Int) {
        return OPENSSL_ERROR();
    }

    uint8_t* buf = nullptr;
    auto     len = i2d_ASN1_INTEGER(asn1Int.Get(), &buf);
    if (len <= 0) {
        return OPENSSL_ERROR();
    }

    auto releaseBuf = DeferRelease(buf, openssl::AOS_OPENSSL_free);

    auto err = asn1Value.Insert(asn1Value.begin(), buf, buf + len);
    if (!err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error OpenSSLCryptoProvider::ASN1EncodeDERSequence(const Array<Array<uint8_t>>& items, Array<uint8_t>& asn1Value)
{
    auto sequence = DeferRelease(ASN1_SEQUENCE_ANY_new(),
        [](ASN1_SEQUENCE_ANY* sequence) { return sk_ASN1_TYPE_pop_free(sequence, ASN1_TYPE_free); });

    for (const auto& item : items) {
        const uint8_t* buff = item.Get();

        auto type = d2i_ASN1_TYPE(nullptr, &buff, item.Size());
        if (!type) {
            return OPENSSL_ERROR();
        }

        if (sk_ASN1_TYPE_push(sequence.Get(), type) == 0) {
            return AOS_ERROR_WRAP(ErrorEnum::eFailed);
        }
    }

    uint8_t* buf = nullptr;

    auto len = i2d_ASN1_SEQUENCE_ANY(sequence.Get(), &buf);
    if (len <= 0) {
        return OPENSSL_ERROR();
    }

    auto releaseBuf = DeferRelease(buf, openssl::AOS_OPENSSL_free);

    auto err = asn1Value.Insert(asn1Value.begin(), buf, buf + len);
    if (!err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error OpenSSLCryptoProvider::ASN1DecodeOctetString(const Array<uint8_t>& src, Array<uint8_t>& result)
{
    const unsigned char* data = src.Get();
    long                 xlen;
    int                  tag, xclass;

    int ret = ASN1_get_object(&data, &xlen, &tag, &xclass, src.Size());
    if (ret != 0) {
        return OPENSSL_ERROR();
    }

    if (tag != V_ASN1_OCTET_STRING) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    result.Clear();

    auto err = result.Insert(result.begin(), data, data + xlen);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error OpenSSLCryptoProvider::ASN1DecodeOID(const Array<uint8_t>& inOID, Array<uint8_t>& result)
{
    const unsigned char* data = inOID.Get();
    long                 xlen;
    int                  tag, xclass;

    int ret = ASN1_get_object(&data, &xlen, &tag, &xclass, inOID.Size());
    if (ret != 0) {
        return OPENSSL_ERROR();
    }

    if (tag != V_ASN1_OBJECT) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    result.Clear();

    auto err = result.Insert(result.begin(), data, data + xlen);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<uuid::UUID> OpenSSLCryptoProvider::CreateUUIDv5(const uuid::UUID& space, const Array<uint8_t>& name)
{
    constexpr auto cUUIDVersion = 5;

    StaticArray<uint8_t, cSHA1InputDataSize> buffer = space;

    auto err = buffer.Insert(buffer.end(), name.begin(), name.end());
    if (!err.IsNone()) {
        return {{}, AOS_ERROR_WRAP(err)};
    }

    StaticArray<uint8_t, cSHA1DigestSize> sha1;

    sha1.Resize(sha1.MaxSize());

    SHA1(buffer.Get(), buffer.Size(), sha1.Get());

    // copy lowest 16 bytes
    uuid::UUID result = Array<uint8_t>(sha1.Get(), uuid::cUUIDSize);

    // The version of the UUID will be the lower 4 bits of cUUIDVersion
    result[6] = (result[6] & 0x0f) | uint8_t((cUUIDVersion & 0xf) << 4);
    result[8] = (result[8] & 0x3f) | 0x80; // RFC 4122 variant

    return result;
}

RetWithError<UniquePtr<HashItf>> OpenSSLCryptoProvider::CreateHash(Hash algorithm)
{
    if (algorithm == HashEnum::eNone) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument)};
    }

    auto hasher = MakeUnique<OpenSSLHash>(&mAllocator);

    auto err = hasher->Init(mLibCtx, algorithm.ToString().CStr());
    if (!err.IsNone()) {
        return {{}, err};
    }

    return {UniquePtr<HashItf>(Move(hasher)), ErrorEnum::eNone};
}

RetWithError<uint64_t> OpenSSLCryptoProvider::RandInt(uint64_t maxValue)
{
    uint64_t result = 0;

    if (RAND_priv_bytes_ex(mLibCtx, reinterpret_cast<unsigned char*>(&result), sizeof(result), cRNGStrength) != 1) {
        return {0, OPENSSL_ERROR()};
    }

    return result % maxValue;
}

Error OpenSSLCryptoProvider::RandBuffer(Array<uint8_t>& buffer, size_t size)
{
    if (size == 0) {
        size = buffer.MaxSize();
    }

    buffer.Resize(size);

    if (RAND_priv_bytes_ex(mLibCtx, buffer.Get(), static_cast<int>(size), cRNGStrength) != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error OpenSSLCryptoProvider::OpenSSLHash::Init(OSSL_LIB_CTX* libCtx, const char* mdtype)
{
    mType = EVP_MD_fetch(libCtx, mdtype, nullptr);
    if (!mType) {
        return OPENSSL_ERROR();
    }

    mMDCtx = EVP_MD_CTX_new();
    if (!mMDCtx) {
        EVP_MD_free(mType);
        mType = nullptr;

        return OPENSSL_ERROR();
    }

    if (EVP_DigestInit_ex(mMDCtx, mType, nullptr) != 1) {
        EVP_MD_CTX_free(mMDCtx);
        EVP_MD_free(mType);
        mMDCtx = nullptr;
        mType  = nullptr;

        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error OpenSSLCryptoProvider::OpenSSLHash::Update(const Array<uint8_t>& data)
{
    if (!mMDCtx) {
        return AOS_ERROR_WRAP(ErrorEnum::eWrongState);
    }

    if (EVP_DigestUpdate(mMDCtx, data.Get(), data.Size()) != 1) {
        return OPENSSL_ERROR();
    }

    return ErrorEnum::eNone;
}

Error OpenSSLCryptoProvider::OpenSSLHash::Finalize(Array<uint8_t>& hash)
{
    if (!mMDCtx || !mType) {
        return AOS_ERROR_WRAP(ErrorEnum::eWrongState);
    }

    auto err = hash.Resize(EVP_MD_size(mType));
    if (!err.IsNone()) {
        return err;
    }

    unsigned int size = hash.Size();
    if (EVP_DigestFinal_ex(mMDCtx, hash.Get(), &size) != 1) {
        return OPENSSL_ERROR();
    }

    EVP_MD_free(mType);
    mType = nullptr;

    EVP_MD_CTX_free(mMDCtx);
    mMDCtx = nullptr;

    err = hash.Resize(size);
    if (!err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

OpenSSLCryptoProvider::OpenSSLHash::~OpenSSLHash()
{
    if (mType) {
        EVP_MD_free(mType);
    }

    if (mMDCtx) {
        EVP_MD_CTX_free(mMDCtx);
    }
}

} // namespace aos::crypto
