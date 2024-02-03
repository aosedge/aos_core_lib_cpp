/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mbedtls/asn1write.h>
#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <mbedtls/platform.h>
#include <mbedtls/x509.h>
#include <psa/crypto.h>

#include "aos/common/crypto/mbedtls/cryptoprovider.hpp"
#include "aos/common/crypto/mbedtls/driverwrapper.hpp"

namespace aos {
namespace crypto {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

static int ASN1EncodeDERSequence(const Array<Array<uint8_t>>& items, unsigned char** p, unsigned char* start)
{
    size_t len = 0;
    int    ret = 0;

    for (int i = items.Size() - 1; i >= 0; i--) {
        const auto& item = items[i];
        MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_raw_buffer(p, start, item.Get(), item.Size()));
    }

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start, MBEDTLS_ASN1_SEQUENCE | MBEDTLS_ASN1_CONSTRUCTED));

    return len;
}

static int ASN1EncodeObjectIds(const Array<asn1::ObjectIdentifier>& oids, unsigned char** p, unsigned char* start)
{
    size_t len = 0;
    int    ret = 0;

    for (int i = oids.Size() - 1; i >= 0; i--) {
        const auto& oid = oids[i];

        mbedtls_asn1_buf resOID = {};

        ret = mbedtls_oid_from_numeric_string(&resOID, oid.Get(), oid.Size());
        if (ret != 0) {
            return ret;
        }

        ret = mbedtls_asn1_write_oid(p, start, reinterpret_cast<const char*>(resOID.p), resOID.len);
        mbedtls_free(resOID.p);

        if (ret < 0) {
            return ret;
        }

        len += ret;
    }

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start, MBEDTLS_ASN1_SEQUENCE | MBEDTLS_ASN1_CONSTRUCTED));

    return len;
}

static int ASN1EncodeBigInt(const Array<uint8_t>& number, unsigned char** p, unsigned char* start)
{
    size_t len = 0;
    int    ret = 0;

    // Implementation currently uses a little endian integer format to make ECDSA::Sign(PKCS11)/Verify(mbedtls)
    // combination work.
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_raw_buffer(p, start, number.Get(), number.Size()));

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start, MBEDTLS_ASN1_INTEGER));

    return len;
}

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

aos::Error MbedTLSCryptoProvider::Init()
{
    auto ret = psa_crypto_init();

    return ret != PSA_SUCCESS ? AOS_ERROR_WRAP(ret) : aos::ErrorEnum::eNone;
}

aos::Error MbedTLSCryptoProvider::CreateCSR(
    const aos::crypto::x509::CSR& templ, const aos::crypto::PrivateKeyItf& privKey, aos::String& pemCSR)
{
    mbedtls_x509write_csr csr;
    mbedtls_pk_context    key;

    InitializeCSR(csr, key);

    auto cleanupCSR = [&]() {
        mbedtls_x509write_csr_free(&csr);
        mbedtls_pk_free(&key);
    };

    auto ret = SetupOpaqueKey(key, privKey);
    if (!ret.mError.IsNone()) {
        cleanupCSR();

        return ret.mError;
    }

    auto keyID = ret.mValue;

    auto cleanupPSA = [&]() {
        AosPsaRemoveKey(keyID);

        cleanupCSR();
    };

    auto err = SetCSRProperties(csr, key, templ);
    if (err != aos::ErrorEnum::eNone) {
        cleanupPSA();

        return err;
    }

    err = WriteCSRPem(csr, pemCSR);

    cleanupPSA();

    return err;
}

aos::Error MbedTLSCryptoProvider::CreateCertificate(const aos::crypto::x509::Certificate& templ,
    const aos::crypto::x509::Certificate& parent, const aos::crypto::PrivateKeyItf& privKey, String& pemCert)
{
    mbedtls_x509write_cert   cert;
    mbedtls_pk_context       pk;
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context ctrDrbg;

    auto err = InitializeCertificate(cert, pk, ctrDrbg, entropy);
    if (err != aos::ErrorEnum::eNone) {
        return err;
    }

    auto cleanupCert = [&]() {
        mbedtls_x509write_crt_free(&cert);
        mbedtls_pk_free(&pk);
        mbedtls_ctr_drbg_free(&ctrDrbg);
        mbedtls_entropy_free(&entropy);
    };

    auto ret = SetupOpaqueKey(pk, privKey);
    if (!ret.mError.IsNone()) {
        cleanupCert();

        return ret.mError;
    }

    auto keyID = ret.mValue;

    auto cleanupPSA = [&]() {
        AosPsaRemoveKey(keyID);

        cleanupCert();
    };

    err = SetCertificateProperties(cert, pk, ctrDrbg, templ, parent);
    if (err != aos::ErrorEnum::eNone) {
        cleanupPSA();

        return err;
    }

    err = WriteCertificatePem(cert, pemCert);

    cleanupPSA();

    return err;
}

aos::Error MbedTLSCryptoProvider::PEMToX509Certs(
    const aos::String& pemBlob, aos::Array<aos::crypto::x509::Certificate>& resultCerts)
{
    mbedtls_x509_crt crt;

    mbedtls_x509_crt_init(&crt);

    int ret = mbedtls_x509_crt_parse(&crt, reinterpret_cast<const uint8_t*>(pemBlob.CStr()), pemBlob.Size() + 1);
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);

        return AOS_ERROR_WRAP(ret);
    }

    mbedtls_x509_crt* currentCrt = &crt;

    while (currentCrt != nullptr) {
        aos::crypto::x509::Certificate cert;

        auto err = ParseX509Certs(currentCrt, cert);
        if (!err.IsNone()) {
            mbedtls_x509_crt_free(&crt);

            return err;
        }

        err = resultCerts.PushBack(cert);
        if (!err.IsNone()) {
            mbedtls_x509_crt_free(&crt);

            return err;
        }

        currentCrt = currentCrt->next;
    }

    mbedtls_x509_crt_free(&crt);

    return aos::ErrorEnum::eNone;
}

aos::Error MbedTLSCryptoProvider::DERToX509Cert(
    const aos::Array<uint8_t>& derBlob, aos::crypto::x509::Certificate& resultCert)
{
    mbedtls_x509_crt crt;

    mbedtls_x509_crt_init(&crt);

    int ret = mbedtls_x509_crt_parse_der(&crt, derBlob.Get(), derBlob.Size());
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);

        return AOS_ERROR_WRAP(ret);
    }

    auto err = ParseX509Certs(&crt, resultCert);

    mbedtls_x509_crt_free(&crt);

    return err;
}

aos::Error MbedTLSCryptoProvider::ASN1EncodeDN(const aos::String& commonName, aos::Array<uint8_t>& result)
{
    mbedtls_asn1_named_data* dn {};

    int ret = mbedtls_x509_string_to_names(&dn, commonName.CStr());
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    result.Resize(result.MaxSize());
    uint8_t* start = result.Get();
    uint8_t* p     = start + result.Size();

    ret = mbedtls_x509_write_names(&p, start, dn);
    if (ret < 0) {
        mbedtls_asn1_free_named_data_list(&dn);

        return AOS_ERROR_WRAP(ret);
    }

    size_t len = start + result.Size() - p;

    memmove(start, p, len);

    mbedtls_asn1_free_named_data_list(&dn);

    return result.Resize(len);
}

aos::Error MbedTLSCryptoProvider::ASN1DecodeDN(const aos::Array<uint8_t>& dn, aos::String& result)
{
    mbedtls_asn1_named_data tmpDN = {};

    uint8_t* p   = const_cast<uint8_t*>(dn.begin());
    size_t   tmp = 0;

    if ((mbedtls_asn1_get_tag(&p, dn.end(), &tmp, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return ErrorEnum::eFailed;
    }

    if (mbedtls_x509_get_name(&p, dn.end(), &tmpDN) != 0) {
        return ErrorEnum::eFailed;
    }

    result.Resize(result.MaxSize());

    int len = mbedtls_x509_dn_gets(result.Get(), result.Size(), &tmpDN);
    mbedtls_asn1_free_named_data_list_shallow(tmpDN.next);

    if (len < 0) {
        return ErrorEnum::eFailed;
    }

    return result.Resize(len);
}

aos::RetWithError<aos::SharedPtr<aos::crypto::PrivateKeyItf>> MbedTLSCryptoProvider::PEMToX509PrivKey(
    const aos::String& pemBlob)
{
    (void)pemBlob;

    return {nullptr, aos::ErrorEnum::eNotSupported};
}

aos::Error MbedTLSCryptoProvider::ASN1EncodeObjectIds(
    const aos::Array<aos::crypto::asn1::ObjectIdentifier>& src, aos::Array<uint8_t>& asn1Value)
{
    asn1Value.Resize(asn1Value.MaxSize());

    uint8_t* start = asn1Value.Get();
    uint8_t* p     = asn1Value.Get() + asn1Value.Size();

    int len = aos::crypto::ASN1EncodeObjectIds(src, &p, start);
    if (len < 0) {
        return ErrorEnum::eFailed;
    }

    memmove(asn1Value.Get(), p, len);

    return asn1Value.Resize(len);
}

aos::Error MbedTLSCryptoProvider::ASN1EncodeBigInt(const aos::Array<uint8_t>& number, aos::Array<uint8_t>& asn1Value)
{
    asn1Value.Resize(asn1Value.MaxSize());
    uint8_t* p = asn1Value.Get() + asn1Value.Size();

    int len = aos::crypto::ASN1EncodeBigInt(number, &p, asn1Value.Get());
    if (len < 0) {
        return ErrorEnum::eFailed;
    }

    memmove(asn1Value.Get(), p, len);

    return asn1Value.Resize(len);
}

aos::Error MbedTLSCryptoProvider::ASN1EncodeDERSequence(
    const aos::Array<aos::Array<uint8_t>>& items, aos::Array<uint8_t>& asn1Value)
{
    asn1Value.Resize(asn1Value.MaxSize());

    uint8_t* start = asn1Value.Get();
    uint8_t* p     = asn1Value.Get() + asn1Value.Size();

    int len = aos::crypto::ASN1EncodeDERSequence(items, &p, start);
    if (len < 0) {
        return ErrorEnum::eFailed;
    }

    memmove(asn1Value.Get(), p, len);

    return asn1Value.Resize(len);
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

aos::Error MbedTLSCryptoProvider::ParseX509Certs(mbedtls_x509_crt* currentCrt, aos::crypto::x509::Certificate& cert)
{
    auto err = GetX509CertData(cert, currentCrt);
    if (!err.IsNone()) {
        return err;
    }

    err = ParseX509CertPublicKey(&currentCrt->pk, cert);
    if (!err.IsNone()) {
        return err;
    }

    return GetX509CertExtensions(cert, currentCrt);
}

Error MbedTLSCryptoProvider::ParseX509CertPublicKey(const mbedtls_pk_context* pk, x509::Certificate& cert)
{
    switch (mbedtls_pk_get_type(pk)) {
    case MBEDTLS_PK_RSA:
        return ParseRSAKey(mbedtls_pk_rsa(*pk), cert);

    case MBEDTLS_PK_ECKEY:
        return ParseECKey(mbedtls_pk_ec(*pk), cert);

    default:
        return aos::ErrorEnum::eNotFound;
    }
}

Error MbedTLSCryptoProvider::ParseECKey(const mbedtls_ecp_keypair* eckey, x509::Certificate& cert)
{
    StaticArray<uint8_t, cECDSAParamsOIDSize> paramsOID;
    StaticArray<uint8_t, cECDSAPointDERSize>  ecPoint;

    size_t      len = 0;
    const char* oid;
    auto        ret = mbedtls_oid_get_oid_by_ec_grp(eckey->MBEDTLS_PRIVATE(grp).id, &oid, &len);
    if (ret != 0) {
        return ret;
    }

    paramsOID.Resize(len);

    memcpy(paramsOID.Get(), oid, len);

    ecPoint.Resize(ecPoint.MaxSize());

    ret = mbedtls_ecp_point_write_binary(&eckey->MBEDTLS_PRIVATE(grp), &eckey->MBEDTLS_PRIVATE(Q),
        MBEDTLS_ECP_PF_UNCOMPRESSED, &len, ecPoint.Get(), ecPoint.Size());
    if (ret != 0) {
        return ret;
    }

    ecPoint.Resize(len);

    cert.mPublicKey = MakeShared<ECDSAPublicKey>(&mAllocator, paramsOID, ecPoint);

    return aos::ErrorEnum::eNone;
}

Error MbedTLSCryptoProvider::ParseRSAKey(const mbedtls_rsa_context* rsa, x509::Certificate& cert)
{
    StaticArray<uint8_t, cRSAModulusSize>     mN;
    StaticArray<uint8_t, cRSAPubExponentSize> mE;
    mbedtls_mpi                               mpiN, mpiE;

    mbedtls_mpi_init(&mpiN);
    mbedtls_mpi_init(&mpiE);

    auto cleanup = [&]() {
        mbedtls_mpi_free(&mpiN);
        mbedtls_mpi_free(&mpiE);
    };

    auto ret = mbedtls_rsa_export(rsa, &mpiN, nullptr, nullptr, nullptr, &mpiE);
    if (ret != 0) {
        cleanup();

        return ret;
    }

    mN.Resize(mbedtls_mpi_size(&mpiN));
    mE.Resize(mbedtls_mpi_size(&mpiE));

    ret = mbedtls_mpi_write_binary(&mpiN, mN.Get(), mN.Size());
    if (ret != 0) {
        cleanup();

        return ret;
    }

    ret = mbedtls_mpi_write_binary(&mpiE, mE.Get(), mE.Size());

    cleanup();

    if (ret != 0) {
        return ret;
    }

    cert.mPublicKey = MakeShared<RSAPublicKey>(&mAllocator, mN, mE);

    return ret;
}

Error MbedTLSCryptoProvider::GetX509CertData(aos::crypto::x509::Certificate& cert, mbedtls_x509_crt* crt)
{
    auto err = cert.mSubject.Resize(crt->subject_raw.len);
    if (!err.IsNone()) {
        return err;
    }

    memcpy(cert.mSubject.Get(), crt->subject_raw.p, crt->subject_raw.len);

    err = cert.mIssuer.Resize(crt->issuer_raw.len);
    if (!err.IsNone()) {
        return err;
    }

    memcpy(cert.mIssuer.Get(), crt->issuer_raw.p, crt->issuer_raw.len);

    err = cert.mSerial.Resize(crt->serial.len);
    if (!err.IsNone()) {
        return err;
    }

    memcpy(cert.mSerial.Get(), crt->serial.p, crt->serial.len);

    err = ConvertTime(crt->valid_from, cert.mNotBefore);
    if (!err.IsNone()) {
        return err;
    }

    err = ConvertTime(crt->valid_to, cert.mNotAfter);
    if (!err.IsNone()) {
        return err;
    }

    err = cert.mRaw.Resize(crt->raw.len);
    if (!err.IsNone()) {
        return err;
    }

    memcpy(cert.mRaw.Get(), crt->raw.p, crt->raw.len);

    return aos::ErrorEnum::eNone;
}

aos::Error MbedTLSCryptoProvider::ConvertTime(const mbedtls_x509_time& src, aos::Time& dst)
{
    tm tmp;

    tmp.tm_year = src.year - 1900;
    tmp.tm_mon  = src.mon - 1;
    tmp.tm_mday = src.day;
    tmp.tm_hour = src.hour;
    tmp.tm_min  = src.min;
    tmp.tm_sec  = src.sec;

    auto seconds = timegm(&tmp);
    if (seconds < 0) {
        return aos::ErrorEnum::eNone;
    }

    dst = Time::Unix(seconds, 0);

    return ErrorEnum::eNone;
}

aos::Error MbedTLSCryptoProvider::GetX509CertExtensions(aos::crypto::x509::Certificate& cert, mbedtls_x509_crt* crt)
{
    mbedtls_asn1_buf buf = crt->v3_ext;

    if (buf.len == 0) {
        return ErrorEnum::eNone;
    }

    mbedtls_asn1_sequence extns;

    auto ret = mbedtls_asn1_get_sequence_of(
        &buf.p, buf.p + buf.len, &extns, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    mbedtls_asn1_sequence* next = &extns;

    while (next != nullptr) {
        size_t tagLen {};

        auto err = mbedtls_asn1_get_tag(&(next->buf.p), next->buf.p + next->buf.len, &tagLen, MBEDTLS_ASN1_OID);
        if (err != 0) {
            return AOS_ERROR_WRAP(err);
        }

        if (!memcmp(next->buf.p, MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER, tagLen)) {
            unsigned char* p = next->buf.p + tagLen;
            err = mbedtls_asn1_get_tag(&p, p + next->buf.len - 2 - tagLen, &tagLen, MBEDTLS_ASN1_OCTET_STRING);
            if (err != 0) {
                return AOS_ERROR_WRAP(err);
            }

            err = mbedtls_asn1_get_tag(&p, p + next->buf.len - 2, &tagLen, MBEDTLS_ASN1_OCTET_STRING);
            if (err != 0) {
                return AOS_ERROR_WRAP(err);
            }

            cert.mSubjectKeyId.Resize(tagLen);
            memcpy(cert.mSubjectKeyId.Get(), p, tagLen);

            if (!cert.mAuthorityKeyId.IsEmpty()) {
                break;
            }
        }

        if (!memcmp(next->buf.p, MBEDTLS_OID_AUTHORITY_KEY_IDENTIFIER, tagLen)) {
            unsigned char* p = next->buf.p + tagLen;
            size_t         len;

            err = mbedtls_asn1_get_tag(&p, next->buf.p + next->buf.len, &len, MBEDTLS_ASN1_OCTET_STRING);
            if (err != 0) {
                return AOS_ERROR_WRAP(err);
            }

            if (*p != (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) {
                return AOS_ERROR_WRAP(MBEDTLS_ERR_ASN1_UNEXPECTED_TAG);
            }

            err = mbedtls_asn1_get_tag(
                &p, next->buf.p + next->buf.len, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
            if (err != 0) {
                return AOS_ERROR_WRAP(err);
            }

            if (*p != (MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0)) {
                return AOS_ERROR_WRAP(MBEDTLS_ERR_ASN1_UNEXPECTED_TAG);
            }

            err = mbedtls_asn1_get_tag(&p, next->buf.p + next->buf.len, &len, MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0);
            if (err != 0) {
                return AOS_ERROR_WRAP(err);
            }

            cert.mAuthorityKeyId.Resize(len);
            memcpy(cert.mAuthorityKeyId.Get(), p, len);

            if (!cert.mSubjectKeyId.IsEmpty()) {
                break;
            }
        }

        next = next->next;
    }

    return aos::ErrorEnum::eNone;
}

void MbedTLSCryptoProvider::InitializeCSR(mbedtls_x509write_csr& csr, mbedtls_pk_context& pk)
{
    mbedtls_x509write_csr_init(&csr);
    mbedtls_pk_init(&pk);

    mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);
}

aos::Error MbedTLSCryptoProvider::SetCSRProperties(
    mbedtls_x509write_csr& csr, mbedtls_pk_context& pk, const aos::crypto::x509::CSR& templ)
{
    mbedtls_x509write_csr_set_key(&csr, &pk);

    aos::StaticString<aos::crypto::cCertSubjSize> subject;
    auto                                          err = ASN1DecodeDN(templ.mSubject, subject);
    if (err != aos::ErrorEnum::eNone) {
        return err;
    }

    auto ret = mbedtls_x509write_csr_set_subject_name(&csr, subject.CStr());
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    err = SetCSRAlternativeNames(csr, templ);
    if (err != aos::ErrorEnum::eNone) {
        return err;
    }

    return SetCSRExtraExtensions(csr, templ);
}

aos::Error MbedTLSCryptoProvider::SetCSRAlternativeNames(
    mbedtls_x509write_csr& csr, const aos::crypto::x509::CSR& templ)
{
    mbedtls_x509_san_list   sanList[aos::crypto::cAltDNSNamesCount];
    aos::crypto::x509::CSR& tmpl         = const_cast<aos::crypto::x509::CSR&>(templ);
    size_t                  dnsNameCount = tmpl.mDNSNames.Size();

    for (size_t i = 0; i < tmpl.mDNSNames.Size(); i++) {
        sanList[i].node.type                      = MBEDTLS_X509_SAN_DNS_NAME;
        sanList[i].node.san.unstructured_name.tag = MBEDTLS_ASN1_IA5_STRING;
        sanList[i].node.san.unstructured_name.len = tmpl.mDNSNames[i].Size();
        sanList[i].node.san.unstructured_name.p   = reinterpret_cast<unsigned char*>(tmpl.mDNSNames[i].Get());

        sanList[i].next = (i < dnsNameCount - 1) ? &sanList[i + 1] : nullptr;
    }

    return AOS_ERROR_WRAP(mbedtls_x509write_csr_set_subject_alternative_name(&csr, sanList));
}

aos::Error MbedTLSCryptoProvider::SetCSRExtraExtensions(mbedtls_x509write_csr& csr, const aos::crypto::x509::CSR& templ)
{
    for (const auto& extension : templ.mExtraExtensions) {
        const char*          oid      = extension.mId.CStr();
        const unsigned char* value    = extension.mValue.Get();
        size_t               oidLen   = extension.mId.Size();
        size_t               valueLen = extension.mValue.Size();

        int ret = mbedtls_x509write_csr_set_extension(&csr, oid, oidLen, 0, value, valueLen);
        if (ret != 0) {
            return AOS_ERROR_WRAP(ret);
        }
    }

    return aos::ErrorEnum::eNone;
}

aos::Error MbedTLSCryptoProvider::WriteCSRPem(mbedtls_x509write_csr& csr, aos::String& pemCSR)
{
    pemCSR.Resize(pemCSR.MaxSize());

    auto ret = mbedtls_x509write_csr_pem(
        &csr, reinterpret_cast<uint8_t*>(pemCSR.Get()), pemCSR.Size() + 1, nullptr, nullptr);
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    pemCSR.Resize(strlen(reinterpret_cast<const char*>(pemCSR.CStr())));

    return aos::ErrorEnum::eNone;
}

aos::RetWithError<mbedtls_svc_key_id_t> MbedTLSCryptoProvider::SetupOpaqueKey(
    mbedtls_pk_context& pk, const aos::crypto::PrivateKeyItf& privKey)
{
    auto statusAddKey = AosPsaAddKey(privKey);
    if (!statusAddKey.mError.IsNone()) {
        return statusAddKey;
    }

    auto ret = mbedtls_pk_setup_opaque(&pk, statusAddKey.mValue);
    if (ret != 0) {
        AosPsaRemoveKey(statusAddKey.mValue);

        return aos::RetWithError<mbedtls_svc_key_id_t>(statusAddKey.mValue, AOS_ERROR_WRAP(ret));
    }

    return aos::RetWithError<mbedtls_svc_key_id_t>(statusAddKey.mValue, aos::ErrorEnum::eNone);
}

aos::Error MbedTLSCryptoProvider::InitializeCertificate(mbedtls_x509write_cert& cert, mbedtls_pk_context& pk,
    mbedtls_ctr_drbg_context& ctr_drbg, mbedtls_entropy_context& entropy)
{
    mbedtls_x509write_crt_init(&cert);
    mbedtls_pk_init(&pk);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    const char* pers = "cert_generation";

    mbedtls_x509write_crt_set_md_alg(&cert, MBEDTLS_MD_SHA256);

    return AOS_ERROR_WRAP(mbedtls_ctr_drbg_seed(
        &ctr_drbg, mbedtls_entropy_func, &entropy, reinterpret_cast<const unsigned char*>(pers), strlen(pers)));
}

aos::Error MbedTLSCryptoProvider::SetCertificateProperties(mbedtls_x509write_cert& cert, mbedtls_pk_context& pk,
    mbedtls_ctr_drbg_context& ctrDrbg, const aos::crypto::x509::Certificate& templ,
    const aos::crypto::x509::Certificate& parent)
{
    mbedtls_x509write_crt_set_subject_key(&cert, &pk);
    mbedtls_x509write_crt_set_issuer_key(&cert, &pk);

    auto err = SetCertificateSerialNumber(cert, ctrDrbg, templ);
    if (err != aos::ErrorEnum::eNone) {
        return err;
    }

    aos::StaticString<aos::crypto::cCertDNStringSize> subject;

    err = ASN1DecodeDN(templ.mSubject, subject);
    if (err != aos::ErrorEnum::eNone) {
        return err;
    }

    auto ret = mbedtls_x509write_crt_set_subject_name(&cert, subject.CStr());
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    err = SetCertificateValidityPeriod(cert, templ);
    if (err != aos::ErrorEnum::eNone) {
        return err;
    }

    aos::StaticString<aos::crypto::cCertDNStringSize> issuer;

    err = ASN1DecodeDN((!parent.mSubject.IsEmpty() ? parent.mSubject : templ.mIssuer), issuer);
    if (err != aos::ErrorEnum::eNone) {
        return err;
    }

    ret = mbedtls_x509write_crt_set_issuer_name(&cert, issuer.CStr());
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    err = SetCertificateSubjectKeyIdentifier(cert, templ);
    if (err != aos::ErrorEnum::eNone) {
        return err;
    }

    return SetCertificateAuthorityKeyIdentifier(cert, templ, parent);
}

aos::Error MbedTLSCryptoProvider::WriteCertificatePem(mbedtls_x509write_cert& cert, aos::String& pemCert)
{
    pemCert.Resize(pemCert.MaxSize());

    auto ret = mbedtls_x509write_crt_pem(
        &cert, reinterpret_cast<uint8_t*>(pemCert.Get()), pemCert.Size() + 1, mbedtls_ctr_drbg_random, nullptr);
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    pemCert.Resize(strlen(pemCert.CStr()));

    return aos::ErrorEnum::eNone;
}

aos::Error MbedTLSCryptoProvider::SetCertificateSerialNumber(
    mbedtls_x509write_cert& cert, mbedtls_ctr_drbg_context& ctrDrbg, const aos::crypto::x509::Certificate& templ)
{
    if (templ.mSerial.IsEmpty()) {
        mbedtls_mpi serial;
        mbedtls_mpi_init(&serial);

        auto ret
            = mbedtls_mpi_fill_random(&serial, MBEDTLS_X509_RFC5280_MAX_SERIAL_LEN, mbedtls_ctr_drbg_random, &ctrDrbg);
        if (ret != 0) {
            return AOS_ERROR_WRAP(ret);
        }

        ret = mbedtls_mpi_shift_r(&serial, 1);
        if (ret != 0) {
            return AOS_ERROR_WRAP(ret);
        }

        ret = mbedtls_x509write_crt_set_serial(&cert, &serial);
        mbedtls_mpi_free(&serial);

        return AOS_ERROR_WRAP(ret);
    }

    return AOS_ERROR_WRAP(mbedtls_x509write_crt_set_serial_raw(
        &cert, const_cast<aos::crypto::x509::Certificate&>(templ).mSerial.Get(), templ.mSerial.Size()));
}

aos::Error MbedTLSCryptoProvider::SetCertificateSubjectKeyIdentifier(
    mbedtls_x509write_cert& cert, const aos::crypto::x509::Certificate& templ)
{
    if (templ.mSubjectKeyId.IsEmpty()) {
        return AOS_ERROR_WRAP(mbedtls_x509write_crt_set_subject_key_identifier(&cert));
    }

    return AOS_ERROR_WRAP(mbedtls_x509write_crt_set_extension(&cert, MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER,
        MBEDTLS_OID_SIZE(MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER), 0, templ.mSubjectKeyId.Get(),
        templ.mSubjectKeyId.Size()));
}

aos::Error MbedTLSCryptoProvider::SetCertificateAuthorityKeyIdentifier(mbedtls_x509write_cert& cert,
    const aos::crypto::x509::Certificate& templ, const aos::crypto::x509::Certificate& parent)
{
    if (!parent.mSubjectKeyId.IsEmpty()) {
        return AOS_ERROR_WRAP(mbedtls_x509write_crt_set_extension(&cert, MBEDTLS_OID_AUTHORITY_KEY_IDENTIFIER,
            MBEDTLS_OID_SIZE(MBEDTLS_OID_AUTHORITY_KEY_IDENTIFIER), 0, parent.mSubjectKeyId.Get(),
            parent.mSubjectKeyId.Size()));
    }

    if (templ.mAuthorityKeyId.IsEmpty()) {
        return AOS_ERROR_WRAP(mbedtls_x509write_crt_set_authority_key_identifier(&cert));
    }

    return AOS_ERROR_WRAP(mbedtls_x509write_crt_set_extension(&cert, MBEDTLS_OID_AUTHORITY_KEY_IDENTIFIER,
        MBEDTLS_OID_SIZE(MBEDTLS_OID_AUTHORITY_KEY_IDENTIFIER), 0, templ.mAuthorityKeyId.Get(),
        templ.mAuthorityKeyId.Size()));
}

aos::Error MbedTLSCryptoProvider::SetCertificateValidityPeriod(
    mbedtls_x509write_cert& cert, const aos::crypto::x509::Certificate& templ)
{
    if (templ.mNotBefore.IsZero() || templ.mNotAfter.IsZero()) {
        return aos::ErrorEnum::eInvalidArgument;
    }

    auto formatTime = [](char* buffer, size_t size, const aos::Time& time) -> aos::Error {
        int day = 0, month = 0, year = 0, hour = 0, min = 0, sec = 0;

        auto err = time.GetDate(&day, &month, &year);
        if (!err.IsNone()) {
            return err;
        }

        err = time.GetTime(&hour, &min, &sec);
        if (!err.IsNone()) {
            return err;
        }

        snprintf(buffer, size, "%04d%02d%02d%02d%02d%02d", year, month, day, hour, min, sec);

        return aos::ErrorEnum::eNone;
    };

    char timeBufferBefore[16];
    char timeBufferAfter[16];

    auto err = formatTime(timeBufferBefore, sizeof(timeBufferBefore), templ.mNotBefore);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = formatTime(timeBufferAfter, sizeof(timeBufferAfter), templ.mNotAfter);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return AOS_ERROR_WRAP(mbedtls_x509write_crt_set_validity(&cert, timeBufferBefore, timeBufferAfter));
}

} // namespace crypto
} // namespace aos
