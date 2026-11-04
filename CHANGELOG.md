# Changelog

## 0.1.4 — 2026-11-04

### Added

- 新增 GETEX 选项：`PERSIST`、`EX`、`EXAT`、`PXAT`（try_getex_persist/ex/px/exat/pxat 及便捷层 getex_ex/exat/pxat）
- 新增 SET GET 选项返回旧值（set_get、set_with_options 返回 Result[String?, RedisError]）
- 新增 SET EXAT/PXAT 选项（绝对过期时间戳，过去时间戳立即删除 key）
- 新增 ZRANGE WITHSCORES（try_zrange）、ZRANGEBYLEX LIMIT（try_zrangebylex_with_limit）、ZREVRANGEBYLEX LIMIT、ZREVRANGEBYSCORE LIMIT
- 新增 ZINTERSTORE/ZUNIONSTORE WEIGHTS/AGGREGATE 测试覆盖
- 新增分数边界语义：`[a`/`(a`/`-inf`/`+inf`（try_zrangebyscore_by_bound 族 4 个 + ZRANGESTORE BYSCORE/BYLEX）
- 新增 ZADD NX/XX/CH/GT/LT/INCR（try_zadd_with_options、try_zadd_incr）
- 新增位图命令：GETBIT/SETBIT/BITPOS/BITOP（AND/OR/XOR/NOT）、BITCOUNT 范围（UTF-16 code unit 位图）
- 新增 WATCH/UNWATCH 乐观锁（try_exec 监视冲突返回 TransactionAborted）
- 新增事务操作：DEL/EXPIRE/PERSIST/LPOP/RPOP/GETSET 排队回放（TransactionOp 扩至 17 种）
- 新增命令：EXPIRETIME/PEXPIRETIME、SPOP count（spop_count）、HSET 多字段（hset_all）、
  SUBSTR、SAVE/BGSAVE/LASTSAVE、COMMAND_COUNT、RESET、PSETEX/BITCOUNT 命令表登记
- 新增 LPOS RANK/COUNT/MAXLEN 选项（try_lpos_options；差异：无 COUNT 时本库恒返回数组，官方返回单整数）
- 新增 BITFIELD 位段读写（try_bitfield_get/set/incrby，宽 1-32 位 WRAP 溢出；差异：code unit 内 LSB-first 位序）
- 新增 GETDEL WRONGTYPE 修复（try_getdel，此前类型错误被吞且不删除）
- 新增 RedisError::WrongArgumentCount(cmd)（"ERR wrong number of arguments for 'x' command"）
- 新增统一 ZRANGE 6.2 修饰符（try_zrange_modifiers：BYSCORE/BYLEX/REV/LIMIT/WITHSCORES 分发）与
  EXISTS 多 key（exists_many）、UNLINK 多 key（unlink_many）、MEMORY_USAGE、try_lpop_count/try_rpop_count
- 新增 try_bitfield_incrby_mode（WRAP/SAT/FAIL 三态溢出控制）
- 新增 try_restore（REPLACE/BUSYKey/ABSTTL）；WAIT/SHUTDOWN/CONFIG RESETSTAT/LOLWUT 占位命令
- 新增 SORT 完整版（try_sort_advanced：BY hash_*->field 权重、GET #/pattern->field 展开、nosort）
- 新增吞错点 try_ 版：try_setnx/try_setex/try_psetex/try_linsert/try_lrem/try_ltrim/try_smove/
  try_zremrangebyrank/try_rename（顺带修复 setex/psetex 缺类型检查）
- 新增 glob 字符类与转义：[abc]/[a-z]/[^...]/\*（KEYS_PATTERN 与 SCAN 家族同语法）；
  修复 hscan/sscan/zscan 弱版匹配器（连 ? 都不支持）
- 新增 SETEX/GETDEL/GETEX/EXPIRETIME 等示例到 demo 与 examples
- 新增 expireat_with_options/pexpireat_with_options（NX/XX/GT/LT）、try_lset
- 新增 HSCAN/SSCAN/ZSCAN MATCH 模式匹配（zscan 改独立快照，迭代期间写入不再重复/漏读）
- 新增 INFO section（server/memory/keyspace/stats 四节，info_section API）
- 新增 hgetall_entries（免 Map 复制遍历）
- 新增 OBJECT refcount 与 idletime、client_list/config_get/slowlog 子命令语义实化
- 新增 ZSet 缓存 LRU 淘汰（MAX_CACHE_SIZE=100）
- 新增 SINTER 最小集合优化

### Fixed

- TTL 取整改为向上取整（剩 1ms 返回 1 而非 0，对齐 Redis）
- EXPIRE/PEXPIRE/EXPIREAT/PEXPIREAT 过去时间戳立即删除 key 并成功返回
- LPUSHX/RPUSHX 类型错误返回 -1（可区分 key 不存在）；hmset 改返回 Bool
- getset 统一走 try_set_get 路径，类型错误不再静默
- set_with_options 补 EX+EXAT/PX+PXAT 互斥校验；互斥错误改 SyntaxError
- info 的 uptime_in_ms 改为真实运行时长（新增 start_time 基准）
- RedisError 新增 OutOfRange/IndexOutOfRange/SyntaxError/BusyKey/TransactionAborted 变体
- flushdb/flushall 返回 "OK"；命令表补齐 DISCARD/EXEC/LMPOP/MULTI/ZMPOP 与 Z1/A1/A3 的 10 条命令
- 事务回放 GetSet/Incr/Decr 对类型错 key 静默覆盖（对齐非事务路径跳过不写）
- ZRANGESTORE 三变体空结果/source 缺失时删除 destination（对齐 Redis dbDelete 语义）
- INCRBYFLOAT 对不存在 key 按 0 处理并创建（此前返回 NotFound）

### Performance

- insertion_sort 改 in-place 移位（消除 O(n²) 次数组分配）
- merge_sort 改单缓冲区归并（分配量 O(n log n) → O(n)）
- random_sample 去除 pool 全量复制（部分 Fisher-Yates 原地交换）
- ZSet 删除/弹出逻辑去重（remove_zset_collected/zpop_extreme）
- ZSet 缓存 LRU 淘汰，防止内存泄漏；SINTER 最小集合优化
- 清除 try_zadd/try_hincrbyfloat 的 Infinity 死分支（Float32 下恒 false）
- 基准基线扩展 12 → 20 项（sort by/bitfield/setbit/zrangestore/zunionstore/exists_many/lpos_options 等 18 轮新增功能纳入回归护栏）
- zrange 1000: 13.46µs；zrank 1000: 169.71ns

### Engineering

- 命令数 104 → 167（String 24、Hash 13、List 17、Set 14、Sorted Set 32、Key 19、服务器 38）
- 测试数 149 → 208；lib 覆盖率 88.06%；zset.mbt 三拆（CRUD/range 系/store 系）
- SCAN 家族匹配器统一（修复 hscan/sscan/zscan 弱版）；glob 支持 [abc]/[a-z]/[^...]/\* 转义
- 吞错点 top 10 补 try_ 版（try_setnx/setex/psetex/linsert/lrem/ltrim/smove/zremrangebyrank/rename）
- cli_repl 支持命令 65 → 94；README 新增"与 Redis 的已知差异"专节与"后续路线图"节
- 魔术数字常量化（DEFAULT_SCAN_COUNT/EMBSTR_MAX_SIZE/HASHTABLE_MAX_ENTRIES/LISTPACK_MAX_ENTRIES）
- RedisError 9 变体对齐 Redis 错误文本；README/API 表/check_docs.py 全程同步

## 0.2.0 — 2026-11-03

### Added

- 新增事务支持：`MULTI`、`EXEC`、`DISCARD`（Set/Incr/Decr/HSet/HDel/LPush/RPush/SAdd/SRem/ZAdd/ZRem）
- 新增批量弹出：`LMPOP`、`ZMPOP`
- 新增 `HRANDFIELD ... WITHVALUES`：`hrandfield_with_values`
- 新增 `ZRANDMEMBER ... WITHSCORES`：`zrandmember_with_scores`
- 新增 `COMMAND INFO`：`command_info`
- 新增 ZSet 二分插入优化：`find_insert_position`

### Fixed

- 事务中命令排队正确性（set/incr/decr/hset/hdel/lpush/rpush/sadd/srem/zadd/zrem）
- 事务嵌套 MULTI 报错
- 事务 EXEC/DISCARD 无 MULTI 报错

### Performance

- `update_sorted_items` 使用二分查找定位插入点
- zrange 1000: 18.83µs → 13.72µs
- zrank 1000: 271.56ns → 150.81ns
- copy zset 500: 203.80µs → 114.72µs

### Engineering

- 命令数 126 → 131
- 测试数 149 → 159

## 0.1.3 — 2026-10-31

- 完成基础五种数据类型与命令集
- 修复负索引、负区间、遍历删除、过期判断等缺陷
