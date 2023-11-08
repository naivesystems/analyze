"""Repository rules to auto-generate source files.

Include all generation actions for compiling libz3.
"""

# Refer to third_party/z3/cmake/z3_add_component.cmake:118
def _gen_hpp_from_pyg_impl(ctx):
    files = []
    for in_file in ctx.files.srcs:
        if not in_file.basename.endswith(".pyg"):
            fail("%s is not a valid .pyg file" % in_file.basename)
        out_file = ctx.actions.declare_file(in_file.basename.replace(".pyg", ".hpp"), sibling = in_file)
        ctx.actions.run(
            inputs = [in_file],
            outputs = [out_file],
            progress_message = "Converting .pyg file %s" % in_file.short_path,
            arguments = [in_file.path, out_file.dirname],
            executable = ctx.executable.pyg2hpp,
        )
        files.append(out_file)
    return DefaultInfo(files = depset(files))

gen_hpp_from_pyg = rule(
    implementation = _gen_hpp_from_pyg_impl,
    attrs = {
        "srcs": attr.label_list(allow_files = True, mandatory = True),
        "pyg2hpp": attr.label(
            executable = True,
            cfg = "exec",
            allow_files = True,
            default = Label("//:pyg2hpp"),
        ),
    },
)

# Refer to third_party/z3/src/api/CMakeLists.txt:20
# Generates api related commands.
def _gen_api_hdr_impl(ctx):
    in_files = ctx.files.srcs
    sibling = in_files[0]
    out_files = [
        ctx.actions.declare_file("api_commands.cpp", sibling = sibling),
        ctx.actions.declare_file("api_log_macros.cpp", sibling = sibling),
        ctx.actions.declare_file("api_log_macros.h", sibling = sibling),
    ]
    output_dir = out_files[0].dirname
    ctx.actions.run(
        inputs = in_files,
        outputs = out_files,
        progress_message = "Generating api updates",
        arguments = [x.path for x in in_files] + ["--api_output_dir"] + [output_dir],
        executable = ctx.executable.update_api,
    )
    return DefaultInfo(files = depset(out_files))

gen_api_hdr = rule(
    implementation = _gen_api_hdr_impl,
    attrs = {
        "srcs": attr.label_list(allow_files = True, mandatory = True),
        "update_api": attr.label(
            executable = True,
            cfg = "exec",
            allow_files = True,
            default = Label("//:update_api"),
        ),
    },
)

# Refer to third_party/z3/src/ast/pattern/CMakeLists.txt:9
# Generate database.h for component 'pattern'.
def _gen_pat_db_impl(ctx):
    in_file = ctx.file.input_smt2
    out_file = ctx.actions.declare_file("database.h", sibling = in_file)
    ctx.actions.run(
        inputs = [in_file],
        outputs = [out_file],
        progress_message = "Generating database.h",
        arguments = [in_file.path, out_file.path],
        executable = ctx.executable.mk_pat_db,
    )
    return DefaultInfo(files = depset([out_file]))

gen_pat_db = rule(
    implementation = _gen_pat_db_impl,
    attrs = {
        "input_smt2": attr.label(allow_single_file = True, mandatory = True),
        "mk_pat_db": attr.label(
            executable = True,
            cfg = "exec",
            allow_files = True,
            default = Label("//:mk_pat_db"),
        ),
    },
)

# Refer to third_party/z3/cmake/z3_add_component.cmake:277
# Generate 'install_tactic.deps' as runfile to further generate 'install_tactic.cpp'.
def _gen_tactic_runfile_impl(ctx):
    in_files = ctx.files.tactic_hdrs

    dep_file = ctx.actions.declare_file("install_tactic.deps")
    dep_content = ""
    for f in in_files:
        dep_content = dep_content + f.path + "\n"
    ctx.actions.write(output = dep_file, content = dep_content)

    return [DefaultInfo(
        runfiles = ctx.runfiles(files = [dep_file] + in_files),
    )]

gen_tactic_runfile = rule(
    implementation = _gen_tactic_runfile_impl,
    attrs = {
        "tactic_hdrs": attr.label_list(allow_files = True, mandatory = True),
    },
)

# Refer to third_party/z3/cmake/z3_add_component.cmake:277
# Genreate 'install_tactic.cpp' for adding tactic commands.
def _gen_tactic_impl(ctx):
    in_files = ctx.attr.runfile[DefaultInfo].default_runfiles.files.to_list()
    dep_file = in_files[0]

    out_file = ctx.actions.declare_file("install_tactic.cpp")
    output_dir = out_file.dirname

    ctx.actions.run(
        inputs = in_files,
        outputs = [out_file],
        progress_message = "Generating install_tactic.cpp",
        arguments = [output_dir, dep_file.path],
        executable = ctx.executable.mk_install_tactic_cpp,
    )

    return DefaultInfo(files = depset([out_file]))

gen_tactic = rule(
    implementation = _gen_tactic_impl,
    attrs = {
        "mk_install_tactic_cpp": attr.label(
            executable = True,
            cfg = "exec",
            allow_files = True,
            default = Label("//:mk_install_tactic_cpp"),
        ),
        "runfile": attr.label(
            allow_files = True,
            default = ":gen_tactic_runfile",
        ),
    },
)

# Refer to third_party/z3/cmake/z3_add_component.cmake:351
# Generate 'gparams_register_modules.cpp'.
def _mk_gparams_register_modules_cpp_impl(ctx):
    in_files = ctx.files.hdrs

    out_file = ctx.actions.declare_file("gparams_register_modules.cpp")
    output_dir = out_file.dirname

    ctx.actions.run(
        inputs = in_files,
        outputs = [out_file],
        progress_message = "Generating gparams_register_modules.cpp",
        arguments = [output_dir] + [f.path for f in in_files],
        executable = ctx.executable.mk_gparams_register_modules_cpp,
    )

    return DefaultInfo(files = depset([out_file]))

gen_gparams_register_modules = rule(
    implementation = _mk_gparams_register_modules_cpp_impl,
    attrs = {
        "hdrs": attr.label_list(allow_files = True, mandatory = True),
        "mk_gparams_register_modules_cpp": attr.label(
            executable = True,
            cfg = "exec",
            allow_files = True,
            default = Label("//:mk_gparams_register_modules_cpp"),
        ),
    },
)

# Refer to third_party/z3/cmake/z3_add_component.cmake:315
# Generate 'mem_initializer.cpp' to provide initializer and finalizer for concerned
# objects.
def _mk_mem_initializer_cpp_impl(ctx):
    in_files = ctx.files.hdrs

    out_file = ctx.actions.declare_file("mem_initializer.cpp")
    output_dir = out_file.dirname

    ctx.actions.run(
        inputs = in_files,
        outputs = [out_file],
        progress_message = "Generating mem_initializer.cpp",
        arguments = [output_dir] + [f.path for f in in_files],
        executable = ctx.executable.mk_mem_initializer_cpp,
    )

    return DefaultInfo(files = depset([out_file]))

gen_mem_initializer_cpp = rule(
    implementation = _mk_mem_initializer_cpp_impl,
    attrs = {
        "hdrs": attr.label_list(allow_files = True, mandatory = True),
        "mk_mem_initializer_cpp": attr.label(
            executable = True,
            cfg = "exec",
            allow_files = True,
            default = Label("//:mk_mem_initializer_cpp"),
        ),
    },
)
