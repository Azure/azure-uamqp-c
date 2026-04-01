///! Azure uAMQP - AMQP 1.0 protocol library for Zig
///!
///! A Zig implementation of the OASIS AMQP 1.0 protocol.
///! Ported from azure-uamqp-c v1.2.12.

// Core types
pub const types = @import("types/amqp_value.zig");
pub const AmqpValue = types.AmqpValue;
pub const Described = types.Described;
pub const MapEntry = types.MapEntry;

// Encoding / decoding
pub const encoder = @import("types/encoder.zig");
pub const decoder = @import("types/decoder.zig");

// Protocol
pub const frame = @import("protocol/frame.zig");
pub const frame_codec = @import("protocol/frame_codec.zig");
pub const definitions = @import("protocol/definitions.zig");
pub const connection = @import("protocol/connection.zig");
pub const session = @import("protocol/session.zig");
pub const link = @import("protocol/link.zig");

// Message layer
pub const message = @import("message.zig");
pub const messaging = @import("messaging.zig");

// SASL
pub const sasl = struct {
    pub const mechanism = @import("sasl/mechanism.zig");
    pub const anonymous = @import("sasl/anonymous.zig");
    pub const plain = @import("sasl/plain.zig");
    pub const client_io = @import("sasl/client_io.zig");
};

// High-level
pub const cbs = @import("cbs.zig");
pub const management = @import("management.zig");

pub const version = "0.1.0";

test {
    const std = @import("std");
    std.testing.refAllDecls(@This());
}
