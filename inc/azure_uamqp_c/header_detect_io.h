// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef HEADER_DETECT_IO_H
#define HEADER_DETECT_IO_H

#include "azure_c_shared_utility/xio.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "azure_c_shared_utility/umock_c_prod.h"

	typedef struct HEADERDETECTIO_CONFIG_TAG
	{
		XIO_HANDLE underlying_io;
	} HEADERDETECTIO_CONFIG;

	MOCKABLE_FUNCTION(, CONCRETE_IO_HANDLE, headerdetectio_create, void*, io_create_parameters);
	MOCKABLE_FUNCTION(, void, headerdetectio_destroy, CONCRETE_IO_HANDLE, header_detect_io);
	MOCKABLE_FUNCTION(, int, headerdetectio_open, CONCRETE_IO_HANDLE, header_detect_io, ON_IO_OPEN_COMPLETE, on_io_open_complete, void*, on_io_open_complete_context, ON_BYTES_RECEIVED, on_bytes_received, void*, on_bytes_received_context, ON_IO_ERROR, on_io_error, void*, on_io_error_context);
	MOCKABLE_FUNCTION(, int, headerdetectio_close, CONCRETE_IO_HANDLE, header_detect_io, ON_IO_CLOSE_COMPLETE, on_io_close_complete, void*, callback_context);
	MOCKABLE_FUNCTION(, int, headerdetectio_send, CONCRETE_IO_HANDLE, header_detect_io, const void*, buffer, size_t, size, ON_SEND_COMPLETE, on_send_complete, void*, callback_context);
	MOCKABLE_FUNCTION(, void, headerdetectio_dowork, CONCRETE_IO_HANDLE, header_detect_io);
    MOCKABLE_FUNCTION(, int, headerdetectio_setoption, CONCRETE_IO_HANDLE, socket_io, const char*, optionName, const void*, value);
    
	MOCKABLE_FUNCTION(, const IO_INTERFACE_DESCRIPTION*, headerdetectio_get_interface_description);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* HEADER_DETECT_IO_H */
