"""Fast tracked-source hygiene check; complements a dedicated secret scanner."""
import pathlib
import re
import subprocess

root = pathlib.Path(__file__).resolve().parents[1]
patterns = [
    rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----",
    rb"gh[pousr]_" + rb"[A-Za-z0-9]{30,}",
    rb"github_pat_" + rb"[A-Za-z0-9_]{30,}",
    rb"AKIA" + rb"[A-Z0-9]{16}",
    rb"(?:/Users/|/home/)[a-zA-Z0-9_.-]+/",
    rb"[A-Za-z]:\\Users\\[a-zA-Z0-9_.-]+\\",
]
paths = subprocess.check_output(["git", "ls-files", "-z"], cwd=root).split(b"\0")
issues = []
for raw in paths:
    if not raw:
        continue
    path = root / raw.decode("utf-8")
    data = path.read_bytes()
    if any(re.search(pattern, data) for pattern in patterns):
        issues.append(str(path.relative_to(root)))
if issues:
    raise SystemExit("Potential sensitive content in: " + ", ".join(issues))
print("Tracked source hygiene: clean")
