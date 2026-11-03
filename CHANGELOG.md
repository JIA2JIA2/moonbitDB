# Changelog

## 0.2.0 — 2026-11-03

### Added

- 新增 String/Key 命令：`INCRBY`、`DECRBY`、`GETRANGE`、`SETRANGE`、`GETEX`、`MSETNX`、`SCAN`、`COPY`、`TOUCH`、`UNLINK`
- 新增 Hash/List 命令：`HSETNX`、`HSCAN`、`LMOVE`
- 新增 Set 命令：`SINTERCARD`、`SMISMEMBER`、`SSCAN`
- 新增 Sorted Set 命令：`ZRANGEBYLEX`、`ZREVRANGEBYLEX`、`ZLEXCOUNT`、`ZREMRANGEBYLEX`、`ZRANDMEMBER`、`ZUNION`、`ZINTER`、`ZDIFF`、`ZSCAN`
- 新增 `SET` 选项 API：`set_with_options`、`setxx`，支持 NX/XX/EX/PX/KEEPTTL
- 新增错误模型：`RedisError`、`Result` 版 `try_*` API（含读写命令）
- 新增批量 API：`lpush_all`、`rpush_all`、`sadd_all`、`srem_all`、`scan_match`、`copy_with_replace`
- 新增基准测试与 `CHANGELOG.md`

### Fixed

- 时间戳与计数改为 `Int64`，修复 32 位溢出
- 跨类型写操作不再静默覆盖已有数据
- `MSET`/`HMSET` 长度不一致时整体不写入
- `APPEND`/`SETRANGE` 增加 512MB 上限
- `INCRBY`/`DECRBY`/`HINCRBY`/`parse_int64` 增加溢出检测
- `EXPIRE`/`PEXPIRE`/`EXPIREAT` 时间计算溢出返回 `false`
- `COPY` 深拷贝容器，避免源/目标共享内部结构
- `RPOPLPUSH` 同 key 单元素时保留 TTL
- ZSet 同分成员按真正字典序（lexicographic）排序
- 修复 ZSet rank/lex 缓存在 rename/删除路径的失效遗漏

### Performance

- `SCAN` 使用稳定快照，避免每次调用全量排序
- 过期清理改为最小堆，并支持删除/persist 后压缩
- `ZRANGEBYSCORE`/`ZCOUNT` 改为二分定位
- ZSet 写入、删除、弹出使用缓存写穿，减少重复排序
- `KEYS_PATTERN` 对有字面前缀的模式使用二分剪枝
- 移除 `LREM` 的索引字符串 Map

### Engineering

- 核心模块按数据类型拆分，测试独立
- 864 处断言迁移为 `@test.assert_eq`，其余多行断言已全部迁移
- 新增属性测试（Deque、ZSet、Hash、Set、List、SCAN）
- CI 启用 `--deny-warn`、基准测试与 lib 覆盖率门槛（88%）

## 0.1.3 — 2026-10-31

- 完成基础五种数据类型与命令集
- 修复负索引、负区间、遍历删除、过期判断等缺陷