// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Standalone test to verify the decoder depth limit.
// Compile with: gcc -O1 -g -DNO_LOGGING -DREFCOUNT_ATOMIC_DONTCARE -D__STDC_NO_ATOMICS__=1
//   -I uamqp/inc -I c-utility/inc -I c-utility/pal/generic
//   -I deps/azure-macro-utils-c/inc -I deps/umock-c/inc -I deps/c-logging/v2/inc
//   test_depth_limit.c amqpvalue.o -o test_depth_limit

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "azure_uamqp_c/amqpvalue.h"

static int value_decoded_count = 0;
static void on_value_decoded(void *ctx, AMQP_VALUE val) { (void)ctx; (void)val; value_decoded_count++; }

// Test 1: A simple described value at depth 1 should work.
int test_normal_described_value_works(void)
{
    // 0x00 = descriptor constructor, 0x40 = null (descriptor), 0x40 = null (value)
    unsigned char buf[] = { 0x00, 0x40, 0x40 };

    value_decoded_count = 0;
    AMQPVALUE_DECODER_HANDLE dec = amqpvalue_decoder_create(on_value_decoded, NULL);
    if (dec == NULL) { printf("FAIL: decoder create returned NULL\n"); return 1; }

    int result = amqpvalue_decode_bytes(dec, buf, sizeof(buf));
    amqpvalue_decoder_destroy(dec);

    if (result == 0 && value_decoded_count == 1) {
        printf("PASS: normal described value decoded successfully\n");
        return 0;
    } else {
        printf("FAIL: normal described value failed (result=%d, count=%d)\n", result, value_decoded_count);
        return 1;
    }
}

// Test 2: Depth at exactly MAX_DECODER_DEPTH (128) should succeed.
int test_max_depth_accepted(void)
{
    // Root decoder is at depth 0. Each 0x00 creates an inner decoder at depth+1.
    // So 128 consecutive 0x00 bytes create decoders at depths 1..128.
    // With the check `depth > MAX_DECODER_DEPTH` (where MAX_DECODER_DEPTH=128),
    // depth 128 should still be accepted.
    // Payload: 128 x 0x00, then 0x40 (innermost descriptor = null), then 128 x 0x40 (values)
    size_t depth = 128;
    size_t payload_size = depth + 1 + depth;
    unsigned char *buf = (unsigned char *)malloc(payload_size);
    if (buf == NULL) { printf("FAIL: malloc\n"); return 1; }

    memset(buf, 0x00, depth);
    buf[depth] = 0x40;
    memset(buf + depth + 1, 0x40, depth);

    value_decoded_count = 0;
    AMQPVALUE_DECODER_HANDLE dec = amqpvalue_decoder_create(on_value_decoded, NULL);
    if (dec == NULL) { free(buf); printf("FAIL: decoder create returned NULL\n"); return 1; }

    int result = amqpvalue_decode_bytes(dec, buf, payload_size);
    amqpvalue_decoder_destroy(dec);
    free(buf);

    if (result == 0) {
        printf("PASS: depth 128 accepted (result=%d, decoded=%d)\n", result, value_decoded_count);
        return 0;
    } else {
        printf("FAIL: depth 128 was rejected (result=%d)\n", result);
        return 1;
    }
}

// Test 3: Depth just over the limit (129) should be rejected.
int test_depth_just_over_limit_rejected(void)
{
    // 129 consecutive 0x00 bytes + 0x40
    // The 129th inner decoder would be at depth 129 > 128 = MAX_DECODER_DEPTH
    size_t payload_size = 130;
    unsigned char *buf = (unsigned char *)malloc(payload_size);
    if (buf == NULL) { printf("FAIL: malloc\n"); return 1; }

    memset(buf, 0x00, 129);
    buf[129] = 0x40;

    AMQPVALUE_DECODER_HANDLE dec = amqpvalue_decoder_create(on_value_decoded, NULL);
    if (dec == NULL) { free(buf); printf("FAIL: decoder create returned NULL\n"); return 1; }

    int result = amqpvalue_decode_bytes(dec, buf, payload_size);
    amqpvalue_decoder_destroy(dec);
    free(buf);

    if (result != 0) {
        printf("PASS: depth 129 correctly rejected (result=%d)\n", result);
        return 0;
    } else {
        printf("FAIL: depth 129 was NOT rejected\n");
        return 1;
    }
}

// Test 4: Large excessive depth (200) should be rejected.
int test_excessive_depth_rejected(void)
{
    size_t payload_size = 201;
    unsigned char *buf = (unsigned char *)malloc(payload_size);
    if (buf == NULL) { printf("FAIL: malloc\n"); return 1; }

    memset(buf, 0x00, 200);
    buf[200] = 0x40;

    AMQPVALUE_DECODER_HANDLE dec = amqpvalue_decoder_create(on_value_decoded, NULL);
    if (dec == NULL) { free(buf); printf("FAIL: decoder create returned NULL\n"); return 1; }

    int result = amqpvalue_decode_bytes(dec, buf, payload_size);
    amqpvalue_decoder_destroy(dec);
    free(buf);

    if (result != 0) {
        printf("PASS: excessive depth (200) correctly rejected (result=%d)\n", result);
        return 0;
    } else {
        printf("FAIL: excessive depth (200) was NOT rejected\n");
        return 1;
    }
}

// Test 5: Very large nesting (88000 levels) should be rejected gracefully.
int test_large_nesting_rejected(void)
{
    size_t payload_size = 88001;
    unsigned char *buf = (unsigned char *)malloc(payload_size);
    if (buf == NULL) { printf("FAIL: malloc\n"); return 1; }

    memset(buf, 0x00, 88000);
    buf[88000] = 0x40;

    AMQPVALUE_DECODER_HANDLE dec = amqpvalue_decoder_create(on_value_decoded, NULL);
    if (dec == NULL) { free(buf); printf("FAIL: decoder create returned NULL\n"); return 1; }

    int result = amqpvalue_decode_bytes(dec, buf, payload_size);
    amqpvalue_decoder_destroy(dec);
    free(buf);

    if (result != 0) {
        printf("PASS: large nesting (88000 bytes) rejected without crash (result=%d)\n", result);
        return 0;
    } else {
        printf("FAIL: large nesting (88000 bytes) was NOT rejected\n");
        return 1;
    }
}

int main(void)
{
    int failures = 0;
    printf("=== Testing decoder depth limit ===\n");
    failures += test_normal_described_value_works();
    failures += test_max_depth_accepted();
    failures += test_depth_just_over_limit_rejected();
    failures += test_excessive_depth_rejected();
    failures += test_large_nesting_rejected();
    printf("=== Results: %d failures ===\n", failures);
    return failures;
}
