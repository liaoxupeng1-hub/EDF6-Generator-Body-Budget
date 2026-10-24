# EDF6 Generator Body Budget

Windows x64 EDF6 MOD source — version **0.2.0**.

本仓库公开当前版本源码、默认 INI、离线测试及构建脚本，不包含游戏文件。构建方法见 [BUILDING.md](BUILDING.md)，验证范围见 [VALIDATION.md](VALIDATION.md)，安全与杀毒告警说明见 [SECURITY.md](SECURITY.md)。

This repository contains the current plugin source, configuration and build/test materials. It does not include game executables, game assets, crash dumps or player logs. See the documents above for building and verification limits.

两个原生优化开关：全关强制完整轻量模式，以及让生成器来源的相关身体残骸接受原版预算管理。默认两项与日志都开启。使用共享入口，不按任务编号设置白名单。

## 原理

- 完整轻量模式：调用游戏已有的 SetFullLightWeightMode()。已确认会使共享身体节点成本预算由512降至256、ModelDebris碎屑上限由400降至200，并影响人形部位对象及其他轻量分支。不是仅取消独立断肢互碰，也不是无视觉取舍的优化。
- 生成器残骸预算：强制 SetRagdollManagerAcceptGeneratorObjectMode(true)。让相关生成器/锚塔来源残骸也参与原版预算；超过预算时由游戏回收旧对象，可能使整具尸体及碰撞提前消失。

512/256是身体节点成本，不是敌人数或独立断肢数。400/200是ModelDebris，不等同真实身体断肢。任务本来已经使用轻量模式时，不会再获得一次同样的限额下降。没有修改任务刷怪、AI、伤害、血量或资源文件。

## 安装和升级

需要已有支持EDF6的EDFModLoader。本包不包含ModLoader。完全退出游戏，将包内Mods文件夹合并到游戏目录；覆盖Mods/Plugins/EDF6GeneratorBodyBudget.dll及同名INI。DLL名称不变，不能改名后同时加载新旧版。

0.2.0使用新的INI节和键，旧[Diagnostic] Mode设置不再使用，请同时更新INI。未找到新配置时按下面的默认值全部开启。

不需要REA、Ghidra、Java或开发者的D盘目录。不能同时加载旧BodyProbe/BodyBudgetProbe/Mixed/Gate/LegExit/LimbFilter/ZeroLimbs实验插件。

## 配置

文件：Mods/Plugins/EDF6GeneratorBodyBudget.ini。启动时读取，修改后必须完全退出并重新启动游戏。

```ini
[Optimization]
FullLightWeightMode=1
AcceptGeneratorObjects=1

[Logging]
Enabled=1
```

1为开启，0为关闭。两个优化项为0时，插件不强制对应功能，仍尊重原任务自己调用的设置；不会把原版已经开启的轻量模式强制关闭。两项都为0时不安装Hook。

INI不存在、读取不到、某项缺失或取值无效时，该项默认1。也支持true/false、yes/no、on/off。建议直接使用0/1，注释放在单独一行。

## 日志

Enabled=1：在DLL旁边写EDF6GeneratorBodyBudget.log，通常是游戏的Mods/Plugins目录。只记录启动、每关加载、实际设置变化和失败，不追踪每个身体部件，不运行持续日志线程。单文件追加写入设1MiB上限，达到上限后停止追加；需要新日志时可在退出游戏后自行备份旧文件。

Enabled=0：本插件不打开或写入日志，已有日志保留。ModLoader或其他MOD的日志独立，不受此选项控制。目录不可写时优化仍可启动，日志失败不会成为优化的前置条件。

关键记录：START的full/budget/logging是配置；READY表示兼容校验和Hook安装完成，尚不等于已实机覆盖所有关卡；FULL_APPLIED表示原生轻量设置已核对为2；BUDGET_APPLIED表示主残骸管理器接纳位从0设置为1。原任务已设置目标值时可能没有APPLIED；FULL_STOP/BUDGET_STOP表示后续不再强制对应功能，原游戏调用仍继续；REFUSED表示版本或入口检查等未通过。

## 兼容性和验证

仅支持已校验的Windows x64 EDF.dll，SHA256：
0D5092393A1B9D8F1B75B0C93D8A8F65615A36A92C46E40349C2B66C733D0916

五个相关入口和四个原预算门控窗口会在启动时检查。检测到不匹配或入口已被修改时拒绝启用，不扩大适配范围。仍不能保证与所有第三方插件兼容。

完整轻量设置在原BeginLoading调用返回后、原任务线程继续预加载前应用；普通SetLightWeightMode调用后再确保Full。预算开关继续在原登记路径保证开启，并覆盖脚本关闭请求。初始化线程只安装Hook，完成后结束。两个功能分别校验状态并分别失败停止，不在后台定时改游戏内存。

静态核对了原版190及DLC84共274份主任务脚本，均使用BeginLoading。此为静态覆盖，不等同274关实机验证。1336项离线断言与真实ModLoader格式EML6_Load独立宿主的日志开/关加载测试通过；没有运行游戏，新0.2.0仍需实机验收。原预算功能此前的0.1.0实机结果不能替代新增Full组合的验证。联机和全敌人兼容性未全面验证。

## 安全软件与源码

此DLL使用公开MinHook在当前游戏进程修改必要函数入口；全开时四个Hook，每个单独功能开启时两个。相关行为可能触发安全软件。此次保留必要校验、移除无关诊断、增加明确版本资源、提供完整源码与构建脚本，不加壳、不混淆、不隐藏行为，不包含独立注入器、联网通信、远程控制、开机启动或防火墙修改。

不能保证消除腾讯电脑管家等告警；没有进行厂商复核或数字签名认证。不要将本版描述成“已通过腾讯安全认证”或“绝不报毒”。如遇告警，应核对文件哈希并提交厂商复核。另提供对应Sources ZIP供审查和自行重建。

DLL SHA256：8F5EADFDA9EE70887F73C246E09D0A30EAE981BC3BAD488DADC0769C5F54C01E

## English

Forces two existing native policies across missions: FullLightWeightMode and acceptance of generator-origin remains into the game's shared ragdoll budget. Both features and local logging default ON, including when the INI is missing or a setting cannot be read. Set each option to 0 to stop enforcing that feature; original mission settings are respected. Restart after configuration changes.

Requires EDF6-compatible EDFModLoader. Merge Mods into the game directory and replace both existing files. Update the INI when upgrading from 0.1.x. No developer tools are needed to run the plugin.

Full mode tightens the native ragdoll cost and some fragment/part limits; the game may reclaim whole remains and their collision earlier. This is not a limb collision filter and has visual/gameplay trade-offs. Only the exact supported EDF.dll is accepted. Offline tests passed; this new combined build has not been tested in-game. Source/build materials are provided separately. Necessary runtime hooks can trigger antivirus alerts; no antivirus clearance or code-signing certification is claimed.

## 卸载

退出游戏后移出DLL和INI。没有改写原版任务或资源；移除插件后仍由任务脚本及其他MOD决定原生设置。旧日志不会自动删除。

Includes MinHook; see MinHook-LICENSE.txt.
