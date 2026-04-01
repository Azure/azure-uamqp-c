///! AMQP 1.0 Messaging helpers (OASIS spec §3.5)
///!
///! Utility functions for creating Source and Target instances.
const defs = @import("protocol/definitions.zig");
const std = @import("std");

/// Create a Source with the given address.
pub fn createSource(address: []const u8) defs.Source {
    return .{ .address = address };
}

/// Create a Target with the given address.
pub fn createTarget(address: []const u8) defs.Target {
    return .{ .address = address };
}

/// Create a Source for receiving from a topic with a filter.
pub fn createFilteredSource(address: []const u8, filter: []defs.MapEntry) defs.Source {
    return .{ .address = address, .filter = filter };
}

// ── Tests ──────────────────────────────────────────────────────────────

test "createSource" {
    const source = createSource("my-queue");
    try std.testing.expectEqualStrings("my-queue", source.address.?);
    try std.testing.expectEqual(defs.TerminusDurability.none, source.durable);
}

test "createTarget" {
    const target = createTarget("my-queue");
    try std.testing.expectEqualStrings("my-queue", target.address.?);
}
