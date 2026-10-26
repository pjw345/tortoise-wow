from pathlib import Path
import sys
r=Path(__file__).resolve().parents[2]
b=r/'modules/mod-playerbots/src/playerbot'
s=(b/'QuestSync.cpp').read_text()
a=s.index('    bool Authorized(');z=s.index('    // Called by',a)
auth='namespace ai { namespace questsync {\n'+s[a:z]+'}}\n'
source=(b/'ResourceMarkers.cpp').read_text()
source=source[source.index('namespace ai { namespace resources'):]
scripts=(b/'PlayerbotScripts.cpp').read_text()
start=scripts.index('        bool CanUseGroupChat(');end=scripts.index('\n        // Was the CreatePlayerbotMgr()',start)
gate='\nstruct ChatGate : GateBase {\n'+scripts[start:end]+'};\n'
mgr=(b/'PlayerbotMgr.cpp').read_text()
handler=mgr[mgr.index('void PlayerbotMgr::HandleCommand('):]
assert 'resourceMarkerRequests.Queue(text.substr(8), time(0))' in handler
assert 'resourceMarkerRequests.Take()' in mgr and 'SendPlayerSnapshot(GetMaster(), request)' in mgr
Path(sys.argv[1]).write_text(auth+source+gate)
