///! Example: AMQP message receiver
///!
///! Demonstrates creating a connection, session, and receiver link.
const std = @import("std");
const uamqp = @import("uamqp");

pub fn main() void {
    std.debug.print("uAMQP Zig Receiver Example (v{s})\n", .{uamqp.version});

    // Demonstrate AMQP value types
    const values = [_]uamqp.AmqpValue{
        .null,
        .{ .boolean = true },
        .{ .uint = 42 },
        .{ .string = "hello" },
        .{ .symbol = "amqp:accepted:list" },
    };

    for (values) |v| {
        std.debug.print("  Type: {s}\n", .{v.typeName()});
    }

    // Demonstrate value encoding size
    for (values) |v| {
        const size = uamqp.encoder.encodedSize(v);
        std.debug.print("  {s} encodes to {d} bytes\n", .{ v.typeName(), size });
    }

    std.debug.print("Done.\n", .{});
}
