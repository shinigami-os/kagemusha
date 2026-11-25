# kagemusha

A decoy screen for Kira Linux. It makes every app believe the screen has any resolution you ask for (2K, 4K, 5K, an
ultrawide), then scales the result down onto the real panel, including an integrated laptop screen.

Status: design phase. Nothing here runs yet. The idea was proven with a throwaway prototype, see
[docs/EXPERIMENT.md](docs/EXPERIMENT.md). The plan is in [docs/DESIGN.md](docs/DESIGN.md).

## How it works, in one line
A virtual monitor (the EVDI kernel module) with a generated EDID declares the fake resolution to the whole system, the
compositor lays everything out on it at scale 1.0, and the real panel mirrors it.

## Names
| What | Name |
|---|---|
| project, recipe | `kagemusha`, `kira-kagemusha` |
| command | `kage` |
| daemon | `kagemushad` |
