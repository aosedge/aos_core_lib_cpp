/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <psa/crypto.h>

#include "aos/common/crypto/mbedtls/cryptoprovider.hpp"
#include "aos/common/crypto/mbedtls/drivers/aos/driverwrap.hpp"

namespace aos {
namespace crypto {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

aos::Error MbedTLSCryptoProvider::PEMToX509Certs(
    const aos::Array<uint8_t>& pemBlob, aos::Array<aos::crypto::x509::Certificate>& resultCerts)
{
    mbedtls_x509_crt crt;

    mbedtls_x509_crt_init(&crt);

    int ret = mbedtls_x509_crt_parse(&crt, pemBlob.Get(), pemBlob.Size());
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
    int                  ret {};
    size_t               len;
    const unsigned char* end        = dn.Get() + dn.Size();
    unsigned char*       currentPos = const_cast<aos::Array<uint8_t>&>(dn).Get();

    if ((ret = mbedtls_asn1_get_tag(&currentPos, end, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    const unsigned char* sequenceEnd = currentPos + len;

    while (currentPos < sequenceEnd) {
        if ((ret = mbedtls_asn1_get_tag(&currentPos, sequenceEnd, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SET))
            != 0) {
            return AOS_ERROR_WRAP(ret);
        }

        const unsigned char* setEnd = currentPos + len;

        if ((ret = mbedtls_asn1_get_tag(&currentPos, setEnd, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE))
            != 0) {
            return AOS_ERROR_WRAP(ret);
        }

        const unsigned char* seqEnd = currentPos + len;

        if ((ret = mbedtls_asn1_get_tag(&currentPos, seqEnd, &len, MBEDTLS_ASN1_OID)) != 0) {
            return AOS_ERROR_WRAP(ret);
        }

        auto oid {aos::Array<uint8_t> {currentPos, len}};

        auto err = GetOIDString(oid, result);
        if (err != aos::ErrorEnum::eNone) {
            return err;
        }

        currentPos += len;

        unsigned char tag = *currentPos;

        if (tag != MBEDTLS_ASN1_UTF8_STRING && tag != MBEDTLS_ASN1_PRINTABLE_STRING) {
            return aos::ErrorEnum::eInvalidArgument;
        }

        if ((ret = mbedtls_asn1_get_tag(&currentPos, seqEnd, &len, tag)) != 0) {
            return AOS_ERROR_WRAP(ret);
        }

        result.Insert(
            result.end(), reinterpret_cast<const char*>(currentPos), reinterpret_cast<const char*>(currentPos) + len);
        result.Append(", ");

        currentPos += len;
    }

    if (!result.IsEmpty()) {
        // Remove the last two characters (", ")
        result.Resize(result.Size() - 2);
    }

    return aos::ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

aos::Error MbedTLSCryptoProvider::ParseX509Certs(mbedtls_x509_crt* currentCrt, aos::crypto::x509::Certificate& cert)
{
    GetX509CertData(cert, currentCrt);

    return GetX509CertExtensions(cert, currentCrt);
}

void MbedTLSCryptoProvider::GetX509CertData(aos::crypto::x509::Certificate& cert, mbedtls_x509_crt* crt)
{
    cert.mSubject.Resize(crt->subject_raw.len);
    memcpy(cert.mSubject.Get(), crt->subject_raw.p, crt->subject_raw.len);

    cert.mIssuer.Resize(crt->issuer_raw.len);
    memcpy(cert.mIssuer.Get(), crt->issuer_raw.p, crt->issuer_raw.len);

    cert.mSerial.Resize(crt->serial.len);
    memcpy(cert.mSerial.Get(), crt->serial.p, crt->serial.len);
}

aos::Error MbedTLSCryptoProvider::GetX509CertExtensions(aos::crypto::x509::Certificate& cert, mbedtls_x509_crt* crt)
{
    mbedtls_asn1_buf      buf = crt->v3_ext;
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

aos::Error MbedTLSCryptoProvider::GetOIDString(aos::Array<uint8_t>& oid, aos::String& result)
{
    mbedtls_asn1_buf oidBuf;
    oidBuf.p   = oid.Get();
    oidBuf.len = oid.Size();

    const char* shortName {};

    int ret = mbedtls_oid_get_attr_short_name(&oidBuf, &shortName);
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    if (shortName == nullptr) {
        return aos::ErrorEnum::eNone;
    }

    result.Append(shortName).Append("=");

    return aos::ErrorEnum::eNone;
}

} // namespace crypto
} // namespace aos
