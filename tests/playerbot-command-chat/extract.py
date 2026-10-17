from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]/'modules/mod-playerbots/src/playerbot'
s=(root/'strategy/ExternalEventHelper.h').read_text()
a=s.index('        bool ParseChatCommand(');b=s.index('        bool HandlePacket(',a)
c=s.index('        bool HandleCommand(',b);d=s.index('    private:',c)
Path(sys.argv[1],'NativeCommandParser.inc').write_text(s[a:b]+s[c:d])
s=(root/'PlayerbotAI.cpp').read_text();a=s.index('bool PlayerbotAI::IsBotCommandMessage(');b=s.index('void PlayerbotAI::QueueChatResponse(',a)
Path(sys.argv[1],'NativeCommandFilter.inc').write_text(s[a:b])
packet=s[s.index('void PlayerbotAI::HandleBotOutgoingPacket'):s.index('void PlayerbotAI::IsBotCommandMessage') if 'void PlayerbotAI::IsBotCommandMessage' in s else a]
assert packet.index('IsBotCommandMessage(message)') < packet.index('if (m_recordIncommingMessages)')
assert 'speaker && IsRealPlayer(speaker)' in packet
