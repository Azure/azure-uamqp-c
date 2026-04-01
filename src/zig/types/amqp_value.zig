const std = @import("std");
const Allocator = std.mem.Allocator;

/// AMQP 1.0 type format codes (from OASIS spec section 1.6)
pub const FormatCode = enum(u8) {
    // Fixed-width: 0 octets
    described = 0x00,
    null = 0x40,
    boolean_true = 0x41,
    boolean_false = 0x42,
    uint_0 = 0x43,
    ulong_0 = 0x44,

    // Fixed-width: 1 octet
    boolean = 0x56,
    ubyte = 0x50,
    byte = 0x51,
    smalluint = 0x52,
    smallulong = 0x53,
    smallint = 0x54,
    smalllong = 0x55,

    // Fixed-width: 2 octets
    ushort = 0x60,
    short = 0x61,

    // Fixed-width: 4 octets
    uint = 0x70,
    int = 0x71,
    float = 0x72,
    char = 0x73,

    // Fixed-width: 8 octets
    ulong = 0x80,
    long = 0x81,
    double = 0x82,
    timestamp = 0x83,

    // Fixed-width: 16 octets
    uuid = 0x98,

    // Variable-width: 1-octet size
    binary_8 = 0xa0,
    string_8 = 0xa1,
    symbol_8 = 0xa3,

    // Variable-width: 4-octet size
    binary_32 = 0xb0,
    string_32 = 0xb1,
    symbol_32 = 0xb3,

    // Compound: 1-octet size+count
    list_0 = 0x45,
    list_8 = 0xc0,
    map_8 = 0xc1,

    // Compound: 4-octet size+count
    list_32 = 0xd0,
    map_32 = 0xd1,

    // Array
    array_8 = 0xe0,
    array_32 = 0xf0,

    _,
};

/// A key-value pair in an AMQP map.
pub const MapEntry = struct {
    key: AmqpValue,
    value: AmqpValue,
};

/// The descriptor for an AMQP described type.
pub const Described = struct {
    descriptor: *AmqpValue,
    value: *AmqpValue,
};

/// Core AMQP 1.0 value type — a tagged union representing all AMQP primitive
/// and composite types as defined in the OASIS AMQP 1.0 specification.
pub const AmqpValue = union(enum) {
    null,
    boolean: bool,
    ubyte: u8,
    ushort: u16,
    uint: u32,
    ulong: u64,
    byte: i8,
    short: i16,
    int: i32,
    long: i64,
    float: f32,
    double: f64,
    char: u21,
    timestamp: i64,
    uuid: [16]u8,
    binary: []const u8,
    string: []const u8,
    symbol: []const u8,
    list: []AmqpValue,
    map: []MapEntry,
    array: []AmqpValue,
    described: Described,

    /// Deep-clone this value, allocating all nested structures.
    pub fn clone(self: AmqpValue, allocator: Allocator) Allocator.Error!AmqpValue {
        return switch (self) {
            .null, .boolean, .ubyte, .ushort, .uint, .ulong, .byte, .short, .int, .long, .float, .double, .char, .timestamp, .uuid => self,
            .binary => |b| .{ .binary = try allocator.dupe(u8, b) },
            .string => |s| .{ .string = try allocator.dupe(u8, s) },
            .symbol => |s| .{ .symbol = try allocator.dupe(u8, s) },
            .list => |items| {
                const new_items = try allocator.alloc(AmqpValue, items.len);
                for (items, 0..) |item, i| {
                    new_items[i] = try item.clone(allocator);
                }
                return .{ .list = new_items };
            },
            .map => |entries| {
                const new_entries = try allocator.alloc(MapEntry, entries.len);
                for (entries, 0..) |entry, i| {
                    new_entries[i] = .{
                        .key = try entry.key.clone(allocator),
                        .value = try entry.value.clone(allocator),
                    };
                }
                return .{ .map = new_entries };
            },
            .array => |items| {
                const new_items = try allocator.alloc(AmqpValue, items.len);
                for (items, 0..) |item, i| {
                    new_items[i] = try item.clone(allocator);
                }
                return .{ .array = new_items };
            },
            .described => |d| {
                const new_desc = try allocator.create(AmqpValue);
                const new_val = try allocator.create(AmqpValue);
                new_desc.* = try d.descriptor.clone(allocator);
                new_val.* = try d.value.clone(allocator);
                return .{ .described = .{ .descriptor = new_desc, .value = new_val } };
            },
        };
    }

    /// Free all memory owned by this value.
    pub fn deinit(self: *AmqpValue, allocator: Allocator) void {
        switch (self.*) {
            .null, .boolean, .ubyte, .ushort, .uint, .ulong, .byte, .short, .int, .long, .float, .double, .char, .timestamp, .uuid => {},
            .binary => |b| allocator.free(b),
            .string => |s| allocator.free(s),
            .symbol => |s| allocator.free(s),
            .list => |items| {
                for (items) |*item| {
                    @constCast(item).deinit(allocator);
                }
                allocator.free(items);
            },
            .map => |entries| {
                for (entries) |*entry| {
                    @constCast(&entry.key).deinit(allocator);
                    @constCast(&entry.value).deinit(allocator);
                }
                allocator.free(entries);
            },
            .array => |items| {
                for (items) |*item| {
                    @constCast(item).deinit(allocator);
                }
                allocator.free(items);
            },
            .described => |d| {
                d.descriptor.deinit(allocator);
                allocator.destroy(d.descriptor);
                d.value.deinit(allocator);
                allocator.destroy(d.value);
            },
        }
        self.* = .null;
    }

    /// Return the AMQP type tag name.
    pub fn typeName(self: AmqpValue) []const u8 {
        return @tagName(self);
    }

    /// Check structural equality (deep comparison).
    pub fn eql(a: AmqpValue, b: AmqpValue) bool {
        const Tag = std.meta.Tag(AmqpValue);
        const tag_a: Tag = a;
        const tag_b: Tag = b;
        if (tag_a != tag_b) return false;

        return switch (a) {
            .null => true,
            .boolean => |v| v == b.boolean,
            .ubyte => |v| v == b.ubyte,
            .ushort => |v| v == b.ushort,
            .uint => |v| v == b.uint,
            .ulong => |v| v == b.ulong,
            .byte => |v| v == b.byte,
            .short => |v| v == b.short,
            .int => |v| v == b.int,
            .long => |v| v == b.long,
            .float => |v| v == b.float,
            .double => |v| v == b.double,
            .char => |v| v == b.char,
            .timestamp => |v| v == b.timestamp,
            .uuid => |v| std.mem.eql(u8, &v, &b.uuid),
            .binary => |v| std.mem.eql(u8, v, b.binary),
            .string => |v| std.mem.eql(u8, v, b.string),
            .symbol => |v| std.mem.eql(u8, v, b.symbol),
            .list => |items| {
                const other = b.list;
                if (items.len != other.len) return false;
                for (items, other) |x, y| {
                    if (!x.eql(y)) return false;
                }
                return true;
            },
            .map => |entries| {
                const other = b.map;
                if (entries.len != other.len) return false;
                for (entries, other) |x, y| {
                    if (!x.key.eql(y.key) or !x.value.eql(y.value)) return false;
                }
                return true;
            },
            .array => |items| {
                const other = b.array;
                if (items.len != other.len) return false;
                for (items, other) |x, y| {
                    if (!x.eql(y)) return false;
                }
                return true;
            },
            .described => |d| {
                const other = b.described;
                return d.descriptor.eql(other.descriptor.*) and d.value.eql(other.value.*);
            },
        };
    }
};

// ── Tests ──────────────────────────────────────────────────────────────

test "AmqpValue primitive types" {
    const testing = std.testing;

    const v_null: AmqpValue = .null;
    try testing.expectEqualStrings("null", v_null.typeName());

    const v_bool: AmqpValue = .{ .boolean = true };
    try testing.expect(v_bool.eql(.{ .boolean = true }));
    try testing.expect(!v_bool.eql(.{ .boolean = false }));

    const v_int: AmqpValue = .{ .int = -42 };
    try testing.expect(v_int.eql(.{ .int = -42 }));

    const v_ulong: AmqpValue = .{ .ulong = 0xDEADBEEF };
    try testing.expect(v_ulong.eql(.{ .ulong = 0xDEADBEEF }));
}

test "AmqpValue clone and deinit" {
    const testing = std.testing;
    const allocator = testing.allocator;

    // Clone a string
    const original: AmqpValue = .{ .string = "hello" };
    var cloned = try original.clone(allocator);
    defer cloned.deinit(allocator);
    try testing.expect(original.eql(cloned));

    // Clone a list
    var items = [_]AmqpValue{
        .{ .uint = 1 },
        .{ .uint = 2 },
        .{ .string = "three" },
    };
    const list_val: AmqpValue = .{ .list = &items };
    var cloned_list = try list_val.clone(allocator);
    defer cloned_list.deinit(allocator);
    try testing.expect(list_val.eql(cloned_list));
}

test "AmqpValue map clone" {
    const testing = std.testing;
    const allocator = testing.allocator;

    var entries = [_]MapEntry{
        .{ .key = .{ .symbol = "key1" }, .value = .{ .string = "val1" } },
        .{ .key = .{ .symbol = "key2" }, .value = .{ .uint = 42 } },
    };
    const map_val: AmqpValue = .{ .map = &entries };
    var cloned = try map_val.clone(allocator);
    defer cloned.deinit(allocator);
    try testing.expect(map_val.eql(cloned));
}
