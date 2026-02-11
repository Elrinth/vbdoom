# VB Doom Level Editor

Browser-based level editor for VB Doom. Edit maps in a top-down grid and export C code that fits the game's raycaster format.

## How to use

1. **Open** `web-editor/index.html` in a browser (no server required; open the file directly).
2. **Map size**: Choose **32×32**, **48×32**, **64×32**, **48×48**, or **64×64**. The grid resizes; export uses the chosen width and height.
3. **Paint walls**: Select a brush (0=empty, 1=STARTAN, 2=STONE, 3=TECH, 4=DOOR, 5=SWITCH, 6–8=secret doors, 9–11=key doors). Click or drag on the grid to paint.
4. **Spawn & entities**:
   - Click **Spawn 1** (or **Spawn 2**), then click a cell to place the player (or player 2) start.
   - Choose an **Enemy** type, then click a cell to place one.
   - Choose a **Pickup** type, then click a cell to place one.
5. **Doors**: Place tiles 4 (normal door), 6/7/8 (secret doors), or 9/10/11 (key doors). In the Doors list, set **N-S** or **E-W** for each door (wall direction: N-S = corridor runs east–west; E-W = corridor runs north–south). If a door won't open in-game, stand right in front of it and press Use; the game also activates doors by the tile in front of the player. Export includes orientation as comments.
6. **Switches**: Place tile 5 (switch). In the right panel, set each switch to **EXIT** or **Open Door N**. Export generates `registerSwitch(tx, ty, SW_EXIT, 0)` or `registerSwitch(tx, ty, SW_DOOR, doorIndex)`.
7. **Export**: Enter a **Level ID** (e.g. `e1m4`). Click **Export C** to get map array, spawn defines, `initEnemies`/`initPickups` stubs, and the exact `registerDoor`/`registerSwitch` calls to paste into the project. Use **Save as &lt;levelId&gt;.h** to download only the map + spawn defines (for the `.h` file); paste the init functions from the export text into `enemy.c` and `pickup.c`.

## Game limits (editor enforces these)

The editor shows **counts and caps** next to Entities, Doors, and Switches (e.g. **Enemies: 5 / 21**). You cannot add more than the game supports; a toast message appears if you hit a limit. To support more, increase the corresponding constant in the game and in the editor’s `GAME_LIMITS` in `editor.js`.

| Limit | Default | Defined in |
|-------|---------|------------|
| Enemies | 21 | `enemy.h` → `MAX_ENEMIES` |
| Pickups | 16 | `pickup.h` → `MAX_PICKUPS` |
| Doors | 24 | `door.h` → `MAX_DOORS` |
| Switches | 4 | `door.h` → `MAX_SWITCHES` |

When loading a JSON that has more entities than the caps, the editor trims to the cap and shows a toast.

## Tile set

| Value | Name | Notes |
|-------|------|--------|
| 0 | Empty | Walkable |
| 1 | STARTAN | Brick wall |
| 2 | STONE | Stone wall |
| 3 | TECH | Tech wall |
| 4 | DOOR | Normal door (opens when activated) |
| 5 | SWITCH | Activatable (exit or opens a door) |
| 6 | Secret (brick) | Looks like brick, opens like door |
| 7 | Secret (stone) | Looks like stone, opens like door |
| 8 | Secret (tech) | Looks like tech, opens like door |
| 9 | Key door (red) | Requires red key (when implemented) |
| 10 | Key door (yellow) | Requires yellow key |
| 11 | Key door (blue) | Requires blue key |

Switches can open any door (including secret doors 6–8) to reveal secret areas. Use **SW_DOOR** and the door index from the Doors list.

## Where to put exported files

- **Map + defines**: Save as e.g. `src/vbdoom/assets/doom/e1m4.h` (or `.c`).
- **Include**: In `RayCasterData.h`, add `#include "../assets/doom/e1m4.h"` after the other level includes.
- **Enemies**: Paste `initEnemiesE1M4()` (or your level id) into `enemy.c`; declare in `enemy.h`.
- **Pickups**: Paste `initPickupsE1M4()` into `pickup.c`; declare in `pickup.h`.

## Integrating a new level in the game

1. **RayCasterData.h**: Add `#include "../assets/doom/e1m4.h"` (or your level file).
2. **gameLoop.c** in `loadLevel()`:
   - Add `else if (levelNum == 4) { ... }` (or your level number).
   - `copymem((u8*)g_map, (u8*)e1m4_map, MAP_CELLS);` (or `1024` for 32×32 then zero-fill: `for (i = 1024; i < MAP_CELLS; i++) ((u8*)g_map)[i] = 0;`).
   - Set `fPlayerX`, `fPlayerY`, `fPlayerAng` from `E1M4_SPAWN_X`, `E1M4_SPAWN_Y`, and spawn angle.
   - Call `initDoors()`, then all `registerDoor(tx, ty)` and `registerSwitch(tx, ty, type, link)` from the export.
   - Call `initEnemiesE1M4()` and `initPickupsE1M4()`.
3. **door.h**: Ensure `MAX_DOORS` and `MAX_SWITCHES` are at least as large as your level’s door/switch counts.
4. **Level transition**: In the same place where `currentLevel < 3` is checked, extend to your max level (e.g. `currentLevel < 6`); when the last level is completed, go to episode end or next episode.
5. **Map size**: If the game is built with **64×64** (`MAP_X`/`MAP_Y` in `RayCaster.h`), levels smaller than 64×64 should copy only their bytes (e.g. 1024 for 32×32) then zero the rest of `g_map`. The editor export comment reminds you of this.

## First-person preview

The editor does not include a first-person raycaster preview. You can approximate layout from the top-down grid; test in the game or add a simple 2D raycaster in JS later if desired.
