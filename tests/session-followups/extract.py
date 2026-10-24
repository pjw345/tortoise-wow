from pathlib import Path
import sys, re
root = Path(__file__).resolve().parents[2]
def block(path, marker):
    text = (root / path).read_text()
    masked = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"', lambda m: ' ' * len(m[0]), text)
    start = text.index(marker); end = masked.index('{', start) + 1; depth = 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}'); end += 1
    return text[start:end]
methods = block('modules/mod-playerbots/src/playerbot/strategy/actions/BuyAction.cpp', 'bool BuyBackAction::Execute(')
Path(sys.argv[1]).write_text(methods + '\n')
Path(sys.argv[2]).write_text(block('src/game/Objects/GameObject.h', '    bool IsRepeatableGatheringChest() const') + '\n')
