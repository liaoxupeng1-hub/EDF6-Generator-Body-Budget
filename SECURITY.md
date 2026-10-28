# Security review notes — 0.3.0

Windows x64 native runtime plugin. DLL SHA256: 558E5822C20E433C776B2B499FCDC9B5D442BDB1BCC91FEEE68B596D957A7F03

The plugin uses current-process MinHook detours: two for native Full Lightweight Mode, two for generator-origin remains acceptance and three for egg-shell creation/lifecycle bookkeeping. Features select their hooks independently, up to seven when all enabled. Necessary executable-memory changes and coordination of game-process threads remain part of this implementation.

Startup reads the adjacent INI, optionally opens EDF6GeneratorBodyBudget.log, validates EDF.dll SHA256 and the byte windows needed by enabled features, installs the selected hooks and exits. There is no periodic background game-pointer polling worker. Logging is append-only and bounded to approximately 2 MiB; disabled logging does not create or write a log and does not delete an old one.

The supplied implementation has no network client, downloader, remote-process injector, credential collection, autostart, antivirus disabling, firewall modification, packer or obfuscator. These are source-review statements, not an antivirus certificate. The DLL is unsigned; no vendor allowlisting or guarantee against antivirus alerts is claimed.

Keep the original sample, exact detection name, hash and security-product version when reporting a suspected false positive. No sample was submitted to an antivirus vendor by this publication workflow. This repository contains project source/tests and MinHook with its original license, not game binaries, assets, dumps or player logs.
