# moonbitDB

基于 [MoonBit](https://www.moonbitlang.com/) 语言特性实现的内存型非关系型数据库，参考 Redis 设计。

## 快速开始

### 运行演示

```bash
moon run . --target native
```

### 运行测试

```bash
moon test
```

当前测试覆盖：**64 个测试用例，全部通过**。

## 项目结构

```
moonbitDB/
├── moonbitDB.mbt              # 核心数据库实现（~3200 行）
├── demo.mbt                   # 快速开始演示（可运行）
├── examples/                  # 场景部署示例（独立子包，均可运行）
│   ├── basic_usage/           # 基础 API 使用示例
│   │   ├── main.mbt
│   │   └── moon.pkg.json
│   ├── leaderboard/           # 游戏排行榜场景
│   │   ├── main.mbt
│   │   └── moon.pkg.json
│   ├── shopping_cart/         # 电商购物车 / 缓存场景
│   │   ├── main.mbt
│   │   └── moon.pkg.json
│   └── cli_repl/              # 命令行交互演示
│       ├── main.mbt
│       └── moon.pkg.json
├── moon.mod.json               # 模块配置
├── moon.pkg.json               # 包配置（is_main: true）
└── README.md                   # 本文档
```

## 支持的命令

共支持 **80+ 个命令**，覆盖 Redis 核心功能。

### String 命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `SET` | `(key, value) -> Unit` | 设置字符串值 |
| `GET` | `(key) -> String?` | 获取字符串值 |
| `DEL` | `(key) -> Bool` | 删除 key |
| `EXISTS` | `(key) -> Bool` | 检查 key 是否存在 |
| `APPEND` | `(key, value) -> Int` | 追加字符串，返回新长度 |
| `STRLEN` | `(key) -> Int` | 字符串长度 |
| `INCR` | `(key) -> Int?` | 自增 1（非数字返回 None） |
| `DECR` | `(key) -> Int?` | 自减 1（非数字返回 None） |

### Key 命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `KEYS` | `() -> Array[String]` | 列出所有 key |
| `KEYS_PATTERN` | `(pattern) -> Array[String]` | 模式匹配 key（支持 `*` `?`） |
| `TYPE` | `(key) -> String` | 返回值类型（string/hash/list/set/zset/none） |
| `EXPIRE` | `(key, seconds) -> Bool` | 设置过期时间（秒） |
| `PEXPIRE` | `(key, ms) -> Bool` | 设置过期时间（毫秒） |
| `TTL` | `(key) -> Int` | 剩余生存时间（秒），-1 永久，-2 不存在 |
| `PTTL` | `(key) -> Int` | 剩余生存时间（毫秒） |
| `PERSIST` | `(key) -> Bool` | 移除过期时间 |
| `RENAME` | `(old_key, new_key) -> Bool` | 重命名 key（含 TTL 转移） |
| `RENAMENX` | `(old_key, new_key) -> Bool` | 仅在新 key 不存在时重命名 |
| `RANDOMKEY` | `() -> String?` | 随机返回一个 key |

### Hash 命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `HSET` | `(key, field, value) -> Unit` | 设置 hash 字段 |
| `HGET` | `(key, field) -> String?` | 获取 hash 字段 |
| `HDEL` | `(key, field) -> Bool` | 删除 hash 字段 |
| `HGETALL` | `(key) -> Map[String, String]` | 获取所有字段和值 |
| `HLEN` | `(key) -> Int` | 字段数量 |

### List 命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `LPUSH` | `(key, value) -> Int` | 左端插入，返回新长度 |
| `RPUSH` | `(key, value) -> Int` | 右端插入，返回新长度 |
| `LPOP` | `(key) -> String?` | 左端弹出 |
| `RPOP` | `(key) -> String?` | 右端弹出 |
| `LLEN` | `(key) -> Int` | 列表长度 |
| `LRANGE` | `(key, start, end) -> Array[String]` | 范围获取（支持负数索引） |
| `LINDEX` | `(key, index) -> String?` | 索引获取（支持负数） |
| `LSET` | `(key, index, value) -> Bool` | 索引设置 |
| `LREM` | `(key, count, value) -> Int` | 按值删除（count>0 从头，<0 从尾，=0 全部） |
| `LINSERT` | `(key, before, pivot, value) -> Int` | 在 pivot 前/后插入 |
| `LPUSHX` | `(key, value) -> Int` | 仅当列表存在时左端插入 |
| `RPUSHX` | `(key, value) -> Int` | 仅当列表存在时右端插入 |
| `RPOPLPUSH` | `(source, dest) -> String?` | source 右端弹出插入 dest 左端 |

### Set 命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `SADD` | `(key, value) -> Bool` | 添加成员（已存在返回 false） |
| `SMEMBERS` | `(key) -> Array[String]` | 所有成员 |
| `SREM` | `(key, value) -> Bool` | 删除成员 |
| `SCARD` | `(key) -> Int` | 成员数量 |
| `SISMEMBER` | `(key, value) -> Bool` | 检查成员是否存在 |
| `SINTER` | `(keys) -> Array[String]` | 多集合交集 |
| `SINTERSTORE` | `(dest, keys) -> Int` | 交集存储到 dest |
| `SUNION` | `(keys) -> Array[String]` | 多集合并集 |
| `SUNIONSTORE` | `(dest, keys) -> Int` | 并集存储到 dest |
| `SDIFF` | `(keys) -> Array[String]` | 多集合差集 |
| `SDIFFSTORE` | `(dest, keys) -> Int` | 差集存储到 dest |
| `SMOVE` | `(source, dest, value) -> Bool` | 跨集合移动元素 |
| `SPOP` | `(key) -> String?` | 随机弹出元素 |

### Sorted Set 命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `ZADD` | `(key, score, member) -> Bool` | 添加成员（已存在更新分数） |
| `ZRANGE` | `(key, start, end) -> Array[String]` | 按排名升序获取范围 |
| `ZCARD` | `(key) -> Int` | 成员数量 |
| `ZSCORE` | `(key, member) -> Float?` | 获取成员分数 |
| `ZREM` | `(key, member) -> Bool` | 删除成员 |
| `ZRANGEBYSCORE` | `(key, min, max) -> Array[String]` | 按分数范围获取 |
| `ZCOUNT` | `(key, min, max) -> Int` | 统计分数范围内成员数 |
| `ZREVRANGE` | `(key, start, end) -> Array[String]` | 按排名降序获取范围 |
| `ZREVRANGEBYSCORE` | `(key, max, min) -> Array[String]` | 按分数降序范围获取 |
| `ZRANK` | `(key, member) -> Int?` | 升序排名（从 0 开始） |
| `ZREVRANK` | `(key, member) -> Int?` | 降序排名（从 0 开始） |
| `ZINCRBY` | `(key, increment, member) -> Float` | 增加分数（支持负数） |
| `ZREMRANGEBYRANK` | `(key, start, end) -> Int` | 按排名范围删除 |
| `ZREMRANGEBYSCORE` | `(key, min, max) -> Int` | 按分数范围删除 |
| `ZPOPMIN` | `(key) -> (String, Float)?` | 弹出最小分数成员 |
| `ZPOPMAX` | `(key) -> (String, Float)?` | 弹出最大分数成员 |

### 批量命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `MSET` | `(keys, values) -> Unit` | 批量设置多个 key-value |
| `MGET` | `(keys) -> Array[String?]` | 批量获取多个 key 的值 |
| `MDEL` | `(keys) -> Int` | 批量删除，返回删除数量 |
| `HMSET` | `(key, fields, values) -> Unit` | 批量设置 hash 字段 |
| `HMGET` | `(key, fields) -> Array[String?]` | 批量获取 hash 字段 |

### 服务器命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `DBSIZE` | `() -> Int` | 当前 key 数量（不含已过期） |
| `FLUSHDB` | `() -> Unit` | 清空当前数据库 |
| `FLUSHALL` | `() -> Unit` | 清空所有数据 |
| `PING` | `() -> String` | 返回 "PONG" |
| `ECHO` | `(message) -> String` | 回显消息 |
| `INFO` | `() -> String` | 服务器信息（版本、key 数、过期数等） |
| `TIME` | `() -> (Int, Int)` | 当前时间（秒, 微秒） |
| `COMMAND` | `() -> Array[String]` | 支持的命令列表 |
| `OBJECT_ENCODING` | `(key) -> String?` | 对象内部编码（embstr/listpack/hashtable/skiplist/quicklist） |

### 辅助方法

| 方法 | 签名 | 说明 |
|------|------|------|
| `new` | `() -> Database` | 创建空数据库 |
| `set_time` | `(time_ms) -> Unit` | 设置当前时间（毫秒） |
| `advance_time` | `(ms) -> Unit` | 推进时间（毫秒） |
| `check_expired` | `(key) -> Bool` | 检查 key 是否已过期（惰性清理） |

## 部署示例

所有示例均为独立子包，可单独运行。运行前需先安装 [MoonBit](https://www.moonbitlang.com/) CLI 工具。

### 1. 快速开始演示

直接运行主包即可看到 8 大功能模块的完整演示：

```bash
moon run . --target native
```

输出预览：

```
========================================
  MoonBitDB - 快速开始演示
========================================

📌 1. String 基础操作
  GET greeting = "Hello, MoonBitDB!"
  INCR counter = 11

📌 2. Hash 用户信息存储
  HLEN user:1001 = 3

📌 3. List 任务队列
  LRANGE tasks 0 -1 = [完成报告, 代码审查, 团队会议]

📌 4. Set 标签系统
  SCARD = 3 (自动去重)

📌 5. Sorted Set 排行榜
  TOP 3 (ZREVRANGE 0 2):
    #1 玩家D - 3200分
    #2 玩家B - 2500分

📌 6. Key 过期机制
  65秒后 EXISTS = false

📌 7. 批量操作
📌 8. 服务器信息
========================================
  ✅ 演示完成！
========================================
```

### 2. 基础 API 使用示例

覆盖所有 5 种数据结构的基础操作演示。

```bash
moon run examples/basic_usage --target native
```

参考 [examples/basic_usage/main.mbt](examples/basic_usage/main.mbt)

### 3. 游戏排行榜系统

10 阶段完整场景：初始化分数 → TOP 5 查询 → 对局更新 → 排名变化 → 分数统计 → 每日榜单 → 领奖弹出 → 好友关系 → 消息队列 → 状态统计。

```bash
moon run examples/leaderboard --target native
```

参考 [examples/leaderboard/main.mbt](examples/leaderboard/main.mbt)

展示了如何使用：
- **ZSET** 实现玩家分数排名
- **ZINCRBY** 实时更新分数
- **ZREVRANGE / ZRANK** 查询排名
- **ZPOPMAX** 领奖台功能
- **SET** 实现好友关系
- **SINTER** 共同好友
- **LIST** 实现消息队列
- **HASH** 存储玩家信息

### 4. 电商购物车 & 缓存

10 阶段完整场景：商品缓存 → 购物车 → 浏览历史 → 收藏 → 共同收藏 → 商品标签 → 销量排行 → 会话管理 → 缓存预热 → 状态总览。

```bash
moon run examples/shopping_cart --target native
```

参考 [examples/shopping_cart/main.mbt](examples/shopping_cart/main.mbt)

展示了如何使用：
- **HASH** 存储购物车商品和商品信息
- **String + EXPIRE** 实现会话管理和缓存
- **LIST** 实现浏览历史
- **SET** 实现收藏和商品标签
- **SINTER / SUNION** 实现标签筛选
- **ZSET** 实现销量排行榜
- **MSET / MGET** 批量预热缓存

### 5. CLI 命令行交互

模拟 Redis 风格的命令行交互，自动执行 45 条演示命令并输出结果。

```bash
moon run examples/cli_repl --target native
```

参考 [examples/cli_repl/main.mbt](examples/cli_repl/main.mbt)

支持解析和执行 60+ 命令，包括 SET/GET/HSET/LPUSH/SADD/ZADD/MSET 等全部命令类型。
