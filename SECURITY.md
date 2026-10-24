# Security review notes

Version 0.2.0, Windows x64.

Purpose: force two native EDF6 performance policies.
DLL SHA256: 8F5EADFDA9EE70887F73C246E09D0A30EAE981BC3BAD488DADC0769C5F54C01E

The only dynamic code changes are the four documented current-process MinHook detours, selected by the two feature settings. Native Full and acceptance setters perform policy changes; existing game code handles resource cleanup. Memory reads use GetCurrentProcess(). MinHook enumerates and coordinates threads owned by that same process. No remote-process injection, network client, credential collection, autostart, security-product tampering, obfuscator or packer is implemented. This is a source/PE review statement, not an antivirus scan certificate.

The startup thread reads the adjacent INI, optionally opens an adjacent status log, checks EDF.dll SHA256 and required bytes, installs selected hooks, and exits. No periodic game-pointer polling thread remains. The optional log is append-only and bounded; a log-open failure does not disable the plugin.

Five entry signatures and four gate signatures are validated before installation. Both features have independent state validation/fail-stop. Necessary executable-memory operations remain because this is a runtime modification. The DLL is unsigned; no claim of vendor allowlisting is made.

Before a false-positive submission, retain the original detected sample, exact detection name, file hash and product version. No sample has been submitted by this build workflow. Full corresponding project source, MinHook license/source, tests and build script are supplied in the Sources ZIP.
