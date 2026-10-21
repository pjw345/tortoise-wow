"""Execute the shipped client Lua in Lua 5.1 with Vanilla API fakes.

Requires lupa with its lua51 module. Game-specific rendering and real network
delivery remain live acceptance checks, not claims made by this harness.
"""
import argparse
import pathlib
import json
import subprocess
import sys
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument('--deps')
parser.add_argument('--wire')
parser.add_argument('--lua50')
parser.add_argument('--pfquest-tracker')
args, remainder = parser.parse_known_args()
if args.deps:
    sys.path.insert(0, args.deps)
from lupa.lua51 import LuaRuntime

ROOT = pathlib.Path(__file__).resolve().parents[2]
ADDON = ROOT / 'client-addons/pfQuest-partybots'
SHIM = r'''
now = 1000
function GetTime() return now end
function time() return 1792590000 end
function date(format, value) return "17:00:00" end
frames = {}
function CreateFrame(kind, name, parent, template)
  local f = { scripts = {}, shown = true, height = 20, width = 300, text = "", children = {} }
  function f:RegisterEvent(name) self.events = self.events or {}; self.events[name] = true end
  function f:SetScript(name, callback) self.scripts[name] = callback end
  function f:GetScript(name) return self.scripts[name] end
  function f:Show() local before = self.shown; self.shown = true; if not before and self.scripts.OnShow then local old = this; this = self; self.scripts.OnShow(); this = old end end
  function f:Hide() self.shown = false end
  function f:IsShown() return self.shown end
  function f:SetHeight(value) self.height = value end
  function f:GetHeight() return self.height end
  function f:SetWidth(value) self.width = value end
  function f:GetWidth() return self.width end
  function f:SetText(value) self.text = value end
  function f:GetText() return self.text end
  function f:SetFont(font, size) self.size = size end
  function f:SetTextColor() end
  function f:SetTexture() end
  function f:SetVertexColor() end
  function f:GetStringWidth() return string.len(string.gsub(self.text, "|c%x%x%x%x%x%x%x%x", "")) * 5 end
  function f:SetPoint(...) self.point = arg end
  function f:ClearAllPoints() self.point = nil end
  function f:SetJustifyH() end
  function f:SetFrameStrata() end
  function f:EnableMouse() end
  function f:SetMovable() end
  function f:RegisterForDrag() end
  function f:SetClampedToScreen() end
  function f:SetBackdrop() end
  function f:StartMoving() end
  function f:StopMovingOrSizing() end
  function f:CreateFontString() local value = CreateFrame('FontString'); table.insert(self.children, value); return value end
  if name then _G[name] = f end
  table.insert(frames, f)
  return f
end
function FireEvent(value, ...)
  local eventFrames = {}; for _, f in ipairs(frames) do table.insert(eventFrames, f) end
  event = value
  arg1, arg2, arg3, arg4 = arg[1], arg[2], arg[3], arg[4]
  for _, f in ipairs(eventFrames) do if f.events and f.events[value] and f.scripts.OnEvent then this = f; f.scripts.OnEvent() end end
end
function Tick() for _, f in ipairs(frames) do if f.scripts.OnUpdate then this = f; arg1 = 0.5; f.scripts.OnUpdate() end end end
members = { 'Mercy', 'Cria', 'Alon', 'Flore' }
online = {}
function GetNumRaidMembers() return 0 end
function GetNumPartyMembers() return table.getn(members) end
function UnitName(unit) if unit == 'player' then return 'Alin' end return members[tonumber(string.sub(unit, 6))] end
function UnitIsUnit(unit, other) return unit == other end
function UnitIsConnected(unit) return online[UnitName(unit)] ~= false end
sent = {}
function SendAddonMessage(prefix, body, channel) table.insert(sent, {prefix,body,channel}) end
function GetItemInfo(entry) return nil end
function GetQuestLogTitle(index) if index == 1 then return 'Shared | title', 20, nil, false, false, nil end end
UIParent = CreateFrame('Frame')
QuestLogFrame = CreateFrame('Frame')
GameTooltip = CreateFrame('Frame')
GameTooltip.lines = {}
function GameTooltip:AddLine(text) table.insert(self.lines, text) end
function GameTooltip:SetOwner() end
UISpecialFrames, SlashCmdList = {}, {}
pfUI = { font_default = 'Arial' }
pfQuest_config = { trackerfontsize = '12' }
pfDB = { items = { loc = { [60688] = 'Moonlight Silk' } }, units = { loc = {} }, objects = { loc = {} } }
pfQuest = { questlog = { [94] = { qlogid = 1, title = 'Shared | title' } } }
local tracker = CreateFrame('Frame')
tracker.mode, tracker.buttons = 'QUEST_TRACKING', {}
function tracker.ButtonEvent(button) button = button or this; button:SetHeight(button.empty and 0 or 20) end
function tracker.ButtonEnter() end
function tracker.DoLayout() tracker:SetWidth(200) end
local button = CreateFrame('Button'); button.questid = 94; button.title = 'Shared | title'
tracker.buttons[1] = button
pfQuest.tracker = tracker
'''


class ClientTests(unittest.TestCase):
    def setUp(self):
        self.lua = LuaRuntime(unpack_returned_tuples=True)
        self.scripts = []
        self.check(SHIM)
        if args.pfquest_tracker:
            text = pathlib.Path(args.pfquest_tracker).read_text(encoding='utf-8')
            native = text[text.index('function tracker.ButtonEvent(self)'):text.index('function tracker.RefreshZoneTracker()')]
            self.check('''
              pfQuest.tracker.buttons[1].text=CreateFrame('FontString')
              pfQuest.tracker.buttons[1].icon=CreateFrame('Texture')
              pfQuest.tracker.buttons[1].node={questid=94}
              pfQuestConfig={path='Interface\\AddOns\\pfQuest'}
              pfMap={str2rgb=function() return 0.8,0.7,0.2 end,tooltip={GetColor=function() return 0.5,0.5,0.5 end}}
              pfQuest_Loc=setmetatable({}, {__index=function(t,k) return k end})
              function GetNumQuestLeaderBoards() return 1 end
              function GetQuestLogLeaderBoard() return 'Moonlight Silk: 2/10','item',false end
              function IsQuestWatched() return true end
              function UnitLevel() return 27 end
              ceil=math.ceil
              gsub,strfind,format=string.gsub,string.find,string.format
            ''' + '''
              do
              local tracker=pfQuest.tracker
              local compat={GetQuestLogTitle=GetQuestLogTitle,GetDifficultyColor=function() return {r=1,g=1,b=0} end}
              pfQuestCompat=compat
              local fontsize,entryheight,panelheight=12,20,16
              local expand_states,board_cache={},{}
              local sort,min=table.sort,math.min
              local function trackersort(a,b) return a.questid<b.questid end
            ''' + native + '\nend\n')
        for name in ('Core.lua', 'Tracker.lua', 'Panel.lua'):
            self.check((ADDON / name).read_text(encoding='utf-8'))
        self.check("FireEvent('PLAYER_LOGIN'); Tick()")

    def tearDown(self):
        if args.lua50:
            with tempfile.TemporaryDirectory() as folder:
                script = pathlib.Path(folder) / 'test.lua'
                script.write_text('\n'.join(self.scripts), encoding='utf-8')
                result = subprocess.run([str(pathlib.Path(args.lua50).resolve()), str(script)], text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def check(self, script):
        self.scripts.append(script)
        self.lua.execute(script)

    def test_native_wire_and_progress(self):
        if not args.wire:
            self.skipTest('Pass --wire from QuestSyncTest to validate actual native packets')
        packets = [bytes.fromhex(line).decode() for line in pathlib.Path(args.wire).read_text().splitlines()]
        self.check("pfQuestPartyBots.pending.Mercy = { token='42', deadline=now+15, chunks={}, count=0 }")
        for packet in packets:
            self.check("assert(pfQuestPartyBots.Receive('PFQBOT', " + json.dumps(packet) + ", 'GUILD', 'Alin'))")
        self.check("local label, record, stale = pfQuestPartyBots.Status('Mercy',94); assert(label=='20%' and record.objectives[1].current==2 and not stale); assert(pfQuestPartyBots.snapshots.Mercy.records[248].state=='R')")

    def test_multipart_out_of_order_and_partial_retention(self):
        self.check(r'''
          local P=pfQuestPartyBots
          local body='94|I|20|536861726564|I,1,60688,2,10\n'
          P.pending.Mercy={token='8',deadline=now+15,chunks={},count=0}
          assert(P.Receive('PFQBOT','S|1|8|Mercy|0000000000000001|2|2|'..string.sub(body,12),'GUILD','Alin'))
          assert(not P.snapshots.Mercy)
          assert(P.Receive('PFQBOT','S|1|8|Mercy|0000000000000001|1|2|'..string.sub(body,1,11),'GUILD','Alin'))
          local old=P.snapshots.Mercy
          P.pending.Mercy={token='9',deadline=now+15,chunks={},count=0}
          assert(P.Receive('PFQBOT','S|1|9|Mercy|0000000000000002|1|2|partial','GUILD','Alin'))
          assert(not P.Receive('PFQBOT','S|1|9|Mercy|0000000000000002|1|2|conflict','GUILD','Alin'))
          assert(P.snapshots.Mercy==old and old.error)
        ''')

    def test_identity_roster_nonce_and_bounds(self):
        self.check(r'''
          local P=pfQuestPartyBots
          local msg='S|1|8|Mercy|0000000000000001|1|1|'
          P.pending.Mercy={token='8',deadline=now+15,chunks={},count=0}
          assert(not P.Receive('PFQBOT',msg,'PARTY','Other'))
          assert(not P.Receive('PFQBOT',msg,'GUILD','Other'))
          assert(not P.Receive('OTHER',msg,'GUILD','Alin'))
          P.pending.Mercy.token='9'; assert(not P.Receive('PFQBOT',msg,'GUILD','Alin'))
          P.pending.Mercy.token='8'; members={'Cria'}; P.BuildRoster()
          assert(not P.Receive('PFQBOT',msg,'GUILD','Alin'))
          members={'Mercy'}; P.BuildRoster()
          P.pending.Mercy={token='8',deadline=now+15,chunks={},count=0}
          assert(not P.Receive('PFQBOT','S|1|8|Mercy|0000000000000001|1|193|x','GUILD','Alin'))
          assert(not P.ParseSnapshot('94|I|20|xx|\n'))
          assert(not P.ParseSnapshot('94|I|20|41|I,1,1,-1,10\n'))
          assert(not P.ParseSnapshot('94|I|20|41|\n94|I|20|42|\n'))
        ''')

    def test_empty_snapshot_and_unchanged_freshness(self):
        self.check(r'''
          local P=pfQuestPartyBots
          P.pending.Mercy={token='8',deadline=now+15,chunks={},count=0}
          assert(P.Receive('PFQBOT','S|1|8|Mercy|0000000000000001|1|1|','GUILD','Alin'))
          local label, record, stale=P.Status('Mercy',94); assert(label=='Not in log' and not record and not stale)
          now=1100; local _,_,stale=P.Status('Mercy',94); assert(stale)
          P.pending.Mercy={token='9',deadline=now+15,chunks={},count=0}
          assert(P.Receive('PFQBOT','U|1|9|Mercy|0000000000000001','GUILD','Alin'))
          local _,_,stale=P.Status('Mercy',94); assert(not stale)
        ''')

    def test_timeout_offline_and_throttle(self):
        self.check(r'''
          local P=pfQuestPartyBots
          local before=table.getn(sent); Tick(); assert(table.getn(sent)==before)
          now=now+16; Tick(); assert(not P.pending.Mercy or P.pending.Mercy.deadline>now)
          online.Mercy=false; P.BuildRoster()
          local label,record,stale=P.Status('Mercy',94); assert(label=='Offline' and stale)
          for _,message in ipairs(sent) do assert(message[1]=='PFQBOT' and message[3]=='PARTY' and string.len(message[2])<=240) end
        ''')

    def test_tracker_no_height_accumulation_and_modes(self):
        self.check(r'''
          local P=pfQuestPartyBots
          P.RefreshTracker(); local height=pfQuest.tracker.buttons[1]:GetHeight()
          assert(height>20); P.RefreshTracker(); assert(pfQuest.tracker.buttons[1]:GetHeight()==height)
          pfQuest.tracker.mode='GIVER_TRACKING'; P.RefreshTracker()
          assert(pfQuest.tracker.buttons[1]:GetHeight()==20 and not pfQuest.tracker.buttons[1].partyBotLines[1]:IsShown())
          pfQuest.tracker.mode='QUEST_TRACKING'; pfQuest.tracker.buttons[1].empty=true; P.RefreshTracker()
          assert(pfQuest.tracker.buttons[1]:GetHeight()==0)
        ''')

    def test_panel_bot_only_duplicate_titles_and_quest_log_button(self):
        self.check(r'''
          local P=pfQuestPartyBots
          P.snapshots.Cria={records={ [100]={id=100,title='Shared | title',state='I',level=20,objectives={}}, [101]={id=101,title='Shared | title',state='C',level=20,objectives={}} },seen=now}
          local quests=P.ActiveQuests(); assert(table.getn(quests)==3 and quests[1].id~=quests[2].id)
          P.TogglePanel(); assert(pfQuestPartyBotsPanel:IsShown())
          assert(pfQuestPartyBotsQuestLogButton and pfQuestPartyBotsTrackerButton)
          P.TogglePanel(); assert(not pfQuestPartyBotsPanel:IsShown()); P.TogglePanel(); assert(table.getn(P.ActiveQuests())==3)
        ''')

    def test_tooltip_counts_and_turn_in_states(self):
        self.check(r'''
          local P=pfQuestPartyBots
          P.snapshots.Mercy={seen=now,received=time(),records={ [94]={id=94,state='C',objectives={{kind='I',entry=60688,current=10,required=10}}} }}
          local label=P.Status('Mercy',94); assert(label=='Ready')
          P.AddTooltip(94); assert(string.find(table.concat(GameTooltip.lines,'\n'),'Moonlight Silk: 10/10',1,true))
          P.snapshots.Mercy.records[94].state='R'; assert(P.Status('Mercy',94)=='Turned in')
        ''')


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]] + remainder)
