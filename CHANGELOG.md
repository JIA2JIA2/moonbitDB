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
- 新增 LPOS RANK/COUNT/MAXLEN 选项（try_lpos_options）
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
- RedisError 新增 OutOfRange/IndexOutOfRange/SyntaxError/TransactionAborted 变体
- flushdb/flushall 返回 "OK"；命令表补齐 DISCARD/EXEC/LMPOP/MULTI/ZMPOP 与 Z1/A1/A3 的 10 条命令

### Performance

- insertion_sort 改 in-place 移位（消除 O(n²) 次数组分配）
- merge_sort 改单缓冲区归并（分配量 O(n log n) → O(n)）
- random_sample 去除 pool 全量复制（部分 Fisher-Yates 原地交换）
- ZSet 删除/弹出逻辑去重（remove_zset_collected/zpop_extreme）
- ZSet 缓存 LRU 淘汰，防止内存泄漏；SINTER 最小集合优化
- 清除 try_zadd/try_hincrbyfloat 的 Infinity 死分支（Float32 下恒 false）
- zrange 1000: 13.46µs；zrank 1000: 169.71ns

### Engineering

- 命令数 104 → 160（String 23、Hash 13、List 17、Set 14、Sorted Set 32、Key 19、服务器 32）
- 测试数 149 → 192；lib 覆盖率 88.75%
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
