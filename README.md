# moonbitDB

基于 MoonBit 语言特性实现的内存型非关系型数据库，参考 Redis 设计。

## 特性

- **五种数据结构**：String、Hash、List、Set、Sorted Set
- **Key 过期机制**：EXPIRE/PEXPIRE/TTL/PTTL/PERSIST
- **批量操作**：MSET/MGET/MDEL/HMSET/HMGET
- **集合运算**：SINTER/SUNION/SDIFF 及其 STORE 变体
- **排序功能**：ZRANGE/ZREVRANGE/ZRANGEBYSCORE 等
- **服务器命令**：DBSIZE/FLUSHDB/PING/INFO/TYPE 等

## 快速开始

### 运行演示

```bash
moon run . --target native
```

### 运行测试

```bash
moon test
```

## 项目结构

```
moonbitDB/
├── moonbitDB.mbt       # 核心数据库实现
├── demo.mbt            # 快速开始演示（可运行）
├── examples/           # 场景部署示例
│   ├── basic_usage.mbt           # 基础API使用示例
│   ├── leaderboard_demo.mbt      # 游戏排行榜场景
│   ├── shopping_cart_demo.mbt    # 电商购物车/缓存场景
│   └── cli_repl_demo.mbt         # 命令行交互演示
├── moon.mod.json       # 模块配置
└── moon.pkg.json       # 包配置
```

## 部署示例

### 1. 快速开始演示

直接运行主包即可看到完整功能演示：

```bash
moon run . --target native
```

### 2. 游戏排行榜系统

参考 [examples/leaderboard_demo.mbt](examples/leaderboard_demo.mbt)

展示了如何使用：
- **ZSET** 实现玩家分数排名
- **ZINCRBY** 实时更新分数
- **ZREVRANGE/ZRANK** 查询排名
- **ZPOPMAX** 领奖台功能
- **SET** 实现好友关系
- **LIST** 实现消息队列
- **HASH** 存储玩家信息

### 3. 电商购物车 & 缓存

参考 [examples/shopping_cart_demo.mbt](examples/shopping_cart_demo.mbt)

展示了如何使用：
- **HASH** 存储购物车商品和商品信息
- **String + EXPIRE** 实现会话管理和缓存
- **LIST** 实现浏览历史
- **SET** 实现收藏和商品标签
- **SINTER/SUNION** 实现标签筛选
- **ZSET** 实现销量排行榜
- **MSET/MGET** 批量预热缓存

### 4. CLI 命令行交互

参考 [examples/cli_repl_demo.mbt](examples/cli_repl_demo.mbt)

模拟 Redis 风格的命令行交互，支持 60+ 命令解析。

## 支持的命令

### String 命令
`SET` `GET` `DEL` `EXISTS` `APPEND` `STRLEN` `INCR` `DECR`

### Hash 命令
`HSET` `HGET` `HDEL` `HGETALL` `HLEN` `HMSET` `HMGET`

### List 命令
`LPUSH` `RPUSH` `LPOP` `RPOP` `LLEN` `LRANGE` `LINDEX` `LSET`
`LREM` `LINSERT` `LPUSHX` `RPUSHX` `RPOPLPUSH`

### Set 命令
`SADD` `SMEMBERS` `SREM` `SCARD` `SISMEMBER`
`SINTER` `SINTERSTORE` `SUNION` `SUNIONSTORE` `SDIFF` `SDIFFSTORE`
`SMOVE` `SPOP`

### Sorted Set 命令
`ZADD` `ZRANGE` `ZCARD` `ZSCORE` `ZREM`
`ZRANGEBYSCORE` `ZCOUNT` `ZREVRANGE` `ZREVRANGEBYSCORE`
`ZRANK` `ZREVRANK` `ZINCRBY` `ZREMRANGEBYRANK` `ZREMRANGEBYSCORE`
`ZPOPMIN` `ZPOPMAX`

### Key 命令
`KEYS` `TYPE` `EXPIRE` `PEXPIRE` `TTL` `PTTL` `PERSIST`
`RENAME` `RENAMENX` `RANDOMKEY`

### 批量命令
`MSET` `MGET` `MDEL` `HMSET` `HMGET`

### 服务器命令
`DBSIZE` `FLUSHDB` `FLUSHALL` `PING` `ECHO` `INFO` `TIME` `COMMAND`

## 核心数据结构

### Deque（双端队列）
使用双栈实现，LPUSH/LPOP/RPUSH/RPOP 均为 O(1) 摊还复杂度。

### RedisValue 枚举
```
String(String)
Hash(Map[String, String])
List(Deque)
Set(Map[String, Bool])
ZSet(Map[String, Float])
```

## 设计亮点

- **模拟时间系统**：通过 `advance_time` 控制时间，便于测试过期机制
- **惰性过期**：Key 在访问时检查是否过期，无需后台线程
- **合并排序**：ZSet 使用 O(n log n) 归并排序
- **双端队列**：List 使用双栈 Deque，头尾操作 O(1) 摊还复杂度
