import pathlib
import sys

# Compile the full production adapter, replacing only its host includes with
# the fixture's native API fakes. Do not maintain a second snapshot algorithm.
text = pathlib.Path(sys.argv[1]).read_text()
text = '\n'.join(line for line in text.splitlines() if not line.startswith('#include'))
pathlib.Path(sys.argv[2]).write_text(text)
