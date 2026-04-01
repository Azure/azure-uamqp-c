///! AMQP 1.0 Connection state machine (OASIS spec §2.4)
///!
///! Manages the lifecycle of an AMQP connection: protocol header exchange,
///! Open/Close performatives, idle timeout, and frame dispatching.
const std = @import("std");
const Allocator = std.mem.Allocator;
const frame_mod = @import("frame.zig");
const FrameHeader = frame_mod.FrameHeader;
const FrameCodec = @import("frame_codec.zig").FrameCodec;
const defs = @import("definitions.zig");

const log = std.log.scoped(.amqp_connection);

/// Connection states per AMQP 1.0 §2.4.6
pub const ConnectionState = enum {
    start,
    hdr_rcvd,
    hdr_sent,
    hdr_exch,
    open_pipe,
    open_sent,
    open_rcvd,
    opened,
    close_pipe,
    close_sent,
    close_rcvd,
    end,
    err,
    discarding,
};

/// Callback signatures
pub const OnConnectionStateChanged = *const fn (
    context: ?*anyopaque,
    new_state: ConnectionState,
    previous_state: ConnectionState,
) void;

pub const OnEndpointFrameReceived = *const fn (
    context: ?*anyopaque,
    performative: defs.Performative,
    channel: u16,
    payload: []const u8,
) void;

/// An endpoint (session) registered on a connection.
pub const Endpoint = struct {
    on_frame_received: OnEndpointFrameReceived,
    context: ?*anyopaque,
    incoming_channel: ?u16 = null,
    outgoing_channel: ?u16 = null,
};

/// AMQP Connection — manages protocol header exchange, open/close,
/// idle timeout tracking, and frame dispatch to session endpoints.
pub const Connection = struct {
    allocator: Allocator,
    state: ConnectionState,
    container_id: []const u8,
    hostname: ?[]const u8,
    max_frame_size: u32,
    channel_max: u16,
    idle_timeout_ms: ?u32,

    // Remote peer settings (populated after receiving Open)
    remote_max_frame_size: u32,
    remote_channel_max: u16,
    remote_idle_timeout_ms: ?u32,

    // Frame codec
    frame_codec: FrameCodec,

    // Endpoints (sessions)
    endpoints: std.ArrayList(Endpoint),

    // Timing
    last_frame_received_ms: i64,
    last_frame_sent_ms: i64,

    // Callbacks
    on_state_changed: ?OnConnectionStateChanged,
    on_state_changed_context: ?*anyopaque,

    // I/O (abstracted)
    io_send: ?*const fn (context: ?*anyopaque, data: []const u8) anyerror!void,
    io_context: ?*anyopaque,

    pub fn init(
        allocator: Allocator,
        container_id: []const u8,
        hostname: ?[]const u8,
        opts: struct {
            max_frame_size: u32 = 4294967295,
            channel_max: u16 = 65535,
            idle_timeout_ms: ?u32 = null,
        },
    ) Connection {
        return .{
            .allocator = allocator,
            .state = .start,
            .container_id = container_id,
            .hostname = hostname,
            .max_frame_size = opts.max_frame_size,
            .channel_max = opts.channel_max,
            .idle_timeout_ms = opts.idle_timeout_ms,
            .remote_max_frame_size = frame_mod.min_max_frame_size,
            .remote_channel_max = 0,
            .remote_idle_timeout_ms = null,
            .frame_codec = FrameCodec.init(allocator, opts.max_frame_size),
            .endpoints = .empty,
            // TODO: replace with proper clock source; zero placeholder for now
            .last_frame_received_ms = 0,
            .last_frame_sent_ms = 0,
            .on_state_changed = null,
            .on_state_changed_context = null,
            .io_send = null,
            .io_context = null,
        };
    }

    pub fn deinit(self: *Connection) void {
        self.frame_codec.deinit();
        self.endpoints.deinit(self.allocator);
    }

    /// Set the I/O send callback.
    pub fn setIo(
        self: *Connection,
        send_fn: *const fn (context: ?*anyopaque, data: []const u8) anyerror!void,
        context: ?*anyopaque,
    ) void {
        self.io_send = send_fn;
        self.io_context = context;
    }

    /// Set the state change callback.
    pub fn setOnStateChanged(self: *Connection, cb: OnConnectionStateChanged, context: ?*anyopaque) void {
        self.on_state_changed = cb;
        self.on_state_changed_context = context;
    }

    /// Register an endpoint (session) on this connection.
    pub fn createEndpoint(self: *Connection, callback: OnEndpointFrameReceived, context: ?*anyopaque) !*Endpoint {
        try self.endpoints.append(self.allocator, .{
            .on_frame_received = callback,
            .context = context,
        });
        return &self.endpoints.items[self.endpoints.items.len - 1];
    }

    /// Initiate the connection by sending the AMQP protocol header.
    pub fn open(self: *Connection) !void {
        if (self.state != .start) return error.InvalidState;
        try self.sendBytes(&frame_mod.amqp_header);
        self.setState(.hdr_sent);
    }

    /// Send a Close performative to gracefully shut down.
    pub fn close(self: *Connection, err_condition: ?[]const u8, err_description: ?[]const u8) !void {
        _ = err_condition;
        _ = err_description;
        if (self.state != .opened) return error.InvalidState;
        // In a full implementation, encode Close performative and send
        self.setState(.close_sent);
    }

    /// Process incoming bytes from the transport.
    pub fn onBytesReceived(self: *Connection, data: []const u8) !void {
        // TODO: replace with proper clock source
        self.last_frame_received_ms = 0;

        switch (self.state) {
            .start, .hdr_sent => {
                // Expecting protocol header
                if (data.len >= 8 and std.mem.eql(u8, data[0..8], &frame_mod.amqp_header)) {
                    const new_state: ConnectionState = if (self.state == .hdr_sent) .hdr_exch else .hdr_rcvd;
                    self.setState(new_state);

                    if (new_state == .hdr_rcvd) {
                        try self.sendBytes(&frame_mod.amqp_header);
                        self.setState(.hdr_exch);
                    }

                    if (data.len > 8) {
                        try self.frame_codec.receiveBytes(data[8..]);
                    }
                } else {
                    log.err("Invalid protocol header received", .{});
                    self.setState(.err);
                }
            },
            .hdr_exch, .open_sent, .opened => {
                try self.frame_codec.receiveBytes(data);
            },
            else => {
                log.warn("Bytes received in unexpected state: {s}", .{@tagName(self.state)});
            },
        }
    }

    /// Called periodically to handle idle timeouts and keep-alives.
    pub fn doWork(self: *Connection) !void {
        // TODO: replace with proper clock source
        const now: i64 = 0;

        // Check if remote peer has timed out
        if (self.remote_idle_timeout_ms) |timeout| {
            if (self.state == .opened) {
                const elapsed: u64 = @intCast(now - self.last_frame_received_ms);
                if (elapsed > @as(u64, timeout) * 2) {
                    log.err("Remote idle timeout exceeded", .{});
                    self.setState(.err);
                    return;
                }
            }
        }

        // Send empty frame as keep-alive if our idle timeout is configured
        if (self.idle_timeout_ms) |timeout| {
            if (self.state == .opened) {
                const elapsed: u64 = @intCast(now - self.last_frame_sent_ms);
                // Send keep-alive at half the timeout interval
                if (elapsed > @as(u64, timeout) / 2) {
                    try self.sendEmptyFrame();
                }
            }
        }
    }

    // ── Internal ──────────────────────────────────────────────────────

    fn setState(self: *Connection, new_state: ConnectionState) void {
        if (self.state == new_state) return;
        const prev = self.state;
        self.state = new_state;
        log.info("Connection state: {s} -> {s}", .{ @tagName(prev), @tagName(new_state) });
        if (self.on_state_changed) |cb| {
            cb(self.on_state_changed_context, new_state, prev);
        }
    }

    fn sendBytes(self: *Connection, data: []const u8) !void {
        if (self.io_send) |send_fn| {
            try send_fn(self.io_context, data);
            self.last_frame_sent_ms = 0; // TODO: replace with proper clock source
        } else {
            return error.NoIoConfigured;
        }
    }

    fn sendEmptyFrame(self: *Connection) !void {
        const hdr = FrameHeader{
            .size = frame_mod.frame_header_size,
            .doff = 2,
            .frame_type = .amqp,
            .channel = 0,
        };
        const bytes = hdr.serialize();
        try self.sendBytes(&bytes);
    }
};

// ── Tests ──────────────────────────────────────────────────────────────

test "Connection init and state" {
    const allocator = std.testing.allocator;
    var conn = Connection.init(allocator, "test-container", "localhost", .{});
    defer conn.deinit();
    try std.testing.expectEqual(ConnectionState.start, conn.state);
}

test "Connection state transitions" {
    const allocator = std.testing.allocator;

    var sent_data: ?[]const u8 = null;
    const S = struct {
        fn send(ctx: ?*anyopaque, data: []const u8) anyerror!void {
            const ptr: *?[]const u8 = @ptrCast(@alignCast(ctx.?));
            ptr.* = data;
        }
    };

    var conn = Connection.init(allocator, "test", null, .{});
    defer conn.deinit();
    conn.setIo(S.send, @ptrCast(&sent_data));

    try conn.open();
    try std.testing.expectEqual(ConnectionState.hdr_sent, conn.state);
    try std.testing.expect(sent_data != null);
}
