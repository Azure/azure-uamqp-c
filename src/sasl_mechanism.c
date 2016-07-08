// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include "azure_uamqp_c/sasl_mechanism.h"
#include "azure_uamqp_c/amqpalloc.h"

typedef struct SASL_MECHANISM_INSTANCE_TAG
{
	const SASL_MECHANISM_INTERFACE_DESCRIPTION* sasl_mechanism_interface_description;
	CONCRETE_SASL_MECHANISM_HANDLE concrete_sasl_mechanism_handle;
} SASL_MECHANISM_INSTANCE;

SASL_MECHANISM_HANDLE saslmechanism_create(const SASL_MECHANISM_INTERFACE_DESCRIPTION* sasl_mechanism_interface_description, void* sasl_mechanism_create_parameters)
{
	SASL_MECHANISM_INSTANCE* sasl_mechanism_instance;

	/* Codes_SRS_SASL_MECHANISM_01_004: [If the argument sasl_mechanism_interface_description is NULL, saslmechanism_create shall return NULL.] */
	if ((sasl_mechanism_interface_description == NULL) ||
		/* Codes_SRS_SASL_MECHANISM_01_005: [If any sasl_mechanism_interface_description member is NULL, sasl_mechanism_create shall fail and return NULL.] */
		(sasl_mechanism_interface_description->concrete_sasl_mechanism_create == NULL) ||
		(sasl_mechanism_interface_description->concrete_sasl_mechanism_destroy == NULL) ||
		(sasl_mechanism_interface_description->concrete_sasl_mechanism_get_init_bytes == NULL) ||
		(sasl_mechanism_interface_description->concrete_sasl_mechanism_get_mechanism_name == NULL))
	{
		sasl_mechanism_instance = NULL;
	}
	else
	{
		sasl_mechanism_instance = (SASL_MECHANISM_INSTANCE*)amqpalloc_malloc(sizeof(SASL_MECHANISM_INSTANCE));

		if (sasl_mechanism_instance != NULL)
		{
			sasl_mechanism_instance->sasl_mechanism_interface_description = sasl_mechanism_interface_description;

			/* Codes_SRS_SASL_MECHANISM_01_002: [In order to instantiate the concrete SASL mechanism implementation the function concrete_sasl_mechanism_create from the sasl_mechanism_interface_description shall be called, passing the sasl_mechanism_create_parameters to it.] */
			sasl_mechanism_instance->concrete_sasl_mechanism_handle = sasl_mechanism_instance->sasl_mechanism_interface_description->concrete_sasl_mechanism_create((void*)sasl_mechanism_create_parameters);
			if (sasl_mechanism_instance->concrete_sasl_mechanism_handle == NULL)
			{
				/* Codes_SRS_SASL_MECHANISM_01_003: [If the underlying concrete_sasl_mechanism_create call fails, saslmechanism_create shall return NULL.] */
				amqpalloc_free(sasl_mechanism_instance);
				sasl_mechanism_instance = NULL;
			}
		}
	}

	/* Codes_SRS_SASL_MECHANISM_01_001: [saslmechanism_create shall return on success a non-NULL handle to a new SASL mechanism interface.] */
	return (SASL_MECHANISM_HANDLE)sasl_mechanism_instance;
}

void saslmechanism_destroy(SASL_MECHANISM_HANDLE sasl_mechanism)
{
	if (sasl_mechanism != NULL)
	{
		SASL_MECHANISM_INSTANCE* sasl_mechanism_instance = (SASL_MECHANISM_INSTANCE*)sasl_mechanism;

		/* Codes_SRS_SASL_MECHANISM_01_008: [saslmechanism_destroy shall also call the concrete_sasl_mechanism_destroy function that is member of the sasl_mechanism_interface_description argument passed to saslmechanism_create, while passing as argument to concrete_sasl_mechanism_destroy the result of the underlying concrete SASL mechanism handle.] */
		sasl_mechanism_instance->sasl_mechanism_interface_description->concrete_sasl_mechanism_destroy(sasl_mechanism_instance->concrete_sasl_mechanism_handle);

		/* Codes_SRS_SASL_MECHANISM_01_007: [saslmechanism_destroy shall free all resources associated with the SASL mechanism handle.] */
		amqpalloc_free(sasl_mechanism_instance);
	}
}

int saslmechanism_get_init_bytes(SASL_MECHANISM_HANDLE sasl_mechanism, SASL_MECHANISM_BYTES* init_bytes)
{
	int result;

	/* Codes_SRS_SASL_MECHANISM_01_012: [If the argument sasl_mechanism is NULL, saslmechanism_get_init_bytes shall fail and return a non-zero value.] */
	if (sasl_mechanism == NULL)
	{
		result = __LINE__;
	}
	else
	{
		SASL_MECHANISM_INSTANCE* sasl_mechanism_instance = (SASL_MECHANISM_INSTANCE*)sasl_mechanism;

		/* Codes_SRS_SASL_MECHANISM_01_010: [saslmechanism_get_init_bytes shall call the specific concrete_sasl_mechanism_get_init_bytes function specified in saslmechanism_create, passing the init_bytes argument to it.] */
		if (sasl_mechanism_instance->sasl_mechanism_interface_description->concrete_sasl_mechanism_get_init_bytes(sasl_mechanism_instance->concrete_sasl_mechanism_handle, init_bytes) != 0)
		{
			/* Codes_SRS_SASL_MECHANISM_01_013: [If the underlying concrete_sasl_mechanism_get_init_bytes fails, saslmechanism_get_init_bytes shall fail and return a non-zero value.] */
			result = __LINE__;
		}
		else
		{
			/* Codes_SRS_SASL_MECHANISM_01_011: [On success, saslmechanism_get_init_bytes shall return 0.] */
			result = 0;
		}
	}

	return result;
}

const char* saslmechanism_get_mechanism_name(SASL_MECHANISM_HANDLE sasl_mechanism)
{
	const char* result;

	/* Codes_SRS_SASL_MECHANISM_01_016: [If the argument sasl_mechanism is NULL, saslmechanism_get_mechanism_name shall fail and return a non-zero value.] */
	if (sasl_mechanism == NULL)
	{
		result = NULL;
	}
	else
	{
		SASL_MECHANISM_INSTANCE* sasl_mechanism_instance = (SASL_MECHANISM_INSTANCE*)sasl_mechanism;

		/* Codes_SRS_SASL_MECHANISM_01_014: [saslmechanism_get_mechanism_name shall call the specific concrete_sasl_mechanism_get_mechanism_name function specified in saslmechanism_create.] */
		/* Codes_SRS_SASL_MECHANISM_01_015: [On success, saslmechanism_get_mechanism_name shall return a pointer to a string with the mechanism name.] */
		/* Codes_SRS_SASL_MECHANISM_01_017: [If the underlying concrete_sasl_mechanism_get_mechanism_name fails, saslmechanism_get_mechanism_name shall return NULL.] */
		result = sasl_mechanism_instance->sasl_mechanism_interface_description->concrete_sasl_mechanism_get_mechanism_name(sasl_mechanism_instance->concrete_sasl_mechanism_handle);
	}

	return result;
}

int saslmechanism_challenge(SASL_MECHANISM_HANDLE sasl_mechanism, const SASL_MECHANISM_BYTES* challenge_bytes, SASL_MECHANISM_BYTES* response_bytes)
{
	int result;

	/* Codes_SRS_SASL_MECHANISM_01_020: [If the argument sasl_mechanism is NULL, saslmechanism_challenge shall fail and return a non-zero value.] */
	if (sasl_mechanism == NULL)
	{
		result = __LINE__;
	}
	else
	{
		/* Codes_SRS_SASL_MECHANISM_01_018: [saslmechanism_challenge shall call the specific concrete_sasl_mechanism_challenge function specified in saslmechanism_create, while passing the challenge_bytes and response_bytes arguments to it.] */
		if (sasl_mechanism->sasl_mechanism_interface_description->concrete_sasl_mechanism_challenge(sasl_mechanism->concrete_sasl_mechanism_handle, challenge_bytes, response_bytes) != 0)
		{
			/* Codes_SRS_SASL_MECHANISM_01_021: [If the underlying concrete_sasl_mechanism_challenge fails, saslmechanism_challenge shall fail and return a non-zero value.] */
			result = __LINE__;
		}
		else
		{
			/* Codes_SRS_SASL_MECHANISM_01_019: [On success, saslmechanism_challenge shall return 0.] */
			result = 0;
		}
	}

	return result;
}
