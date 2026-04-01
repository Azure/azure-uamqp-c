const std = @import("std");

/// AMQP 1.0 frame constants (OASIS spec §2.3)
pub const amqp_header = [_]u8{ 'A', 'M', 'Q', 'P', 0, 1, 0, 0 };
pub const sasl_header = [_]u8{ 'A', 'M', 'Q', 'P', 3, 1, 0, 0 };

pub const min_frame_size: u32 = 512;
pub const frame_header_size: usize = 8;
pub const min_max_frame_size: u32 = 512;

/// Frame type identifiers.
pub const FrameType = enum(u8) {
    amqp = 0x00,
    sasl = 0x01,
    _,
};

/// A parsed AMQP frame header.
pub const FrameHeader = struct {
    /// Total frame size including header.
    size: u32,
    /// Data offset in 4-byte words (minimum 2 = 8 bytes for the header).
    doff: u8,
    /// Frame type.
    frame_type: FrameType,
    /// Channel number (type-specific: channel for AMQP, 0 for SASL).
    channel: u16,

    /// Size of the frame body (total size minus header as indicated by doff).
    pub fn bodySize(self: FrameHeader) u32 {
        return self.size - @as(u32, self.doff) * 4;
    }

    /// Parse a frame header from 8 bytes.
    pub fn parse(data: *const [frame_header_size]u8) error{InvalidFrame}!FrameHeader {
        const size = std.mem.readInt(u32, data[0..4], .big);
        const doff = data[4];
        const frame_type: FrameType = @enumFromInt(data[5]);
        const channel = std.mem.readInt(u16, data[6..8], .big);

        if (size < frame_header_size) return error.InvalidFrame;
        if (doff < 2) return error.InvalidFrame;

        return .{
            .size = size,
            .doff = doff,
            .frame_type = frame_type,
            .channel = channel,
        };
    }

    /// Serialize the frame header to 8 bytes.
    pub fn serialize(self: FrameHeader) [frame_header_size]u8 {
        var buf: [frame_header_size]u8 = undefined;
        std.mem.writeInt(u32, buf[0..4], self.size, .big);
        buf[4] = self.doff;
        buf[5] = @intFromEnum(self.frame_type);
        std.mem.writeInt(u16, buf[6..8], self.channel, .big);
        return buf;
    }
};

/// Represents a complete frame with header and body.
pub const Frame = struct {
    header: FrameHeader,
    /// The performative + payload bytes (everything after the extended header).
    body: []const u8,
};

// ── Tests ──────────────────────────────────────────────────────────────

test "parse and serialize frame header roundtrip" {
    const original = FrameHeader{
        .size = 100,
        .doff = 2,
        .frame_type = .amqp,
        .channel = 5,
    };
    const bytes = original.serialize();
    const parsed = try FrameHeader.parse(&bytes);
    try std.testing.expectEqual(original.size, parsed.size);
    try std.testing.expectEqual(original.doff, parsed.doff);
    try std.testing.expectEqual(original.frame_type, parsed.frame_type);
    try std.testing.expectEqual(original.channel, parsed.channel);
}

test "frame header body size" {
    const hdr = FrameHeader{ .size = 100, .doff = 2, .frame_type = .amqp, .channel = 0 };
    try std.testing.expectEqual(@as(u32, 92), hdr.bodySize());
}

test "frame header validates minimum" {
    var data = [_]u8{ 0, 0, 0, 4, 1, 0, 0, 0 }; // size=4 (too small for doff=2), doff=1 (too small)
    try std.testing.expectError(error.InvalidFrame, FrameHeader.parse(&data));
}

test "AMQP protocol header" {
    try std.testing.expectEqualSlices(u8, &[_]u8{ 'A', 'M', 'Q', 'P', 0, 1, 0, 0 }, &amqp_header);
}

test "SASL protocol header" {
    try std.testing.expectEqualSlices(u8, &[_]u8{ 'A', 'M', 'Q', 'P', 3, 1, 0, 0 }, &sasl_header);
}
