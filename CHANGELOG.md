# 0.3.0 — 2026-10-28

- Integrate the tested standalone ant egg-shell limiter into the existing plugin.
- Add the E619 small squid egg break resource through the same verified GiantAntEgg/GiantAntEgg_Break path.
- Add [EggShells] Enabled and MaxShellGroups. The shared cap defaults to 32 groups; zero skips all targeted shell groups when the feature is enabled.
- Preserve original shell lifetimes and the native enemy-spawn branch; block excess groups before shell object creation.
- Share one DLL, INI, startup worker, MinHook installation and optional bounded log. Detect the standalone egg-shell DLL to prevent duplicate loading.
- Preserve the two original optimization switches. All three features can be configured independently.
- Pass 2431 offline checks/assertions and two loader hosts. Author reported a substantial gameplay improvement; no fixed benchmark gain or exhaustive compatibility claim.

# 0.2.0

- 添加全关原生Full轻量模式，在任务加载路径应用，并保留早期设置/独立失败停止。
- 保留生成器残骸预算强制开启；两个功能可分别关闭，关闭时尊重原任务。
- 新INI三项默认1，缺文件、缺项或无效值默认开启。
- 日志可开关，只记录状态变化；关闭后不创建或写入插件日志。
- 五入口/四门控窗口兼容检查，带版本资源，提供源码构建包。
- 保留必要MinHook，不作杀毒放行保证。
- 离线验证通过；新增组合尚未实机验证。
