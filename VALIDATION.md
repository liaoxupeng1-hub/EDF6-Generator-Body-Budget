# Validation for 0.2.0

The published src/, tests/ and vendor/ files match the source snapshot used for version 0.2.0 byte-for-byte.

- 1336 offline assertions passed: feature combinations; missing INI and missing keys defaulting ON; Boolean settings; logging on/off and file cap; original callback forwarding; mission loading order; retry after state initialization; restart handling; flag preservation; independent failure-stop; signature mismatch rejection; four MinHook trampolines.
- Two isolated real EML6_Load hosts passed with both optimization features disabled: one with logging ON and one with logging OFF. No EDF6 game was loaded.
- Final build completed without compiler warnings. PE exports only EML6_Load. File/product version is 0.2.0; DYNAMIC_BASE and NX_COMPAT are enabled.
- Static inspection found BeginLoading in 190 base-game and 84 DLC main mission scripts. This is static coverage, not 274 in-game tests.

The new combined FullLightWeightMode + generator-budget build has not yet been validated in-game. Earlier testing of the generator-budget feature alone does not establish the new combined version's stability or multiplayer compatibility.

Reference DLL SHA256:

8F5EADFDA9EE70887F73C246E09D0A30EAE981BC3BAD488DADC0769C5F54C01E

Supported EDF.dll SHA256:

0D5092393A1B9D8F1B75B0C93D8A8F65615A36A92C46E40349C2B66C733D0916

No game binaries, player logs, local system crash reports, credential files or compiler outputs are included in this repository. Antivirus vendor review and code signing have not been completed; no guarantee of antivirus clearance is made.
