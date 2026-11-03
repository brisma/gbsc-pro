// The Python interpreter the tools spawn: PYTHON when set (tools/dev.py sets it), else "python" on Windows (there is
// no python3 there) and "python3" elsewhere
export const python = () => process.env.PYTHON || (process.platform === "win32" ? "python" : "python3");
