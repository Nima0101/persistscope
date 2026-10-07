"""Check C++ formatting with the pinned clang-format 19 tool."""
import pathlib
import subprocess

root = pathlib.Path(__file__).resolve().parents[1]
files = sorted(str(p) for directory in ("src", "include", "tests", "examples", "benchmarks", "safe-update/src", "safe-update/include", "safe-update/tests", "safe-update/examples")
               for p in (root / directory).rglob("*")
               if p.suffix in (".cpp", ".hpp"))
subprocess.run(["clang-format", "--dry-run", "--Werror", *files], check=True)
