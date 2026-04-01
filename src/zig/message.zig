///! AMQP 1.0 Message abstraction (OASIS spec §3.2)
///!
///! Combines header, delivery-annotations, message-annotations,
///! properties, application-properties, body, and footer into a
///! single message structure.
const std = @import("std");
const Allocator = std.mem.Allocator;
const defs = @import("protocol/definitions.zig");
const AmqpValue = @import("types/amqp_value.zig").AmqpValue;
const MapEntry = @import("types/amqp_value.zig").MapEntry;

/// How the message body is encoded.
pub const BodyType = enum {
    none,
    data,
    sequence,
    value,
};

/// A single data section (binary payload).
pub const DataSection = struct {
    bytes: []const u8,
};

/// A complete AMQP message.
pub const Message = struct {
    allocator: Allocator,

    // §3.2.1 Header
    header: ?defs.Header = null,

    // §3.2.2 Delivery Annotations (map)
    delivery_annotations: ?[]MapEntry = null,

    // §3.2.3 Message Annotations (map)
    message_annotations: ?[]MapEntry = null,

    // §3.2.4 Properties
    properties: ?defs.Properties = null,

    // §3.2.5 Application Properties (map)
    application_properties: ?[]MapEntry = null,

    // Body — one of: data sections, sequence sections, or a single value
    body_type: BodyType = .none,
    body_data_sections: std.ArrayList(DataSection),
    body_sequence_sections: std.ArrayList([]AmqpValue),
    body_value: ?AmqpValue = null,

    // §3.2.9 Footer (map)
    footer: ?[]MapEntry = null,

    // Message format (default 0)
    message_format: u32 = 0,

    pub fn init(allocator: Allocator) Message {
        return .{
            .allocator = allocator,
            .body_data_sections = .empty,
            .body_sequence_sections = .empty,
        };
    }

    pub fn deinit(self: *Message) void {
        for (self.body_data_sections.items) |section| {
            self.allocator.free(section.bytes);
        }
        self.body_data_sections.deinit(self.allocator);

        for (self.body_sequence_sections.items) |seq| {
            for (seq) |*item| {
                @constCast(item).deinit(self.allocator);
            }
            self.allocator.free(seq);
        }
        self.body_sequence_sections.deinit(self.allocator);

        if (self.body_value) |*v| {
            @constCast(v).deinit(self.allocator);
        }
    }

    /// Add a binary data section to the message body.
    pub fn addBodyData(self: *Message, data: []const u8) !void {
        if (self.body_type != .none and self.body_type != .data) return error.BodyTypeMismatch;
        self.body_type = .data;
        const owned = try self.allocator.dupe(u8, data);
        try self.body_data_sections.append(self.allocator, .{ .bytes = owned });
    }

    /// Set the message body to a single AMQP value.
    pub fn setBodyValue(self: *Message, value: AmqpValue) !void {
        if (self.body_type != .none) return error.BodyTypeMismatch;
        self.body_type = .value;
        self.body_value = try value.clone(self.allocator);
    }

    /// Set a string application property.
    pub fn setApplicationProperty(self: *Message, key: []const u8, value: []const u8) !void {
        // Simple implementation: append to list (a real impl would check for duplicates)
        if (self.application_properties == null) {
            self.application_properties = try self.allocator.alloc(MapEntry, 0);
        }
        const old = self.application_properties.?;
        const new = try self.allocator.alloc(MapEntry, old.len + 1);
        @memcpy(new[0..old.len], old);
        new[old.len] = .{
            .key = .{ .string = try self.allocator.dupe(u8, key) },
            .value = .{ .string = try self.allocator.dupe(u8, value) },
        };
        self.allocator.free(old);
        self.application_properties = new;
    }

    /// Get the total number of body data sections.
    pub fn bodyDataCount(self: *const Message) usize {
        return self.body_data_sections.items.len;
    }

    /// Clone the entire message.
    pub fn clone(self: *const Message) !Message {
        var new_msg = Message.init(self.allocator);
        new_msg.header = self.header;
        new_msg.properties = self.properties;
        new_msg.message_format = self.message_format;
        new_msg.body_type = self.body_type;

        for (self.body_data_sections.items) |section| {
            try new_msg.addBodyData(section.bytes);
        }

        if (self.body_value) |v| {
            new_msg.body_value = try v.clone(self.allocator);
        }

        return new_msg;
    }
};

// ── Tests ──────────────────────────────────────────────────────────────

test "Message create and add body data" {
    const allocator = std.testing.allocator;

    var msg = Message.init(allocator);
    defer msg.deinit();

    try msg.addBodyData("hello world");
    try std.testing.expectEqual(@as(usize, 1), msg.bodyDataCount());
    try std.testing.expectEqual(BodyType.data, msg.body_type);

    try msg.addBodyData("second section");
    try std.testing.expectEqual(@as(usize, 2), msg.bodyDataCount());
}

test "Message set body value" {
    const allocator = std.testing.allocator;

    var msg = Message.init(allocator);
    defer msg.deinit();

    try msg.setBodyValue(.{ .string = "test value" });
    try std.testing.expectEqual(BodyType.value, msg.body_type);
    try std.testing.expect(msg.body_value != null);
}

test "Message body type mismatch" {
    const allocator = std.testing.allocator;

    var msg = Message.init(allocator);
    defer msg.deinit();

    try msg.addBodyData("data");
    try std.testing.expectError(error.BodyTypeMismatch, msg.setBodyValue(.null));
}

test "Message with header and properties" {
    const allocator = std.testing.allocator;

    var msg = Message.init(allocator);
    defer msg.deinit();

    msg.header = .{
        .durable = true,
        .priority = 7,
        .ttl = 60000,
    };
    msg.properties = .{
        .subject = "test-subject",
        .content_type = "application/json",
    };

    try std.testing.expect(msg.header.?.durable);
    try std.testing.expectEqual(@as(u8, 7), msg.header.?.priority);
    try std.testing.expectEqualStrings("test-subject", msg.properties.?.subject.?);
}

test "Message clone" {
    const allocator = std.testing.allocator;

    var original = Message.init(allocator);
    defer original.deinit();

    original.header = .{ .durable = true };
    try original.addBodyData("payload");

    var cloned = try original.clone();
    defer cloned.deinit();

    try std.testing.expect(cloned.header.?.durable);
    try std.testing.expectEqual(@as(usize, 1), cloned.bodyDataCount());
}
