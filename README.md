# kagemusha

A decoy screen for Kira Linux. It makes every app believe the screen has any resolution you ask for (2K, 4K, 5K, an
ultrawide), then scales the result down onto the real panel, including an integrated laptop screen.

Status: M1 (EDID generator) and M2 (the `kagemushad` daemon) done. The compositor glue and the `kage` CLI are not
written yet; for now the compositor side is driven by hand. The idea was
proven with a throwaway prototype, see [docs/EXPERIMENT.md](docs/EXPERIMENT.md). The plan is in
[docs/DESIGN.md](docs/DESIGN.md).

## Build and test
```
make            # build/kage-edid and the tests
make test       # 576 checks
build/kage-edid 2560x1440@60 > edid.bin

make daemon     # build/kagemushad, needs libevdi (set EVDI_CFLAGS and EVDI_LIBS if it is not installed)
build/kagemushad 2560x1440@60 -v         # runs until SIGINT or SIGTERM, -t SECONDS to stop by itself
```
The daemon needs the `evdi` kernel module and access to `/dev/dri` and `/sys/devices/evdi/add` (run it as root, as the
runit service will).

## How it works, in one line
A virtual monitor (the EVDI kernel module) with a generated EDID declares the fake resolution to the whole system, the
compositor lays everything out on it at scale 1.0, and the real panel mirrors it.

## Names
| What | Name |
|---|---|
| project, recipe | `kagemusha`, `kira-kagemusha` |
| command | `kage` |
| daemon | `kagemushad` |
