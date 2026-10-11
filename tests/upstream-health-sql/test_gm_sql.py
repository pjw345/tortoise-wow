"""Execute the production GM-list query on an isolated MySQL CI service."""
import os
import pathlib
import re
import subprocess

root = pathlib.Path(__file__).resolve().parents[2]
source = (root / 'src/game/Commands/Commands.cpp').read_text()
function = source.split('bool ChatHandler::HandleGMListFullCommand(', 1)[1]
match = re.search(r'LoginDatabase.Query\((.*?)\);', function, re.S)
if not match:
    raise RuntimeError('Cannot extract GM list query')
query = ''.join(re.findall(r'"([^"\\]*)"', match[1]))
if not re.search(r'WHERE\s+`rank`\s*>\s*0', query):
    raise RuntimeError('GM list filter must quote its reserved identifier')
sql = '''CREATE DATABASE regression; USE regression;
CREATE TABLE account (username VARCHAR(30), `rank` INT);
INSERT INTO account VALUES ('player',0),('negative',-1),('admin',3),('observer',1);
''' + query + ';\n'
result = subprocess.run(['docker', 'exec', '-i', os.environ['MYSQL_CONTAINER_ID'],
    'mysql', '-uroot', '-pci-test-only', '--batch', '--skip-column-names'],
    input=sql, text=True, capture_output=True, check=True)
rows = set(result.stdout.strip().splitlines())
assert rows == {'admin\t3', 'observer\t1'}, rows
print('Production GM query passed on MySQL 8.4; positive-rank filtering preserved.')
