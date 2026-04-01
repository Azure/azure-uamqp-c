///! AMQP 1.0 Link state machine (OASIS spec §2.6)
///!
///! Manages the lifecycle of a message transfer link: attach/detach,
///! credit-based flow control, and delivery tracking.
const std = @import("std");
const Allocator = std.mem.Allocator;
const defs = @import("definitions.zig");
const Session = @import("session.zig").Session;
const LinkEndpoint = @import("session.zig").LinkEndpoint;

const log = std.log.scoped(.amqp_link);

/// Link states per AMQP 1.0 §2.6.4
pub const LinkState = enum {
    detached,
    half_attached_attach_sent,
    half_attached_attach_received,
    attached,
    err,
};

pub const OnLinkStateChanged = *const fn (
    context: ?*anyopaque,
    new_state: LinkState,
    previous_state: LinkState,
) void;

pub const OnTransferReceived = *const fn (
    context: ?*anyopaque,
    transfer: defs.Transfer,
    payload: []const u8,
) defs.DeliveryState;

pub const OnLinkFlowOn = *const fn (context: ?*anyopaque) void;

/// Tracks a pending delivery (sent transfer awaiting disposition).
pub const Delivery = struct {
    delivery_id: u32,
    delivery_tag: []const u8,
    settled: bool,
    on_settled: ?*const fn (context: ?*anyopaque, delivery_id: u32, state: defs.DeliveryState) void = null,
    context: ?*anyopaque = null,
};

/// AMQP Link — a unidirectional channel for message transfer.
pub const Link = struct {
    allocator: Allocator,
    state: LinkState,
    name: []const u8,
    role: defs.Role,
    session: *Session,
    endpoint: *LinkEndpoint,

    // Settle modes
    snd_settle_mode: defs.SenderSettleMode,
    rcv_settle_mode: defs.ReceiverSettleMode,

    // Source and target
    source: ?defs.Source,
    target: ?defs.Target,

    // Flow control
    delivery_count: u32,
    link_credit: u32,
    available: u32,

    // Delivery tracking
    pending_deliveries: std.ArrayList(Delivery),
    next_delivery_id: u32,

    // Max message size
    max_message_size: ?u64,
    peer_max_message_size: ?u64,

    // Callbacks
    on_state_changed: ?OnLinkStateChanged,
    on_state_changed_context: ?*anyopaque,
    on_transfer_received: ?OnTransferReceived,
    on_transfer_received_context: ?*anyopaque,

    pub fn init(
        allocator: Allocator,
        session: *Session,
        name: []const u8,
        role: defs.Role,
        source: ?defs.Source,
        target: ?defs.Target,
    ) !Link {
        const endpoint = try session.createLinkEndpoint(name);
        return .{
            .allocator = allocator,
            .state = .detached,
            .name = name,
            .role = role,
            .session = session,
            .endpoint = endpoint,
            .snd_settle_mode = .mixed,
            .rcv_settle_mode = .first,
            .source = source,
            .target = target,
            .delivery_count = 0,
            .link_credit = 0,
            .available = 0,
            .pending_deliveries = .empty,
            .next_delivery_id = 0,
            .max_message_size = null,
            .peer_max_message_size = null,
            .on_state_changed = null,
            .on_state_changed_context = null,
            .on_transfer_received = null,
            .on_transfer_received_context = null,
        };
    }

    pub fn deinit(self: *Link) void {
        self.pending_deliveries.deinit(self.allocator);
        self.session.destroyLinkEndpoint(self.endpoint);
    }

    /// Initiate link attachment by sending Attach.
    pub fn attach(self: *Link) !void {
        if (self.state != .detached) return error.InvalidState;
        // In full implementation: encode Attach and send via session
        self.setState(.half_attached_attach_sent);
    }

    /// Detach the link.
    pub fn detach(self: *Link, close: bool, err: ?defs.AmqpError) !void {
        _ = close;
        _ = err;
        switch (self.state) {
            .attached, .half_attached_attach_sent => {
                self.setState(.detached);
            },
            else => return error.InvalidState,
        }
    }

    /// Set link credit (receiver only).
    pub fn setLinkCredit(self: *Link, credit: u32) void {
        self.link_credit = credit;
    }

    /// Handle a received Attach performative from the peer.
    pub fn onAttachReceived(self: *Link, attach_perf: defs.Attach) void {
        _ = attach_perf;
        switch (self.state) {
            .half_attached_attach_sent => self.setState(.attached),
            .detached => self.setState(.half_attached_attach_received),
            else => {
                log.err("Attach received in unexpected state: {s}", .{@tagName(self.state)});
                self.setState(.err);
            },
        }
    }

    /// Handle a received Flow performative.
    pub fn onFlowReceived(self: *Link, flow: defs.Flow) void {
        if (flow.delivery_count) |dc| {
            self.delivery_count = dc;
        }
        if (flow.link_credit) |credit| {
            self.link_credit = credit;
        }
    }

    /// Handle a received Transfer performative.
    pub fn onTransferReceived(self: *Link, transfer: defs.Transfer, payload: []const u8) ?defs.DeliveryState {
        if (self.on_transfer_received) |cb| {
            return cb(self.on_transfer_received_context, transfer, payload);
        }
        return null;
    }

    /// Check if the link has credit available for sending.
    pub fn hasCredit(self: *const Link) bool {
        return self.link_credit > 0;
    }

    // ── Internal ──────────────────────────────────────────────────────

    fn setState(self: *Link, new_state: LinkState) void {
        if (self.state == new_state) return;
        const prev = self.state;
        self.state = new_state;
        log.info("Link '{s}' state: {s} -> {s}", .{ self.name, @tagName(prev), @tagName(new_state) });
        if (self.on_state_changed) |cb| {
            cb(self.on_state_changed_context, new_state, prev);
        }
    }
};

// ── Tests ──────────────────────────────────────────────────────────────

const Connection = @import("connection.zig").Connection;

test "Link init and state" {
    const allocator = std.testing.allocator;
    var conn = Connection.init(allocator, "test", null, .{});
    defer conn.deinit();
    var session = Session.init(allocator, &conn, .{});
    defer session.deinit();

    var link = try Link.init(
        allocator,
        &session,
        "my-sender",
        .sender,
        .{ .address = "queue1" },
        .{ .address = "queue1" },
    );
    defer link.deinit();

    try std.testing.expectEqual(LinkState.detached, link.state);
    try std.testing.expectEqual(defs.Role.sender, link.role);
    try std.testing.expectEqualStrings("my-sender", link.name);
}

test "Link attach state transition" {
    const allocator = std.testing.allocator;
    var conn = Connection.init(allocator, "test", null, .{});
    defer conn.deinit();
    var session = Session.init(allocator, &conn, .{});
    defer session.deinit();

    var link = try Link.init(allocator, &session, "test-link", .sender, null, null);
    defer link.deinit();

    try link.attach();
    try std.testing.expectEqual(LinkState.half_attached_attach_sent, link.state);

    link.onAttachReceived(.{ .name = "test-link", .handle = 0, .role = .receiver });
    try std.testing.expectEqual(LinkState.attached, link.state);
}

test "Link credit management" {
    const allocator = std.testing.allocator;
    var conn = Connection.init(allocator, "test", null, .{});
    defer conn.deinit();
    var session = Session.init(allocator, &conn, .{});
    defer session.deinit();

    var link = try Link.init(allocator, &session, "rcv", .receiver, null, null);
    defer link.deinit();

    try std.testing.expect(!link.hasCredit());
    link.setLinkCredit(10);
    try std.testing.expect(link.hasCredit());
    try std.testing.expectEqual(@as(u32, 10), link.link_credit);
}
