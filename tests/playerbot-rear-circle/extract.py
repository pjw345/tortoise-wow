from pathlib import Path
import sys
source,out=map(Path,sys.argv[1:]);out.mkdir(parents=True,exist_ok=True);s=source.read_text()
start=s.index('    class CircleFormation : public MoveFormation');end=s.index('    class LineFormation',start)
(out/'native-circle.inc').write_text(s[start:end])
start=s.index('float Formation::GetFollowAngle()');end=s.index('FormationValue::FormationValue(',start)
(out/'native-angle.inc').write_text(s[start:end])
