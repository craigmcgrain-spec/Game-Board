# Hexboard

Hexboard is a native Qt 6 desktop app for arranging game pieces on an infinite
hexagonal game board. It is designed for Fedora 44 and KDE Plasma 6.

## Features

- Infinite pan-and-zoom hexagonal canvas
- Drag-and-drop game-piece images from Dolphin or other desktop apps
- Multiple automatically arranged game pieces in each hex
- Individually drag or remove pieces
- Per-piece scaling from 50% to 1000%, anchored to the owning hex
- Equipment pieces linked to an owner, rendered at its left, and moved/scaled as a group
- Editable game-piece names rendered above and moved with their pieces
- Right-side player panel with piece assignment, heart-based health, and equipment inventory
- Icon-only player-piece selector and persistent free-text notes on every player card
- Collapsible player cards with compact icon, name, and health summaries
- Right-click player identification with the player name rendered above the assigned piece
- Zero-health player pieces are dimmed on the board
- Selectable equipment inventory; only checked equipment renders beside its owner
- Draw configurable links between hexes with optional arrowheads
- Hidden grid with transient hover, source, and destination indicators
- Optional full hex-grid overlay from the board toolbar
- Individual tile selection from sprite sheets, grouped by source subdirectory
- Persistent favorite tile assets promoted to the top of the picker
- Paintable per-hex backgrounds from a configurable image collection
- Solid color or image board backgrounds
- Portable session files with embedded images and recent-file access
- Lazily opened tabletop tools for dice, weighted choices, fantasy gear, and names
- Native KDE icons, dialogs, menus, and desktop integration

## Build on Fedora 44

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/hexboard
```

To install for all users:

```bash
sudo cmake --install build
```

To build an RPM:

```bash
cd build
cpack -G RPM
```

To build a portable x86_64 AppImage:

```bash
./scripts/build-appimage.sh
./Hexboard-x86_64.AppImage
```

The script downloads linuxdeploy and its Qt plugin into an ignored local tools
directory, then bundles Hexboard and its Qt runtime dependencies.

## Controls

| Action | Control |
| --- | --- |
| Add a piece | Drag an image file onto the board |
| Move a piece | Left-drag it to another tile |
| Resize a piece | Right-click it and use the **Game piece size** slider |
| Name a piece | Right-click it and choose **Rename game piece** |
| Identify a player piece | Right-click it and enable **Player** |
| Link equipment | Right-click a piece, mark **Equipment**, choose **Link to game piece**, then click its owner |
| Track a player | Add a player in the right panel, assign a piece, then set current and total hearts |
| Paint tile backgrounds | Choose an image, enable **Paint tiles**, then click or drag |
| Erase tile backgrounds | Enable **Erase tiles**, then click or drag |
| Link two hexes | Enable **Link hexes**, then click the start and end hex |
| Toggle the hex grid | Enable **Show hex grid** or press `G` |
| Pan without moving pieces | Enable **Navigate** or press `N`, then drag anywhere |
| Pan | Left-drag empty space or middle-drag anywhere |
| Zoom | Mouse wheel |
| Remove content | Right-click a piece or hex and choose a removal action |
| Save the board | **File > Save** or `Ctrl+S` |
| Open a board | **File > Open** or `Ctrl+O` |
| Open a tabletop tool | Choose it from the **Tools** menu |

Sessions use the `.hexboard` extension and include all tile backgrounds, game
pieces, links, board settings, and the current viewport. Images are embedded in the file,
so saved sessions can be moved to another Fedora system without copying their
original image files.

The Dice Roller, Chance Wheel, Gear Generator, and Name Generator open in
closable tabs in the **Tabletop Tools** panel on the left side of the board.
Use its launcher buttons or the **Tools** menu; the panel can also be hidden
with its close button, from **View > Tabletop Tools Panel**, or with
`Ctrl+Shift+T`. The movable panel floats over the board so showing it never
resizes or shifts the gameboard. Tool preferences are stored separately from `.hexboard`
sessions; closing a tool tab discards its in-memory results and reopening it
starts a clean instance.

## License

MIT
