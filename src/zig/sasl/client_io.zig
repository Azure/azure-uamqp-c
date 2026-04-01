///! SASL Client I/O layer (OASIS spec §5.3)
///!
///! Wraps an underlying I/O transport with SASL negotiation.
///! This is the Zig equivalent of saslclientio.c.
const std = @import("std");
const Allocator = std.mem.Allocator;
const Mechanism = @import("mechanism.zig").Mechanism;
const frame_mod = @import("../protocol/frame.zig");
const defs = @import("../protocol/definitions.zig");

const log = std.log.scoped(.sasl_client_io);

pub const SaslState = enum {
    not_started,
    header_sent,
    header_exchanged,
    waiting_for_mechanisms,
    init_sent,
    waiting_for_outcome,
    complete,
    err,
};

pub const SaslClientIo = struct {
    allocator: Allocator,
    state: SaslState,
    mechanism: Mechanism,
    hostname: ?[]const u8,

    // I/O callbacks
    io_send: ?*const fn (context: ?*anyopaque, data: []const u8) anyerror!void,
    io_context: ?*anyopaque,

    // Completion callback
    on_open_complete: ?*const fn (context: ?*anyopaque, success: bool) void,
    on_open_complete_context: ?*anyopaque,

    pub fn init(allocator: Allocator, mechanism: Mechanism, hostname: ?[]const u8) SaslClientIo {
        return .{
            .allocator = allocator,
            .state = .not_started,
            .mechanism = mechanism,
            .hostname = hostname,
            .io_send = null,
            .io_context = null,
            .on_open_complete = null,
            .on_open_complete_context = null,
        };
    }

    pub fn setIo(
        self: *SaslClientIo,
        send_fn: *const fn (context: ?*anyopaque, data: []const u8) anyerror!void,
        context: ?*anyopaque,
    ) void {
        self.io_send = send_fn;
        self.io_context = context;
    }

    /// Begin SASL negotiation by sending the SASL protocol header.
    pub fn open(self: *SaslClientIo) !void {
        if (self.state != .not_started) return error.InvalidState;
        try self.sendBytes(&frame_mod.sasl_header);
        self.state = .header_sent;
    }

    /// Process received bytes during SASL negotiation.
    pub fn onBytesReceived(self: *SaslClientIo, data: []const u8) !void {
        switch (self.state) {
            .header_sent => {
                if (data.len >= 8 and std.mem.eql(u8, data[0..8], &frame_mod.sasl_header)) {
                    self.state = .header_exchanged;
                    self.state = .waiting_for_mechanisms;
                    if (data.len > 8) {
                        try self.processSaslFrames(data[8..]);
                    }
                } else {
                    log.err("Invalid SASL header received", .{});
                    self.state = .err;
                }
            },
            .waiting_for_mechanisms, .init_sent, .waiting_for_outcome => {
                try self.processSaslFrames(data);
            },
            else => {
                log.warn("Bytes received in unexpected SASL state: {s}", .{@tagName(self.state)});
            },
        }
    }

    /// Handle SASL mechanisms being offered by the server.
    pub fn onMechanismsReceived(self: *SaslClientIo, mechanisms: defs.SaslMechanisms) !void {
        const our_name = self.mechanism.getMechanismName();

        // Check if our mechanism is offered
        for (mechanisms.sasl_server_mechanisms) |mech_name| {
            if (std.mem.eql(u8, mech_name, our_name)) {
                // Send SASL init
                self.state = .init_sent;
                self.state = .waiting_for_outcome;
                return;
            }
        }

        log.err("Mechanism '{s}' not offered by server", .{our_name});
        self.state = .err;
    }

    /// Handle SASL outcome from server.
    pub fn onOutcomeReceived(self: *SaslClientIo, outcome: defs.SaslOutcome) void {
        if (outcome.code == .ok) {
            self.state = .complete;
            log.info("SASL authentication successful", .{});
            if (self.on_open_complete) |cb| {
                cb(self.on_open_complete_context, true);
            }
        } else {
            self.state = .err;
            log.err("SASL authentication failed with code: {d}", .{@intFromEnum(outcome.code)});
            if (self.on_open_complete) |cb| {
                cb(self.on_open_complete_context, false);
            }
        }
    }

    pub fn isComplete(self: *const SaslClientIo) bool {
        return self.state == .complete;
    }

    // ── Internal ──────────────────────────────────────────────────────

    fn sendBytes(self: *SaslClientIo, data: []const u8) !void {
        if (self.io_send) |send_fn| {
            try send_fn(self.io_context, data);
        } else {
            return error.NoIoConfigured;
        }
    }

    fn processSaslFrames(self: *SaslClientIo, data: []const u8) !void {
        // Placeholder: in full implementation, decode SASL frames
        // and dispatch to onMechanismsReceived/onOutcomeReceived
        _ = self;
        _ = data;
    }
};

// ── Tests ──────────────────────────────────────────────────────────────

test "SaslClientIo init" {
    const allocator = std.testing.allocator;
    var anon = @import("anonymous.zig").Anonymous{};
    const mech = anon.mechanism();

    var sasl = SaslClientIo.init(allocator, mech, "localhost");
    try std.testing.expectEqual(SaslState.not_started, sasl.state);
    try std.testing.expect(!sasl.isComplete());
}

test "SaslClientIo outcome handling" {
    const allocator = std.testing.allocator;
    var anon = @import("anonymous.zig").Anonymous{};
    const mech = anon.mechanism();

    var sasl = SaslClientIo.init(allocator, mech, null);
    sasl.state = .waiting_for_outcome;

    sasl.onOutcomeReceived(.{ .code = .ok });
    try std.testing.expectEqual(SaslState.complete, sasl.state);
    try std.testing.expect(sasl.isComplete());
}
