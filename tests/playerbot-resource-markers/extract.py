from pathlib import Path
import sys
r=Path(__file__).resolve().parents[2]
b=r/'modules/mod-playerbots/src/playerbot'
s=(b/'QuestSync.cpp').read_text()
a=s.index('    bool Authorized(');z=s.index('    // Called by',a)
auth='namespace ai { namespace questsync {\n'+s[a:z]+'}}\n'
source=(b/'ResourceMarkers.cpp').read_text()
source=source[source.index('namespace ai { namespace resources'):]
Path(sys.argv[1]).write_text(auth+source)
