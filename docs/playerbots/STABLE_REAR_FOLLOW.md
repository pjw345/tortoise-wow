# Stable rear geometry for the far formation

Issue #81, 25 October 2026. Source correction only; no full server image build,
deployment, live setting change or gameplay acceptance is claimed.

## Evidence and reproduced fault

The user's second video (20261025-1741-03.0959508.mp4, 22.133 seconds) shows
Alon following unevenly across the Lakeshire bridge and road, occasionally
ahead of the player. The earlier 24 October North Gate recording remains a
separate observation. Current image/source is 18d7060b.

The saved profile records Alon as `formation>far`, `range::follow>2`; controls
use near. Alon retains the legitimate 1.08 run multiplier; moving flags lack
walk mode. Bounded 17:41 logs contain 446 successful follow actions for Alon
versus four for Cria and three for Flore. Existing-follow FAILED results are
not evidence of a path failure. Alon's active controller remains FOLLOW (14),
with short sampled splines including 118,123,203ms durations and elapsed values
often close to one 50ms update. Sparse state sampling is not a full packet or
per-spline velocity capture.

FarFormation::GetAngle previously read the active controller's angle, moved it
halfway toward pi, then could return the current positional angle on its next
call. With unchanged current positional angle 1.5 rad, the extracted production
function alternates requests 2.320796,1.5,2.320796,1.5. This independently
demonstrates unstable requested geometry. The shared IsFollowing comparison
rejects an angle change beyond 0.01 rad; MoveFollow clears/replaces the active
controller. The getter therefore feeds its own earlier request back into
admission and can restart a valid route every AI tick.

## Source correction and preserved behavior

FarFormation now requests pi radians relative to its existing follow target.
It describes a rear formation instead of performing interpolation in a getter.
The native follow generator rotates that offset with the target's orientation
and owns route updates, pathfinding, prediction and spline lifecycle.

Target selection (`master target`), saved follow range and radius conversion
are retained. Other formation types, explicit commands, jump/travel/combat
behavior, movement speed calculations/talents, collision and pet handling are
unchanged. Generic follow matching retains its existing tolerances. There are
no bot/map names in the implementation, timers, parallel movement controllers,
new live diagnostics or teleport/collision bypasses.

Native target prediction remains packet age plus 500ms with the existing
Movement.ExtrapolatePetPosition setting. Predicted endpoints may be ahead of a
target's current reported position; that alone does not prove physical overshoot
and is not a reason to disable prediction globally. Any residual close-follow
speed mismatch needs separate measured validation after this angular correction.

## Verification and remaining work

The production FarFormation class is extracted into the existing native follow
fixture, alongside the real facade matching and dispatch. Front/side/wrapped
starting angles and 100 successive target headings retain the rear geometry
and launch once, while explicit range changes still replace the controller.
The old production class fails this new fixture. The existing follow boundary,
movement dispatch and RPG movement CTest executables all pass.

These fixtures do not simulate a navmesh, real packets or visual animation.
A full image build and live straight/turn/start-stop/slope follow test remain
required. Preserve Alon's 8% talent during testing. Reconcile the other saved
party formations/ranges with the user's requested two-yard rear follow through
normal commands at deployment/testing time; no online AI values or database
settings were edited by this investigation.

Evidence remains under /home/paul/development/alon-follow-20261025 and local
outputs/alon-follow-20261025. Existing opt-in bot action/state logs were read;
no logging level, service or runtime movement configuration was changed.
