"""Check the fixed commit protocol and replay the missing-directory-sync control."""
import json
import pathlib
import subprocess
import sys
root = pathlib.Path(__file__).resolve().parents[1]
for name, code in (("fixed", 0), ("broken", 1)):
    result = subprocess.run([sys.argv[1], "check", str(root / "models" / f"activation-{name}.pscope"), "--json"], capture_output=True, text=True)
    print(result.stdout)
    assert result.returncode == code, (result.returncode, result.stderr)
    document = json.loads(result.stdout)
    assert document["status"] == ("PASS" if code == 0 else "FAIL")
    if code == 1:
        witness = document["witness"]
        events = ",".join(map(str, witness["durable_events"])) or "-"
        replay = subprocess.run([sys.argv[1], "replay", str(root / "models" / f"activation-{name}.pscope"), str(witness["cut"]), events, "--json"], capture_output=True, text=True)
        print(replay.stdout)
        assert replay.returncode == 1 and json.loads(replay.stdout)["witness"] == witness
print("Fixed model passed; broken commit protocol rejected")
