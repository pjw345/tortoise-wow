"""Create a staged, exact-client Spell.dbc compatibility patch. Never installs it."""
import argparse, hashlib, json, pathlib, shutil, struct, sys

RODS = [6218, 6339, 11130, 11145, 16207]
ROD_NAMES = ['Runed Copper Rod', 'Runed Silver Rod', 'Runed Golden Rod', 'Runed Truesilver Rod', 'Runed Arcanite Rod']
SPELL_PATH = r'DBFilesClient\Spell.dbc'
SKILL_PATH = r'DBFilesClient\SkillLineAbility.dbc'

def sha(data): return hashlib.sha256(data).hexdigest()

def client_identity(client, source):
    archives = sorted([p for p in (client/'Data').iterdir() if p.suffix.lower()=='.mpq'],key=lambda p:p.name.lower())
    return {'client_exe_sha256': sha((client/'WoW.exe').read_bytes()),
            'source_archive_sha256': sha((client/'Data'/source).read_bytes()),
            'client_archives': [{'name':p.name,'bytes':p.stat().st_size,'modified_ticks':p.stat().st_mtime_ns//100+621355968000000000} for p in archives]}

def dbc(data, expected_fields):
    if len(data) < 20: raise ValueError('Truncated DBC')
    magic, count, fields, size, strings = struct.unpack_from('<4s4I', data)
    if magic != b'WDBC' or fields != expected_fields or size != fields * 4 or len(data) != 20 + count * size + strings:
        raise ValueError('Unexpected DBC format; refusing to patch')
    rows = [struct.unpack_from('<%dI' % fields, data, 20 + i * size) for i in range(count)]
    if len({row[0] for row in rows}) != count: raise ValueError('Duplicate spell/ability IDs')
    return rows, data[20 + count * size:]

def patch_spell(spell_data, skill_data):
    rows, strings = dbc(spell_data, 173)
    abilities, _ = dbc(skill_data, 15)
    enchanting = {row[2] for row in abilities if row[1] == 333}
    if 7788 not in enchanting: raise ValueError('Minor Striking missing from Enchanting skill data')
    lookup = {row[0]: row for row in rows}
    if lookup[7788][40:42] != (6218, 0): raise ValueError('Minor Striking tool contract differs')
    changed = bytearray(spell_data); changes = []; allowed_offsets = set()
    def text(offset):
        if offset >= len(strings): raise ValueError('Invalid string offset')
        return strings[offset:].split(b'\0', 1)[0].decode('utf8')
    for index, row in enumerate(rows):
        if row[0] not in enchanting: continue
        tools = [(slot, row[slot]) for slot in (40, 41) if row[slot] in RODS]
        if not tools: continue
        names = [(text(row[120+i]), text(row[129+i])) for i in range(8) if row[120+i]]
        if not names or not names[0][0]: raise ValueError('Missing enchanting recipe name')
        for slot, required in tools:
            offset = 20 + index * 173 * 4 + slot * 4
            struct.pack_into('<I', changed, offset, 0); allowed_offsets.update(range(offset, offset + 4))
        changes.append({'spell': row[0], 'name': names[0][0], 'names': names,
                        'required_rods': [required for slot, required in tools],
                        'tool_fields': [slot for slot, required in tools],
                        'reagents': list(row[42:50]), 'reagent_counts': list(row[50:58])})
    actual = {i for i, (a, b) in enumerate(zip(spell_data, changed)) if a != b}
    if not actual or not actual <= allowed_offsets: raise AssertionError('Unexpected changed bytes')
    # Re-read the output and verify every other field, row, string and header.
    new_rows, new_strings = dbc(changed, 173)
    assert new_strings == strings and changed[:20] == spell_data[:20]
    edited = {entry['spell']: entry for entry in changes}
    for old, new in zip(rows, new_rows):
        slots = edited.get(old[0], {}).get('tool_fields', [])
        assert all(a == b or (i in slots and b == 0) for i, (a, b) in enumerate(zip(old, new)))
    return bytes(changed), changes, len(actual), len(rows)

def archive_rank(path):
    name = path.stem.lower()
    if name.startswith('patch-'):
        suffix = name[6:]
        return (3, int(suffix)) if suffix.isdigit() else (4, suffix)
    return (2, '') if name == 'patch' else (1, name)

def read_overlay(client, mpyq):
    found = {}; providers = {SPELL_PATH: [], SKILL_PATH: []}
    archives = sorted([p for p in (client/'Data').iterdir() if p.suffix.lower() == '.mpq'], key=archive_rank)
    for path in archives:
        archive = mpyq.MPQArchive(str(path), listfile=False)
        try:
            for name in providers:
                data = archive.read_file(name)
                if data:
                    found[name] = (data, path.name)
                    providers[name].append({'archive': path.name, 'sha256': sha(data)})
        finally: archive.file.close()
    if len(found) != 2: raise ValueError('Missing client Spell/SkillLineAbility data')
    return found, providers

def lua_string(value):
    # Decimal UTF-8 byte escapes are also valid under WoW's Lua 5.0 parser.
    return '"' + ''.join(chr(b) if 32 <= b < 127 and b not in (34, 92) else '\\%03d' % b for b in value.encode('utf8')) + '"'

def recipe_lua(changes, patched_hash):
    lines = ['-- Generated from this exact client; install together with patch-Z.MPQ.',
             'WarbandEnchantingRodsData = { version = 1, patchedSpellHash = '+lua_string(patched_hash)+',',
             'rods = {'+', '.join(str(x) for x in RODS)+'},', 'rodNames = {'+', '.join(lua_string(x) for x in ROD_NAMES)+'},', 'recipes = {']
    names = {}
    for entry in changes:
        tiers = [RODS.index(rod)+1 for rod in entry['required_rods']]
        lines.append('[%d] = {%s},' % (entry['spell'], ','.join(map(str, tiers))))
        for name, rank in entry['names']:
            key = name + '\x1f' + rank
            if key in names and names[key][1] != tiers: raise ValueError('Ambiguous recipe name: '+name)
            names[key] = (entry['spell'], tiers)
    lines += ['}, byName = {']
    lines += ['[%s] = %d,' % (lua_string(name), value[0]) for name, value in sorted(names.items())]
    lines += ['} }', '']
    return '\n'.join(lines)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client', required=True, type=pathlib.Path)
    parser.add_argument('--output', required=True, type=pathlib.Path)
    parser.add_argument('--mpyq-dir', type=pathlib.Path)
    args = parser.parse_args()
    if args.mpyq_dir: sys.path.insert(0, str(args.mpyq_dir))
    import mpyq
    client = args.client.resolve(); output = args.output.resolve()
    if client == output or client in output.parents: raise ValueError('Output must be outside the installed client')
    if output.exists(): raise FileExistsError('Refusing to overwrite an existing output directory')
    if (client/'Data/patch-Z.MPQ').exists(): raise FileExistsError('patch-Z.MPQ already exists; choose another reviewed filename')
    found, providers = read_overlay(client, mpyq)
    original, source = found[SPELL_PATH]; skills, skill_source = found[SKILL_PATH]
    patched, changes, bytes_changed, count = patch_spell(original, skills)
    output.mkdir(parents=True); (output/'raw').mkdir()
    (output/'raw/Spell.dbc').write_bytes(patched)
    addon = output/'Interface/AddOns/WarbandEnchantingRods'; addon.mkdir(parents=True)
    source_dir = pathlib.Path(__file__).parent/'WarbandEnchantingRods'
    for name in ['WarbandEnchantingRods.lua', 'WarbandEnchantingRods.toc']: shutil.copyfile(source_dir/name, addon/name)
    (addon/'Recipes.lua').write_text(recipe_lua(changes, sha(patched)), encoding='utf8')
    shutil.copyfile(pathlib.Path(__file__).parent/'Install.ps1', output/'Install.ps1')
    manifest = {'version': '1.0.0', 'status': 'offline candidate; not installed or gameplay accepted',
                'client_root': str(client), 'spell_source': source, 'skill_source': skill_source,
                'format_fields': 173, 'spell_rows': count, 'changed_recipe_count': len(changes),
                'changed_byte_count': bytes_changed, 'original_spell_sha256': sha(original),
                'patched_spell_sha256': sha(patched), 'skill_sha256': sha(skills),
                'archive_providers': providers, 'changed_recipes': changes,
                'server_requirement': 'tortoise-wow 2721b195 or compatible higher-rod tool validation',
                'preserved': ['DBC header and string table', 'all other spells and fields', 'consumed reagents and counts', 'unrelated tools', 'server rod, skill and target checks']}
    manifest.update(client_identity(client,source))
    (output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(json.dumps({key: manifest[key] for key in ['spell_source','skill_source','spell_rows','changed_recipe_count','changed_byte_count','original_spell_sha256','patched_spell_sha256']}, indent=2))

if __name__ == '__main__': main()
