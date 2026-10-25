"""Audit and package a generated candidate; does not install or launch WoW."""
import argparse,hashlib,json,pathlib,shutil,struct,sys,zipfile
from build_patch import client_identity,patch_spell,read_overlay,recipe_lua,SPELL_PATH,SKILL_PATH

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate',required=True,type=pathlib.Path)
    parser.add_argument('--mpyq-dir',type=pathlib.Path)
    args=parser.parse_args()
    if args.mpyq_dir:sys.path.insert(0,str(args.mpyq_dir))
    import mpyq
    root=args.candidate.resolve();manifest=json.loads((root/'manifest.json').read_text());client=pathlib.Path(manifest['client_root'])
    found,providers=read_overlay(client,mpyq);original,source=found[SPELL_PATH];skills,_=found[SKILL_PATH]
    expected,changes,different,total=patch_spell(original,skills)
    assert hashlib.sha256(original).hexdigest()==manifest['original_spell_sha256']
    archive=mpyq.MPQArchive(str(root/'Data/patch-Z.MPQ'),listfile=False)
    actual=archive.read_file(SPELL_PATH)
    assert actual==expected,'Independent MPQ reader output differs'
    # StormLib encrypts its internal listfile. The one game-data block is not
    # encrypted; validate both known hash entries rather than decoding metadata.
    assert len(archive.block_table)==2
    assert {archive.get_hash_table_entry(name).block_table_index for name in [SPELL_PATH,'(listfile)']}=={0,1}
    assert archive.header['format_version']==0,'Archive is not classic-compatible'
    addon=root/'Interface/AddOns/WarbandEnchantingRods'
    assert (addon/'Recipes.lua').read_text(encoding='utf8')==recipe_lua(changes,manifest['patched_spell_sha256'])
    manifest.update(client_identity(client,source))
    manifest['addon_sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in addon.iterdir() if p.is_file()}
    assert hashlib.sha256((root/'Data/patch-Z.MPQ').read_bytes()).hexdigest()==manifest['mpq_sha256']
    manifest['independent_mpq_roundtrip']='mpyq byte-identical; one game-data file; classic header version 0'
    (root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({'changed_recipes':len(changes),'changed_bytes':different,'preserved_rows':total,'independent_roundtrip':manifest['independent_mpq_roundtrip']},indent=2))

if __name__=='__main__':main()
