# 与 Redis 的已知差异

以下差异源于实现架构（MoonBit Float32 / UTF-16 字符串 / 内存库无复制），均已在对应命令行标注。

> **与 Redis 的已知差异（精度）**：分数与浮点值为 **Float32**（约 7 位有效数字，Redis 为 Double）：① `0.1+0.2` 类累加误差在 1e-8 量级即暴露，INCRBYFLOAT 连续累加存在双重舍入漂移；② 绝对值 ≥ 2^24（16777216）的整数分数会静默丢失低位；③ `format_float` 输出最多 10 位小数且为截断（非四舍五入）、无科学计数法；④ 计数类数值用 `Int64`（与 Redis 一致）。

> **与 Redis 的已知差异（字符串索引与 Unicode）**：字符串按 **UTF-16 code unit** 索引（Redis 按字节）。中文/emoji 场景：emoji（代理对）占 2 个 code unit，`GETRANGE`/`SETRANGE`/`SETBIT` 等落在代理对中间会**拆出孤立代理（无效字符）**；`STRLEN` 对 emoji 返回 2（Redis 按字节返回 4）；位命令对 emoji 的一半置位会破坏整个字符。

> **与 Redis 的已知差异（事务）**：MULTI 期间仅 17 种操作排队（见 `MULTI` 命令行），其余写命令直接执行；WATCH 冲突时 EXEC 返回错误（官方返回 nil 回复）。

