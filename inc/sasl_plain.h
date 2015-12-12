// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef SASL_PLAIN_H
#define SASL_PLAIN_H

#include "sasl_mechanism.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

	typedef struct SASL_PLAIN_CONFIG_TAG
	{
		const char* authcid;
		const char* passwd;
	} SASL_PLAIN_CONFIG;

	extern CONCRETE_SASL_MECHANISM_HANDLE saslplain_create(void* config);
	extern void saslplain_destroy(CONCRETE_SASL_MECHANISM_HANDLE sasl_mechanism_concrete_handle);
	extern int saslplain_get_init_bytes(CONCRETE_SASL_MECHANISM_HANDLE sasl_mechanism_concrete_handle, SASL_MECHANISM_BYTES* init_bytes);
	extern const char* saslplain_get_mechanism_name(CONCRETE_SASL_MECHANISM_HANDLE sasl_mechanism);
	extern const SASL_MECHANISM_INTERFACE_DESCRIPTION* saslplain_get_interface(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SASL_PLAIN_H */
