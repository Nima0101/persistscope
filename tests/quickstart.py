"""Exercise the documented demo expectations on every CI platform."""
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
binary = pathlib.Path(sys.argv[1]).resolve()
if sys.platform == "win32":
    binary = binary.with_suffix(".exe")
for name, expected in (("atomic-broken", 1), ("atomic-fixed", 0),
                       ("torn-patch", 1), ("journal-recovery", 0)):
    result = subprocess.run([str(binary), "check", str(root / "examples" / (name + ".pscope"))],
                            capture_output=True, text=True, timeout=30)
    if result.returncode != expected:
        raise RuntimeError(f"{name}: expected {expected}, got {result.returncode}: {result.stdout} {result.stderr}")
    print(name + ": verified")
