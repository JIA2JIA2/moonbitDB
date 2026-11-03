# Changelog

## 0.2.0 — 2026-11-04

### Added

- 新增 GETEX 选项：`PERSIST`、`EX`、`EXAT`、`PXAT`（try_getex_persist/ex/px/exat/pxat）
- 新增 SET GET 选项返回旧值（set_get、set_with_options 返回 Result[String?, RedisError]）
- 新增 ZRANGEBYSCORE LIMIT 选项（zrangebyscore_with_limit）
- 新增 HSCAN/SSCAN/ZSCAN MATCH 模式匹配
- 新增 OBJECT refcount 与 idletime
- 新增 ZSet 缓存 LRU 淘汰（MAX_CACHE_SIZE=100）
- 新增 SINTER 最小集合优化

### Fixed

- SET 命令类型检查：覆盖非字符串类型 key 时返回 WrongType
- setxx 返回值逻辑：key 不存在时返回 false
- 事务中 SET/INCR/DECR 命令排队正确性

### Performance

- ZSet 缓存 LRU 淘汰，防止内存泄漏
- SINTER 使用最小集合作为基准，减少比较次数
- zrange 1000: 13.93µs
- zrank 1000: 149.43ns
- sinter 性能提升（大数据集时）

### Engineering

- 命令数 131 → 131（无新增命令，API 扩展）
- 测试数 159 → 159

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
