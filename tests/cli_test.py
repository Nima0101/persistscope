"""Black-box scenario contract tests; Python standard library only."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())
ROOT = Path(__file__).resolve().parents[1]


class CliTests(unittest.TestCase):
    def invoke(self, text, *args, code=0):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'input.pscope'
            path.write_bytes(text.encode('utf-8'))
            result = subprocess.run([BINARY, 'check', str(path), *args],
                                    capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        return result

    def test_atomic_replacement_and_replay(self):
        for name, code, status in [('atomic-broken', 1, 'FAIL'), ('atomic-fixed', 0, 'PASS')]:
            path = ROOT / 'examples' / (name + '.pscope')
            result = subprocess.run([BINARY, 'check', str(path), '--json'], capture_output=True,
                                    text=True, check=False)
            self.assertEqual(result.returncode, code, result.stderr)
            data = json.loads(result.stdout)
            self.assertEqual(data['status'], status)
            self.assertEqual(data['schema_version'], 1)
            self.assertEqual(data['model'], 'ordered-file-v1')
            if code:
                witness = data['witness']
                ids = ','.join(map(str, witness['durable_events'])) or '-'
                replay = subprocess.run([BINARY, 'replay', str(path), str(witness['cut']), ids,
                                         '--json'], capture_output=True, text=True, check=False)
                self.assertEqual(replay.returncode, 1, replay.stderr)
                self.assertEqual(json.loads(replay.stdout)['witness'], witness)

    def test_budget_is_not_success(self):
        result = self.invoke('persistscope 1\nrun\ncreate a\nwrite a x\ncheck\n', '--max-nodes', '1',
                             '--json', code=3)
        self.assertEqual(json.loads(result.stdout)['status'], 'INCOMPLETE')

    def test_quoted_data_and_comments(self):
        text = 'persistscope 1\ninitial a "a # \\\" \\\\"\nrun\ncheck\nexpect a "a # \\\" \\\\" # end\n'
        self.invoke(text, '--json')

    def test_invalid_inputs(self):
        cases = [
            '', 'persistscope 2\nrun\ncheck\n',
            'persistscope 1\ninitial a x\ninitial a y\nrun\ncheck\n',
            'persistscope 1\nrun\ncreate ../a x\ncheck\n',
            'persistscope 1\nrun\ncreate a "unterminated\ncheck\n',
            'persistscope 1\nrun\ncreate a "x\\n"\ncheck\n',
            'persistscope 1\nrun\ncreate a "x"suffix\ncheck\n',
            'persistscope 1\nrun\ncreate a \x1b\ncheck\n',
            'persistscope 1\nrun\ncheck\nexpect a\n',
            'persistscope 1\nrun\ncheck\nexists ../a\n',
            'persistscope 1\nrun\ncheck\nexpect a ' + 'x' * 4097 + '\n',
            'persistscope 1\nrun\nack extra\ncheck\n',
            'persistscope 1\nrun\n',
            '#' * 65537,
            'persistscope 1\ninitial a "x\ty"\nrun\ncheck\n',
            # Invalid later operation takes precedence over cut-zero violation.
            'persistscope 1\nrun\nwrite absent x\ncheck\nexists absent\n',
        ]
        for text in cases:
            with self.subTest(text=text[:100]):
                result = self.invoke(text, '--json', code=2)
                self.assertNotIn('\x1b', result.stderr)
                self.assertEqual(json.loads(result.stdout)['status'], 'INVALID')

    def test_invalid_options(self):
        text = 'persistscope 1\nrun\ncheck\n'
        for args in [('--wat',), ('--max-nodes', '-1'), ('--max-nodes', '0'),
                     ('--max-nodes', '10000001'), ('--json', '--json')]:
            self.invoke(text, *args, code=2)

    def test_replay_rejects_invalid_event_sets(self):
        path = ROOT / 'examples' / 'atomic-fixed.pscope'
        # Cut four follows fsync: data event 1 is forced, create event 0 is optional.
        for cut, ids in [('4', '-'), ('4', '99'), ('4', '1,1'), ('4', '1,0'),
                         ('999', '-'), ('0', ''), ('0', '0,'), ('-1', '-')]:
            with self.subTest(cut=cut, ids=ids):
                result = subprocess.run([BINARY, 'replay', str(path), cut, ids, '--json'],
                                        capture_output=True, text=True, check=False)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        result = subprocess.run([BINARY, 'replay', str(path), '0', '-', '--json'],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads(result.stdout)['witness']['reason'], '')

    def test_recovery_and_torn_examples(self):
        for name, code in [('torn-patch', 1), ('journal-recovery', 0)]:
            self.invoke((ROOT / 'examples' / (name + '.pscope')).read_text(), code=code)


if __name__ == '__main__':
    unittest.main()
