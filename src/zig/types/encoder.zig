const std = @import("std");
const amqp = @import("amqp_value.zig");
const AmqpValue = amqp.AmqpValue;
const MapEntry = amqp.MapEntry;
const FormatCode = amqp.FormatCode;
const Allocator = std.mem.Allocator;

/// A simple growable byte buffer for encoding.
pub const Buffer = struct {
    data: []u8,
    pos: usize,
    allocator: ?Allocator,
    is_fixed: bool,

    pub fn initFixed(buf: []u8) Buffer {
        return .{ .data = buf, .pos = 0, .allocator = null, .is_fixed = true };
    }

    pub fn initDynamic(allocator: Allocator) Buffer {
        return .{ .data = &.{}, .pos = 0, .allocator = allocator, .is_fixed = false };
    }

    pub fn deinit(self: *Buffer) void {
        if (!self.is_fixed) {
            if (self.allocator) |a| {
                if (self.data.len > 0) a.free(self.data);
            }
        }
    }

    pub fn written(self: *const Buffer) []const u8 {
        return self.data[0..self.pos];
    }

    pub fn reset(self: *Buffer) void {
        self.pos = 0;
    }

    fn ensureCapacity(self: *Buffer, additional: usize) !void {
        const needed = self.pos + additional;
        if (needed <= self.data.len) return;
        if (self.is_fixed) return error.OutOfMemory;
        const a = self.allocator orelse return error.OutOfMemory;
        const new_cap = @max(self.data.len * 2, needed, 64);
        if (self.data.len > 0) {
            self.data = try a.realloc(self.data, new_cap);
        } else {
            self.data = try a.alloc(u8, new_cap);
        }
    }

    pub fn writeByte(self: *Buffer, byte: u8) !void {
        try self.ensureCapacity(1);
        self.data[self.pos] = byte;
        self.pos += 1;
    }

    pub fn writeAll(self: *Buffer, bytes: []const u8) !void {
        try self.ensureCapacity(bytes.len);
        @memcpy(self.data[self.pos .. self.pos + bytes.len], bytes);
        self.pos += bytes.len;
    }
};

pub const EncodeError = error{OutOfMemory};

/// Encode an AmqpValue into AMQP 1.0 binary wire format.
pub fn encode(value: AmqpValue, buf: *Buffer) EncodeError!void {
    switch (value) {
        .null => try buf.writeByte(@intFromEnum(FormatCode.null)),
        .boolean => |v| {
            try buf.writeByte(@intFromEnum(if (v) FormatCode.boolean_true else FormatCode.boolean_false));
        },
        .ubyte => |v| {
            try buf.writeByte(@intFromEnum(FormatCode.ubyte));
            try buf.writeByte(v);
        },
        .ushort => |v| {
            try buf.writeByte(@intFromEnum(FormatCode.ushort));
            try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u16, v, .big)));
        },
        .uint => |v| {
            if (v == 0) {
                try buf.writeByte(@intFromEnum(FormatCode.uint_0));
            } else if (v <= 0xFF) {
                try buf.writeByte(@intFromEnum(FormatCode.smalluint));
                try buf.writeByte(@truncate(v));
            } else {
                try buf.writeByte(@intFromEnum(FormatCode.uint));
                try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, v, .big)));
            }
        },
        .ulong => |v| {
            if (v == 0) {
                try buf.writeByte(@intFromEnum(FormatCode.ulong_0));
            } else if (v <= 0xFF) {
                try buf.writeByte(@intFromEnum(FormatCode.smallulong));
                try buf.writeByte(@truncate(v));
            } else {
                try buf.writeByte(@intFromEnum(FormatCode.ulong));
                try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u64, v, .big)));
            }
        },
        .byte => |v| {
            try buf.writeByte(@intFromEnum(FormatCode.byte));
            try buf.writeByte(@bitCast(v));
        },
        .short => |v| {
            try buf.writeByte(@intFromEnum(FormatCode.short));
            try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u16, @as(u16, @bitCast(v)), .big)));
        },
        .int => |v| {
            if (v >= -128 and v <= 127) {
                try buf.writeByte(@intFromEnum(FormatCode.smallint));
                try buf.writeByte(@bitCast(@as(i8, @intCast(v))));
            } else {
                try buf.writeByte(@intFromEnum(FormatCode.int));
                try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, @as(u32, @bitCast(v)), .big)));
            }
        },
        .long => |v| {
            if (v >= -128 and v <= 127) {
                try buf.writeByte(@intFromEnum(FormatCode.smalllong));
                try buf.writeByte(@bitCast(@as(i8, @intCast(v))));
            } else {
                try buf.writeByte(@intFromEnum(FormatCode.long));
                try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u64, @as(u64, @bitCast(v)), .big)));
            }
        },
        .float => |v| {
            try buf.writeByte(@intFromEnum(FormatCode.float));
            try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, @as(u32, @bitCast(v)), .big)));
        },
        .double => |v| {
            try buf.writeByte(@intFromEnum(FormatCode.double));
            try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u64, @as(u64, @bitCast(v)), .big)));
        },
        .char => |v| {
            try buf.writeByte(@intFromEnum(FormatCode.char));
            try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, @as(u32, v), .big)));
        },
        .timestamp => |v| {
            try buf.writeByte(@intFromEnum(FormatCode.timestamp));
            try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u64, @as(u64, @bitCast(v)), .big)));
        },
        .uuid => |v| {
            try buf.writeByte(@intFromEnum(FormatCode.uuid));
            try buf.writeAll(&v);
        },
        .binary => |v| try writeVariable(buf, v, FormatCode.binary_8, FormatCode.binary_32),
        .string => |v| try writeVariable(buf, v, FormatCode.string_8, FormatCode.string_32),
        .symbol => |v| try writeVariable(buf, v, FormatCode.symbol_8, FormatCode.symbol_32),
        .list => |items| {
            if (items.len == 0) {
                try buf.writeByte(@intFromEnum(FormatCode.list_0));
                return;
            }

            // Encode all items to a temp buffer to compute size
            var tmp = Buffer.initDynamic(std.heap.page_allocator);
            defer tmp.deinit();
            for (items) |item| {
                try encode(item, &tmp);
            }

            const count = items.len;
            if (tmp.pos <= 0xFF and count <= 0xFF) {
                try buf.writeByte(@intFromEnum(FormatCode.list_8));
                try buf.writeByte(@intCast(tmp.pos + 1));
                try buf.writeByte(@intCast(count));
            } else {
                try buf.writeByte(@intFromEnum(FormatCode.list_32));
                try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, @intCast(tmp.pos + 4), .big)));
                try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, @intCast(count), .big)));
            }
            try buf.writeAll(tmp.written());
        },
        .map => |entries| {
            var tmp = Buffer.initDynamic(std.heap.page_allocator);
            defer tmp.deinit();
            for (entries) |entry| {
                try encode(entry.key, &tmp);
                try encode(entry.value, &tmp);
            }

            const count = entries.len * 2;
            if (tmp.pos <= 0xFF and count <= 0xFF) {
                try buf.writeByte(@intFromEnum(FormatCode.map_8));
                try buf.writeByte(@intCast(tmp.pos + 1));
                try buf.writeByte(@intCast(count));
            } else {
                try buf.writeByte(@intFromEnum(FormatCode.map_32));
                try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, @intCast(tmp.pos + 4), .big)));
                try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, @intCast(count), .big)));
            }
            try buf.writeAll(tmp.written());
        },
        .array => |items| {
            if (items.len == 0) {
                try buf.writeByte(@intFromEnum(FormatCode.list_0));
                return;
            }

            var tmp = Buffer.initDynamic(std.heap.page_allocator);
            defer tmp.deinit();

            // Write constructor (format code of first item)
            const fc = firstFormatCode(items[0]);
            try tmp.writeByte(@intFromEnum(fc));

            // Write each item's data (without constructor)
            for (items) |item| {
                try encodeRaw(item, &tmp);
            }

            const count = items.len;
            if (tmp.pos <= 0xFF and count <= 0xFF) {
                try buf.writeByte(@intFromEnum(FormatCode.array_8));
                try buf.writeByte(@intCast(tmp.pos + 1));
                try buf.writeByte(@intCast(count));
            } else {
                try buf.writeByte(@intFromEnum(FormatCode.array_32));
                try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, @intCast(tmp.pos + 4), .big)));
                try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, @intCast(count), .big)));
            }
            try buf.writeAll(tmp.written());
        },
        .described => |d| {
            try buf.writeByte(0x00); // described type constructor
            try encode(d.descriptor.*, buf);
            try encode(d.value.*, buf);
        },
    }
}

/// Compute the encoded size of a value without writing it.
pub fn encodedSize(value: AmqpValue) usize {
    var tmp = Buffer.initDynamic(std.heap.page_allocator);
    defer tmp.deinit();
    encode(value, &tmp) catch return 0;
    return tmp.pos;
}

fn writeVariable(buf: *Buffer, data: []const u8, small_code: FormatCode, large_code: FormatCode) !void {
    if (data.len <= 0xFF) {
        try buf.writeByte(@intFromEnum(small_code));
        try buf.writeByte(@intCast(data.len));
    } else {
        try buf.writeByte(@intFromEnum(large_code));
        try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, @intCast(data.len), .big)));
    }
    try buf.writeAll(data);
}

/// Write the raw data of a value (without the format code constructor).
fn encodeRaw(value: AmqpValue, buf: *Buffer) EncodeError!void {
    switch (value) {
        .null => {},
        .boolean => |v| try buf.writeByte(if (v) 1 else 0),
        .ubyte => |v| try buf.writeByte(v),
        .ushort => |v| try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u16, v, .big))),
        .uint => |v| try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, v, .big))),
        .ulong => |v| try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u64, v, .big))),
        .byte => |v| try buf.writeByte(@bitCast(v)),
        .short => |v| try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u16, @as(u16, @bitCast(v)), .big))),
        .int => |v| try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, @as(u32, @bitCast(v)), .big))),
        .long => |v| try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u64, @as(u64, @bitCast(v)), .big))),
        .float => |v| try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, @as(u32, @bitCast(v)), .big))),
        .double => |v| try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u64, @as(u64, @bitCast(v)), .big))),
        .char => |v| try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u32, @as(u32, v), .big))),
        .timestamp => |v| try buf.writeAll(&std.mem.toBytes(std.mem.nativeTo(u64, @as(u64, @bitCast(v)), .big))),
        .uuid => |v| try buf.writeAll(&v),
        .binary => |v| try buf.writeAll(v),
        .string => |v| try buf.writeAll(v),
        .symbol => |v| try buf.writeAll(v),
        else => try encode(value, buf),
    }
}

fn firstFormatCode(value: AmqpValue) FormatCode {
    return switch (value) {
        .null => FormatCode.null,
        .boolean => FormatCode.boolean,
        .ubyte => FormatCode.ubyte,
        .ushort => FormatCode.ushort,
        .uint => FormatCode.uint,
        .ulong => FormatCode.ulong,
        .byte => FormatCode.byte,
        .short => FormatCode.short,
        .int => FormatCode.int,
        .long => FormatCode.long,
        .float => FormatCode.float,
        .double => FormatCode.double,
        .char => FormatCode.char,
        .timestamp => FormatCode.timestamp,
        .uuid => FormatCode.uuid,
        .binary => FormatCode.binary_32,
        .string => FormatCode.string_32,
        .symbol => FormatCode.symbol_32,
        .list => FormatCode.list_32,
        .map => FormatCode.map_32,
        .array => FormatCode.array_32,
        .described => FormatCode.described,
    };
}

// ── Tests ──────────────────────────────────────────────────────────────

test "encode null" {
    var buf_arr: [1]u8 = undefined;
    var buf = Buffer.initFixed(&buf_arr);
    try encode(.null, &buf);
    try std.testing.expectEqual(@as(u8, 0x40), buf.written()[0]);
}

test "encode boolean" {
    var buf_arr: [1]u8 = undefined;
    var buf = Buffer.initFixed(&buf_arr);
    try encode(.{ .boolean = true }, &buf);
    try std.testing.expectEqual(@as(u8, 0x41), buf.written()[0]);

    buf.reset();
    try encode(.{ .boolean = false }, &buf);
    try std.testing.expectEqual(@as(u8, 0x42), buf.written()[0]);
}

test "encode uint compact forms" {
    // uint_0
    {
        var buf_arr: [1]u8 = undefined;
        var buf = Buffer.initFixed(&buf_arr);
        try encode(.{ .uint = 0 }, &buf);
        try std.testing.expectEqual(@as(u8, 0x43), buf.written()[0]);
    }
    // smalluint
    {
        var buf_arr: [2]u8 = undefined;
        var buf = Buffer.initFixed(&buf_arr);
        try encode(.{ .uint = 200 }, &buf);
        try std.testing.expectEqual(@as(u8, 0x52), buf.written()[0]);
        try std.testing.expectEqual(@as(u8, 200), buf.written()[1]);
    }
    // uint
    {
        var buf_arr: [5]u8 = undefined;
        var buf = Buffer.initFixed(&buf_arr);
        try encode(.{ .uint = 0x12345678 }, &buf);
        try std.testing.expectEqual(@as(u8, 0x70), buf.written()[0]);
        try std.testing.expectEqualSlices(u8, &.{ 0x12, 0x34, 0x56, 0x78 }, buf.written()[1..5]);
    }
}

test "encode string" {
    var buf_arr: [32]u8 = undefined;
    var buf = Buffer.initFixed(&buf_arr);
    try encode(.{ .string = "hello" }, &buf);
    const w = buf.written();
    try std.testing.expectEqual(@as(u8, 0xa1), w[0]); // string_8
    try std.testing.expectEqual(@as(u8, 5), w[1]); // length
    try std.testing.expectEqualStrings("hello", w[2..7]);
}

test "encode list" {
    var buf_arr: [64]u8 = undefined;
    var buf = Buffer.initFixed(&buf_arr);
    var items = [_]AmqpValue{ .{ .uint = 1 }, .{ .boolean = true }, .null };
    try encode(.{ .list = &items }, &buf);
    try std.testing.expectEqual(@as(u8, 0xc0), buf.written()[0]); // list_8
}

test "encode empty list" {
    var buf_arr: [1]u8 = undefined;
    var buf = Buffer.initFixed(&buf_arr);
    const empty: []AmqpValue = &.{};
    try encode(.{ .list = empty }, &buf);
    try std.testing.expectEqual(@as(u8, 0x45), buf.written()[0]); // list_0
}
