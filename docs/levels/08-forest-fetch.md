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

## Open questions
- How are directions expressed so they're learnable but not trivial (named landmarks?
  cardinal 4D directions? a diegetic compass)?
- One fetch leg or several?
- Does the held object occlude the view / change controls?
