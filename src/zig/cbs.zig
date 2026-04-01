///! Claims-Based Security (CBS) for Azure AMQP
///!
///! Implements the CBS protocol for authenticating with Azure services
///! using SAS tokens or other claim types.
const std = @import("std");
const Allocator = std.mem.Allocator;
const defs = @import("protocol/definitions.zig");
const AmqpValue = @import("types/amqp_value.zig").AmqpValue;
const MapEntry = @import("types/amqp_value.zig").MapEntry;

const log = std.log.scoped(.amqp_cbs);

pub const CbsOperationResult = enum {
    ok,
    cbs_error,
    instance_closed,
};

pub const CbsState = enum {
    closed,
    opening,
    open,
    closing,
    err,
};

pub const OnCbsOperationComplete = *const fn (
    context: ?*anyopaque,
    result: CbsOperationResult,
    status_code: u32,
    status_description: ?[]const u8,
) void;

pub const OnCbsStateChanged = *const fn (
    context: ?*anyopaque,
    new_state: CbsState,
    previous_state: CbsState,
) void;

/// CBS (Claims-Based Security) handle for Azure AMQP authentication.
pub const Cbs = struct {
    allocator: Allocator,
    state: CbsState,

    on_state_changed: ?OnCbsStateChanged,
    on_state_changed_context: ?*anyopaque,

    /// Pending put-token operations
    pending_operations: std.ArrayList(PendingOp),

    const PendingOp = struct {
        on_complete: OnCbsOperationComplete,
        context: ?*anyopaque,
    };

    pub fn init(allocator: Allocator) Cbs {
        return .{
            .allocator = allocator,
            .state = .closed,
            .on_state_changed = null,
            .on_state_changed_context = null,
            .pending_operations = .empty,
        };
    }

    pub fn deinit(self: *Cbs) void {
        self.pending_operations.deinit(self.allocator);
    }

    pub fn setOnStateChanged(self: *Cbs, cb: OnCbsStateChanged, context: ?*anyopaque) void {
        self.on_state_changed = cb;
        self.on_state_changed_context = context;
    }

    /// Open the CBS session (creates management link pair).
    pub fn open(self: *Cbs) !void {
        if (self.state != .closed) return error.InvalidState;
        self.setState(.opening);
        // In full implementation: create sender/receiver links to $cbs
        self.setState(.open);
    }

    /// Submit a put-token operation for authentication.
    pub fn putToken(
        self: *Cbs,
        token_type: []const u8,
        audience: []const u8,
        token: []const u8,
        on_complete: OnCbsOperationComplete,
        context: ?*anyopaque,
    ) !void {
        _ = token_type;
        _ = audience;
        _ = token;
        if (self.state != .open) return error.InvalidState;
        try self.pending_operations.append(self.allocator, .{
            .on_complete = on_complete,
            .context = context,
        });
        // In full implementation: send CBS put-token message via management link
    }

    /// Close the CBS session.
    pub fn close(self: *Cbs) void {
        self.setState(.closing);
        self.setState(.closed);
    }

    fn setState(self: *Cbs, new_state: CbsState) void {
        if (self.state == new_state) return;
        const prev = self.state;
        self.state = new_state;
        log.info("CBS state: {s} -> {s}", .{ @tagName(prev), @tagName(new_state) });
        if (self.on_state_changed) |cb| {
            cb(self.on_state_changed_context, new_state, prev);
        }
    }
};

// ── Tests ──────────────────────────────────────────────────────────────

test "CBS init and open" {
    const allocator = std.testing.allocator;
    var cbs = Cbs.init(allocator);
    defer cbs.deinit();

    try std.testing.expectEqual(CbsState.closed, cbs.state);
    try cbs.open();
    try std.testing.expectEqual(CbsState.open, cbs.state);
}

test "CBS close" {
    const allocator = std.testing.allocator;
    var cbs = Cbs.init(allocator);
    defer cbs.deinit();

    try cbs.open();
    cbs.close();
    try std.testing.expectEqual(CbsState.closed, cbs.state);
}
