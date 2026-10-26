"""Exercise the production native tile reader/writer using synthetic records."""
import argparse
import pathlib
import struct
import subprocess
import tempfile
import unittest

PARSER = argparse.ArgumentParser()
PARSER.add_argument('--tool', required=True)
OPTIONS, REMAINING = PARSER.parse_known_args()
TOOL = str(pathlib.Path(OPTIONS.tool).resolve())

def record(ident, name, flags=4):
    raw = struct.pack('<IHI3f3ff', flags, 71, ident, 100., 200., 30., 0., 10., 0., 1.)
    if flags & 4:
        raw += struct.pack('<6f', 90., 190., 20., 110., 210., 40.)
    data = name.encode()
    return raw + struct.pack('<I', len(data)) + data + struct.pack('<I', ident + 8)

def tile(*records):
    return b'VMAP_6.0' + struct.pack('<I', len(records)) + b''.join(records)

class NativeModelToolTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.temp.name)
        self.source = self.root / 'source.vmtile'
        self.output = self.root / 'output.vmtile'
        self.other = record(15, 'brokengateside01.m2', flags=1)
        self.source.write_bytes(tile(record(12, 'Magetower.wmo'), self.other))

    def tearDown(self):
        self.temp.cleanup()

    def rebind(self, ident='12', old='Magetower.wmo', new='wmo_correct_magetower.wmo'):
        return subprocess.run([TOOL, '--rebind', str(self.source), str(self.output), ident, old, new], capture_output=True)

    def test_only_target_name_changes_and_native_nolos_side_effect_does_not_rewrite_other_records(self):
        self.assertEqual(self.rebind().returncode, 0)
        self.assertEqual(self.output.read_bytes(), tile(record(12, 'wmo_correct_magetower.wmo'), self.other))

    def test_identity_rebind_is_byte_exact(self):
        self.assertEqual(self.rebind(new='Magetower.wmo').returncode, 0)
        self.assertEqual(self.output.read_bytes(), self.source.read_bytes())

    def test_missing_or_wrong_identity_never_writes(self):
        for ident, old in [('99', 'Magetower.wmo'), ('12', 'Different.wmo'), ('-1', 'Magetower.wmo'), ('', 'Magetower.wmo'), ('4294967296', 'Magetower.wmo')]:
            with self.subTest(ident=ident, old=old):
                self.assertNotEqual(self.rebind(ident, old).returncode, 0)
                self.assertFalse(self.output.exists())

    def test_duplicate_spawn_rejected(self):
        self.source.write_bytes(tile(record(12, 'Magetower.wmo'), record(12, 'Magetower.wmo')))
        self.assertNotEqual(self.rebind().returncode, 0)
        self.assertFalse(self.output.exists())

    def test_existing_output_never_overwritten(self):
        self.output.write_bytes(b'keep')
        self.assertNotEqual(self.rebind().returncode, 0)
        self.assertEqual(self.output.read_bytes(), b'keep')

    def test_malformed_inputs_never_write(self):
        for data in [b'', b'WRONGHDR' + struct.pack('<I', 0), tile(record(12, 'Magetower.wmo'))[:-2], tile(record(12, 'Magetower.wmo')) + b'extra']:
            with self.subTest(length=len(data)):
                self.source.write_bytes(data)
                self.assertNotEqual(self.rebind().returncode, 0)
                self.assertFalse(self.output.exists())

    def test_path_or_non_wmo_rebinding_rejected(self):
        for name in ['../escape.wmo', 'a/b.wmo', 'wrong.m2', '']:
            with self.subTest(name=name):
                self.assertNotEqual(self.rebind(new=name).returncode, 0)
                self.assertFalse(self.output.exists())

    def test_assembly_refuses_existing_output(self):
        output = self.root / 'Magetower.wmo.vmo'
        output.write_bytes(b'keep')
        result = subprocess.run([TOOL, '--assemble', str(self.root), str(self.root), 'Magetower.wmo'], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(output.read_bytes(), b'keep')

    def test_inspection_uses_native_records_without_writing(self):
        result = subprocess.run([TOOL, '--inspect', str(self.source)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('12\t20\tMagetower.wmo', result.stdout)
        self.assertFalse(self.output.exists())

    def include(self, data=None, target=None, ident='12', existing=None):
        self.include_source = self.root / '000_00_01.vmtile'
        self.include_target = self.root / (target or '000_00_00.vmtile')
        self.include_source.write_bytes(data if data is not None else tile(record(12, 'Stormwind.wmo')))
        self.include_target.write_bytes(existing if existing is not None else tile(self.other))
        return subprocess.run([TOOL, '--include', str(self.include_source), str(self.include_target), str(self.output), ident], capture_output=True)

    def test_include_preserves_native_index_transform_and_every_existing_byte(self):
        self.assertEqual(self.include().returncode, 0)
        self.assertEqual(self.output.read_bytes(), tile(self.other, record(12, 'Stormwind.wmo')))
        self.assertEqual(self.include_target.read_bytes(), tile(self.other))

    def test_include_rejects_outside_bounds_different_map_global_m2_missing_and_duplicate(self):
        cases = [
            {'target': '000_01_00.vmtile'}, {'target': '001_00_00.vmtile'},
            {'data': tile(record(12, 'Stormwind.wmo', flags=6))},
            {'data': tile(record(12, 'test.m2', flags=5))},
            {'data': tile(record(12, 'Stormwind.wmo', flags=0))},
            {'ident': '99'}, {'ident': '-1'}, {'ident': '4294967296'},
            {'data': tile(record(12, 'Stormwind.wmo'), record(12, 'Stormwind.wmo'))},
            {'existing': tile(record(12, 'Stormwind.wmo'))},
            {'data': b'invalid'}, {'existing': tile(self.other) + b'trailing'},
        ]
        for case in cases:
            with self.subTest(case=case):
                self.assertNotEqual(self.include(**case).returncode, 0)
                self.assertFalse(self.output.exists())

    def test_include_does_not_overwrite_output(self):
        self.output.write_bytes(b'keep')
        self.assertNotEqual(self.include().returncode, 0)
        self.assertEqual(self.output.read_bytes(), b'keep')

if __name__ == '__main__':
    unittest.main(argv=[__file__] + REMAINING)
