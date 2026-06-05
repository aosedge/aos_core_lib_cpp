# Crypto Module Documentation

This document describes the crypto module in the AOS Core Library, including its interfaces, architecture, and usage.

## Overview

The crypto module provides a comprehensive set of interfaces for cryptographic operations including hashing, encryption,
digital signatures, certificate handling, ASN.1 parsing, and random number generation.

### Crypto Module Implementations

The crypto module implements a dual-backend architecture, providing two complete implementations:

- **MbedTLS Implementation**: lightweight, embedded-focused cryptographic library
- **OpenSSL Implementation**: full-featured, enterprise-grade cryptographic library

### Build Configuration

The implementation is selected at build time using CMake flags:

- `WITH_MBEDTLS=ON` (default): Enables MbedTLS implementation
- `WITH_OPENSSL=ON`: Enables OpenSSL implementation
- Both can be enabled simultaneously for testing/comparison purposes

### Default CryptoProvider

The module provides a unified `CryptoProviderItf` that abstracts the underlying implementation.
The`DefaultCryptoProvider` alias is used to instantiate the interface with the same name regardless of the build
configuration.

## Interface Descriptions

### UUIDItf

UUID generation interface for creating unique identifiers.

**Methods:**

- `CreateUUIDv4()`: Creates a UUID version 4
- `CreateUUIDv5(space, name)`: Creates a UUID version 5 based on namespace and name

### AES Interfaces

AES encryption and decryption interfaces for AES cryptographic operations.

#### AESCipherItf

AES cipher interface for encrypting and decrypting data blocks using the AES algorithm.

**Methods:**

- `EncryptBlock(input, output)`: Encrypts a data block
- `DecryptBlock(input, output)`: Decrypts a data block
- `Finalize(output)`: Finalizes encryption/decryption

#### AESEncoderDecoderItf

Factory interface for creating AES encoders and decoders.

**Methods:**

- `CreateAESEncoder(mode, key, iv)`: Creates a new AES encoder
- `CreateAESDecoder(mode, key, iv)`: Creates a new AES decoder

### Hash Interfaces

Hash computation interfaces for cryptographic hashing operations.

#### HashItf

Interface for hash computation operations.

**Methods:**

- `Update(data)`: Updates hash with input data
- `Finalize(hash)`: Finalizes hash computation and returns result

#### HasherItf

Hash factory interface for creating hash instances.

**Methods:**

- `CreateHash(algorithm)`: Creates a hash instance for specified algorithm

**Types:**

- `Hash`: Hash algorithm type used to specify which hash function to use. Supported algorithms: `eSHA1`, `eSHA224`,
  `eSHA256`, `eSHA384`, `eSHA512`, `eSHA512_224`, `eSHA512_256`, `eSHA3_224`, `eSHA3_256`, `eNONE`

### Private/Public Key Interfaces

Private/Public key interfaces for asymmetric cryptographic operations. Currently supports RSA and ECDSA keys.

#### PublicKeyItf

Public key interface for key type identification and comparison.

**Methods:**

- `GetKeyType()`: Returns the type of the public key
- `IsEqual(pubKey)`: Tests whether current key is equal to the provided one

#### PrivateKeyItf

Private key operations interface for signing and decryption.

**Methods:**

- `GetPublic()`: Returns public part of a private key
- `Sign(digest, options, signature)`: Calculates a signature of a given digest
- `Decrypt(cipher, options, result)`: Decrypts a cipher message

**Types:**

- `SignOptions`: Signature options contain optional hash algorithm to be used
- `DecryptionOptions`: Decryption options that can be either `PKCS1v15DecryptionOptions` or `OAEPDecryptionOptions`
  (used with RSA keys only)

#### RSAPublicKey

RSA public key structure containing modulus and exponent.

**Methods:**

- `GetN()`: Returns RSA modulus
- `GetE()`: Returns RSA public exponent

#### ECDSAPublicKey

ECDSA public key structure containing curve parameters and point.

**Methods:**

- `GetECParamsOID()`: Returns ECDSA parameters OID
- `GetECPoint()`: Returns ECDSA point

### ASN.1 Interfaces

ASN.1 interfaces for parsing and encoding ASN.1 data structures.

#### ASN1HandlerItf

ASN.1 element handler interface for processing ASN.1 elements.

**Methods:**

- `OnASN1Element(value)`: Handles ASN.1 element

#### ASN1DecoderItf

ASN.1 decoder interface for parsing ASN.1 structures.

**Methods:**

- `ReadStruct(data, opt, asn1reader)`: Reads ASN.1 SEQUENCE
- `ReadSet(data, opt, asn1reader)`: Reads ASN.1 SET
- `ReadSequence(data, opt, asn1reader)`: Reads ASN.1 SEQUENCE
- `ReadInteger(data, opt, value)`: Reads ASN.1 INTEGER
- `ReadBigInt(data, opt, result)`: Reads ASN.1 big integer
- `ReadOID(data, opt, oid)`: Reads ASN.1 object identifier
- `ReadAID(data, opt, aid)`: Reads algorithm identifier
- `ReadOctetString(data, opt, result)`: Reads ASN.1 octet string
- `ReadRawValue(data, opt, result)`: Reads raw ASN.1 value

**Types:**

- `ASN1Value`: ASN.1 value structure
- `ASN1ParseResult`: ASN.1 parsing result
- `ASN1ParseOptions`: ASN.1 parsing options
- `ObjectIdentifier`: ASN.1 object identifier type
- `Extension`: ASN.1 extension structure

### RandomItf

Random number generator interface for generating random data.

**Methods:**

- `RandInt(maxValue)`: Generates random integer in range [0..maxValue]
- `RandBuffer(buffer, size)`: Generates random buffer

### CertLoaderItf

Interface for loading certificates and private keys from URLs. Currently supports file URLs (file: prefix) and PKCS#11
URLs (pkcs11: prefix).

**Methods:**

- `LoadCertsChainByURL(url)`: Loads certificate chain by URL
- `LoadPrivKeyByURL(url)`: Loads private key by URL

### CryptoHelperItf

Interface for decrypting and validating cloud data.

**Methods:**

- `Decrypt(encryptedPath, decryptedPath, decryptionInfo)`: Decrypts file using decryption information
- `ValidateSigns(decryptedPath, signs, chains, certs)`: Validates digital signatures
- `DecryptMetadata(input, output)`: Decrypts metadata from binary buffer

### ProviderItf (x509)

X.509 certificate provider interface for certificate operations.

**Methods:**

- `CreateCertificate(templ, parent, privKey, pemCert)`: Creates certificate from template
- `CreateClientCert(csr, caKey, caCert, serial, clientCert)`: Creates client certificate
- `PEMToX509Certs(pemBlob, resultCerts)`: Reads certificates from PEM
- `X509CertToPEM(certificate, dst)`: Serializes certificate to PEM
- `PEMToX509PrivKey(pemBlob)`: Reads private key from PEM
- `DERToX509Cert(derBlob, resultCert)`: Reads certificate from DER
- `CreateCSR(templ, privKey, pemCSR)`: Creates certificate signing request
- `ASN1EncodeDN(commonName, result)`: Constructs distinguished name
- `ASN1DecodeDN(dn, result)`: Returns text representation of DN
- `ASN1EncodeObjectIds(src, asn1Value)`: Encodes object identifiers
- `ASN1EncodeBigInt(number, asn1Value)`: Encodes big integer
- `ASN1EncodeDERSequence(items, asn1Value)`: Creates ASN.1 sequence
- `ASN1DecodeOctetString(src, dst)`: Decodes octet string
- `ASN1DecodeOID(inOID, dst)`: Decodes object identifier
- `Verify(pubKey, hashFunc, padding, digest, signature)`: Verifies digital signature
- `Verify(rootCerts, intermCerts, options, cert)`: Verifies certificate chain

**Note:** Methods starting with "ASN1" will be relocated to a dedicated ASN1 interface in future versions.

**Types:**

- `Certificate`: X.509 certificate structure
- `CSR`: Certificate signing request structure
- `CertificateChain`: Certificate chain type
- `PaddingType`: Padding types (PKCS1v1.5, PSS, None)
- `VerifyOptions`: Certificate verification options

## Utility Functions

### URL Parsing Functions

- `ParseURLScheme(url, scheme)`: Parses scheme part of URL
- `ParseFileURL(url, path)`: Parses URL with file scheme
- `ParsePKCS11URL(url, library, token, label, id, userPin)`: Parses PKCS#11 URL

### PKCS#11 Functions

- `EncodePKCS11ID(id, idStr)`: Encodes PKCS#11 ID to percent-encoded string
- `DecodeToPKCS11ID(idStr, id)`: Decodes PKCS#11 ID from percent-encoded string

### ASN.1 Utility Functions

- `ConvertTimeToASN1Str(time)`: Converts time to ASN.1 GeneralizedTime string

## Summary

This document provided information about all the crypto interfaces available in the AOS Core Library crypto module:

`UUIDItf`, `AESCipherItf`, `AESEncoderDecoderItf`, `HashItf`, `HasherItf`, `PublicKeyItf`, `PrivateKeyItf`,
`RSAPublicKey`, `ECDSAPublicKey`, `ProviderItf` (x509), `ASN1HandlerItf`, `ASN1DecoderItf`, `RandomItf`,
`CertLoaderItf`, `CryptoHelperItf`

### Example Usage

```cpp
// Create crypto provider
auto cryptoProvider = DefaultCryptoProvider();

// Get random number generator
auto randomItf = cryptoProvider->GetRandom();

// Generate random integer
auto randomValue = randomItf->RandInt(100);

// Generate random buffer
Array<uint8_t> buffer;
randomItf->RandBuffer(buffer, 32);
```
