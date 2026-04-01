///! AMQP 1.0 Session state machine (OASIS spec §2.5)
///!
///! Manages multiple links, flow control windows, and transfer numbering.
const std = @import("std");
const Allocator = std.mem.Allocator;
const defs = @import("definitions.zig");
const Connection = @import("connection.zig").Connection;

const log = std.log.scoped(.amqp_session);

/// Session states per AMQP 1.0 §2.5.5
pub const SessionState = enum {
    unmapped,
    begin_sent,
    begin_rcvd,
    mapped,
    end_sent,
    end_rcvd,
    discarding,
    err,
};

pub const OnSessionStateChanged = *const fn (
    context: ?*anyopaque,
    new_state: SessionState,
    previous_state: SessionState,
) void;

pub const OnSessionFlowOn = *const fn (context: ?*anyopaque) void;

/// A link endpoint registered within a session.
pub const LinkEndpoint = struct {
    name: []const u8,
    handle: u32,
    input_handle: ?u32 = null,
    on_frame_received: ?*const fn (context: ?*anyopaque, performative: defs.Performative, payload: []const u8) void = null,
    context: ?*anyopaque = null,
};

/// AMQP Session — manages link endpoints and flow control within a connection.
pub const Session = struct {
    allocator: Allocator,
    state: SessionState,
    connection: *Connection,

    // Flow control
    next_outgoing_id: u32,
    next_incoming_id: u32,
    incoming_window: u32,
    outgoing_window: u32,
    remote_incoming_window: u32,
    remote_outgoing_window: u32,
    handle_max: u32,

    // Link endpoints
    link_endpoints: std.ArrayList(LinkEndpoint),
    next_handle: u32,

    // Channel
    outgoing_channel: ?u16,
    incoming_channel: ?u16,

    // Callbacks
    on_state_changed: ?OnSessionStateChanged,
    on_state_changed_context: ?*anyopaque,

    pub fn init(allocator: Allocator, connection: *Connection, opts: struct {
        incoming_window: u32 = 2147483647,
        outgoing_window: u32 = 65536,
        handle_max: u32 = 4294967295,
    }) Session {
        return .{
            .allocator = allocator,
            .state = .unmapped,
            .connection = connection,
            .next_outgoing_id = 0,
            .next_incoming_id = 0,
            .incoming_window = opts.incoming_window,
            .outgoing_window = opts.outgoing_window,
            .remote_incoming_window = 0,
            .remote_outgoing_window = 0,
            .handle_max = opts.handle_max,
            .link_endpoints = .empty,
            .next_handle = 0,
            .outgoing_channel = null,
            .incoming_channel = null,
            .on_state_changed = null,
            .on_state_changed_context = null,
        };
    }

    pub fn deinit(self: *Session) void {
        self.link_endpoints.deinit(self.allocator);
    }

    /// Set the state change callback.
    pub fn setOnStateChanged(self: *Session, cb: OnSessionStateChanged, context: ?*anyopaque) void {
        self.on_state_changed = cb;
        self.on_state_changed_context = context;
    }

    /// Create a link endpoint within this session.
    pub fn createLinkEndpoint(self: *Session, name: []const u8) !*LinkEndpoint {
        const handle = self.next_handle;
        self.next_handle += 1;

        try self.link_endpoints.append(self.allocator, .{
            .name = name,
            .handle = handle,
        });
        return &self.link_endpoints.items[self.link_endpoints.items.len - 1];
    }

    /// Destroy a link endpoint.
    pub fn destroyLinkEndpoint(self: *Session, endpoint: *LinkEndpoint) void {
        for (self.link_endpoints.items, 0..) |*ep, i| {
            if (ep.handle == endpoint.handle) {
                _ = self.link_endpoints.orderedRemove(i);
                return;
            }
        }
    }

    /// Initiate the session by sending Begin.
    pub fn begin(self: *Session) !void {
        if (self.state != .unmapped) return error.InvalidState;
        // In full implementation: encode Begin performative and send via connection
        self.setState(.begin_sent);
    }

    /// End the session.
    pub fn end(self: *Session, err: ?defs.AmqpError) !void {
        _ = err;
        if (self.state != .mapped) return error.InvalidState;
        self.setState(.end_sent);
    }

    /// Handle a received Begin performative.
    pub fn onBeginReceived(self: *Session, begin_perf: defs.Begin) void {
        self.remote_incoming_window = begin_perf.incoming_window;
        self.remote_outgoing_window = begin_perf.outgoing_window;
        self.next_incoming_id = begin_perf.next_outgoing_id;

        switch (self.state) {
            .begin_sent => self.setState(.mapped),
            .unmapped => self.setState(.begin_rcvd),
            else => {
                log.err("Begin received in unexpected state: {s}", .{@tagName(self.state)});
                self.setState(.err);
            },
        }
    }

    /// Handle a received Flow performative.
    pub fn onFlowReceived(self: *Session, flow: defs.Flow) void {
        if (flow.next_incoming_id) |id| {
            self.remote_incoming_window = id +% flow.incoming_window -% self.next_outgoing_id;
        }
        self.remote_outgoing_window = flow.outgoing_window;

        // Dispatch to link if handle specified
        if (flow.handle) |handle| {
            for (self.link_endpoints.items) |*ep| {
                if (ep.handle == handle) {
                    if (ep.on_frame_received) |cb| {
                        cb(ep.context, .{ .flow = flow }, &.{});
                    }
                    return;
                }
            }
            log.warn("Flow for unknown handle: {d}", .{handle});
        }
    }

    // ── Internal ──────────────────────────────────────────────────────

    fn setState(self: *Session, new_state: SessionState) void {
        if (self.state == new_state) return;
        const prev = self.state;
        self.state = new_state;
        log.info("Session state: {s} -> {s}", .{ @tagName(prev), @tagName(new_state) });
        if (self.on_state_changed) |cb| {
            cb(self.on_state_changed_context, new_state, prev);
        }
    }
};

// ── Tests ──────────────────────────────────────────────────────────────

test "Session init" {
    const allocator = std.testing.allocator;
    var conn = Connection.init(allocator, "test", null, .{});
    defer conn.deinit();

    var session = Session.init(allocator, &conn, .{});
    defer session.deinit();

    try std.testing.expectEqual(SessionState.unmapped, session.state);
    try std.testing.expectEqual(@as(u32, 2147483647), session.incoming_window);
}

test "Session create link endpoint" {
    const allocator = std.testing.allocator;
    var conn = Connection.init(allocator, "test", null, .{});
    defer conn.deinit();

    var session = Session.init(allocator, &conn, .{});
    defer session.deinit();

    const ep = try session.createLinkEndpoint("my-link");
    try std.testing.expectEqual(@as(u32, 0), ep.handle);
    try std.testing.expectEqualStrings("my-link", ep.name);

    const ep2 = try session.createLinkEndpoint("my-link-2");
    try std.testing.expectEqual(@as(u32, 1), ep2.handle);
}
