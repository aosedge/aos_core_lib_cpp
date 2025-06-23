/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <openssl/asn1t.h>

// clang-format behaves weird with asn1 macros. Had to separate them in a
// dedicated header so it didn't pollute the rest of the code.

typedef STACK_OF(ASN1_OBJECT) SEQ_OID;

ASN1_ITEM_TEMPLATE(SEQ_OID)
// cppcheck-suppress unknownMacro
= ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, SeqOID, ASN1_OBJECT) ASN1_ITEM_TEMPLATE_END(SEQ_OID)

    IMPLEMENT_ASN1_FUNCTIONS(SEQ_OID) IMPLEMENT_ASN1_ALLOC_FUNCTIONS(ASN1_SEQUENCE_ANY)
