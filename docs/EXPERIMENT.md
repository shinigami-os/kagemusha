# The experiment (2026-11-25)

A throwaway prototype proved the idea on KIRA-POWER (Hyprland 0.55.4, kernel `7.3.1-shinigami-26.11-2`, the 1920x1080
eDP-1 panel). This file keeps what it taught us. The prototype code is not in this repo.

## What was built
- `evdi.ko` 1.15.1 (DisplayLink, out of tree), compiled against the Shinigami source with clang 20.1.8, in a separate
  kbuild output directory (`O=`) so the kernel clone stays untouched.
- `libevdi.so.1.15.1`, compiled with gcc.
- A 250-line C program that generates an EDID, connects to EVDI, and drains frames.

## Results
- The kernel exposes `card1-DVI-I-1` as connected with exactly the modes of the generated EDID.
- Hyprland identifies the screen by the EDID name, enables the mode, and lays windows out on it at scale 1.0
  (windows were sized 3814x2082 on a 3840x2160 output).
- Mirroring the real panel onto the virtual output works: `hl.monitor({ output = "eDP-1", mirror = "DVI-I-1", ... })`.
  Hyprland then hides the mirroring monitor from `hyprctl monitors`.
- Frame rate at 2560x1440 under a continuously redrawing terminal: 63 to 66 fps, copy 2.4 ms per frame on average,
  about 17% of one core for the daemon and about 16% for Hyprland.
- An idle desktop sends 16 to 20 full frames per second. That is damage-driven, not a limit.

## Facts to keep (each one cost time)
1. **Always drain.** When `evdi_request_update()` returns true the update is ready now: call `evdi_grab_pixels()` at
   once. Ignoring it left the compositor stuck in `drm_crtc_commit_wait` (D state) until the client died. The
   daemon must never block or crash while an output is enabled.
2. **No resolution limit in EVDI.** `evdi_connect2(handle, edid, len, 0, 0)` makes `evdi_mode_valid()` accept every mode.
   The EDID is the only limit.
3. **Hyprland auto-scales new outputs.** A fresh 4K output came up at scale 1.5. Set `scale = 1.00` in the rule before
   or at enable time.
4. **Hyprland here uses the Lua config.** `hyprctl keyword` is rejected. Use `hyprctl eval 'hl.monitor({...})'` and
   `hl.exec_cmd(...)`. The monitor rule has `mirror`, `mode`, `position`, `scale`, `disabled`.
5. **A disabled-rule on the connector name keeps Hyprland away** until the daemon is ready:
   `hl.monitor({ output = "DVI-I-1", disabled = true })`.
6. **The module stays "in use".** `seatd` opens the new card for Hyprland, so `rmmod evdi` fails until the session ends.
7. **libevdi on musl** needs `#include <limits.h>` in `library/evdi_lib.c` (`PATH_MAX`). One-line patch.
8. **Out-of-tree build without a built kernel.** `modules_prepare` into `O=` works (needs `elfutils-dev` for objtool),
   `modpost` then warns about the missing `Module.symvers`. With `CONFIG_MODVERSIONS` off, `KBUILD_MODPOST_WARN=1` is safe.
   `CONFIG_WERROR=y` must be overridden for the module.
9. **Capture limits.** `grim -o <name>` fails on a mirroring output ("unknown output").
10. **Test hygiene.** Dead-man restore timers must not overlap, and `pkill -f <pattern>` kills its own shell if the
    pattern is in its own command line.

## Cost of resolutions (CPU copy of every frame)
| Mode | Full frame | Copy at about 3.5 ms per 14.7 MB |
|---|---|---|
| 2560x1440 | 14.7 MB | 2.4 to 5 ms (measured) |
| 3840x2160 | 33 MB | about 8 to 10 ms (estimated, not measured under load) |
| 5120x2880 | 59 MB | about 15 ms or more (estimate), too slow for 60 fps |

## EDID generator checked against the kernel (M1, 2026-11-25)
Eight generated EDIDs (1280x720, 1920x1080, 2560x1440, 3840x2160 at 60, 4096x2160 at 30, 5120x1440, 5120x2880 and
7680x4320 at 60) were connected to EVDI one after the other and `/sys/class/drm/cardN-DVI-I-1/modes` was read back.
The kernel listed exactly the requested mode each time, including the DisplayID extension path (everything wider
than 4095 pixels). `edid-decode` is not packaged on Kira, so the kernel is the reference.
