"""Exercise the production assembler with colliding MDDF/MODF placement IDs."""
import argparse,pathlib,struct,subprocess,tempfile,unittest
parser=argparse.ArgumentParser();parser.add_argument('--assembler',required=True);options,remaining=parser.parse_known_args()
def spawn(ident,name,flags,bounds=(500.,500.,20.,1100.,1100.,40.)):
 b=struct.pack('<IHI7f',flags,71,ident,100.,200.,30.,0.,0.,0.,1.)
 if flags&4:b+=struct.pack('<6f',*bounds)
 name=name.encode();return b+struct.pack('<I',len(name))+name
def models(raw):
 raw.joinpath('test.wmo').write_bytes(b'VMAP005\0'+struct.pack('<III',0,0,123))
 group=struct.pack('<II6fI',0,1,0.,0.,0.,1.,1.,1.,0)
 group+=b'GRP '+struct.pack('<II',4,0)
 group+=b'INDX'+struct.pack('<II3H',10,3,0,1,2)
 group+=b'VERT'+struct.pack('<II9f',40,3,0.,0.,0.,1.,0.,0.,0.,1.,1.)
 raw.joinpath('test.m2').write_bytes(b'VMAP005\0'+struct.pack('<III',3,1,0)+group)
def read_tile(path):
 b=path.read_bytes();assert b[:8]==b'VMAP_6.0';count=struct.unpack_from('<I',b,8)[0];o=12;out=[]
 for _ in range(count):
  flags,adt,ident,*trans=struct.unpack_from('<IHI7f',b,o);o+=38
  if flags&4:o+=24
  n=struct.unpack_from('<I',b,o)[0];o+=4;name=b[o:o+n].decode();o+=n
  index=struct.unpack_from('<I',b,o)[0];o+=4;out.append((ident,name,index,flags))
 assert o==len(b);return out
class PlacementIdentityTest(unittest.TestCase):
 def test_same_id_m2_and_wmo_survive_in_both_input_orders(self):
  for m2_first in [True,False]:
   with self.subTest(m2_first=m2_first),tempfile.TemporaryDirectory() as directory:
    root=pathlib.Path(directory);raw=root/'raw';out=root/'out';raw.mkdir();out.mkdir();models(raw)
    wmo=spawn(5525455,'test.wmo',4);m2=spawn(5525455,'test.m2',1)
    records=[struct.pack('<III',0,5,5)+m2,struct.pack('<III',0,0,0)+wmo]
    if not m2_first:records.reverse()
    # A repeated WMO on another tile must retain its original index.
    records.append(struct.pack('<III',0,1,1)+wmo)
    raw.joinpath('dir_bin').write_bytes(b''.join(records))
    p=subprocess.run([options.assembler,str(raw),str(out)],capture_output=True,text=True)
    self.assertEqual(p.returncode,0,p.stdout+p.stderr)
    for x in range(3):
     for y in range(3):
      self.assertEqual(read_tile(out/f'000_{x:02}_{y:02}.vmtile'),[(5525455,'test.wmo',0,4)])
    self.assertEqual(read_tile(out/'000_05_05.vmtile'),[(5525455,'test.m2',1,5)])
    self.assertTrue(out.joinpath('test.wmo.vmo').exists());self.assertTrue(out.joinpath('test.m2.vmo').exists())
 def test_same_kind_repeated_id_keeps_existing_first_record_semantics(self):
  with tempfile.TemporaryDirectory() as directory:
   root=pathlib.Path(directory);raw=root/'raw';out=root/'out';raw.mkdir();out.mkdir();models(raw)
   first=spawn(17,'test.wmo',4);second=spawn(17,'test.wmo',4,(550.,550.,21.,1150.,1150.,41.))
   raw.joinpath('dir_bin').write_bytes(struct.pack('<III',0,0,0)+first+struct.pack('<III',0,1,1)+second)
   p=subprocess.run([options.assembler,str(raw),str(out)],capture_output=True,text=True)
   self.assertEqual(p.returncode,0,p.stdout+p.stderr)
   for tile in out.glob('000_*.vmtile'):
    self.assertEqual(tile.read_bytes(),b'VMAP_6.0'+struct.pack('<I',1)+first+struct.pack('<I',0))
if __name__=='__main__':unittest.main(argv=[__file__]+remaining)
