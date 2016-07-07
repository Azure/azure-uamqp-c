// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "azure_uamqp_c/cbs.h"
#include "azure_uamqp_c/amqp_management.h"
#include "azure_uamqp_c/amqpalloc.h"
#include "azure_uamqp_c/session.h"

typedef struct CBS_INSTANCE_TAG
{
    AMQP_MANAGEMENT_HANDLE amqp_management;
} CBS_INSTANCE;

static int add_string_key_value_pair_to_map(AMQP_VALUE map, const char* key, const char* value)
{
    int result;

    AMQP_VALUE key_value = amqpvalue_create_string(key);
    if (key == NULL)
    {
        result = __LINE__;
    }
    else
    {
        AMQP_VALUE value_value = amqpvalue_create_string(value);
        if (value_value == NULL)
        {
            result = __LINE__;
        }
        else
        {
            if (amqpvalue_set_map_value(map, key_value, value_value) != 0)
            {
                result = __LINE__;
            }
            else
            {
                result = 0;
            }

            amqpvalue_destroy(key_value);
        }

        amqpvalue_destroy(value_value);
    }

    return result;
}

static int set_pending_operation_properties(MESSAGE_HANDLE message)
{
    int result = 0;

    PROPERTIES_HANDLE properties = properties_create();
    if (properties == NULL)
    {
        result = __LINE__;
    }
    else
    {
        AMQP_VALUE reply_to = amqpvalue_create_address_string("cbs");
        if (reply_to == NULL)
        {
            result = __LINE__;
        }
        else
        {
            if (properties_set_reply_to(properties, reply_to) != 0)
            {
                result = __LINE__;
            }

            amqpvalue_destroy(reply_to);
        }

        AMQP_VALUE message_id = amqpvalue_create_message_id_ulong(0x43);
        if (message_id == NULL)
        {
            result = __LINE__;
        }
        else
        {
            if (properties_set_message_id(properties, message_id) != 0)
            {
                result = __LINE__;
            }

            amqpvalue_destroy(message_id);
        }

        if (message_set_properties(message, properties) != 0)
        {
            result = __LINE__;
        }

        properties_destroy(properties);
    }

    return result;
}

CBS_HANDLE cbs_create(SESSION_HANDLE session, ON_AMQP_MANAGEMENT_STATE_CHANGED on_amqp_management_state_changed, void* callback_context)
{
    CBS_INSTANCE* result;

    if (session == NULL)
    {
        result = NULL;
    }
    else
    {
        result = (CBS_INSTANCE*)amqpalloc_malloc(sizeof(CBS_INSTANCE));
        if (result != NULL)
        {
            result->amqp_management = amqpmanagement_create(session, "$cbs", on_amqp_management_state_changed, callback_context);
            if (result->amqp_management == NULL)
            {
                amqpalloc_free(result);
                result = NULL;
            }
        }
    }
    return result;
}

void cbs_destroy(CBS_HANDLE cbs)
{
    if (cbs != NULL)
    {
        (void)cbs_close(cbs);
        amqpmanagement_destroy(cbs->amqp_management);
        amqpalloc_free(cbs);
    }
}

int cbs_open(CBS_HANDLE cbs)
{
    int result;

    if (cbs == NULL)
    {
        result = __LINE__;
    }
    else
    {
        if (amqpmanagement_open(cbs->amqp_management) != 0)
        {
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }

    return result;
}

int cbs_close(CBS_HANDLE cbs)
{
    int result;

    if (cbs == NULL)
    {
        result = __LINE__;
    }
    else
    {
        if (amqpmanagement_close(cbs->amqp_management) != 0)
        {
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }

    return result;
}

int cbs_put_token(CBS_HANDLE cbs, const char* type, const char* audience, const char* token, ON_CBS_OPERATION_COMPLETE on_operation_complete, void* context)
{
    int result;

    if ((cbs == NULL) ||
        (token == NULL))
    {
        result = __LINE__;
    }
    else
    {
        MESSAGE_HANDLE message = message_create();
        if (message == NULL)
        {
            result = __LINE__;
        }
        else
        {
            AMQP_VALUE token_value = amqpvalue_create_string(token);
            if (token_value == NULL)
            {
                message_destroy(message);
                result = __LINE__;
            }
            else
            {
                if (message_set_body_amqp_value(message, token_value) != 0)
                {
                    result = __LINE__;
                }
                else
                {
                    AMQP_VALUE application_properties = amqpvalue_create_map();
                    if (application_properties == NULL)
                    {
                        result = __LINE__;
                    }
                    else
                    {
                        if (add_string_key_value_pair_to_map(application_properties, "name", audience) != 0)
                        {
                            result = __LINE__;
                        }
                        else
                        {
                            if (message_set_application_properties(message, application_properties) != 0)
                            {
                                result = __LINE__;
                            }
                            else
                            {
                                if (set_pending_operation_properties(message) != 0)
                                {
                                    result = __LINE__;
                                }
                                else
                                {
                                    if (amqpmanagement_start_operation(cbs->amqp_management, "put-token", type, NULL, message, (ON_OPERATION_COMPLETE)on_operation_complete, context) != 0)
                                    {
                                        result = __LINE__;
                                    }
                                    else
                                    {
                                        result = 0;
                                    }
                                }
                            }
                        }

                        amqpvalue_destroy(application_properties);
                    }

                    amqpvalue_destroy(token_value);
                }
            }

            message_destroy(message);
        }
    }

    return result;
}

int cbs_delete_token(CBS_HANDLE cbs, const char* type, const char* audience, ON_CBS_OPERATION_COMPLETE on_operation_complete, void* context)
{
    int result;

    if (cbs == NULL)
    {
        result = __LINE__;
    }
    else
    {
        MESSAGE_HANDLE message = message_create();
        if (message == NULL)
        {
            result = __LINE__;
        }
        else
        {
            AMQP_VALUE application_properties = amqpvalue_create_map();
            if (application_properties == NULL)
            {
                result = __LINE__;
            }
            else
            {
                if (add_string_key_value_pair_to_map(application_properties, "name", audience) != 0)
                {
                    result = __LINE__;
                }
                else
                {
                    if (message_set_application_properties(message, application_properties) != 0)
                    {
                        result = __LINE__;
                    }
                    else
                    {
                        if (set_pending_operation_properties(message) != 0)
                        {
                            result = __LINE__;
                        }
                        else
                        {
                            if (amqpmanagement_start_operation(cbs->amqp_management, "delete-token", type, NULL, message, (ON_OPERATION_COMPLETE)on_operation_complete, context) != 0)
                            {
                                result = __LINE__;
                            }
                            else
                            {
                                result = 0;
                            }
                        }
                    }
                }

                amqpvalue_destroy(application_properties);
            }

            message_destroy(message);
        }
    }
    return result;
}

void cbs_set_trace(CBS_HANDLE cbs, bool traceOn)
{
    if (cbs != NULL)
    {
        amqpmanagement_set_trace(cbs->amqp_management, traceOn);
    }
}
