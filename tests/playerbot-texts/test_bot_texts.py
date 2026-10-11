"""Exercise seed and repair SQL on a disposable CI database, never production."""
import json
import os
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
SEED = ROOT / 'modules/mod-playerbots/sql/world/ai_playerbot_texts.sql'
MIGRATION = ROOT / 'sql/database_updates/world/20261011150000_world.sql'
ENCODED_MIGRATION = ROOT / 'sql/database_updates/world/20261011170000_world.sql'
ORIGINAL = json.loads((pathlib.Path(__file__).parent / 'original-stock-messages.json').read_text())
client = os.environ.get('DATABASE_CLIENT', 'mysql')

def execute(sql):
    result = subprocess.run(['docker', 'exec', '-i', os.environ['DATABASE_CONTAINER_ID'],
        client, '-uroot', '-pci-test-only', '--default-character-set=utf8mb4',
        '--batch', '--skip-column-names'], input=sql, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stderr)
    return result.stdout.strip().splitlines()

def value(text):
    if not text:
        return "''"
    return 'CONVERT(0x' + text.encode().hex() + ' USING utf8mb4)'

fixture = '''CREATE DATABASE stock_upgrade CHARACTER SET utf8mb4; USE stock_upgrade;
CREATE TABLE ai_playerbot_texts (
 id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(255), text VARCHAR(1024),
 say_type INT, reply_type INT, text_loc1 VARCHAR(1024));
'''
variants = ORIGINAL + [dict(row, text=row['text'].encode().decode('cp1252')) for row in ORIGINAL]
variants += [dict(row, text=row['text'].replace('\u2019', "'")) for row in ORIGINAL]
for row in variants:
    fixture += 'INSERT INTO ai_playerbot_texts (name,text,say_type,reply_type,text_loc1) VALUES (' + \
        ','.join([value(row['name']), value(row['text']), '1', '7', value('Localized: l\u2019ami')]) + ');\n'
fixture += 'INSERT INTO ai_playerbot_texts (name,text,say_type,reply_type,text_loc1) VALUES (' + \
    ','.join([value('hello'), value('Custom: how\u2019s everything?'), '0', '0', value('')]) + ');\n'
fixture += 'INSERT INTO ai_playerbot_texts (name,text,say_type,reply_type,text_loc1) VALUES (' + \
    ','.join([value('unrelated'), value(ORIGINAL[0]['text']), '0', '0', value('')]) + ');\n'
execute(fixture)
query = 'SELECT name, HEX(text), say_type, reply_type, HEX(text_loc1) FROM ai_playerbot_texts ORDER BY id;'
execute('USE stock_upgrade;\n' + MIGRATION.read_text() + '\n' + ENCODED_MIGRATION.read_text())
once = execute('USE stock_upgrade;\n' + query)
assert len(once) == len(ORIGINAL) * 3 + 2
for row, actual in zip(ORIGINAL * 3, once):
    fields = actual.split('\t')
    assert fields[0] == row['name']
    assert bytes.fromhex(fields[1]).decode() == row['text'].replace('\u2019', "'")
    assert fields[2:4] == ['1', '7']
    assert bytes.fromhex(fields[4]).decode() == 'Localized: l\u2019ami'
assert bytes.fromhex(once[-2].split('\t')[1]).decode() == 'Custom: how\u2019s everything?'
assert bytes.fromhex(once[-1].split('\t')[1]).decode() == ORIGINAL[0]['text']
assert any("Hey, how's your day going?" == bytes.fromhex(row.split('\t')[1]).decode() for row in once)
execute('USE stock_upgrade;\n' + MIGRATION.read_text() + '\n' + ENCODED_MIGRATION.read_text())
assert execute('USE stock_upgrade;\n' + query) == once, 'Repair must be replayable'
execute('CREATE DATABASE stock_fresh CHARACTER SET utf8mb4; USE stock_fresh;\n' + SEED.read_text())
fresh = execute('USE stock_fresh; SELECT HEX(text) FROM ai_playerbot_texts WHERE name=CONVERT(0x68656c6c6f USING utf8mb4);')
assert "Hey, how's your day going?" in [bytes.fromhex(row).decode() for row in fresh]
assert all('\u2019' not in bytes.fromhex(row).decode() for row in fresh)
execute('USE stock_fresh;\n' + MIGRATION.read_text() + '\n' + ENCODED_MIGRATION.read_text())
execute('CREATE DATABASE bots_disabled; USE bots_disabled;\n' + MIGRATION.read_text() + '\n' + ENCODED_MIGRATION.read_text())
assert execute('USE bots_disabled; SHOW TABLES;') == [], 'No table should be created when bots are absent'
assert any("It's good to meet you." == bytes.fromhex(row.split('\t')[1]).decode() for row in once)
print('Seed, UTF-8 and Windows-1252 stock imports, already-fixed rows, replay, translations, custom text and no-bots checks passed.')
