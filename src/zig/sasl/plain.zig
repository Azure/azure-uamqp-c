///! SASL PLAIN mechanism (RFC 4616)
///!
///! Encodes credentials as: \0<authcid>\0<passwd>
const std = @import("std");
const Allocator = std.mem.Allocator;
const Mechanism = @import("mechanism.zig").Mechanism;

pub const Plain = struct {
    allocator: Allocator,
    authcid: []const u8,
    passwd: []const u8,
    authzid: ?[]const u8,
    init_bytes: ?[]u8 = null,

    pub fn init(allocator: Allocator, authcid: []const u8, passwd: []const u8, authzid: ?[]const u8) Plain {
        return .{
            .allocator = allocator,
            .authcid = authcid,
            .passwd = passwd,
            .authzid = authzid,
        };
    }

    pub fn deinit(self: *Plain) void {
        if (self.init_bytes) |bytes| {
            self.allocator.free(bytes);
        }
    }

    pub fn mechanism(self: *Plain) Mechanism {
        return .{
            .ptr = @ptrCast(self),
            .vtable = &vtable,
        };
    }

    const vtable = Mechanism.VTable{
        .get_mechanism_name = getMechanismName,
        .get_init_bytes = getInitBytes,
        .on_challenge = onChallenge,
    };

    fn getMechanismName(_: *anyopaque) []const u8 {
        return "PLAIN";
    }

    fn getInitBytes(ptr: *anyopaque) ?[]const u8 {
        const self: *Plain = @ptrCast(@alignCast(ptr));
        if (self.init_bytes) |bytes| return bytes;

        // Format: [authzid] \0 authcid \0 passwd
        const authzid_len = if (self.authzid) |a| a.len else 0;
        const total = authzid_len + 1 + self.authcid.len + 1 + self.passwd.len;

        const buf = self.allocator.alloc(u8, total) catch return null;
        var offset: usize = 0;

        if (self.authzid) |a| {
            @memcpy(buf[0..a.len], a);
            offset += a.len;
        }
        buf[offset] = 0;
        offset += 1;
        @memcpy(buf[offset .. offset + self.authcid.len], self.authcid);
        offset += self.authcid.len;
        buf[offset] = 0;
        offset += 1;
        @memcpy(buf[offset .. offset + self.passwd.len], self.passwd);

        self.init_bytes = buf;
        return buf;
    }

    fn onChallenge(_: *anyopaque, _: []const u8) ?[]const u8 {
        // PLAIN doesn't support challenges
        return null;
    }
};

test "PLAIN mechanism init bytes" {
    const allocator = std.testing.allocator;
    var plain = Plain.init(allocator, "user", "pass", null);
    defer plain.deinit();

    const mech = plain.mechanism();
    try std.testing.expectEqualStrings("PLAIN", mech.getMechanismName());

    const init_bytes = mech.getInitBytes().?;
    // Expected: \0user\0pass
    try std.testing.expectEqual(@as(usize, 10), init_bytes.len);
    try std.testing.expectEqual(@as(u8, 0), init_bytes[0]);
    try std.testing.expectEqualStrings("user", init_bytes[1..5]);
    try std.testing.expectEqual(@as(u8, 0), init_bytes[5]);
    try std.testing.expectEqualStrings("pass", init_bytes[6..10]);
}

test "PLAIN mechanism with authzid" {
    const allocator = std.testing.allocator;
    var plain = Plain.init(allocator, "user", "pass", "admin");
    defer plain.deinit();

    const mech = plain.mechanism();
    const init_bytes = mech.getInitBytes().?;
    // Expected: admin\0user\0pass
    try std.testing.expectEqual(@as(usize, 15), init_bytes.len);
    try std.testing.expectEqualStrings("admin", init_bytes[0..5]);
    try std.testing.expectEqual(@as(u8, 0), init_bytes[5]);
    try std.testing.expectEqualStrings("user", init_bytes[6..10]);
}
