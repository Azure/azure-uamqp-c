

// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef AMQP_DEFINITIONS_H
#define AMQP_DEFINITIONS_H

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#include "azure_uamqp_c/amqpvalue.h"
#include "azure_c_shared_utility/umock_c_prod.h"

#include "azure_uamqp_c/amqp_generated_types/amqp_role.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_sender_settle_mode.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_receiver_settle_mode.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_handle.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_seconds.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_milliseconds.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_delivery_tag.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_sequence_no.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_delivery_number.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_transfer_number.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_message_format.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_ietf_language_tag.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_fields.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_error.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_amqp_error.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_connection_error.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_session_error.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_link_error.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_open.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_begin.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_attach.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_flow.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_transfer.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_disposition.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_detach.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_end.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_close.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_sasl_code.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_sasl_mechanisms.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_sasl_init.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_sasl_challenge.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_sasl_response.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_sasl_outcome.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_terminus_durability.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_terminus_expiry_policy.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_node_properties.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_filter_set.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_source.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_target.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_annotations.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_message_id_ulong.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_message_id_uuid.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_message_id_binary.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_message_id_string.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_address_string.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_header.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_delivery_annotations.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_message_annotations.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_application_properties.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_data.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_amqp_sequence.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_amqp_value.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_footer.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_properties.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_received.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_accepted.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_rejected.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_released.h"
#include "azure_uamqp_c/amqp_generated_types/amqp_modified.h"


#endif /* AMQP_DEFINITIONS_H */
