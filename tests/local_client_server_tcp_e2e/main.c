// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "testrunnerswitcher.h"

extern void set_port_number(const char* seed);

int main(int argc, char** argv)
{
    (void)argc;
    set_port_number(argv[0]);

    size_t failedTestCount = 0;
    RUN_TEST_SUITE(local_client_server_tcp_e2e, failedTestCount);
    return failedTestCount;
}
