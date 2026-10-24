"""Run the production assembler against small synthetic native raw models."""
import argparse
import pathlib
import struct
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument('--assembler', required=True)
options, remaining = parser.parse_known_args()


def spawn(ident, name, flags, bounds=(500., 500., 20., 1100., 1100., 40.)):
    data = struct.pack('<IHI3f3ff', flags, 71, ident, 100., 200., 30., 0., 0., 0., 1.)
    if flags & 4:
        data += struct.pack('<6f', *bounds)
    name = name.encode()
    return data + struct.pack('<I', len(name)) + name


class AssemblerCoverageTest(unittest.TestCase):
    def test_bounds_complete_wmo_references_once_preserving_global_and_m2_semantics(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            raw, out = root / 'raw', root / 'out'
            raw.mkdir()
            out.mkdir()
            wmo = spawn(12, 'test.wmo', 4)
            global_wmo = spawn(13, 'test.wmo', 6)
            m2 = spawn(14, 'test.m2', 1)
            raw.joinpath('dir_bin').write_bytes(
                struct.pack('<III', 0, 0, 0) + wmo + struct.pack('<III', 0, 1, 1) + wmo
                + struct.pack('<III', 1, 65, 65) + global_wmo
                + struct.pack('<III', 2, 5, 5) + m2)
            raw.joinpath('test.wmo').write_bytes(b'VMAP005\0' + struct.pack('<III', 0, 0, 123))
            # One triangle lets the native M2 bounds calculation run normally.
            group = struct.pack('<II6fI', 0, 1, 0., 0., 0., 1., 1., 1., 0)
            group += b'GRP ' + struct.pack('<II', 4, 0)
            group += b'INDX' + struct.pack('<II3H', 10, 3, 0, 1, 2)
            group += b'VERT' + struct.pack('<II9f', 40, 3, 0., 0., 0., 1., 0., 0., 0., 1., 1.)
            raw.joinpath('test.m2').write_bytes(b'VMAP005\0' + struct.pack('<III', 3, 1, 0) + group)
            result = subprocess.run([options.assembler, str(raw), str(out)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            expected = {f'000_{x:02}_{y:02}.vmtile' for x in range(3) for y in range(3)}
            self.assertEqual({p.name for p in out.glob('000_*.vmtile')}, expected)
            for name in expected:
                self.assertEqual(out.joinpath(name).read_bytes(), b'VMAP_6.0' + struct.pack('<I', 1) + wmo + struct.pack('<I', 0))
            self.assertTrue(out.joinpath('001.vmtree').exists())
            self.assertFalse(list(out.glob('001_*.vmtile')))
            self.assertEqual([p.name for p in out.glob('002_*.vmtile')], ['002_05_05.vmtile'])


if __name__ == '__main__':
    unittest.main(argv=[__file__] + remaining)
