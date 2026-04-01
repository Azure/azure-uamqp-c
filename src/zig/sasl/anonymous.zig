///! SASL ANONYMOUS mechanism (OASIS spec §5.3.3.1)
const std = @import("std");
const Mechanism = @import("mechanism.zig").Mechanism;

pub const Anonymous = struct {
    pub fn mechanism(self: *Anonymous) Mechanism {
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
        return "ANONYMOUS";
    }

    fn getInitBytes(_: *anyopaque) ?[]const u8 {
        return null;
    }

    fn onChallenge(_: *anyopaque, _: []const u8) ?[]const u8 {
        return null;
    }
};

test "ANONYMOUS mechanism" {
    var anon = Anonymous{};
    const mech = anon.mechanism();
    try std.testing.expectEqualStrings("ANONYMOUS", mech.getMechanismName());
    try std.testing.expect(mech.getInitBytes() == null);
}
