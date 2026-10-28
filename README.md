# EDF6 Corpse Optimization 0.3.0

整合原有残骸优化和蛋壳数量限制。使用同一个 DLL、INI 与日志文件。

## 功能和设置

退出游戏后编辑 Mods/Plugins/EDF6GeneratorBodyBudget.ini，再重新启动：

```ini
[Optimization]
FullLightWeightMode=1
AcceptGeneratorObjects=1

[EggShells]
Enabled=1
MaxShellGroups=32

[Logging]
Enabled=1
```

- FullLightWeightMode：强制启用原生完整轻量模式，收紧原版残骸预算及部分碎屑限制。
- AcceptGeneratorObjects：让锚塔等生成器产生的相关残骸参与原版预算。
- EggShells Enabled：控制蛋壳限额。0 为关闭此项，1 为开启。
- MaxShellGroups：蚂蚁卵与 E619 小鱿鱼卵**共用**的同时存在蛋壳对象组上限。默认 32，允许 0–1024；开启时设 0，跳过全部受管理蛋壳创建。一个组可能包含多片蛋壳，数值不代表单片数量。
- Logging Enabled：0 为不创建/写入日志，1 为开启。日志为 DLL 旁的 EDF6GeneratorBodyBudget.log，旧日志保留，最多追加至约 2 MiB。

所有开关缺失、无效或 INI 读不到时默认开启；数量缺失、无效或超范围时默认 32。原有两项优化设为 0 时停止插件强制启用，仍尊重关卡自己的原生设置。旧版 INI 没有 EggShells 段时，新功能默认开启并使用 32。

## 安装与升级

需要 EDFModLoader。

1. 完全退出游戏，备份现有 EDF6GeneratorBodyBudget.dll 和 INI。
2. **移除独立测试版 EDF6AntEggShellBudget.dll。** 同名 INI 不再使用，也可以移出插件目录。不要同时保留两个蛋壳插件；检测到独立版仍在时，默认配置的整合版会拒绝启用并写入 REFUSED。
3. 将压缩包中的 Mods 文件夹合并到游戏目录，更新 EDF6GeneratorBodyBudget.dll 与 INI。若原有开关做过自定义，按需保留其值并加入 EggShells 段。
4. 启动游戏，日志应出现 READY version=0.3.0，默认全开时 hooks=7。

不改动磁盘上的原版游戏文件、资源或任务脚本。不要在游戏运行中加载、卸载或替换插件。

## 蛋壳范围与表现

覆盖普通/红色/金色蚂蚁卵，以及 E619_SQUID_SMALL_EGG / EGGB 对应的小鱿鱼卵。限定四个原版破壳资源路径及已核对的共同创建点；不自动覆盖蜘蛛卵囊、其他卵或未知自定义路径。

满额时只跳过新蛋壳创建，保留原孵化单位生成流程；现有蛋壳保持原版寿命，由原销毁流程恢复名额。不会主动提前删除已生成的蛋壳，不修改孵化单位数量、血量、伤害或孵化触发时机。

## 简单复测

先保持默认值，分别进入已测试的蚂蚁卵关卡和小鱿鱼卵关卡，触发集中孵化，观察是否顺畅、敌人是否正常孵出以及任务能否继续。再重开一次，核对功能仍正常。

若需要对照，仅把 EggShells Enabled 改为 0，保留原有两项优化及其他 MOD 设置一致；完全重启后重复。同一开关设为 1、MaxShellGroups=0 可测不生成蛋壳的情况。

EGG_BUDGET 日志的 blocked 表示在工厂执行前拦截的组数，live/peak 表示登记的同时存在组数；累计 created 可以超过上限，因为名额可以回收。LIMITER_STOP 表示蛋壳身份等核验失败后已停止继续限额，恢复原创建流程。REFUSED 表示插件未启用。

作者在实机测试后反馈本版效果显著。这是体验反馈，尚无受控帧时间数据或固定提升百分比；没有完成全关卡、全 MOD 组合或联机兼容验证。

## 兼容与源码

只支持已核验的 EDF.dll SHA256：0D5092393A1B9D8F1B75B0C93D8A8F65615A36A92C46E40349C2B66C733D0916。文件哈希或启用功能的关键内存字节不匹配时拒绝启用。与修改同一入口、替换相同破壳资源的其他 MOD 不保证兼容。

本包为未签名原生 Hook DLL，没有联网、下载、远控或自动安装功能；不保证安全软件不会误报。

本仓库包含本版源码、默认 INI、测试和 MinHook 原许可。构建方法见 [BUILDING.md](BUILDING.md)，验证范围见 [VALIDATION.md](VALIDATION.md)，安全说明见 [SECURITY.md](SECURITY.md)。运行 build.ps1 可编译并执行离线测试，不启动游戏。

## English

Version 0.3.0 combines the original native Full Lightweight Mode and generator-origin remains budget with a shared egg-shell group cap. It covers the three original ant egg break resources and e619_squid_small_egg_break.sgo. The shared default cap is 32 object groups, not 32 individual fragments. Set [EggShells] Enabled=0 to disable this feature, or keep it enabled and set MaxShellGroups=0 to skip all targeted shell groups. Existing shells retain their native lifetime.

Remove the standalone EDF6AntEggShellBudget.dll when upgrading, update both the main DLL and INI, and restart the game. All features use EDF6GeneratorBodyBudget.ini and the same optional log. The author reported a substantial improvement in gameplay testing; no fixed FPS gain, exhaustive compatibility, multiplayer certification or antivirus clearance is claimed.

Source and tests only; game executables, assets, crash dumps and player logs are excluded. Includes MinHook under its original license.
