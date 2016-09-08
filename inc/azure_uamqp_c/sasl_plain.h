// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef SASL_PLAIN_H
#define SASL_PLAIN_H

#include "azure_uamqp_c/sasl_mechanism.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "azure_c_shared_utility/umock_c_prod.h"

	typedef struct SASL_PLAIN_CONFIG_TAG
	{
		const char* authcid;
		const char* passwd;
		const char* authzid;
	} SASL_PLAIN_CONFIG;

	MOCKABLE_FUNCTION(, CONCRETE_SASL_MECHANISM_HANDLE, saslplain_create, void*, config);
	MOCKABLE_FUNCTION(, void, saslplain_destroy, CONCRETE_SASL_MECHANISM_HANDLE, sasl_mechanism_concrete_handle);
	MOCKABLE_FUNCTION(, int, saslplain_get_init_bytes, CONCRETE_SASL_MECHANISM_HANDLE, sasl_mechanism_concrete_handle, SASL_MECHANISM_BYTES*, init_bytes);
	MOCKABLE_FUNCTION(, const char*, saslplain_get_mechanism_name, CONCRETE_SASL_MECHANISM_HANDLE, sasl_mechanism);
	MOCKABLE_FUNCTION(, int, saslplain_challenge, CONCRETE_SASL_MECHANISM_HANDLE, concrete_sasl_mechanism, const SASL_MECHANISM_BYTES*, challenge_bytes, SASL_MECHANISM_BYTES*, response_bytes);
	MOCKABLE_FUNCTION(, const SASL_MECHANISM_INTERFACE_DESCRIPTION*, saslplain_get_interface);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SASL_PLAIN_H */
