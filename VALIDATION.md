# Validation for 0.3.0

The published src/, tests/ and vendor/ files, build.ps1 and default INI match the delivered 0.3.0 source snapshot byte-for-byte. Publication adds documentation and source checksums; it does not change the tested implementation.

## Offline evidence

- 1336 original policy checks: callback forwarding/order, flags, restart, independent failures, configuration/logging and original entry trampolines.
- 985 egg-shell assertions: all four targeted resource paths, caps including zero, native cleanup and reuse, exceptions, pointer generations, contended reservations, INI/logging, exact prologue relocation and executed synthetic x64 callsites through real detours.
- 110 integration assertions: all eight three-feature switch combinations, one installation of up to seven unique hooks, byte restoration, one INI and legacy configuration defaults.
- Two real EML6_Load hosts with all optimization features disabled and logging on/off. No EDF6 module was loaded.
- Build used -Wall -Wextra -Werror. Tests ran in synthetic/isolated hosts; none of the game code was executed by this workflow.

## Gameplay feedback

The author reported the standalone ant shell limiter was effective, and later reported a substantial improvement with the current version after the duplicate standalone-DLL installation issue was identified. No controlled frame-time trace, fixed percentage gain or exhaustive mission/multiplayer coverage has been supplied. This feedback is distinct from automated tests.

## Scope and limits

Only the exact SHA256-pinned EDF.dll is supported. Egg filtering additionally requires the verified break-object callsite and initializer, one of the four admitted original resource paths, and matching constructed-object identity. One group is one break object, not a measured individual-body count. Ordinary ants, red ants, gold ants and E619 eggs share one group budget.

Feature failure-stop remains independent; a compatibility or duplicate-plugin refusal during initialization can prevent activation. Native lightweight/budget behavior may reclaim whole remains earlier. Egg-shell limiting reduces decorative break-object creation; it is not a global collision-filter rewrite. Other MOD combinations, all game missions, prolonged runtime stability and multiplayer behavior remain incompletely verified.
