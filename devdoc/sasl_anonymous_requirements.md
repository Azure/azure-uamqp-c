#sasl_anonymous requirements
 
##Overview

sasl_anonymous is a module that implements the sasl mechanism ANONYMOUS so that it can be used through the sasl_mechanism interface.

##Exposed API

```C
	extern CONCRETE_SASL_MECHANISM_HANDLE saslanonymous_create(void* config);
	extern void saslanonymous_destroy(CONCRETE_SASL_MECHANISM_HANDLE concrete_sasl_mechanism);
	extern int saslanonymous_get_init_bytes(CONCRETE_SASL_MECHANISM_HANDLE concrete_sasl_mechanism, SASL_MECHANISM_BYTES* init_bytes);
	extern const char* saslanonymous_get_mechanism_name(CONCRETE_SASL_MECHANISM_HANDLE concrete_sasl_mechanism);
	extern int saslanonymous_challenge(CONCRETE_SASL_MECHANISM_HANDLE concrete_sasl_mechanism, const SASL_MECHANISM_BYTES* challenge_bytes, SASL_MECHANISM_BYTES* response_bytes);
	extern const SASL_MECHANISM_INTERFACE_DESCRIPTION* saslanonymous_get_interface(void);
```

###saslanonymous_create

```C
extern CONCRETE_SASL_MECHANISM_HANDLE saslanonymous_create(void* config);
```

**SRS_SASL_ANONYMOUS_01_001: [**saslanonymous_create shall return on success a non-NULL handle to a new SASL anonymous mechanism.**]** 
**SRS_SASL_ANONYMOUS_01_002: [**If allocating the memory needed for the saslanonymous instance fails then saslanonymous_create shall return NULL.**]** 
**SRS_SASL_ANONYMOUS_01_003: [**Since this is the ANONYMOUS SASL mechanism, config shall be ignored.**]**

###saslanonymous_destroy

```C
extern void saslanonymous_destroy(CONCRETE_SASL_MECHANISM_HANDLE concrete_sasl_mechanism);
```

**SRS_SASL_ANONYMOUS_01_004: [**saslanonymous_destroy shall free all resources associated with the SASL mechanism.**]** 
**SRS_SASL_ANONYMOUS_01_005: [**If the argument concrete_sasl_mechanism is NULL, saslanonymous_destroy shall do nothing.**]**

###saslanonymous_get_init_bytes

```C
extern int saslanonymous_get_init_bytes(CONCRETE_SASL_MECHANISM_HANDLE concrete_sasl_mechanism, INIT_BYTES* init_bytes);
```

**SRS_SASL_ANONYMOUS_01_006: [**saslanonymous_get_init_bytes shall validate the concrete_sasl_mechanism argument and set the length of the init_bytes argument to be zero.**]** 
**SRS_SASL_ANONYMOUS_01_012: [**The bytes field of init_buffer shall be set to NULL.**]** 
**SRS_SASL_ANONYMOUS_01_011: [**On success saslanonymous_get_init_bytes shall return zero.**]** 
**SRS_SASL_ANONYMOUS_01_007: [**If the any argument is NULL, saslanonymous_get_init_bytes shall return a non-zero value.**]**

###saslanonymous_get_mechanism_name

```C
extern const char* saslanonymous_get_mechanism_name(CONCRETE_SASL_MECHANISM_HANDLE concrete_sasl_mechanism);
```

**SRS_SASL_ANONYMOUS_01_008: [**saslanonymous_get_mechanism_name shall validate the argument concrete_sasl_mechanism and on success it shall return a pointer to the string "ANONYMOUS".**]** 
**SRS_SASL_ANONYMOUS_01_009: [**If the argument concrete_sasl_mechanism is NULL, saslanonymous_get_mechanism_name shall return NULL.**]** 

###saslanonymous_challenge

```C
extern int saslanonymous_challenge(CONCRETE_SASL_MECHANISM_HANDLE concrete_sasl_mechanism, const SASL_MECHANISM_BYTES* challenge_bytes, SASL_MECHANISM_BYTES* response_bytes);
```

**SRS_SASL_ANONYMOUS_01_013: [**saslanonymous_challenge shall set the response_bytes buffer to NULL and 0 size as the ANONYMOUS SASL mechanism does not implement challenge/response.**]** 
**SRS_SASL_ANONYMOUS_01_014: [**On success, saslanonymous_challenge shall return 0.**]** 
**SRS_SASL_ANONYMOUS_01_015: [**If the concrete_sasl_mechanism or response_bytes argument is NULL then saslanonymous_challenge shall fail and return a non-zero value.**]** 

###saslanonymous_get_interface

```C
extern const SASL_MECHANISM_INTERFACE_DESCRIPTION* saslanonymous_get_interface(void);
```

**SRS_SASL_ANONYMOUS_01_010: [**saslanonymous_get_interface shall return a pointer to a SASL_MECHANISM_INTERFACE_DESCRIPTION  structure that contains pointers to the functions: saslanonymous_create, saslanonymous_destroy, saslanonymous_get_init_bytes, saslanonymous_get_mechanism_name, saslanonymous_challenge.**]** 
