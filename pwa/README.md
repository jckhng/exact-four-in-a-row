# Exact Four in a Row PWA

An installable browser Connect Four game in grayscale e-ink style.

## Features

- Play against the built-in AI (Easy/Medium/Hard) or 2-player local.
- AI demo mode.
- Undo moves.
- Save/Load a manual restore point.
- Works offline after first load.
- Installable via Chrome "Add to Home Screen."

## Building

```bash
npm install
npm run typecheck
npm run build
```

## Rules

Drop pieces into columns by clicking the ▼ button above each column. First player
to connect four pieces horizontally, vertically, or diagonally wins. Dark plays first.

## Attribution

Rules engine ported from the GNECT Connect Four implementation in GNOME Games.
Part of the Exact Games / GnomeGames4Kindle project.

## License

GPL-3.0-or-later. See THIRD_PARTY.md for dependency details.
