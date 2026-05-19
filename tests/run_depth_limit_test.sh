#!/bin/bash
set -e

apt-get update -qq && apt-get install -y -qq gcc python3 > /dev/null 2>&1

echo "=== Compiling amqpvalue.c ==="
gcc -O1 -g \
  -DNO_LOGGING -DREFCOUNT_ATOMIC_DONTCARE -D__STDC_NO_ATOMICS__=1 \
  -I uamqp/inc \
  -I c-utility/inc \
  -I c-utility/pal/generic \
  -I deps/azure-macro-utils-c/inc \
  -I deps/umock-c/inc \
  -I deps/c-logging/v2/inc \
  -c uamqp/src/amqpvalue.c -o /tmp/amqpvalue.o

echo "=== Compiling test_depth_limit.c ==="
gcc -O1 -g \
  -DNO_LOGGING -DREFCOUNT_ATOMIC_DONTCARE -D__STDC_NO_ATOMICS__=1 \
  -I uamqp/inc \
  -I c-utility/inc \
  -I c-utility/pal/generic \
  -I deps/azure-macro-utils-c/inc \
  -I deps/umock-c/inc \
  -I deps/c-logging/v2/inc \
  uamqp/tests/test_depth_limit.c /tmp/amqpvalue.o -o /tmp/test_depth_limit

echo "=== Running depth limit tests ==="
/tmp/test_depth_limit

echo ""
echo "=== Generating large nesting payload (88001 bytes) ==="
python3 -c "
import sys
sys.stdout.buffer.write(b'\x00' * 88000 + b'\x40')
" > /tmp/large_nesting.bin
ls -la /tmp/large_nesting.bin

echo "=== Compiling large nesting driver ==="
cat > /tmp/large_nesting_driver.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "azure_uamqp_c/amqpvalue.h"

static void on_value_decoded(void *ctx, AMQP_VALUE val) { (void)ctx; (void)val; }

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("open"); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    unsigned char *buf = malloc((size_t)sz);
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { perror("fread"); return 1; }
    fclose(f);

    fprintf(stderr, "feeding %ld bytes\n", sz);

    AMQPVALUE_DECODER_HANDLE dec = amqpvalue_decoder_create(on_value_decoded, NULL);
    int r = amqpvalue_decode_bytes(dec, buf, (size_t)sz);
    fprintf(stderr, "decode returned: %d\n", r);
    amqpvalue_decoder_destroy(dec);
    free(buf);

    if (r != 0) {
        printf("PASS: large nesting payload rejected\n");
        return 0;
    } else {
        printf("FAIL: large nesting payload was accepted\n");
        return 1;
    }
}
EOF

gcc -O1 -g \
  -DNO_LOGGING -DREFCOUNT_ATOMIC_DONTCARE -D__STDC_NO_ATOMICS__=1 \
  -I uamqp/inc \
  -I c-utility/inc \
  -I c-utility/pal/generic \
  -I deps/azure-macro-utils-c/inc \
  -I deps/umock-c/inc \
  -I deps/c-logging/v2/inc \
  /tmp/large_nesting_driver.c /tmp/amqpvalue.o -o /tmp/large_nesting_driver

echo "=== Running large nesting payload with 8MB stack limit ==="
ulimit -s 8192
/tmp/large_nesting_driver /tmp/large_nesting.bin

echo ""
echo "=== ALL TESTS PASSED ==="
