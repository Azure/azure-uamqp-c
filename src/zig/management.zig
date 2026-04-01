///! AMQP Management operations (OASIS AMQP Management spec)
///!
///! Provides request-response operations over AMQP management links.
const std = @import("std");
const Allocator = std.mem.Allocator;
const defs = @import("protocol/definitions.zig");
const AmqpValue = @import("types/amqp_value.zig").AmqpValue;
const MapEntry = @import("types/amqp_value.zig").MapEntry;

const log = std.log.scoped(.amqp_management);

pub const ManagementState = enum {
    idle,
    opening,
    open,
    closing,
    err,
};

pub const ManagementOperationResult = enum {
    ok,
    error_result,
    instance_closed,
};

pub const OnManagementOperationComplete = *const fn (
    context: ?*anyopaque,
    result: ManagementOperationResult,
    status_code: u32,
    status_description: ?[]const u8,
    response: ?AmqpValue,
) void;

pub const OnManagementStateChanged = *const fn (
    context: ?*anyopaque,
    new_state: ManagementState,
    previous_state: ManagementState,
) void;

/// AMQP Management — request-response over management links.
pub const Management = struct {
    allocator: Allocator,
    state: ManagementState,
    reply_to: []const u8,
    next_message_id: u64,

    on_state_changed: ?OnManagementStateChanged,
    on_state_changed_context: ?*anyopaque,

    pending_operations: std.ArrayList(PendingOp),

    const PendingOp = struct {
        message_id: u64,
        on_complete: OnManagementOperationComplete,
        context: ?*anyopaque,
    };

    pub fn init(allocator: Allocator, node_address: []const u8) Management {
        _ = node_address;
        return .{
            .allocator = allocator,
            .state = .idle,
            .reply_to = "management-reply",
            .next_message_id = 0,
            .on_state_changed = null,
            .on_state_changed_context = null,
            .pending_operations = .empty,
        };
    }

    pub fn deinit(self: *Management) void {
        self.pending_operations.deinit(self.allocator);
    }

    pub fn setOnStateChanged(self: *Management, cb: OnManagementStateChanged, context: ?*anyopaque) void {
        self.on_state_changed = cb;
        self.on_state_changed_context = context;
    }

    /// Open the management session.
    pub fn open(self: *Management) !void {
        if (self.state != .idle) return error.InvalidState;
        self.setState(.opening);
        self.setState(.open);
    }

    /// Execute a management operation.
    pub fn executeOperation(
        self: *Management,
        operation: []const u8,
        entity_type: []const u8,
        locales: ?[]const u8,
        body: ?AmqpValue,
        on_complete: OnManagementOperationComplete,
        context: ?*anyopaque,
    ) !void {
        _ = operation;
        _ = entity_type;
        _ = locales;
        _ = body;
        if (self.state != .open) return error.InvalidState;

        const msg_id = self.next_message_id;
        self.next_message_id += 1;

        try self.pending_operations.append(self.allocator, .{
            .message_id = msg_id,
            .on_complete = on_complete,
            .context = context,
        });
    }

    /// Close the management session.
    pub fn close(self: *Management) void {
        self.setState(.closing);
        self.setState(.idle);
    }

    fn setState(self: *Management, new_state: ManagementState) void {
        if (self.state == new_state) return;
        const prev = self.state;
        self.state = new_state;
        log.info("Management state: {s} -> {s}", .{ @tagName(prev), @tagName(new_state) });
        if (self.on_state_changed) |cb| {
            cb(self.on_state_changed_context, new_state, prev);
        }
    }
};

// ── Tests ──────────────────────────────────────────────────────────────

test "Management init and open" {
    const allocator = std.testing.allocator;
    var mgmt = Management.init(allocator, "$management");
    defer mgmt.deinit();

    try std.testing.expectEqual(ManagementState.idle, mgmt.state);
    try mgmt.open();
    try std.testing.expectEqual(ManagementState.open, mgmt.state);
}

test "Management close" {
    const allocator = std.testing.allocator;
    var mgmt = Management.init(allocator, "$management");
    defer mgmt.deinit();

    try mgmt.open();
    mgmt.close();
    try std.testing.expectEqual(ManagementState.idle, mgmt.state);
}
