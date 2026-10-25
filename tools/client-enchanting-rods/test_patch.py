import pathlib,struct,unittest
from build_patch import dbc,patch_spell,recipe_lua

def table(rows,fields,strings=b'\0'):
    return b'WDBC'+struct.pack('<4I',len(rows),fields,fields*4,len(strings))+b''.join(struct.pack('<%dI'%fields,*r) for r in rows)+strings
def row(ident,tool=0,second=0):
    r=[0]*173;r[0]=ident;r[40]=tool;r[41]=second;r[120]=1;return r
def skills(ids):return table([[i+1,333,ident]+[0]*12 for i,ident in enumerate(ids)],15)

class PatchTests(unittest.TestCase):
    def test_only_enchanting_rod_tool_fields_change(self):
        rows=[row(7788,6218),row(20,6339,2901),row(21,2901),row(22,6218),row(23,16207)]
        rows[4][42]=6218;rows[4][50]=1
        original=table(rows,173,b'\0Test enchant\0')
        patched,changes,count,total=patch_spell(original,skills([7788,20,21,23]))
        rr,strings=dbc(patched,173)
        self.assertEqual([e['spell'] for e in changes],[7788,20,23]);self.assertEqual(total,5)
        self.assertEqual(rr[1][41],2901);self.assertEqual(rr[2],tuple(rows[2]));self.assertEqual(rr[3],tuple(rows[3]))
        self.assertEqual(rr[4][42:58],tuple(rows[4][42:58]));self.assertEqual(strings,b'\0Test enchant\0')
        self.assertEqual(patched[:20],original[:20])
        for i,(a,b) in enumerate(zip(original,patched)):
            if a!=b:self.assertIn((i-20)//4%173,[40,41])
    def test_rejects_wrong_format_truncation_and_duplicate_ids(self):
        for bad in [table([[0]*234],234),table([row(7788,6218)],173,b'\0Test\0')[:-1],table([row(7788,6218)]*2,173,b'\0Test\0')]:
            self.assertRaises(ValueError,patch_spell,bad,skills([7788]))
    def test_rejects_unexpected_minor_striking_contract(self):
        self.assertRaises(ValueError,patch_spell,table([row(7788,0)],173,b'\0Test\0'),skills([7788]))
        self.assertRaises(ValueError,patch_spell,table([row(7788,6218)],173,b'\0Test\0'),skills([]))
    def test_generated_aliases_preserve_rank_and_fail_ambiguous_requirements(self):
        data=[{'spell':7788,'required_rods':[6218],'names':[('Enchant Weapon','Minor')]}]
        lua=recipe_lua(data,'abc');self.assertIn('Enchant Weapon\\031Minor',lua)
        data.append({'spell':20,'required_rods':[6339],'names':[('Enchant Weapon','Minor')]})
        self.assertRaises(ValueError,recipe_lua,data,'abc')

if __name__=='__main__':unittest.main()
