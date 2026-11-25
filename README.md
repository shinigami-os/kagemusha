# kagemusha

A decoy screen for Kira Linux. It makes every app believe the screen has any resolution you ask for (2K, 4K, 5K, an
ultrawide), then scales the result down onto the real panel, including an integrated laptop screen.

Status: M1 (EDID generator), M2 (the `kagemushad` daemon) and M3 (Hyprland glue and the `kage` command) work on the
dev machine. Packaging (the evdi kernel patch, recipes, device creation at boot) is M4. The idea was
proven with a throwaway prototype, see [docs/EXPERIMENT.md](docs/EXPERIMENT.md). The plan is in
[docs/DESIGN.md](docs/DESIGN.md).

## Build and test
```
make            # build/kage, build/kage-edid and the tests
make test       # 627 checks
build/kage-edid 2560x1440@60 > edid.bin

make daemon     # build/kagemushad, needs libevdi (set EVDI_CFLAGS and EVDI_LIBS if it is not installed)
build/kagemushad 2560x1440@60 -v         # runs until SIGINT or SIGTERM, -t SECONDS to stop by itself
```
The daemon needs the `evdi` kernel module, a created evdi device, and access to `/dev/dri` (group `video`).

## Use
```
kage up 2560x1440@60 [--on eDP-1]   # virtual output of that size, the real one mirrors it
kage status
kage down                           # Hyprland reloads its config, the daemon stops
```
`kage up` needs a running Hyprland (it talks to its socket) and an evdi device (`/sys/class/drm/cardN-DVI-I-M`).
The daemon runs as your user, started by `kage up`; its log is `$XDG_RUNTIME_DIR/kagemusha/daemon.log`.

## How it works, in one line
A virtual monitor (the EVDI kernel module) with a generated EDID declares the fake resolution to the whole system, the
compositor lays everything out on it at scale 1.0, and the real panel mirrors it.

## Names
| What | Name |
|---|---|
| project, recipe | `kagemusha`, `kira-kagemusha` |
| command | `kage` |
| daemon | `kagemushad` |
