///! SASL Mechanism interface
///!
///! Defines the trait (vtable) for SASL authentication mechanisms.
const std = @import("std");

/// SASL mechanism interface — implemented by ANONYMOUS, PLAIN, MSSBCBS.
pub const Mechanism = struct {
    ptr: *anyopaque,
    vtable: *const VTable,

    pub const VTable = struct {
        get_mechanism_name: *const fn (ptr: *anyopaque) []const u8,
        get_init_bytes: *const fn (ptr: *anyopaque) ?[]const u8,
        on_challenge: *const fn (ptr: *anyopaque, challenge: []const u8) ?[]const u8,
    };

    pub fn getMechanismName(self: Mechanism) []const u8 {
        return self.vtable.get_mechanism_name(self.ptr);
    }

    pub fn getInitBytes(self: Mechanism) ?[]const u8 {
        return self.vtable.get_init_bytes(self.ptr);
    }

    pub fn onChallenge(self: Mechanism, challenge: []const u8) ?[]const u8 {
        return self.vtable.on_challenge(self.ptr, challenge);
    }
};

test "Mechanism vtable compiles" {
    // Compile-time check that the interface is well-formed
    const info = @typeInfo(Mechanism);
    try std.testing.expect(info == .@"struct");
}
