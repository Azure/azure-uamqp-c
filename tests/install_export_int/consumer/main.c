// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdint.h>
#include "azure_uamqp_c/amqpvalue.h"
#include "azure_uamqp_c/message.h"
#include "azure_uamqp_c/amqp_definitions.h"

// Not run; this translation unit exists so that the include directories
// exported by the installed uamqp package are exercised by a real compile.
int uamqp_install_export_consumer(void)
{
    AMQP_VALUE value = amqpvalue_create_uint(42u);
    uint32_t result = 0u;

    if (value == NULL)
    {
        return 1;
    }

    if ((amqpvalue_get_uint(value, &result) != 0) || (result != 42u))
    {
        amqpvalue_destroy(value);
        return 1;
    }

    amqpvalue_destroy(value);
    return 0;
}
