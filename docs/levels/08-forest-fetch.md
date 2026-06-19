# Level 8 — Forest Fetch

## Teaches (in isolation)
**Mapping the space.** A fetch quest in a complex scene: find a golden object, carry it back
to an NPC, who then gives directions to another location. The goal is to get the player
building a mental map of 4D space and following spatial directions — orientation and memory,
not a new movement.

## Camera / controls
First-person FPS (the player now has full, unrestricted controls — this is a navigation
test). No locks, no head-return.

## Scene
- A forest: reuse the tree `Object4D` and ground from `Level2.cpp` (`objects/tree.json`),
  scaled up into a denser, larger area scattered through the X-Z-W floor.
- A **golden object** hidden somewhere in the forest.
- An **NPC** at a landmark hub.

## Quest flow / win
1. NPC asks the player to fetch the golden object ("it lies toward …").
2. Player explores, finds it, picks it up (Space near it).
3. Player returns it to the NPC.
4. NPC gives directions to a new location ("head W-ward past the tall tree…"), sending the
   player on a second leg.
5. Reaching the final directed location → win.

## New engine systems required
- **Pickup/carry**: interact (Space) to attach an object to the player; render it as held;
  drop/deliver on reaching the NPC.
- **NPC + dialogue**: a static entity with proximity-triggered text (ImGui panel) and a
  small **quest state machine** (ask → fetched → delivered → directed → done).
- **Directional hint system**: phrase directions in the game's axis language and/or show a
  compass/marker; needs a way to express "toward +W / past landmark X".
- **Interaction prompt** when near an interactable (reuse the Corridor's near-goal prompt
  pattern).
- Larger-scene performance check (more instances than current levels).

## Decisions (as built)
- **Directions:** text directions phrased in the game's axis language ("deep toward +W, past the
  Tall Red Pine") plus **named landmark trees** — no homing compass/marker. The existing
  bottom-right Facing and Position HUD gauges give the player live 4D heading/position so the text
  directions are followable.
- **Two legs:** NPC asks → fetch the golden cube (+W, past the Tall Red Pine) → carry it back →
  NPC directs the player to a clearing (-X, past the Crooked Blue Tree) → reaching it wins.
- **Held object:** the golden cube renders in front of the camera while carried; it does not change
  the controls and is small enough not to occlude the view.

## Implementation
`src/levels/ForestFetchLevel.{h,cpp}`. Self-contained (no engine changes): reuses `objects/tree.json`
(scaled for forest + landmark trees), `generateBox`/`generateHypercube`, the tiled-floor + collider
idiom, and `hud::drawFacingWidget`/`drawCoordWidget`. The ground is a 3-volume of tiles across
X-Z-W at the surface; gravity keeps the player grounded. Quest is a small state machine
(Ask → Fetching → Returning → Directed → Done) driven by proximity + Space.

## Open questions
- One fetch leg or several? (Built with two; could chain more.)
- Tune forest density / region size and landmark placement after playtest.
