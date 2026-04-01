///! Example: AMQP message sender
///!
///! Demonstrates creating a connection, session, link, and sending a message.
const std = @import("std");
const uamqp = @import("uamqp");

pub fn main() !void {
    const allocator = std.heap.page_allocator;

    std.debug.print("uAMQP Zig Sender Example (v{s})\n", .{uamqp.version});

    // Create a message
    var msg = uamqp.message.Message.init(allocator);
    defer msg.deinit();

    msg.header = .{
        .durable = true,
        .priority = 4,
    };

    msg.properties = .{
        .subject = "test-message",
        .content_type = "application/octet-stream",
    };

    try msg.addBodyData("Hello from Zig uAMQP!");

    std.debug.print("Message created with {d} body section(s)\n", .{msg.bodyDataCount()});

    // Create source/target
    const source = uamqp.messaging.createSource("ingress");
    const target = uamqp.messaging.createTarget("localhost/ingress");

    std.debug.print("Source: {?s}\n", .{source.address});
    std.debug.print("Target: {?s}\n", .{target.address});

    // Demonstrate AMQP value encoding size
    const size = uamqp.encoder.encodedSize(.{ .string = "hello" });
    std.debug.print("'hello' encodes to {d} bytes\n", .{size});

    std.debug.print("Done.\n", .{});
}
