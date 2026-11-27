# kagemusha design

## Goal
Make every app on the machine believe the screen has a resolution of the user's choice, then scale it onto the real
panel. It must work on an integrated laptop screen that offers no such mode, and it must work for Flatpak apps, which
Hyprland's own fractional scale does not (VS Code laid out wrongly at scale 0.8, see EXPERIMENT.md).

Non-goals: driving a physical panel at a mode it cannot show, HDR, audio over the virtual output, X11-only setups
(Kira is Wayland only), remote displays.

## Architecture
```
kage (CLI) --spawns--> kagemushad (daemon, runs as the user)
                              |  EDID generator (pure code)
                              |  libevdi
                              v
                           evdi.ko ---> virtual DRM output (DVI-I-N, WxH, from the EDID)
                              ^
                    compositor lays apps out on it at scale 1.0
                              |
                    real panel mirrors it (downscale done by the compositor GPU)
```
Why this layer: the fake lives in the kernel as a real connector, so Wayland clients, Xwayland, Flatpak apps,
`/sys/class/drm` and libdrm all agree. Nothing per app.

## Components

### 1. EDID generator (`src/edid.c`, no dependencies, unit-tested)
Input: width, height, refresh, optional name and physical size. Output: a valid EDID (base block, plus extension when
needed).
- Timings: CVT 1.2 reduced blanking v1 (already proven for 2560x1440, pixel clock 241.50 MHz). Standard CVT and the
  CTA-861 tables as an alternative for TV-like modes.
- Base block detailed timing holds up to 4095 pixels and 655.35 MHz. Beyond that (5K, 8K, high refresh) it needs a
  DisplayID 2.0 type VII timing in an extension block. That is the part that decides "any resolution".
- Always writes the monitor name, range limits, physical size and the checksum. Optional CTA block (so Hyprland and
  apps see a sane colour format).
- Tests: decode every generated EDID with `edid-decode` and compare, plus golden files for the common modes.

### 2. Daemon `kagemushad`
- One EVDI device per virtual monitor, found through `evdi_add_device()` and `evdi_check_device()`.
- Connects with `evdi_connect2(h, edid, len, 0, 0)` (no mode limit).
- Event loop: poll on every `evdi_get_event_ready()` fd plus the control socket. On `mode_changed` it (re)allocates the
  buffer and registers it. On `update_ready` it grabs. It always loops `while (evdi_request_update()) grab()`.
- Never blocks the loop, never crashes with an output enabled (a dead daemon freezes the compositor, see EXPERIMENT.md).
  On any fatal path it disconnects first.
- Control socket (unix, in `$XDG_RUNTIME_DIR`): `up`, `down`, `status`, `list`.
- Frame handling in v1: the CPU copy that libevdi does, then discard. The compositor mirrors from its own render, the
  daemon only has to keep the commit pipeline moving.

### 3. Compositor glue
Hyprland first, behind a small interface (`enable(output, mode, scale)`, `mirror(real, virtual)`, `restore()`).
- Talks to Hyprland through its IPC socket (`.socket.sock`), not by spawning `hyprctl`.
- Order matters: disabled rule for the connector first, then connect EVDI, then enable with `scale = 1.00`, then set
  the real output to mirror it.
- `restore()` is idempotent and also runs on daemon exit and from `kage down`. It resets the real output to the
  values saved before `up`.
- Later: sway/other compositors and `fht-compositor`, same interface.

### 3b. What M3 changed from the first plan
- No control socket and no always-running service. `kage up` starts `kagemushad` as the user, detached, and records its pid in
  `$XDG_RUNTIME_DIR/kagemusha/state`. The Hyprland IPC socket belongs to the user session, so a root daemon would have had to
  guess the session. The evdi device is created once by root at boot (M4), the user only opens it (group `video`).
- The compositor glue lives in `kage`, not in the daemon (`src/hypr.c`: IPC client, monitor-list parser, rule builder that
  validates every name before it goes into Lua).
- `kage down` calls Hyprland's `reload`, which restores the owner's own config (monitors.lua), so no monitor values are saved.
  Anything changed at runtime with `hyprctl` is reset by it too.

### 4. CLI `kage`
```
kage up 2560x1440 [--hz 60] [--on eDP-1]    create the virtual monitor and mirror the real one
kage down                                   undo everything
kage status                                 what is active
kage modes                                  presets and the largest mode that still holds 60 fps here
```
No config file in v1. Arguments only.

## Packaging and kernel
- **evdi module (decided 2026-11-25):** carried as a Shinigami patch, `patches/compat/0003-evdi.patch`, like acpi_call and
  tuxedo-drivers: sources in `drivers/gpu/drm/evdi/`, `CONFIG_DRM_EVDI=m`. It ships with every kernel release, signed with the
  kernel key, no DKMS. Changes against upstream: a vblank timer (flips complete at the mode's refresh rate through the DRM
  helper of 7.x, switchable with the module parameter `vblank_timer`) and a size cap of 16384. `evdi_detect.h` is a static
  copy of upstream's build-time probe output and is regenerated at each kernel base upgrade.
- **Device at boot:** the recipe `kira-kagemusha` ships the runit service `kagemusha-evdi`: `modprobe evdi initial_device_count=1`,
  then it sleeps forever. Not a oneshot (a recipe-shipped oneshot runs only once, `/etc/runit/2` re-enables two named ones).
- **libevdi:** the recipe `kira-libevdi`, pinned to the commit the module was imported from (tarball hash), with the musl `limits.h` fix.
- **kagemusha:** the recipe `kira-kagemusha` (rolling from main; it needs this repo to be public). `kagemushad` is started by `kage up`, not by runit.
- **Size cap:** EVDI hard-codes `max_width = 7680` and `max_height = 4320` in `evdi_modeset.c` (lines 544 and 545). The
  patch must raise them for resolutions beyond 8K. The generator itself goes up to 16384x16384.
- Open item to check: the module has to be in the kernel before `seatd` opens cards, otherwise nothing changes.

## Language
**C**, decided by the owner on 2026-11-25. libevdi is C, flux is C, the daemon is small (an event loop, a socket, an
EDID builder), and a static musl binary with no dependencies fits the base.

## Milestones
| # | What | Done when | Rough size |
|---|---|---|---|
| M0 | Repo, notes, name | this document pushed | done |
| M1 | EDID generator and tests | 28 modes decode correctly, golden values match, the kernel accepts them through EVDI | done 2026-11-25 |
| M2 | Daemon (single monitor), no compositor glue | `kagemushad` holds a 2K output, frames drained, clean exit | done 2026-11-25 |
| M3 | Hyprland glue and `kage up/down` | one command gives a mirrored 2K desktop, `down` restores | done 2026-11-25 |
| M4 | Packaging: patch in Shinigami, recipes, service | installs and works on a clean machine | written 2026-11-27, not yet run on a kernel that carries evdi |
| M5 | Large modes (DisplayID), polish, notes | 5K accepted, docs written | 1 week |

Total to a usable tool: about 3 to 4 weeks. M1 to M3 give a working tool on the dev machine in about 2 weeks.

## Risks and open questions
- The output is not paced: under a continuously redrawing window the compositor flips at about 80 per second on a 60 Hz
  mode (daemon 36% of a core, Hyprland 21%). Pacing from the daemon and skipping the copy were both tried and both
  made it worse (EXPERIMENT.md). The fix is a vblank timer in the evdi patch (M4).
- Per-frame CPU copy limits large modes (see the table in EXPERIMENT.md). If 4K at 60 fps is wanted later, the copy
  has to go away (dmabuf path), which means work in the module or a different virtual-output driver.
- The idle desktop sends full frames at 16 to 20 per second. Cause not found (something in the shell repaints). Not
  caused by kagemusha, but it costs a copy each time.
- The Line's shell (Quickshell) restarted itself once during the first of three up/down cycles (its launcher restarts it when
  it dies from a signal). Not reproduced in the next two. Unknown whether a screen appearing or disappearing triggers it.
- Hyprland multi-GPU handling of a third (virtual) card. It worked here without a crash, but the log shows page-flip
  warnings, 6 in one run.
- If the daemon dies while mirrored, the real screen keeps the mirror rule. `restore()` from a service supervisor hook
  (runit `finish`) is needed.
- evdi is GPL and out of tree upstream. Carrying it as a patch means rebasing on each kernel bump (the Shinigami
  flow C already does this for the other patches).

## Rules for this repo
Follow the standing rules: conventional one-line commits with no attribution, no comments in code unless the why is
non-obvious, no speculative abstractions, no em dashes in text.
