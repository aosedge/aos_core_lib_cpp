/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(__ZEPHYR__)
#include <zephyr/sys/timeutil.h>
#else
#include <time.h>
#endif

#include <mbedtls/asn1write.h>
#include <mbedtls/oid.h>
#include <mbedtls/pem.h>
#include <mbedtls/pk.h>
#include <mbedtls/platform.h>
#include <mbedtls/sha1.h>
#include <mbedtls/x509.h>
#include <psa/crypto.h>

#include "aos/common/crypto/mbedtls/cryptoprovider.hpp"

#include "../log.hpp"

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

static Error ASN1RemoveTag(const Array<uint8_t>& src, Array<uint8_t>& dst, int tag)
{
    uint8_t* p   = const_cast<uint8_t*>(src.Get());
    size_t   len = 0;

    int ret = mbedtls_asn1_get_tag(&p, src.end(), &len, tag);
    if (ret < 0) {
        return ret;
    }

    size_t tagAndLenSize = p - src.Get();
    if (src.Size() - tagAndLenSize != len) {
        return ErrorEnum::eInvalidArgument;
    }

    auto err = dst.Resize(len);
    if (!err.IsNone()) {
        return err;
    }

    memmove(dst.Get(), p, len);

    return ErrorEnum::eNone;
}

static Error ParseDN(const mbedtls_x509_name& dn, String& result)
{
    result.Resize(result.MaxSize());

    int ret = mbedtls_x509_dn_gets(result.Get(), result.Size(), &dn);
    if (ret <= 0) {
        return AOS_ERROR_WRAP(ret);
    }

    result.Resize(ret);

    return ErrorEnum::eNone;
}

static Error ParsePrivateKey(const String& pemCAKey, mbedtls_pk_context& privKey)
{
    mbedtls_ctr_drbg_context ctrDrbg;
    mbedtls_entropy_context  entropy;

    mbedtls_ctr_drbg_init(&ctrDrbg);
    mbedtls_entropy_init(&entropy);

    const char* pers = "test";
    Error       err  = ErrorEnum::eNone;

    int ret = mbedtls_ctr_drbg_seed(&ctrDrbg, mbedtls_entropy_func, &entropy, (const uint8_t*)pers, strlen(pers));
    if (ret != 0) {
        err = AOS_ERROR_WRAP(ret);
        goto free;
    }

    ret = mbedtls_pk_parse_key(&privKey, reinterpret_cast<const uint8_t*>(pemCAKey.Get()), pemCAKey.Size() + 1, nullptr,
        0, mbedtls_ctr_drbg_random, &ctrDrbg);
    if (ret != 0) {
        err = AOS_ERROR_WRAP(ret);
        goto free;
    }

free:
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctrDrbg);

    return err;
}

// Implementation is based on https://github.com/Mbed-TLS/mbedtls/blob/development/programs/x509/cert_write.c
static Error CreateClientCert(const mbedtls_x509_csr& csr, const mbedtls_pk_context& caKey,
    const mbedtls_x509_crt& caCert, const Array<uint8_t>& serial, String& pemClientCert)
{
    mbedtls_x509write_cert clientCert;

    mbedtls_x509write_crt_init(&clientCert);

    mbedtls_x509write_crt_set_md_alg(&clientCert, MBEDTLS_MD_SHA256);

    // set CSR properties
    StaticString<crypto::cCertSubjSize> subject;
    Error                               err = ParseDN(csr.subject, subject);
    if (!err.IsNone()) {
        mbedtls_x509write_crt_free(&clientCert);

        return AOS_ERROR_WRAP(err);
    }

    int ret = mbedtls_x509write_crt_set_subject_name(&clientCert, subject.Get());
    if (ret != 0) {
        mbedtls_x509write_crt_free(&clientCert);

        return AOS_ERROR_WRAP(ret);
    }

    mbedtls_x509write_crt_set_subject_key(&clientCert, const_cast<mbedtls_pk_context*>(&csr.pk));

    // set CA certificate properties
    StaticString<crypto::cCertIssuerSize> issuer;

    err = ParseDN(caCert.subject, issuer);
    if (!err.IsNone()) {
        mbedtls_x509write_crt_free(&clientCert);

        return AOS_ERROR_WRAP(err);
    }

    ret = mbedtls_x509write_crt_set_issuer_name(&clientCert, issuer.Get());
    if (ret != 0) {
        mbedtls_x509write_crt_free(&clientCert);

        return AOS_ERROR_WRAP(ret);
    }

    // set CA key
    mbedtls_x509write_crt_set_issuer_key(&clientCert, const_cast<mbedtls_pk_context*>(&caKey));

    // set additional properties: serial, valid time interval
    ret = mbedtls_x509write_crt_set_serial_raw(&clientCert, const_cast<uint8_t*>(serial.Get()), serial.Size());
    if (ret != 0) {
        mbedtls_x509write_crt_free(&clientCert);

        return AOS_ERROR_WRAP(ret);
    }

    StaticString<cTimeStrLen> notBefore, notAfter;

    Tie(notBefore, err) = Time::Now().ToString();
    if (!err.IsNone()) {
        mbedtls_x509write_crt_free(&clientCert);

        return AOS_ERROR_WRAP(err);
    }

    Tie(notAfter, err) = Time::Now().Add(Years(1)).ToString();
    if (!err.IsNone()) {
        mbedtls_x509write_crt_free(&clientCert);

        return AOS_ERROR_WRAP(err);
    }

    ret = mbedtls_x509write_crt_set_validity(&clientCert, notBefore.CStr(), notAfter.CStr());
    if (ret != 0) {
        mbedtls_x509write_crt_free(&clientCert);

        return AOS_ERROR_WRAP(ret);
    }

    // write client certificate to the buffer
    pemClientCert.Resize(pemClientCert.MaxSize());

    ret = mbedtls_x509write_crt_pem(&clientCert, reinterpret_cast<uint8_t*>(pemClientCert.Get()),
        pemClientCert.Size() + 1, mbedtls_ctr_drbg_random, nullptr);
    if (ret != 0) {
        mbedtls_x509write_crt_free(&clientCert);

        return AOS_ERROR_WRAP(ret);
    }

    pemClientCert.Resize(strlen(pemClientCert.Get()));

    // free
    mbedtls_x509write_crt_free(&clientCert);

    return aos::ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error MbedTLSCryptoProvider::Init()
{
    LOG_INF() << "Initialize mbedTLS crypto provider";

    auto ret = psa_crypto_init();

    return ret != PSA_SUCCESS ? AOS_ERROR_WRAP(ret) : ErrorEnum::eNone;
}

Error MbedTLSCryptoProvider::CreateCSR(const x509::CSR& templ, const PrivateKeyItf& privKey, String& pemCSR)
{
    mbedtls_x509write_csr csr;
    mbedtls_pk_context    key;

    LOG_DBG() << "Create CSR";

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

    auto keyID = ret.mValue.mKeyID;

    auto cleanupPSA = [&]() {
        AosPsaRemoveKey(keyID);

        cleanupCSR();
    };

    mbedtls_x509write_csr_set_md_alg(&csr, ret.mValue.mMDType);

    auto err = SetCSRProperties(csr, key, templ);
    if (err != ErrorEnum::eNone) {
        cleanupPSA();

        return err;
    }

    err = WriteCSRPem(csr, pemCSR);

    cleanupPSA();

    return err;
}

Error MbedTLSCryptoProvider::CreateCertificate(
    const x509::Certificate& templ, const x509::Certificate& parent, const PrivateKeyItf& privKey, String& pemCert)
{
    mbedtls_x509write_cert   cert;
    mbedtls_pk_context       pk;
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context ctrDrbg;

    LOG_DBG() << "Create certificate";

    auto err = InitializeCertificate(cert, pk, ctrDrbg, entropy);
    if (err != ErrorEnum::eNone) {
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

    auto keyID = ret.mValue.mKeyID;

    auto cleanupPSA = [&]() {
        AosPsaRemoveKey(keyID);

        cleanupCert();
    };

    mbedtls_x509write_crt_set_md_alg(&cert, ret.mValue.mMDType);

    err = SetCertificateProperties(cert, pk, ctrDrbg, templ, parent);
    if (err != ErrorEnum::eNone) {
        cleanupPSA();

        return err;
    }

    err = WriteCertificatePem(cert, pemCert);

    cleanupPSA();

    return err;
}

Error MbedTLSCryptoProvider::CreateClientCert(const String& pemCSR, const String& pemCAKey, const String& pemCACert,
    const Array<uint8_t>& serial, String& pemClientCert)
{
    Error              err = ErrorEnum::eNone;
    mbedtls_x509_csr   csr;
    mbedtls_pk_context caKey;
    mbedtls_x509_crt   caCrt;

    // parse CSR
    mbedtls_x509_csr_init(&csr);
    auto ret = mbedtls_x509_csr_parse(&csr, reinterpret_cast<const uint8_t*>(pemCSR.Get()), pemCSR.Size() + 1);
    if (ret != 0) {
        err = AOS_ERROR_WRAP(ret);
        goto freeCSR;
    }

    // parse CA key
    mbedtls_pk_init(&caKey);
    err = ParsePrivateKey(pemCAKey, caKey);
    if (!err.IsNone()) {
        goto freeKey;
    }

    // parse CA cert
    mbedtls_x509_crt_init(&caCrt);
    ret = mbedtls_x509_crt_parse(&caCrt, reinterpret_cast<const uint8_t*>(pemCACert.CStr()), pemCACert.Size() + 1);
    if (ret != 0) {
        err = AOS_ERROR_WRAP(ret);
        goto freeCRT;
    }

    // create client certificate
    err = aos::crypto::CreateClientCert(csr, caKey, caCrt, serial, pemClientCert);

freeCRT:
    mbedtls_x509_crt_free(&caCrt);
freeKey:
    mbedtls_pk_free(&caKey);
freeCSR:
    mbedtls_x509_csr_free(&csr);

    return err;
}

Error MbedTLSCryptoProvider::PEMToX509Certs(const String& pemBlob, Array<x509::Certificate>& resultCerts)
{
    mbedtls_x509_crt crt;

    LOG_DBG() << "Convert certs from PEM to x509";

    mbedtls_x509_crt_init(&crt);

    int ret = mbedtls_x509_crt_parse(&crt, reinterpret_cast<const uint8_t*>(pemBlob.CStr()), pemBlob.Size() + 1);
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);

        return AOS_ERROR_WRAP(ret);
    }

    mbedtls_x509_crt* currentCrt = &crt;

    while (currentCrt != nullptr) {
        auto err = resultCerts.EmplaceBack();
        if (!err.IsNone()) {
            mbedtls_x509_crt_free(&crt);

            return err;
        }

        auto& cert = resultCerts.Back().mValue;

        err = ParseX509Certs(currentCrt, cert);
        if (!err.IsNone()) {
            mbedtls_x509_crt_free(&crt);

            return err;
        }

        currentCrt = currentCrt->next;
    }

    mbedtls_x509_crt_free(&crt);

    return ErrorEnum::eNone;
}

Error MbedTLSCryptoProvider::X509CertToPEM(const x509::Certificate& certificate, String& dst)
{
    static constexpr auto cPEMBeginCert = "-----BEGIN CERTIFICATE-----\n";
    static constexpr auto cPEMEndCert   = "-----END CERTIFICATE-----\n";

    size_t olen;

    auto ret = mbedtls_pem_write_buffer(cPEMBeginCert, cPEMEndCert, certificate.mRaw.Get(), certificate.mRaw.Size(),
        reinterpret_cast<uint8_t*>(dst.Get()), dst.Size(), &olen);
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    if (olen <= 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eFailed);
    }

    dst.Resize(olen - 1);

    return ErrorEnum::eNone;
}

Error MbedTLSCryptoProvider::DERToX509Cert(const Array<uint8_t>& derBlob, x509::Certificate& resultCert)
{
    mbedtls_x509_crt crt;

    LOG_DBG() << "Convert certs from DER to x509";

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

Error MbedTLSCryptoProvider::ASN1EncodeDN(const String& commonName, Array<uint8_t>& result)
{
    mbedtls_asn1_named_data* dn {};

    LOG_DBG() << "Encode ASN1 DN";

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

Error MbedTLSCryptoProvider::ASN1DecodeDN(const Array<uint8_t>& dn, String& result)
{
    mbedtls_asn1_named_data tmpDN = {};

    LOG_DBG() << "Decode ASN1 DN";

    uint8_t* p   = const_cast<uint8_t*>(dn.begin());
    size_t   tmp = 0;

    auto ret = mbedtls_asn1_get_tag(&p, dn.end(), &tmp, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (ret != 0) {
        return ret;
    }

    ret = mbedtls_x509_get_name(&p, dn.end(), &tmpDN);
    if (ret != 0) {
        return ret;
    }

    result.Resize(result.MaxSize());

    int len = mbedtls_x509_dn_gets(result.Get(), result.Size(), &tmpDN);
    mbedtls_asn1_free_named_data_list_shallow(tmpDN.next);

    if (len < 0) {
        return len;
    }

    return result.Resize(len);
}

RetWithError<SharedPtr<PrivateKeyItf>> MbedTLSCryptoProvider::PEMToX509PrivKey(const String& pemBlob)
{
    (void)pemBlob;

    LOG_DBG() << "Create private key from PEM";

    return {nullptr, ErrorEnum::eNotSupported};
}

Error MbedTLSCryptoProvider::ASN1EncodeObjectIds(const Array<asn1::ObjectIdentifier>& src, Array<uint8_t>& asn1Value)
{
    asn1Value.Resize(asn1Value.MaxSize());

    LOG_DBG() << "Encode ASN1 object IDs";

    uint8_t* start = asn1Value.Get();
    uint8_t* p     = asn1Value.Get() + asn1Value.Size();

    int len = crypto::ASN1EncodeObjectIds(src, &p, start);
    if (len < 0) {
        return len;
    }

    memmove(asn1Value.Get(), p, len);

    return asn1Value.Resize(len);
}

Error MbedTLSCryptoProvider::ASN1EncodeBigInt(const Array<uint8_t>& number, Array<uint8_t>& asn1Value)
{
    asn1Value.Resize(asn1Value.MaxSize());
    uint8_t* p = asn1Value.Get() + asn1Value.Size();

    LOG_DBG() << "Encode ASN1 big int";

    int len = crypto::ASN1EncodeBigInt(number, &p, asn1Value.Get());
    if (len < 0) {
        return len;
    }

    memmove(asn1Value.Get(), p, len);

    return asn1Value.Resize(len);
}

Error MbedTLSCryptoProvider::ASN1EncodeDERSequence(const Array<Array<uint8_t>>& items, Array<uint8_t>& asn1Value)
{
    asn1Value.Resize(asn1Value.MaxSize());

    uint8_t* start = asn1Value.Get();
    uint8_t* p     = asn1Value.Get() + asn1Value.Size();

    LOG_DBG() << "Encode ASN1 DER sequence";

    int len = crypto::ASN1EncodeDERSequence(items, &p, start);
    if (len < 0) {
        return len;
    }

    memmove(asn1Value.Get(), p, len);

    return asn1Value.Resize(len);
}

Error MbedTLSCryptoProvider::ASN1DecodeOctetString(const Array<uint8_t>& src, Array<uint8_t>& dst)
{
    LOG_DBG() << "Decode ASN1 octet string";

    return crypto::ASN1RemoveTag(src, dst, MBEDTLS_ASN1_OCTET_STRING);
}

Error MbedTLSCryptoProvider::ASN1DecodeOID(const Array<uint8_t>& inOID, Array<uint8_t>& dst)
{
    LOG_DBG() << "Decode ASN1 OID";

    return crypto::ASN1RemoveTag(inOID, dst, MBEDTLS_ASN1_OID);
}

RetWithError<uuid::UUID> MbedTLSCryptoProvider::CreateUUIDv5(const uuid::UUID& space, const Array<uint8_t>& name)
{
    constexpr auto cUUIDVersion = 5;

    StaticArray<uint8_t, cSHA1InputDataSize> buffer = space;

    auto err = buffer.Insert(buffer.end(), name.begin(), name.end());
    if (!err.IsNone()) {
        return {{}, AOS_ERROR_WRAP(err)};
    }

    StaticArray<uint8_t, cSHA1DigestSize> sha1;

    sha1.Resize(sha1.MaxSize());

    int ret = mbedtls_sha1(buffer.Get(), buffer.Size(), sha1.Get());
    if (ret != 0) {
        return {{}, AOS_ERROR_WRAP(ret)};
    }

    // copy lowest 16 bytes
    uuid::UUID result = Array<uint8_t>(sha1.Get(), uuid::cUUIDSize);

    // The version of the UUID will be the lower 4 bits of cUUIDVersion
    result[6] = (result[6] & 0x0f) | uint8_t((cUUIDVersion & 0xf) << 4);
    result[8] = (result[8] & 0x3f) | 0x80; // RFC 4122 variant

    return result;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error MbedTLSCryptoProvider::ParseX509Certs(mbedtls_x509_crt* currentCrt, x509::Certificate& cert)
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
        return ErrorEnum::eNotFound;
    }
}

Error MbedTLSCryptoProvider::ParseECKey(const mbedtls_ecp_keypair* eckey, x509::Certificate& cert)
{
    StaticArray<uint8_t, cECDSAParamsOIDSize> paramsOID;
    StaticArray<uint8_t, cECDSAPointDERSize>  ecPoint;

    size_t      len = 0;
    const char* oid;

    auto ret = mbedtls_oid_get_oid_by_ec_grp(eckey->MBEDTLS_PRIVATE(grp).id, &oid, &len);
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    auto err = paramsOID.Resize(len);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    memcpy(paramsOID.Get(), oid, len);

    err = ecPoint.Resize(ecPoint.MaxSize());
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    ret = mbedtls_ecp_point_write_binary(&eckey->MBEDTLS_PRIVATE(grp), &eckey->MBEDTLS_PRIVATE(Q),
        MBEDTLS_ECP_PF_UNCOMPRESSED, &len, ecPoint.Get(), ecPoint.Size());
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    err = ecPoint.Resize(len);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    cert.mPublicKey.SetValue<ECDSAPublicKey>(paramsOID, ecPoint);

    return ErrorEnum::eNone;
}

Error MbedTLSCryptoProvider::ParseRSAKey(const mbedtls_rsa_context* rsa, x509::Certificate& cert)
{
    StaticArray<uint8_t, cRSAModulusSize>     n;
    StaticArray<uint8_t, cRSAPubExponentSize> e;
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

        return AOS_ERROR_WRAP(ret);
    }

    auto err = n.Resize(mbedtls_mpi_size(&mpiN));
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = e.Resize(mbedtls_mpi_size(&mpiE));
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    ret = mbedtls_mpi_write_binary(&mpiN, n.Get(), n.Size());
    if (ret != 0) {
        cleanup();

        return AOS_ERROR_WRAP(ret);
    }

    ret = mbedtls_mpi_write_binary(&mpiE, e.Get(), e.Size());

    cleanup();

    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    cert.mPublicKey.SetValue<RSAPublicKey>(n, e);

    return aos::ErrorEnum::eNone;
}

Error MbedTLSCryptoProvider::GetX509CertData(x509::Certificate& cert, mbedtls_x509_crt* crt)
{
    auto err = cert.mSubject.Resize(crt->subject_raw.len);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    memcpy(cert.mSubject.Get(), crt->subject_raw.p, crt->subject_raw.len);

    err = cert.mIssuer.Resize(crt->issuer_raw.len);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    memcpy(cert.mIssuer.Get(), crt->issuer_raw.p, crt->issuer_raw.len);

    err = cert.mSerial.Resize(crt->serial.len);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    memcpy(cert.mSerial.Get(), crt->serial.p, crt->serial.len);

    aos::Tie(cert.mNotBefore, err) = ConvertTime(crt->valid_from);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    aos::Tie(cert.mNotAfter, err) = ConvertTime(crt->valid_to);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = cert.mRaw.Resize(crt->raw.len);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    memcpy(cert.mRaw.Get(), crt->raw.p, crt->raw.len);

    return ErrorEnum::eNone;
}

RetWithError<Time> MbedTLSCryptoProvider::ConvertTime(const mbedtls_x509_time& src)
{
    tm tmp;

    tmp.tm_year = src.year - 1900;
    tmp.tm_mon  = src.mon - 1;
    tmp.tm_mday = src.day;
    tmp.tm_hour = src.hour;
    tmp.tm_min  = src.min;
    tmp.tm_sec  = src.sec;

#if defined(__ZEPHYR__)
    auto seconds = timeutil_timegm(&tmp);
#else
    auto seconds = timegm(&tmp);
#endif
    if (seconds < 0) {
        return {Time(), AOS_ERROR_WRAP(errno)};
    }

    return Time::Unix(seconds, 0);
}

Error MbedTLSCryptoProvider::GetX509CertExtensions(x509::Certificate& cert, mbedtls_x509_crt* crt)
{
    Error            returnError = ErrorEnum::eNone;
    mbedtls_asn1_buf buf         = crt->v3_ext;

    if (buf.len == 0) {
        return ErrorEnum::eNone;
    }

    mbedtls_asn1_sequence extns;
    extns.next = NULL;

    auto ret = mbedtls_asn1_get_sequence_of(
        &buf.p, buf.p + buf.len, &extns, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    mbedtls_asn1_sequence* next = &extns;

    if (extns.buf.len == 0) {
        goto cleanup;
    }

    while (next != nullptr) {
        size_t tagLen {};

        auto err = mbedtls_asn1_get_tag(&(next->buf.p), next->buf.p + next->buf.len, &tagLen, MBEDTLS_ASN1_OID);
        if (err != 0) {
            returnError = AOS_ERROR_WRAP(err);

            goto cleanup;
        }

        if (!memcmp(next->buf.p, MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER, tagLen)) {
            unsigned char* p = next->buf.p + tagLen;
            err = mbedtls_asn1_get_tag(&p, p + next->buf.len - 2 - tagLen, &tagLen, MBEDTLS_ASN1_OCTET_STRING);
            if (err != 0) {
                returnError = AOS_ERROR_WRAP(err);

                goto cleanup;
            }

            err = mbedtls_asn1_get_tag(&p, p + next->buf.len - 2, &tagLen, MBEDTLS_ASN1_OCTET_STRING);
            if (err != 0) {
                returnError = AOS_ERROR_WRAP(err);

                goto cleanup;
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
                returnError = AOS_ERROR_WRAP(err);

                goto cleanup;
            }

            if (*p != (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) {
                returnError = AOS_ERROR_WRAP(MBEDTLS_ERR_ASN1_UNEXPECTED_TAG);

                goto cleanup;
            }

            err = mbedtls_asn1_get_tag(
                &p, next->buf.p + next->buf.len, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
            if (err != 0) {
                returnError = AOS_ERROR_WRAP(err);

                goto cleanup;
            }

            if (*p != (MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0)) {
                returnError = AOS_ERROR_WRAP(MBEDTLS_ERR_ASN1_UNEXPECTED_TAG);

                goto cleanup;
            }

            err = mbedtls_asn1_get_tag(&p, next->buf.p + next->buf.len, &len, MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0);
            if (err != 0) {
                returnError = AOS_ERROR_WRAP(err);

                goto cleanup;
            }

            cert.mAuthorityKeyId.Resize(len);
            memcpy(cert.mAuthorityKeyId.Get(), p, len);

            if (!cert.mSubjectKeyId.IsEmpty()) {
                break;
            }
        }

        next = next->next;
    }

cleanup:
    mbedtls_asn1_sequence_free(extns.next);

    return returnError;
}

void MbedTLSCryptoProvider::InitializeCSR(mbedtls_x509write_csr& csr, mbedtls_pk_context& pk)
{
    mbedtls_x509write_csr_init(&csr);
    mbedtls_pk_init(&pk);

    mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);
}

Error MbedTLSCryptoProvider::SetCSRProperties(
    mbedtls_x509write_csr& csr, mbedtls_pk_context& pk, const x509::CSR& templ)
{
    mbedtls_x509write_csr_set_key(&csr, &pk);

    StaticString<cCertSubjSize> subject;
    auto                        err = ASN1DecodeDN(templ.mSubject, subject);
    if (err != ErrorEnum::eNone) {
        return err;
    }

    auto ret = mbedtls_x509write_csr_set_subject_name(&csr, subject.CStr());
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    err = SetCSRAlternativeNames(csr, templ);
    if (err != ErrorEnum::eNone) {
        return err;
    }

    return SetCSRExtraExtensions(csr, templ);
}

Error MbedTLSCryptoProvider::SetCSRAlternativeNames(mbedtls_x509write_csr& csr, const x509::CSR& templ)
{
    mbedtls_x509_san_list sanList[cAltDNSNamesCount];
    size_t                dnsNameCount = templ.mDNSNames.Size();

    if (templ.mDNSNames.Size() == 0) {
        return ErrorEnum::eNone;
    }

    for (size_t i = 0; i < templ.mDNSNames.Size(); i++) {
        sanList[i].node.type                      = MBEDTLS_X509_SAN_DNS_NAME;
        sanList[i].node.san.unstructured_name.tag = MBEDTLS_ASN1_IA5_STRING;
        sanList[i].node.san.unstructured_name.len = templ.mDNSNames[i].Size();
        sanList[i].node.san.unstructured_name.p
            = reinterpret_cast<unsigned char*>(const_cast<char*>(templ.mDNSNames[i].Get()));

        sanList[i].next = (i < dnsNameCount - 1) ? &sanList[i + 1] : nullptr;
    }

    return AOS_ERROR_WRAP(mbedtls_x509write_csr_set_subject_alternative_name(&csr, sanList));
}

Error MbedTLSCryptoProvider::SetCSRExtraExtensions(mbedtls_x509write_csr& csr, const x509::CSR& templ)
{
    for (const auto& extension : templ.mExtraExtensions) {
        mbedtls_asn1_buf resOID = {};

        auto ret = mbedtls_oid_from_numeric_string(&resOID, extension.mID.Get(), extension.mID.Size());
        if (ret != 0) {
            return AOS_ERROR_WRAP(ret);
        }

        const unsigned char* value    = extension.mValue.Get();
        size_t               valueLen = extension.mValue.Size();

        ret = mbedtls_x509write_csr_set_extension(
            &csr, reinterpret_cast<const char*>(resOID.p), resOID.len, 0, value, valueLen);
        mbedtls_free(resOID.p);
        if (ret != 0) {
            return AOS_ERROR_WRAP(ret);
        }
    }

    return ErrorEnum::eNone;
}

Error MbedTLSCryptoProvider::WriteCSRPem(mbedtls_x509write_csr& csr, String& pemCSR)
{
    pemCSR.Resize(pemCSR.MaxSize());

    auto ret = mbedtls_x509write_csr_pem(
        &csr, reinterpret_cast<uint8_t*>(pemCSR.Get()), pemCSR.Size() + 1, nullptr, nullptr);
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    pemCSR.Resize(strlen(reinterpret_cast<const char*>(pemCSR.CStr())));

    return ErrorEnum::eNone;
}

RetWithError<KeyInfo> MbedTLSCryptoProvider::SetupOpaqueKey(mbedtls_pk_context& pk, const PrivateKeyItf& privKey)
{
    auto statusAddKey = AosPsaAddKey(privKey);
    if (!statusAddKey.mError.IsNone()) {
        return statusAddKey;
    }

    auto ret = mbedtls_pk_setup_opaque(&pk, statusAddKey.mValue.mKeyID);
    if (ret != 0) {
        AosPsaRemoveKey(statusAddKey.mValue.mKeyID);

        return RetWithError<KeyInfo>(statusAddKey.mValue, AOS_ERROR_WRAP(ret));
    }

    return RetWithError<KeyInfo>(statusAddKey.mValue, ErrorEnum::eNone);
}

Error MbedTLSCryptoProvider::InitializeCertificate(mbedtls_x509write_cert& cert, mbedtls_pk_context& pk,
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

Error MbedTLSCryptoProvider::SetCertificateProperties(mbedtls_x509write_cert& cert, mbedtls_pk_context& pk,
    mbedtls_ctr_drbg_context& ctrDrbg, const x509::Certificate& templ, const x509::Certificate& parent)
{
    mbedtls_x509write_crt_set_subject_key(&cert, &pk);
    mbedtls_x509write_crt_set_issuer_key(&cert, &pk);

    auto err = SetCertificateSerialNumber(cert, ctrDrbg, templ);
    if (err != ErrorEnum::eNone) {
        return err;
    }

    StaticString<cCertDNStringSize> subject;

    err = ASN1DecodeDN(templ.mSubject, subject);
    if (err != ErrorEnum::eNone) {
        return err;
    }

    auto ret = mbedtls_x509write_crt_set_subject_name(&cert, subject.CStr());
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    err = SetCertificateValidityPeriod(cert, templ);
    if (err != ErrorEnum::eNone) {
        return err;
    }

    StaticString<cCertDNStringSize> issuer;

    err = ASN1DecodeDN((!parent.mSubject.IsEmpty() ? parent.mSubject : templ.mIssuer), issuer);
    if (err != ErrorEnum::eNone) {
        return err;
    }

    ret = mbedtls_x509write_crt_set_issuer_name(&cert, issuer.CStr());
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    err = SetCertificateSubjectKeyIdentifier(cert, templ);
    if (err != ErrorEnum::eNone) {
        return err;
    }

    return SetCertificateAuthorityKeyIdentifier(cert, templ, parent);
}

Error MbedTLSCryptoProvider::WriteCertificatePem(mbedtls_x509write_cert& cert, String& pemCert)
{
    pemCert.Resize(pemCert.MaxSize());

    auto ret = mbedtls_x509write_crt_pem(
        &cert, reinterpret_cast<uint8_t*>(pemCert.Get()), pemCert.Size() + 1, mbedtls_ctr_drbg_random, nullptr);
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    pemCert.Resize(strlen(pemCert.CStr()));

    return ErrorEnum::eNone;
}

Error MbedTLSCryptoProvider::SetCertificateSerialNumber(
    mbedtls_x509write_cert& cert, mbedtls_ctr_drbg_context& ctrDrbg, const x509::Certificate& templ)
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
        &cert, const_cast<x509::Certificate&>(templ).mSerial.Get(), templ.mSerial.Size()));
}

Error MbedTLSCryptoProvider::SetCertificateSubjectKeyIdentifier(
    mbedtls_x509write_cert& cert, const x509::Certificate& templ)
{
    if (templ.mSubjectKeyId.IsEmpty()) {
        return AOS_ERROR_WRAP(mbedtls_x509write_crt_set_subject_key_identifier(&cert));
    }

    return AOS_ERROR_WRAP(mbedtls_x509write_crt_set_extension(&cert, MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER,
        MBEDTLS_OID_SIZE(MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER), 0, templ.mSubjectKeyId.Get(),
        templ.mSubjectKeyId.Size()));
}

Error MbedTLSCryptoProvider::SetCertificateAuthorityKeyIdentifier(
    mbedtls_x509write_cert& cert, const x509::Certificate& templ, const x509::Certificate& parent)
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

Error MbedTLSCryptoProvider::SetCertificateValidityPeriod(mbedtls_x509write_cert& cert, const x509::Certificate& templ)
{
    if (templ.mNotBefore.IsZero() || templ.mNotAfter.IsZero()) {
        return ErrorEnum::eInvalidArgument;
    }

    StaticString<cTimeStrLen> notBefore, notAfter;
    Error                     err = ErrorEnum::eNone;

    Tie(notBefore, err) = templ.mNotBefore.ToString();
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    Tie(notAfter, err) = templ.mNotAfter.ToString();
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return AOS_ERROR_WRAP(mbedtls_x509write_crt_set_validity(&cert, notBefore.Get(), notAfter.Get()));
}

} // namespace crypto
} // namespace aos
