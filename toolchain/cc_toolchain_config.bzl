load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
load("@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl", "feature", "flag_group", "flag_set", "tool_path")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/toolchains:cc_toolchain_config_info.bzl", "CcToolchainConfigInfo")
load("//toolchain:abs_path.bzl", "module_abs_path")

all_link_actions = [
    ACTION_NAMES.cpp_link_executable,
    ACTION_NAMES.cpp_link_dynamic_library,
    ACTION_NAMES.cpp_link_nodeps_dynamic_library,
]

def _impl(ctx):
    clang_path = module_abs_path + "/toolchain/clang"
    tool_paths = [
        tool_path(
            name = "gcc",  # Compiler is referenced by the name "gcc" for historic reasons.
            path = clang_path + "/bin/clang++",
        ),
        tool_path(
            name = "ld",
            path = clang_path + "/bin/ld.lld",
        ),
        tool_path(
            name = "ar",
            path = clang_path + "/bin/llvm-ar",
        ),
        tool_path(
            name = "cpp",
            path = clang_path + "/bin/clang-cpp",
        ),
        tool_path(
            name = "gcov",
            path = clang_path + "/bin/llvm-cov",
        ),
        tool_path(
            name = "nm",
            path = clang_path + "/bin/llvm-nm",
        ),
        tool_path(
            name = "objdump",
            path = clang_path + "/bin/llvm-objdump",
        ),
        tool_path(
            name = "strip",
            path = clang_path + "/bin/llvm-strip",
        ),
    ]

    features = [
        feature(
            name = "default_compiler_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [ACTION_NAMES.cpp_compile],
                    flag_groups = ([
                        flag_group(
                            flags = [
                                "-stdlib=libc++",
                            ],
                        ),
                    ]),
                ),
            ],
        ),
        feature(
            name = "default_linker_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = all_link_actions,
                    flag_groups = ([
                        flag_group(
                            flags = [
                                "-fuse-ld=lld",
                                "-lc++",
                            ],
                        ),
                        # Make libc++ runtime available when running tests.
                        flag_group(
                            expand_if_true = "is_cc_test",
                            flags = ["-Wl,-rpath=" + clang_path + "/lib/x86_64-unknown-linux-gnu"],
                        ),
                    ]),
                ),
            ],
        ),
        feature(
            name = "asan",
            flag_sets = [
                flag_set(
                    actions = [ACTION_NAMES.cpp_compile],
                    flag_groups = ([
                        flag_group(
                            flags = [
                                "-fsanitize=address",
                                "-DADDRESS_SANITIZER",
                                "-g",
                                "-fno-omit-frame-pointer",
                            ],
                        ),
                    ]),
                ),
                flag_set(
                    actions = all_link_actions,
                    flag_groups = ([
                        flag_group(
                            flags = [
                                "-fsanitize=address",
                            ],
                        ),
                    ]),
                ),
            ],
        ),
        feature(
            name = "tsan",
            flag_sets = [
                flag_set(
                    actions = [ACTION_NAMES.cpp_compile],
                    flag_groups = ([
                        flag_group(
                            flags = [
                                "-fsanitize=thread",
                                "-DTHREAD_SANITIZER",
                                "-g",
                            ],
                        ),
                    ]),
                ),
                flag_set(
                    actions = all_link_actions,
                    flag_groups = ([
                        flag_group(
                            flags = [
                                "-fsanitize=thread",
                            ],
                        ),
                    ]),
                ),
            ],
        ),
    ]

    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        features = features,
        cxx_builtin_include_directories = [
            clang_path + "/include/x86_64-unknown-linux-gnu/c++/v1",
            clang_path + "/include/c++/v1",
            clang_path + "/lib/clang/22/include",
            "/usr/include",
            # Needed for ASAN's asan_ignorelist.txt
            clang_path + "/lib/clang/22/share",
        ],
        toolchain_identifier = "k8-clang-toolchain",
        host_system_name = "local",
        target_system_name = "local",
        target_cpu = "k8",
        target_libc = "unknown",
        compiler = "clang",
        abi_version = "unknown",
        abi_libc_version = "unknown",
        tool_paths = tool_paths,
    )

cc_toolchain_config = rule(
    implementation = _impl,
    attrs = {},
    provides = [CcToolchainConfigInfo],
)
