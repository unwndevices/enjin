# Tomodachi Web App — Brainstorm (v0.1)

---

## 1. The Two Pillars

The web app is really **two products sharing a shell**:

| Pillar | Name idea | Audience | Platform |
|--------|-----------|----------|----------|
| **Studio** | `tomo studio` / `tomo dev` | Script authors, tinkerers | Desktop only (MVP) |
| **Repository** | `tomo repo` / `tomo hub` / `tomo shelf` | Players, collectors, device owners | Desktop + mobile |

They share the same TUI-flavored chrome, the same theming engine, and the same user account — but they have very different layouts and interaction models.

---

## 2. Visual Identity: "Web TUI"

### 2.1 The Aesthetic

The goal isn't to literally emulate a terminal. It's to capture the **feeling** of modern TUI apps (lazygit, btop, helix, yazi) — the density, the crispness, the beautiful use of box-drawing characters, the respect for the grid — but adapted for the web where we have hover states, smooth animations, and real layout flexibility.

**Key characteristics:**

- **Box-drawing borders** (`─ │ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼`) used for panel frames, not CSS borders
- **Status bars** at top and bottom of every major view (like vim/tmux)
- **Pane-based layout** — panels tile and resize, reminiscent of tmux splits
- **Dense, information-rich** — no wasted space, every pixel earns its place
- **Keyboard-first** with mouse as a welcome guest
- **Single-character indicators** for state: `●` active, `○` inactive, `▸` selected, `◆` modified
- **Color used structurally**, not decoratively — colors convey meaning (error, active, muted)

### 2.2 Typography

| Role | Font | Fallback |
|------|------|----------|
| Everything (code, UI, panels, labels) | **JetBrains Mono** or **Berkeley Mono** or **Iosevka** | `monospace` |
| Titles / headings (repo cards, hero text) | **Inter** (tight tracking, medium weight) | `system-ui` |
| Device screen preview | **Custom pixel font** or nearest bitmap-feel mono | — |

The mono font is the **soul** of the interface. Inter appears only in specific moments where readability at larger sizes matters (repository card titles, modal headings).

### 2.3 Theming Engine — iTerm2 Color Schemes

This is a killer feature for the community. The iTerm2 colorscheme format gives us a well-established ecosystem of hundreds of themes.

**Implementation concept:**

```
Theme = {
  // ANSI 0-15 mapped to semantic roles
  bg:        color0_bg,       // main background
  fg:        color7,          // main text
  bg_alt:    color0 + slight lightness shift,  // panel backgrounds
  border:    color8 (bright black),            // box-drawing chars
  accent:    color4 (blue) or color6 (cyan),   // active elements
  success:   color2 (green),
  warning:   color3 (yellow),
  error:     color1 (red),
  muted:     color8,          // secondary text
  highlight: color5 (magenta), // selections, badges
  // + all 16 ANSI colors available for syntax highlighting
}
```

**Theme picker UI:** A grid of swatches, each showing the theme name in its own colors. Clicking applies instantly. Could show a mini preview of a code snippet in each theme. Popular themes front-loaded: Dracula, Tokyo Night, Catppuccin, Gruvbox, Nord, Solarized, Rosé Pine, Kanagawa...

**Persistence:** Theme choice saved per-user (account) or localStorage for anonymous users.

---

## 3. Studio (Editor + Simulator)

### 3.1 Layout Concept

```
┌─ TOMODACHI STUDIO ──────────────────────────────────────────────────┐
│ [≡ files] [main.lua] [manifest.json] [sprites]     ◆ unsaved  ⌘S  │
├────────────────────────────────────────────┬─────────────────────────┤
│                                            │ ┌─ DEVICE ───────────┐ │
│                                            │ │                    │ │
│   -- main.lua                              │ │   320×240 canvas   │ │
│   function init()                          │ │   (pixel-perfect)  │ │
│     state = { x=160, y=120 }               │ │                    │ │
│   end                                      │ │                    │ │
│                                            │ └────────────────────┘ │
│   function update(dt)                      │ [A] [B]  ◎encoder     │
│     local dx = sensors.imu.tiltX()         │                       │
│     state.x = state.x + dx * dt            │ ┌─ SENSORS ─────────┐ │
│     gfx.clear(0)                           │ │ IMU  [·····pad····]│ │
│     gfx.fillCircle(state.x, state.y, 4, 1) │ │ ToF  ──●────── 250│ │
│   end                                      │ │ Light ────●──── 0.6│ │
│                                            │ │ Mic   ─●──────  0.1│ │
│                                            │ │ Piezo [TAP]  v=0.8 │ │
│                                            │ └───────────────────┘ │
├────────────────────────────────────────────┤ ┌─ MIDI LOG ─────────┐ │
│ ┌─ CONSOLE ──────────────────────────────┐ │ │ CC  1:74  = 64     │ │
│ │ [info] app loaded (0.8ms)              │ │ │ NoteOn 1:60 v=100  │ │
│ │ [info] tick budget: 2.1ms avg          │ │ │ NoteOff 1:60       │ │
│ │ [warn] sprite id 12 not found          │ │ │ CC  1:74  = 67     │ │
│ │ [err]  line 14: attempt to index nil   │ │ │                    │ │
│ └────────────────────────────────────────┘ │ └───────────────────┘ │
├─────────────────────────────────────────────────────────────────────┤
│ ▸ RUN │ ⏸ PAUSE │ ↻ RELOAD │ fps: 30 │ tick: 1.2ms │ mem: 14KB   │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.2 Panel System

Panels are **resizable panes** (like tmux), not floating windows. They snap to a grid.

**Default layout:**
- **Left:** Editor (dominant, ~60% width)
- **Top-right:** Device preview (fixed aspect ratio 320×240, scaled up 1.5-2×)
- **Mid-right:** Sensor controls
- **Bottom-right:** MIDI log
- **Bottom-left:** Console/debug output
- **Bottom bar:** Status + run controls

**Panels can be:**
- Collapsed to a single title bar (click to expand)
- Swapped (drag title bar)
- Keyboard-toggled (`⌘1` editor, `⌘2` device, `⌘3` sensors, etc.)

### 3.3 Editor

**CodeMirror 6** is the better fit for this project (vs Monaco):
- Much lighter bundle
- Better mobile story if we ever go there
- Easier to theme to match the TUI aesthetic
- Lua language support available
- Excellent extension architecture

**Features:**
- Lua syntax highlighting (using theme's ANSI colors)
- Inline errors (from Lua VM compile/runtime errors, shown as gutter markers + inline text)
- Autocomplete for the `tomodachi` API (gfx.*, midi.*, sensors.*, etc.)
- Minimap optional (off by default — not very TUI)
- File tabs for: `main.lua`, `manifest.json`, sprite viewer
- Line numbers in `muted` color, active line highlighted

### 3.4 Device Preview

The 320×240 canvas rendered at integer scale (e.g., 2× = 640×480 CSS pixels). The preview has:

- **Box-drawing border** around it with the label "DEVICE" or the app name
- **CRT/scanline filter toggle** (fun, optional — a CSS filter overlay)
- The device "bezel" could be minimal: just the canvas + controls below it
- **Virtual controls** below the canvas:
  - `[A]` `[B]` buttons (clickable, keyboard-bound to Z/X or similar)
  - Encoder: a rotary knob widget (drag to rotate, click to press)
  - Keyboard shortcuts shown on hover

### 3.5 Sensor Panel

Each sensor gets a compact row:
- **IMU:** 2D tilt pad (small square, drag a dot around). Shows tiltX/tiltY values
- **ToF:** Horizontal slider (0–2000mm range). Shows mm value
- **Light:** Horizontal slider (0.0–1.0). Shows float
- **Mic:** Slider + "noise" toggle that generates random envelope data
- **Piezo:** Button labeled "TAP" + velocity slider. Click sends a hit event

All values update in real-time and stream into the WASM core.

### 3.6 Debug Tools

**Console panel:**
- Lua `print()` output
- System messages (app loaded, errors, warnings)
- Colored by severity: `success` for info, `warning` for warn, `error` for error
- Click error messages to jump to line in editor
- Clear button, auto-scroll toggle

**MIDI log:**
- Scrolling list of MIDI events
- Color-coded: NoteOn (green), NoteOff (muted), CC (cyan), etc.
- Filter toggles per message type
- Optional: "flash" animation on new events

**Performance overlay (toggle):**
- Tick timing histogram
- Memory usage bar
- FPS counter
- Frame timing graph (sparkline style — very TUI)

### 3.7 File Management

- **Open .tomo** — drag & drop or file picker, unpacks into editor tabs
- **Save .tomo** — packs current state into downloadable .tomo
- **New from template** — a few starter templates (blank, midi-cc, game, audio-toy)
- **Sprite manager** — upload PNGs, auto-pack to 4-bit, assign IDs
  - Preview sprites in a grid
  - Shows dimensions, ID, bit depth

### 3.8 Future: Remote Device Connection

- WebSerial or BLE connection to ESP32
- "Push to device" button
- Live sensor data FROM device (replaces fake sensors)
- Bidirectional: edit on web, run on device
- Connection status indicator in status bar: `● connected` / `○ disconnected`

---

## 4. Repository

### 4.1 Concept

Think: **a TUI-styled app store / community library**. Not a marketplace — everything is free and open. It's closer to itch.io for scripts.

### 4.2 Layout — Desktop

```
┌─ TOMODACHI REPO ─────────────────────────────────────────────────────┐
│ ┌─ FILTER ────────────────────────────────────────────────────┐      │
│ │ [search_______________] sort: [★ popular ▾] api: [v0.1 ▾]  │      │
│ │ tags: [midi] [game] [audio] [sensor] [visual] [tool] [all] │      │
│ └─────────────────────────────────────────────────────────────┘      │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌─ tilt-midi-cc ──────────┐  ┌─ snake-beats ─────────────┐        │
│  │ ▸ by @synthwizard       │  │ ▸ by @pixelcrafter         │        │
│  │ v1.2.0 · api v0.1       │  │ v0.4.0 · api v0.1         │        │
│  │                          │  │                            │        │
│  │ Turn tilt into MIDI CC.  │  │ Snake game that triggers   │        │
│  │ Map X/Y axes to any     │  │ MIDI notes on food pickup. │        │
│  │ controller pair.         │  │                            │        │
│  │                          │  │ ┌──────────────────┐      │        │
│  │ uses: midi imu           │  │ │  ▄▀▀▀▄  ◆       │      │        │
│  │ ★ 42  ↓ 380              │  │ │  █   █    ◆     │      │        │
│  │                          │  │ └──────────────────┘      │        │
│  │ [open in studio]         │  │ uses: midi gfx            │        │
│  │ [+ add to device]        │  │ ★ 89  ↓ 1.2k             │        │
│  └──────────────────────────┘  └────────────────────────────┘        │
│                                                                      │
│  ┌─ light-theremin ────────┐  ┌─ drum-pad ────────────────┐        │
│  │ ▸ by @signalhead        │  │ ▸ by @beatmaker           │        │
│  │ ...                      │  │ ...                        │        │
│  └──────────────────────────┘  └────────────────────────────┘        │
│                                                                      │
│ ── page 1 of 12 ─── [◂ prev] [next ▸] ──────────────────────────── │
└──────────────────────────────────────────────────────────────────────┘
```

### 4.3 Layout — Mobile

Single column, card-based. Each card is a condensed version:

```
┌─ tilt-midi-cc ──────────────────┐
│ by @synthwizard · v1.2.0        │
│ Turn tilt into MIDI CC values   │
│ midi · imu    ★ 42   ↓ 380     │
└─────────────────────────────────┘
```

Tapping opens the detail view (full-screen on mobile).

### 4.4 Detail View

```
┌─ tilt-midi-cc ───────────────────────────────────────────────────┐
│                                                                   │
│  by @synthwizard · v1.2.0 · api v0.1 · updated 3 days ago       │
│  ★ 42 favorites · ↓ 380 downloads                                │
│                                                                   │
│  ┌─ PREVIEW ──────────────────┐  ┌─ CHEATSHEET ───────────────┐ │
│  │                             │  │ Controls:                  │ │
│  │   (320×240 screenshot       │  │  Tilt X → CC 74           │ │
│  │    or live sim preview)     │  │  Tilt Y → CC 71           │ │
│  │                             │  │  Btn A  → toggle active   │ │
│  │                             │  │  Enc    → select CC pair  │ │
│  └─────────────────────────────┘  │                            │ │
│                                    │ MIDI:                      │ │
│  ┌─ DESCRIPTION ──────────────┐  │  Channel: 1 (default)      │ │
│  │ Full markdown description   │  │  CC range: 0-127           │ │
│  │ of the app, its purpose,   │  └────────────────────────────┘ │
│  │ usage, tips, etc.          │                                  │
│  └─────────────────────────────┘                                 │
│                                                                   │
│  uses: [midi] [imu]                                               │
│                                                                   │
│  [★ favorite] [open in studio] [+ add to device] [↓ download]   │
│                                                                   │
│  ┌─ VERSIONS ─────────────────────────────────────────────────┐  │
│  │ v1.2.0 (latest)  — added CC pair selection via encoder     │  │
│  │ v1.1.0           — smoothing filter for tilt data          │  │
│  │ v1.0.0           — initial release                         │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

### 4.5 Repository Features

**Filtering & sorting:**
- Full-text search (name, description, author)
- Tag filter: `midi`, `game`, `audio`, `sensor`, `visual`, `tool`
- Sort: popular (★), newest, most downloaded, recently updated
- API version compatibility filter

**User actions:**
- ★ Favorite (requires account)
- Download .tomo
- "Open in Studio" → loads directly into the editor
- "+ Add to Device" → adds to your device collection (synced via account)
- View source (all scripts are open by design)

**Author features:**
- Submit new app (upload .tomo + write description)
- Edit description / cheatsheet
- Publish new versions
- View download/favorite stats

### 4.6 Collection / Device Manager

A user's "device" is a collection of scripts they want on their physical tomodachi:

```
┌─ MY DEVICE ──────────────────────────────┐
│ 3 apps · 12KB / 128KB                   │
│                                           │
│ 1. tilt-midi-cc      v1.2.0   4.2KB  [×]│
│ 2. snake-beats       v0.4.0   6.1KB  [×]│
│ 3. light-theremin    v2.0.0   1.8KB  [×]│
│                                           │
│ [reorder ↕] [sync to device] [export all]│
└───────────────────────────────────────────┘
```

---

## 5. Shared UI Components (TUI Primitives)

Building a small component library that enforces the TUI aesthetic:

### 5.1 Core Components

| Component | Description |
|-----------|-------------|
| `<TuiPanel>` | Box-drawn border, title in top-left, optional status in top-right |
| `<TuiStatusBar>` | Full-width bar with key-value pairs, segmented by `│` |
| `<TuiButton>` | `[label]` with bracket syntax, highlight on hover/active |
| `<TuiTabs>` | Tab bar with `[active]` bracketed active tab |
| `<TuiInput>` | Underscored input field, mono font |
| `<TuiSelect>` | Dropdown with `[value ▾]` display |
| `<TuiTag>` | Small `[tag]` badge using accent colors |
| `<TuiSlider>` | `──●──────` horizontal slider with box-drawing track |
| `<TuiList>` | Scrollable list with `▸` selection indicator |
| `<TuiSplitPane>` | Resizable pane split (horizontal/vertical) |
| `<TuiToast>` | Notification that appears in a status bar format |
| `<TuiDialog>` | Modal with box-drawing border, centered |
| `<TuiSparkline>` | Inline sparkline chart using block characters `▁▂▃▄▅▆▇█` |

### 5.2 Animation Philosophy

- **Fast transitions** (100-150ms) for panel focus, tab switches
- **No bouncing, no spring physics** — everything is crisp and snappy
- **Cursor blink** on focused inputs (like a real terminal)
- **Typing effect** for status messages (optional, subtle)
- **Scanline/flicker** effects only as opt-in fun features, never default

### 5.3 Keyboard Shortcuts

Global shortcuts visible in a `?` help overlay:

```
┌─ KEYBOARD ──────────────────────┐
│ ⌘S       save                   │
│ ⌘R       run / reload           │
│ ⌘P       command palette        │
│ ⌘1-5     focus panel            │
│ ⌘\       toggle sidebar         │
│ ⌘K ⌘T    change theme           │
│ ?        this help               │
│ Z/X      button A/B              │
│ ↑↓       encoder                 │
│ Enter    encoder click           │
│ Esc      stop / close            │
└──────────────────────────────────┘
```

---

## 6. Navigation & Information Architecture

### 6.1 Top-Level Navigation

```
┌─ TOMODACHI ──────────────────────────────────────────────┐
│ [studio]  [repo]  [my device]  [settings]     @username  │
└──────────────────────────────────────────────────────────┘
```

Four sections, always accessible:

1. **Studio** — Editor + simulator (desktop only; mobile shows a "desktop required" message)
2. **Repo** — Browse/search scripts
3. **My Device** — Collection manager
4. **Settings** — Theme picker, account, preferences

### 6.2 URL Structure

```
/studio                    → editor with blank project
/studio?app=tilt-midi-cc   → opens app from repo into editor
/repo                      → repository browse
/repo/tilt-midi-cc         → app detail page
/repo/tilt-midi-cc/v1.2.0  → specific version
/device                    → my device collection
/settings                  → settings & theme
/settings/theme            → theme picker
```

---

## 7. Technical Decisions to Discuss

### 7.1 Editor Choice: CodeMirror 6 vs Monaco

| Factor | CodeMirror 6 | Monaco |
|--------|-------------|--------|
| Bundle size | ~150KB | ~2MB+ |
| Theming | CSS-based, easy to match TUI | Theme API, harder to customize deeply |
| Mobile | Works (with limitations) | Not designed for mobile |
| Lua support | Community extension | Community extension |
| API completions | Custom via extensions | Built-in IntelliSense |
| Verdict | **Recommended** | Overkill for Lua editing |

### 7.2 State Management

Options for the studio:
- **Zustand** — lightweight, great for panel state, editor state, sim state
- **Jotai** — atom-based, good for reactive sensor values
- Recommendation: **Zustand** for app state + React context for theme

### 7.3 Repo Backend (Post-MVP)

- Supabase or similar (auth + DB + storage)
- .tomo files in object storage (R2/S3)
- Metadata in Postgres
- Search via pg_trgm or Meilisearch

### 7.4 WASM Integration

The C++ core compiled to WASM exposes:
- `loadApp(luaSource, manifest)` → initialize
- `tick(dt, inputState, sensorState)` → run one frame
- `getFramebuffer()` → pointer to composed pixel data
- `getMidiEvents()` → queue of pending MIDI messages
- `getConsoleOutput()` → print buffer

The React app drives the loop with `requestAnimationFrame` + timing logic.

---

## 8. Open Questions

1. **Theme scope:** Should themes also affect the device preview (the 320×240 canvas palette)? Or is the device palette always fixed to the app's chosen 15 colors? → Probably fixed; themes are for the IDE chrome only.

2. **Live collaboration?** Not MVP, but worth considering the architecture. WebRTC or CRDT-based? → Defer entirely.

3. **Embedded previews?** Should repo cards show a live running preview (WASM in an iframe) or just a static screenshot? → Screenshot for MVP (live previews are expensive). Maybe a "try it" button that boots the sim.

4. **Script size display:** Should the repo show script complexity metrics (lines of code, asset size, API usage)?

5. **Rating vs favorites:** Just favorites (binary) or star ratings (1-5)? → Favorites only. Simpler, more honest signal.

6. **Comments/discussion:** Per-app comments in the repo? Or defer to external (GitHub issues)? → Defer for MVP.

7. **Naming:** "Tomodachi Studio" and "Tomodachi Repo"? Or something more playful? "Tomo Lab"? "Tomo Shelf"?

---

## 9. Example User Flows

### Flow 1: New User Creates Their First Script

1. Land on `/studio` → sees a starter template pre-loaded
2. Code editor has `main.lua` with a commented hello-world
3. Hit `⌘R` or click `[▸ RUN]` → device preview shows output
4. Tweak code → hot reload shows changes immediately
5. Open sensor panel → drag IMU pad → see values change in device
6. Satisfied → `⌘S` → downloads `.tomo` file

### Flow 2: Browse Repo and Try a Script

1. Visit `/repo` → sees popular scripts
2. Filter by `[midi]` tag → list updates
3. Click `tilt-midi-cc` → detail page with description + cheatsheet
4. Click `[open in studio]` → redirects to `/studio?app=tilt-midi-cc`
5. Studio loads the script, can modify and test
6. Click `[★ favorite]` and `[+ add to device]`

### Flow 3: Mobile User Browses Repo

1. Open `/repo` on phone → mobile layout, single column cards
2. Scroll, filter by tags
3. Tap a card → full detail view
4. Tap `[+ add to device]` → added to collection
5. Tap `[open in studio]` → sees "Studio requires desktop" message with option to bookmark

---

## 10. Mood & Inspiration

**TUI references to study:**
- lazygit — panel layout, keybinding display
- btop — sparklines, density, color usage
- helix editor — status bar design, mode indicators
- yazi — file browser, preview panes
- charm.sh tools (glow, soft-serve) — polished TUI aesthetic

**Web references:**
- itch.io — community game browser feel
- shadcn/ui — component quality baseline
- Warp terminal — TUI feel in a modern app
- GitHub CLI — command palette patterns

**The vibe in one sentence:** *"What if lazygit and itch.io had a baby that lived in your browser and spoke MIDI."*
