// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "azure_uamqp_c/header_detect_io.h"
#include "azure_uamqp_c/amqpalloc.h"

typedef enum IO_STATE_TAG
{
	IO_STATE_NOT_OPEN,
	IO_STATE_OPENING_UNDERLYING_IO,
	IO_STATE_WAIT_FOR_HEADER,
	IO_STATE_OPEN,
	IO_STATE_CLOSING,
	IO_STATE_ERROR
} IO_STATE;

typedef struct HEADER_DETECT_IO_INSTANCE_TAG
{
	XIO_HANDLE underlying_io;
	size_t header_pos;
	IO_STATE io_state;
	ON_IO_OPEN_COMPLETE on_io_open_complete;
	ON_IO_CLOSE_COMPLETE on_io_close_complete;
	ON_IO_ERROR on_io_error;
	ON_BYTES_RECEIVED on_bytes_received;
	void* on_io_open_complete_context;
	void* on_io_close_complete_context;
    void* on_io_error_context;
    void* on_bytes_received_context;
} HEADER_DETECT_IO_INSTANCE;

static const unsigned char amqp_header[] = { 'A', 'M', 'Q', 'P', 0, 1, 0, 0 };

static void indicate_error(HEADER_DETECT_IO_INSTANCE* header_detect_io_instance)
{
	if (header_detect_io_instance->on_io_error != NULL)
	{
		header_detect_io_instance->on_io_error(header_detect_io_instance->on_io_error_context);
	}
}

static void indicate_open_complete(HEADER_DETECT_IO_INSTANCE* header_detect_io_instance, IO_OPEN_RESULT open_result)
{
	if (header_detect_io_instance->on_io_open_complete != NULL)
	{
		header_detect_io_instance->on_io_open_complete(header_detect_io_instance->on_io_open_complete_context, open_result);
	}
}

static void indicate_close_complete(HEADER_DETECT_IO_INSTANCE* header_detect_io_instance)
{
    if (header_detect_io_instance->on_io_close_complete != NULL)
    {
        header_detect_io_instance->on_io_close_complete(header_detect_io_instance->on_io_close_complete_context);
    }
}

static void on_underlying_io_error(void* context);
static void on_send_complete_close(void* context, IO_SEND_RESULT send_result)
{
    (void)send_result;
    on_underlying_io_error(context);
}

static void on_underlying_io_bytes_received(void* context, const unsigned char* buffer, size_t size)
{
	HEADER_DETECT_IO_INSTANCE* header_detect_io_instance = (HEADER_DETECT_IO_INSTANCE*)context;

	while (size > 0)
	{
		switch (header_detect_io_instance->io_state)
		{
		default:
			break;

		case IO_STATE_WAIT_FOR_HEADER:
			if (amqp_header[header_detect_io_instance->header_pos] != buffer[0])
			{
                /* Send expected header, then close as per spec.  We do not care if we fail */
                (void)xio_send(header_detect_io_instance->underlying_io, amqp_header, sizeof(amqp_header), on_send_complete_close, context);

                header_detect_io_instance->io_state = IO_STATE_NOT_OPEN;
				indicate_open_complete(header_detect_io_instance, IO_OPEN_ERROR);
				size = 0;
			}
			else
			{
				header_detect_io_instance->header_pos++;
				size--;
				buffer++;
				if (header_detect_io_instance->header_pos == sizeof(amqp_header))
				{
					if (xio_send(header_detect_io_instance->underlying_io, amqp_header, sizeof(amqp_header), NULL, NULL) != 0)
					{
						header_detect_io_instance->io_state = IO_STATE_NOT_OPEN;
						indicate_open_complete(header_detect_io_instance, IO_OPEN_ERROR);
					}
					else
					{
						header_detect_io_instance->io_state = IO_STATE_OPEN;
						indicate_open_complete(header_detect_io_instance, IO_OPEN_OK);
					}
				}
			}
			break;

		case IO_STATE_OPEN:
			header_detect_io_instance->on_bytes_received(header_detect_io_instance->on_bytes_received_context, buffer, size);
			size = 0;
			break;
		}
	}
}

static void on_underlying_io_close_complete(void* context)
{
	HEADER_DETECT_IO_INSTANCE* header_detect_io_instance = (HEADER_DETECT_IO_INSTANCE*)context;

	switch (header_detect_io_instance->io_state)
	{
	default:
		break;

    case IO_STATE_CLOSING:
        header_detect_io_instance->io_state = IO_STATE_NOT_OPEN;
        indicate_close_complete(header_detect_io_instance);
        break;

	case IO_STATE_WAIT_FOR_HEADER:
	case IO_STATE_OPENING_UNDERLYING_IO:
		header_detect_io_instance->io_state = IO_STATE_NOT_OPEN;
		indicate_open_complete(header_detect_io_instance, IO_OPEN_ERROR);
		break;
	}
}

static void on_underlying_io_open_complete(void* context, IO_OPEN_RESULT open_result)
{
	HEADER_DETECT_IO_INSTANCE* header_detect_io_instance = (HEADER_DETECT_IO_INSTANCE*)context;

	switch (header_detect_io_instance->io_state)
	{
	default:
		break;

	case IO_STATE_OPENING_UNDERLYING_IO:
		if (open_result == IO_OPEN_OK)
		{
			header_detect_io_instance->io_state = IO_STATE_WAIT_FOR_HEADER;
		}
		else
		{
			if (xio_close(header_detect_io_instance->underlying_io, on_underlying_io_close_complete, header_detect_io_instance) != 0)
			{
				header_detect_io_instance->io_state = IO_STATE_NOT_OPEN;
				indicate_open_complete(header_detect_io_instance, IO_OPEN_ERROR);
			}
		}

		break;
	}
}

static void on_underlying_io_error(void* context)
{
	HEADER_DETECT_IO_INSTANCE* header_detect_io_instance = (HEADER_DETECT_IO_INSTANCE*)context;

	switch (header_detect_io_instance->io_state)
	{
	default:
		break;

	case IO_STATE_WAIT_FOR_HEADER:
	case IO_STATE_OPENING_UNDERLYING_IO:
		header_detect_io_instance->io_state = IO_STATE_NOT_OPEN;
		indicate_open_complete(header_detect_io_instance, IO_OPEN_ERROR);
		break;

	case IO_STATE_OPEN:
		header_detect_io_instance->io_state = IO_STATE_ERROR;
		indicate_error(header_detect_io_instance);
		break;
	}
}

CONCRETE_IO_HANDLE headerdetectio_create(void* io_create_parameters)
{
	HEADER_DETECT_IO_INSTANCE* result;

	if (io_create_parameters == NULL)
	{
		result = NULL;
	}
	else
	{
		HEADERDETECTIO_CONFIG* header_detect_io_config = (HEADERDETECTIO_CONFIG*)io_create_parameters;
		result = (HEADER_DETECT_IO_INSTANCE*)amqpalloc_malloc(sizeof(HEADER_DETECT_IO_INSTANCE));
		if (result != NULL)
		{
			result->underlying_io = header_detect_io_config->underlying_io;
			result->on_io_open_complete = NULL;
            result->on_io_close_complete = NULL;
			result->on_io_error = NULL;
			result->on_bytes_received = NULL;
			result->on_io_open_complete_context = NULL;
            result->on_io_close_complete_context = NULL;
            result->on_io_error_context = NULL;
            result->on_bytes_received_context = NULL;

			result->io_state = IO_STATE_NOT_OPEN;
		}
	}

	return result;
}

void headerdetectio_destroy(CONCRETE_IO_HANDLE header_detect_io)
{
	if (header_detect_io != NULL)
	{
		HEADER_DETECT_IO_INSTANCE* header_detect_io_instance = (HEADER_DETECT_IO_INSTANCE*)header_detect_io;
		(void)headerdetectio_close(header_detect_io, NULL, NULL);
		xio_destroy(header_detect_io_instance->underlying_io);
		amqpalloc_free(header_detect_io);
	}
}

int headerdetectio_open(CONCRETE_IO_HANDLE header_detect_io, ON_IO_OPEN_COMPLETE on_io_open_complete, void* on_io_open_complete_context, ON_BYTES_RECEIVED on_bytes_received, void* on_bytes_received_context, ON_IO_ERROR on_io_error, void* on_io_error_context)
{
	int result;

	if (header_detect_io == NULL)
	{
		result = __LINE__;
	}
	else
	{
		HEADER_DETECT_IO_INSTANCE* header_detect_io_instance = (HEADER_DETECT_IO_INSTANCE*)header_detect_io;

        if (header_detect_io_instance->io_state != IO_STATE_NOT_OPEN &&
            header_detect_io_instance->io_state != IO_STATE_OPEN)
        {
            result = __LINE__;
        }
		else
		{
			header_detect_io_instance->on_bytes_received = on_bytes_received;
			header_detect_io_instance->on_io_open_complete = on_io_open_complete;
			header_detect_io_instance->on_io_error = on_io_error;
            header_detect_io_instance->on_bytes_received_context = on_bytes_received_context;
            header_detect_io_instance->on_io_open_complete_context = on_io_open_complete_context;
            header_detect_io_instance->on_io_error_context = on_io_error_context;

            if (header_detect_io_instance->io_state == IO_STATE_OPEN)
            {
                indicate_open_complete(header_detect_io_instance, IO_OPEN_OK);
                result = 0;
            }
            else
            {
                header_detect_io_instance->header_pos = 0;
                header_detect_io_instance->io_state = IO_STATE_OPENING_UNDERLYING_IO;

                if (xio_open(header_detect_io_instance->underlying_io, on_underlying_io_open_complete, header_detect_io_instance, on_underlying_io_bytes_received, header_detect_io_instance, on_underlying_io_error, header_detect_io_instance) != 0)
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

	return result;
}

int headerdetectio_close(CONCRETE_IO_HANDLE header_detect_io, ON_IO_CLOSE_COMPLETE on_io_close_complete, void* callback_context)
{
	int result;

	if (header_detect_io == NULL)
	{
		result = __LINE__;
	}
	else
	{
		HEADER_DETECT_IO_INSTANCE* header_detect_io_instance = (HEADER_DETECT_IO_INSTANCE*)header_detect_io;

		if ((header_detect_io_instance->io_state == IO_STATE_NOT_OPEN) ||
			(header_detect_io_instance->io_state == IO_STATE_CLOSING))
		{
			result = __LINE__;
		}
		else
		{
			header_detect_io_instance->io_state = IO_STATE_CLOSING;
            header_detect_io_instance->on_io_close_complete = on_io_close_complete;
            header_detect_io_instance->on_io_close_complete_context = callback_context;

			if (xio_close(header_detect_io_instance->underlying_io, on_underlying_io_close_complete, header_detect_io_instance) != 0)
			{
				result = __LINE__;
			}
			else
			{
				result = 0;
			}
		}
	}

	return result;
}

int headerdetectio_send(CONCRETE_IO_HANDLE header_detect_io, const void* buffer, size_t size, ON_SEND_COMPLETE on_send_complete, void* callback_context)
{
	int result;

	if (header_detect_io == NULL)
	{
		result = __LINE__;
	}
	else
	{
		HEADER_DETECT_IO_INSTANCE* header_detect_io_instance = (HEADER_DETECT_IO_INSTANCE*)header_detect_io;

		if (header_detect_io_instance->io_state != IO_STATE_OPEN)
		{
			result = __LINE__;
		}
		else
		{
			if (xio_send(header_detect_io_instance->underlying_io, buffer, size, on_send_complete, callback_context) != 0)
			{
				result = __LINE__;
			}
			else
			{
				result = 0;
			}
		}
	}

	return result;
}

void headerdetectio_dowork(CONCRETE_IO_HANDLE header_detect_io)
{
	if (header_detect_io != NULL)
	{
		HEADER_DETECT_IO_INSTANCE* header_detect_io_instance = (HEADER_DETECT_IO_INSTANCE*)header_detect_io;

		if ((header_detect_io_instance->io_state != IO_STATE_NOT_OPEN) &&
			(header_detect_io_instance->io_state != IO_STATE_ERROR))
		{
			xio_dowork(header_detect_io_instance->underlying_io);
		}
	}
}

int headerdetectio_setoption(CONCRETE_IO_HANDLE header_detect_io, const char* optionName, const void* value)
{
    int result;

    if (header_detect_io == NULL)
    {
        result = __LINE__;
    }
    else
    {
        HEADER_DETECT_IO_INSTANCE* header_detect_io_instance = (HEADER_DETECT_IO_INSTANCE*)header_detect_io;

        if (header_detect_io_instance->underlying_io == NULL)
        {
            result = __LINE__;
        }
        else
        {
            result = xio_setoption(header_detect_io_instance->underlying_io, optionName, value);
        }
    }

    return result;
}

/*this function will clone an option given by name and value*/
static void* headerdetectio_CloneOption(const char* name, const void* value)
{
    (void)(name, value);
    return NULL;
}

/*this function destroys an option previously created*/
static void headerdetectio_DestroyOption(const char* name, const void* value)
{
    (void)(name, value);
}

static OPTIONHANDLER_HANDLE headerdetectio_retrieveoptions(CONCRETE_IO_HANDLE handle)
{
    OPTIONHANDLER_HANDLE result;
    (void)handle;
    result = OptionHandler_Create(headerdetectio_CloneOption, headerdetectio_DestroyOption, headerdetectio_setoption);
    if (result == NULL)
    {
        LogError("unable to OptionHandler_Create");
        /*return as is*/
    }
    else
    {
        /*insert here work to add the options to "result" handle*/
    }
    return result;
}

static const IO_INTERFACE_DESCRIPTION header_detect_io_interface_description =
{
    headerdetectio_retrieveoptions,
	headerdetectio_create,
	headerdetectio_destroy,
	headerdetectio_open,
	headerdetectio_close,
	headerdetectio_send,
	headerdetectio_dowork,
    headerdetectio_setoption
};

const IO_INTERFACE_DESCRIPTION* headerdetectio_get_interface_description(void)
{
	return &header_detect_io_interface_description;
}
