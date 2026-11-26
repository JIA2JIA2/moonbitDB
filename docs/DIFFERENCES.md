# 与 Redis 的已知差异

以下差异源于实现架构（UTF-16 字符串 / 内存库无复制 / 事务沙箱），均已在对应命令行标注。

> **与 Redis 的已知差异（精度）**：分数与浮点值已采用 **Double**（f64，与 Redis 一致）——Float32 时代的累加误差/2^24 整数丢失/漂移问题已根治；计数类数值用 `Int64`（与 Redis 一致）。残余差异：① `format_float` 采用 15 位有效数字生成 + 尾零剥离（`INCRBYFLOAT` 累加 `0.1`、`0.2` 输出 `0.3`）；≥ 2^63 的有限值与 ±inf 一律输出 `inf`/`-inf`（Redis 用 `%Lg` 科学计数法区分 1e+308 与 inf）；② `parse_float` 接受 `inf`/`+inf`/`-inf` 与 ±308 指数（超界按 base 符号返回 ±inf/±0.0），浮点写读闭环成立。

> **与 Redis 的已知差异（字符串索引与 Unicode）**：字符串按 **UTF-16 code unit** 索引（Redis 按字节）。中文/emoji 场景：emoji（代理对）占 2 个 code unit，`GETRANGE`/`SETRANGE`/`SETBIT` 等落在代理对中间会**拆出孤立代理（无效字符）**；`STRLEN` 对 emoji 返回 2（Redis 按字节返回 4）；位命令对 emoji 的一半置位会破坏整个字符。

> **与 Redis 的已知差异（事务）**：MULTI 期间 24 种操作排队（SET/GETSET/INCR/DECR/INCRBY/DECRBY/APPEND/SETNX/GETDEL/HSET/HDEL/HSETNX/HINCRBY/LPUSH/RPUSH/LPOP/RPOP/SADD/SREM/ZADD/ZREM/DEL/EXPIRE/PERSIST，N1 扩展后），其余写命令直接执行；WATCH 冲突时 EXEC 返回错误（官方返回 nil 回复）。MULTI 内排队的 `SET` 不携带 EX/PX/EXAT/PXAT/KEEPTTL/NX/XX/GET 选项（`TransactionOp::Set` 仅键值），带选项的 SET 在 MULTI 中直接执行、破坏原子性——待新增事务变体修复。

> **与 Redis 的已知差异（BITFIELD）**：读写返回 **Int64**（u32 无符号读值可达 4294967295，不再按 Int32 截断）；SET 对无法放入位段的 value 返回错误（对齐 Redis）；INCRBY 的 increment 入参仍为 Int32（u32 饱和上溢仅可通过多次 INCRBY 累积触发）。

> **与 Redis 的已知差异（SORT）**：`LIMIT offset 0` 返回自 offset 起全部结果（Redis count=0 返回空数组）；DESC 通过数组翻转实现，相等 sort key 的元素顺序与升序结果相反；STORE 目标 key 类型非 list 时返回 WRONGTYPE（Redis 覆盖任意类型）。

