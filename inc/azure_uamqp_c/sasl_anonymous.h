// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef SASL_ANONYMOUS_H
#define SASL_ANONYMOUS_H

#include "azure_uamqp_c/sasl_mechanism.h"
#include "azure_c_shared_utility/xlogging.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "azure_c_shared_utility/umock_c_prod.h"

	MOCKABLE_FUNCTION(, CONCRETE_SASL_MECHANISM_HANDLE, saslanonymous_create, void*, config);
	MOCKABLE_FUNCTION(, void, saslanonymous_destroy, CONCRETE_SASL_MECHANISM_HANDLE, concrete_sasl_mechanism);
	MOCKABLE_FUNCTION(, int, saslanonymous_get_init_bytes, CONCRETE_SASL_MECHANISM_HANDLE, concrete_sasl_mechanism, SASL_MECHANISM_BYTES*, init_bytes);
	MOCKABLE_FUNCTION(, const char*, saslanonymous_get_mechanism_name, CONCRETE_SASL_MECHANISM_HANDLE, concrete_sasl_mechanism);
	MOCKABLE_FUNCTION(, int, saslanonymous_challenge, CONCRETE_SASL_MECHANISM_HANDLE, concrete_sasl_mechanism, const SASL_MECHANISM_BYTES*, challenge_bytes, SASL_MECHANISM_BYTES*, response_bytes);
	MOCKABLE_FUNCTION(, const SASL_MECHANISM_INTERFACE_DESCRIPTION*, saslanonymous_get_interface);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SASL_ANONYMOUS_H */
