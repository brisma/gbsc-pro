"""PlatformIO pre-script of [env:native]: GOLDEN_DIR and WEB_DIR as C string literals with forward slashes (the
backslashes of a Windows path would be escapes inside the literal)."""
Import("env")  # noqa: F821 (SCons)
root = env["PROJECT_DIR"].replace("\\", "/")  # noqa: F821
env.Append(CPPDEFINES=[("GOLDEN_DIR", env.StringifyMacro(root + "/test/golden")),  # noqa: F821
                       ("WEB_DIR", env.StringifyMacro(root + "/public"))])  # noqa: F821
