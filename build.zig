const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Main library module
    const lib_mod = b.createModule(.{
        .root_source_file = b.path("src/zig/uamqp.zig"),
        .target = target,
        .optimize = optimize,
    });

    // Static library artifact
    const lib = b.addLibrary(.{
        .linkage = .static,
        .name = "uamqp",
        .root_module = lib_mod,
    });
    b.installArtifact(lib);

    // Unit tests
    const test_mod = b.createModule(.{
        .root_source_file = b.path("src/zig/uamqp.zig"),
        .target = target,
        .optimize = optimize,
    });
    const lib_unit_tests = b.addTest(.{
        .root_module = test_mod,
    });
    const run_lib_unit_tests = b.addRunArtifact(lib_unit_tests);

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_lib_unit_tests.step);

    // Examples
    inline for (.{
        .{ "sender", "examples/sender.zig" },
        .{ "receiver", "examples/receiver.zig" },
    }) |example| {
        const exe_mod = b.createModule(.{
            .root_source_file = b.path(example[1]),
            .target = target,
            .optimize = optimize,
        });
        exe_mod.addImport("uamqp", lib_mod);
        const exe = b.addExecutable(.{
            .name = example[0],
            .root_module = exe_mod,
        });
        b.installArtifact(exe);
    }
}

