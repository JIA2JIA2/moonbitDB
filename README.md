# moonbitDB

[![CI](https://github.com/JIA2JIA2/moonbitdb/actions/workflows/ci.yml/badge.svg)](https://github.com/JIA2JIA2/moonbitdb/actions/workflows/ci.yml)

基于 [MoonBit](https://www.moonbitlang.com/) 语言特性实现的内存型非关系型数据库，参考 Redis 设计。

> ⚠️ **项目能力边界**
>
> moonbitDB 是一个**单进程内存型数据库库**，以下能力**暂不在**项目范围内：
>
> - **暂不提供网络服务** — 无 TCP/HTTP 服务器，无远程连接能力，不支持客户端-服务器架构
> - **暂不提供数据持久化** — 所有数据仅存储于内存中（`Map[String, RedisValue]`），进程退出后数据即丢失
> - **暂不提供并发处理** — 无异步操作，无线程安全保证，无锁机制
>
> **适用场景**：嵌入式场景、测试模拟、本地缓存、学习参考
>
> README 中的 PING/ECHO/INFO 等命令为 API 方法，非网络服务协议命令。

## 安装

### 前置条件

运行 moonbitDB 需要 [MoonBit](https://www.moonbitlang.com/) CLI 工具。

### 安装 MoonBit CLI

**macOS / Linux：**

```bash
curl -fsSL https://cli.moonbitlang.com/install/unix.sh | bash
```

**Windows：**

```powershell
Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
irm https://cli.moonbitlang.com/install/powershell.ps1 | iex
```

**npm 安装（跨平台）：**

```bash
npm install -g @moonbit/cli
```

> 更多安装方式请参考 [MoonBit 官方安装文档](https://www.moonbitlang.com/download/)。

### 验证安装

```bash
moon version
```

成功安装后将输出 MoonBit CLI 版本信息。

## 快速开始

### 运行演示

```bash
moon run . --target native
```

### 运行测试

```bash
moon test
```

当前测试覆盖：**201 个测试用例，全部通过**。

### 运行基准与覆盖率

```bash
moon check --deny-warn
moon test --deny-warn
moon bench > /tmp/bench.txt
python3 scripts/check_docs.py
python3 scripts/check_bench.py /tmp/bench.txt
moon coverage analyze -- -f summary
```

CI 同时执行命令表一致性校验、基准基线对比，并对 `lib/` 覆盖率设置 **88%** 门槛；另对 `js` / `wasm-gc` 目标做静态检查。

参考基线（容器环境，仅供趋势对比）：

| 场景 | 参考耗时 |
|------|----------|
| `ZCOUNT` 1000 成员 | ≈ 0.14 µs |
| `ZRANK` 1000 成员 | ≈ 0.14 µs |
| `SCAN` 1000 key / 批 100 | ≈ 2 µs |
| `KEYS_PATTERN` 500 key（前缀 `user:1*`） | ≈ 12.5 µs |
| `LREM` 1000 元素 | ≈ 48 µs |

## 库导入指南

在其他 MoonBit 项目中使用 moonbitDB 作为依赖，需要以下步骤：

### 1. 添加模块依赖

```bash
moon add JIA2JIA2/moonbitdb@0.1.4
```

该命令会更新 `moon.mod` 并同步依赖，推荐优先使用。

### 2. 声明包导入

在你的包 `moon.pkg` 中引入核心库：

```moonbit
import {
  "JIA2JIA2/moonbitdb/lib",
}
```

### 3. 使用

```moonbit
fn main {
  let db = @lib.Database::new()
  db.set("greeting", "Hello, moonbitDB!")
  let value = db.get("greeting")
  println(value) // Some("Hello, moonbitDB!")
}
```

> **注意**：若手动编辑 `moon.mod` 添加依赖，需执行 `moon update` 同步，否则会导致编译错误。
> 导入路径为 `JIA2JIA2/moonbitdb/lib`（注意包含 `/lib` 后缀），不是 `JIA2JIA2/moonbitdb`。

## 项目结构

```
moonbitDB/
├── lib/                        # 核心库（非 main 包，可被其他包 import）
│   ├── database.mbt            # Database 结构与核心辅助方法
│   ├── string.mbt              # String 与批量命令
│   ├── key.mbt                 # Key 与过期命令
│   ├── hash.mbt                # Hash 命令
│   ├── list.mbt                # List 命令
│   ├── set.mbt                 # Set 命令
│   ├── zset.mbt                # Sorted Set 命令
│   ├── deque.mbt               # List 底层双端队列
│   ├── util.mbt                # 整数解析与通配符匹配
│   ├── error.mbt               # RedisError 错误模型
│   ├── server.mbt              # 服务器命令
│   ├── database_wbtest.mbt     # 白盒测试（171 个用例）
│   ├── coverage_gaps_wbtest.mbt # 覆盖率补充测试
│   ├── coverage_gaps2_wbtest.mbt # 覆盖率补充测试二
│   ├── database_property_wbtest.mbt # 属性测试
│   ├── database_bench_test.mbt # 基准测试
│   └── moon.pkg
├── demo.mbt                    # 快速开始演示（主包入口）
├── moon.pkg                    # 主包配置（is_main: true，依赖 lib）
├── examples/                   # 场景部署示例（独立子包，均可运行）
│   ├── basic_usage/            # 基础 API 使用示例
│   │   ├── main.mbt
│   │   └── moon.pkg
│   ├── leaderboard/            # 游戏排行榜场景
│   │   ├── main.mbt
│   │   └── moon.pkg
│   ├── shopping_cart/          # 电商购物车 / 缓存场景
│   │   ├── main.mbt
│   │   └── moon.pkg
│   └── cli_repl/               # 命令行交互演示
│       ├── main.mbt
│       └── moon.pkg
├── moon.mod                    # 模块配置（JIA2JIA2/moonbitdb）
└── README.md                   # 本文档
```

> **包架构说明**：核心逻辑放在 `lib/` 子包（非 main），主包和 examples 均依赖 lib。这种设计符合 MoonBit 规范，避免了 "examples depend on main package" 的警告。

## 支持的命令

共支持 **163 个命令**，覆盖 Redis 核心功能。

> **类型安全约定**：对已存在但类型不匹配的 key，写操作**不会覆盖**原数据。失败返回值约定如下：`HSET`/`LPUSH`/`RPUSH`/`APPEND`/`SETRANGE`/`LPUSHX`/`RPUSHX` 返回 `-1`，`SADD`/`ZADD`/`HSETNX`/`HMSET` 返回 `false`，`INCR`/`DECR`/`INCRBY`/`DECRBY`/`HINCRBY` 返回 `None`，`SINTERSTORE`/`SUNIONSTORE`/`SDIFFSTORE` 返回 `-1`。时间相关 API 使用 `Int64` 毫秒，可安全表示真实 Unix 时间戳与超过 24 天的 TTL。

### String 命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `SET` | `(key, value) -> Unit` | 设置字符串值 |
| `GET` | `(key) -> String?` | 获取字符串值 |
| `DEL` | `(key) -> Bool` | 删除 key |
| `EXISTS` | `(key) -> Bool` | 检查 key 是否存在 |
| `APPEND` | `(key, value) -> Int` | 追加字符串，返回新长度（单值上限 512MB） |
| `STRLEN` | `(key) -> Int` | 字符串长度 |
| `GETRANGE` | `(key, start, end) -> String` | 获取子串（支持负数索引） |
| `SUBSTR` | `(key, start, end) -> String` | GETRANGE 别名 |
| `SETRANGE` | `(key, offset, value) -> Int` | 从 offset 覆盖写入，返回新长度（缺失部分以 NUL 填充，单值上限 512MB） |
| `GETBIT` | `(key, offset) -> Int` | 获取位图位值（UTF-16 code unit 索引，超出范围返回 0） |
| `SETBIT` | `(key, offset, value) -> Int` | 设置位图位值，返回原位值（自动扩容，NUL 填充） |
| `BITPOS` | `(key, bit) -> Int` | 查找第一个指定值的位（返回 -1 表示未找到） |
| `BITCOUNT` | `(key, start?, end?) -> Int` | 统计置位位数（可选 code unit 范围，负索引从尾） |
| `BITOP` | `(op, dest, src_keys) -> Int` | AND/OR/XOR/NOT 按位运算写入 dest，返回 dest 长度 |
| `BITFIELD` | `(key, GET/SET/INCRBY u/i width offset [value]) -> Array[Int?]` | 位段读写（宽 1-32 位，u 无符号/i 符号扩展，INCRBY 溢出 WRAP；差异：本库位序为 code unit 内 LSB，与 Redis 字节序 MSB 不同） |
| `INCR` | `(key) -> Int64?` | 自增 1（非数字返回 None） |
| `DECR` | `(key) -> Int64?` | 自减 1（非数字返回 None） |
| `INCRBY` | `(key, increment) -> Int64?` | 增加指定整数（非数字返回 None） |
| `DECRBY` | `(key, decrement) -> Int64?` | 减少指定整数（非数字返回 None） |
| `SETNX` | `(key, value) -> Bool` | 仅当 key 不存在时设置 |
| `SETEX` | `(key, seconds, value) -> Bool` | 设置值并指定过期时间（秒） |
| `GETDEL` | `(key) -> String?` | 获取值并删除 key |
| `GETSET` | `(key, value) -> String?` | 设置新值并返回旧值（清除 TTL） |
| `GETEX` | `(key, expire_ms) -> String?` | 获取值并设置/移除 TTL（`None` 表示 PERSIST） |
| `PSETEX` | `(key, ms, value) -> Bool` | 设置值并指定过期时间（毫秒） |

> `STRLEN`/`GETRANGE`/`SETRANGE` 的索引按 **UTF-16 code unit** 计数，与 Redis 的字节索引不同；中文、emoji 等场景需要字节语义时请先自行编码。

### Key 命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `KEYS` | `() -> Array[String]` | 列出所有 key |
| `KEYS_PATTERN` | `(pattern) -> Array[String]` | 模式匹配 key（支持 `*` `?`） |
| `SCAN` | `(cursor, count) -> (Int, Array[String])` | 游标遍历 key，返回下一游标（0 表示结束） |
| `TYPE` | `(key) -> String` | 返回值类型（string/hash/list/set/zset/none） |
| `EXPIRE` | `(key, seconds) -> Bool` | 设置过期时间（秒） |
| `PEXPIRE` | `(key, ms) -> Bool` | 设置过期时间（毫秒） |
| `EXPIREAT` | `(key, unix_seconds) -> Bool` | 设置过期时间戳（秒，过去时间戳立即删除 key） |
| `PEXPIREAT` | `(key, unix_ms) -> Bool` | 设置过期时间戳（毫秒，过去时间戳立即删除 key） |
| `EXPIRETIME` | `(key) -> Int64` | 绝对过期时间戳（秒），-1 永久，-2 不存在 |
| `PEXPIRETIME` | `(key) -> Int64` | 绝对过期时间戳（毫秒），-1 永久，-2 不存在 |
| `TTL` | `(key) -> Int64` | 剩余生存时间（秒，向上取整），-1 永久，-2 不存在 |
| `PTTL` | `(key) -> Int64` | 剩余生存时间（毫秒） |
| `PERSIST` | `(key) -> Bool` | 移除过期时间 |
| `RENAME` | `(old_key, new_key) -> Bool` | 重命名 key（含 TTL 转移） |
| `RENAMENX` | `(old_key, new_key) -> Bool` | 仅在新 key 不存在时重命名 |
| `RANDOMKEY` | `() -> String?` | 随机返回一个 key |
| `COPY` | `(source, destination) -> Bool` | 复制 key（深拷贝，不复制 TTL，目标已存在则失败） |
| `TOUCH` | `(keys) -> Int` | 返回存在的 key 数量 |
| `UNLINK` | `(key) -> Bool` | 删除 key（与 DEL 语义一致） |

> `EXPIRE`/`PEXPIRE`/`EXPIREAT` 在时间计算溢出时返回 `false`，不会写入非法 TTL；`advance_time` 溢出时不推进时间。

> `KEYS_PATTERN` 对含字面前缀的模式（如 `user:1*`）复用有序 key 快照并二分定位候选区间；纯通配符模式仍走全量扫描。

### Hash 命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `HSET` | `(key, field, value) -> Int` | 设置 hash 字段，返回新建字段数（0/1） |
| `HSETNX` | `(key, field, value) -> Bool` | 仅当字段不存在时设置 |
| `HGET` | `(key, field) -> String?` | 获取 hash 字段 |
| `HDEL` | `(key, field) -> Bool` | 删除 hash 字段 |
| `HGETALL` | `(key) -> Map[String, String]` | 获取所有字段和值 |
| `HLEN` | `(key) -> Int` | 字段数量 |
| `HEXISTS` | `(key, field) -> Bool` | 检查 hash 字段是否存在 |
| `HKEYS` | `(key) -> Array[String]` | 获取 hash 所有字段名 |
| `HVALS` | `(key) -> Array[String]` | 获取 hash 所有字段值 |
| `HINCRBY` | `(key, field, increment) -> Int64?` | hash 字段整数值自增（非数字返回 None） |
| `HSTRLEN` | `(key, field) -> Int` | hash 字段值长度 |
| `HSCAN` | `(key, cursor, count) -> (Int, Array[(String, String)])` | 增量遍历 hash 字段 |
| `HRANDFIELD` | `(key, count) -> Array[String]` | 随机获取字段（count<0 允许重复） |

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
| `LPUSHX` | `(key, value) -> Int` | 仅当列表存在时左端插入（key 不存在返回 0，类型错误返回 -1） |
| `RPUSHX` | `(key, value) -> Int` | 仅当列表存在时右端插入 |
| `RPOPLPUSH` | `(source, dest) -> String?` | source 右端弹出插入 dest 左端 |
| `LMOVE` | `(source, dest, from_left, to_left) -> String?` | 从 source 指定端弹出并插入 dest 指定端 |
| `LPOS` | `(key, value) -> Int?` | 返回元素首次出现的索引（`try_lpos_options` 支持 RANK/COUNT/MAXLEN；差异：无 COUNT 时本库恒返回数组，官方返回单整数） |
| `LTRIM` | `(key, start, end) -> Bool` | 裁剪列表至指定区间（区间外删除） |
| `LMPOP` | `(keys, left, count) -> (String, Array[String])?` | 从多个列表中弹出第一个非空列表的元素 |

### Set 命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `SADD` | `(key, value) -> Bool` | 添加成员（已存在返回 false） |
| `SMEMBERS` | `(key) -> Array[String]` | 所有成员 |
| `SREM` | `(key, value) -> Bool` | 删除成员 |
| `SCARD` | `(key) -> Int` | 成员数量 |
| `SISMEMBER` | `(key, value) -> Bool` | 检查成员是否存在 |
| `SMISMEMBER` | `(key, members) -> Array[Bool]` | 批量检查成员是否存在 |
| `SINTERCARD` | `(keys, limit) -> Int` | 交集基数（limit<=0 不限制） |
| `SINTER` | `(keys) -> Array[String]` | 多集合交集 |
| `SINTERSTORE` | `(dest, keys) -> Int` | 交集存储到 dest |
| `SUNION` | `(keys) -> Array[String]` | 多集合并集 |
| `SUNIONSTORE` | `(dest, keys) -> Int` | 并集存储到 dest |
| `SDIFF` | `(keys) -> Array[String]` | 多集合差集 |
| `SDIFFSTORE` | `(dest, keys) -> Int` | 差集存储到 dest |
| `SMOVE` | `(source, dest, value) -> Bool` | 跨集合移动元素 |
| `SPOP` | `(key) -> String?` | 随机弹出元素 |
| `SRANDMEMBER` | `(key) -> String?` | 随机返回一个成员（不删除） |
| `SSCAN` | `(key, cursor, count) -> (Int, Array[String])` | 增量遍历集合成员 |

### Sorted Set 命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `ZADD` | `(key, score, member) -> Bool` | 添加成员（已存在更新分数） |
| `ZRANGE` | `(key, start, end) -> Array[String]` | 按排名升序获取范围；`try_zrange_modifiers` 支持 6.2 统一修饰符（BYSCORE/BYLEX/REV/LIMIT/WITHSCORES） |
| `ZCARD` | `(key) -> Int` | 成员数量 |
| `ZSCORE` | `(key, member) -> Float?` | 获取成员分数 |
| `ZREM` | `(key, member) -> Bool` | 删除成员 |
| `ZRANGEBYSCORE` | `(key, min, max) -> Array[String]` | 按分数范围获取 |
| `ZRANGEBYLEX` | `(key, min, max) -> Array[String]` | 按成员字典序范围获取（`-`/`+`/`[a`/`(a`） |
| `ZREVRANGEBYLEX` | `(key, max, min) -> Array[String]` | 按成员字典序降序获取 |
| `ZLEXCOUNT` | `(key, min, max) -> Int` | 统计字典序范围内成员数 |
| `ZREMRANGEBYLEX` | `(key, min, max) -> Int` | 按字典序范围删除 |
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
| `ZMPOP` | `(keys, min, count) -> (String, Array[(String, Float)])?` | 从多个有序集合中弹出第一个非空集合的元素 |
| `ZMSCORE` | `(key, members) -> Array[Float?]` | 批量获取成员分数 |
| `ZRANDMEMBER` | `(key) -> String?` | 随机返回一个成员（不删除） |
| `ZUNION` | `(keys) -> Array[(String, Float)]` | 多集合并集（分数求和） |
| `ZINTER` | `(keys) -> Array[(String, Float)]` | 多集合交集（分数求和） |
| `ZDIFF` | `(keys) -> Array[(String, Float)]` | 多集合差集（保留第一个集合分数） |
| `ZSCAN` | `(key, cursor, count) -> (Int, Array[(String, Float)])` | 增量遍历成员与分数 |
| `ZUNIONSTORE` | `(dest, keys) -> Int` | 并集存储到 dest |
| `ZINTERSTORE` | `(dest, keys) -> Int` | 交集存储到 dest |
| `ZINTERCARD` | `(keys, limit) -> Int` | 交集基数 |
| `ZDIFFSTORE` | `(dest, keys) -> Int` | 差集存储到 dest |
| `ZRANGESTORE` | `(dest, source, start, end) -> Int` | 按排名范围复制到 dest |

> 分数相同时按 member 字典序排列（与 Redis 语义一致）。`ZRANGEBYSCORE`/`ZCOUNT` 使用二分查找定位分数区间；`ZRANGEBYLEX`/`ZREVRANGEBYLEX`/`ZLEXCOUNT`/`ZREMRANGEBYLEX` 使用 lex 有序缓存 + 二分定位成员区间。
> `ZRANGE ... WITHSCORES` 对应 `zrange_with_scores`/`zrevrange_with_scores` 方法，返回 `(member, score)` 数组。`ZRANGEBYLEX` 要求集合内分数一致，否则结果不具备 Redis 语义。

### 批量命令

| 命令 | 签名 | 说明 |
|------|------|------|
| `MSET` | `(keys, values) -> Unit` | 批量设置多个 key-value |
| `MSETNX` | `(keys, values) -> Bool` | 仅当所有 key 都不存在时批量设置 |
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
| `INFO` | `(section?) -> String` | 服务器信息（无参返回 Server/Memory/Keyspace/Stats 四节；`info_section` API 按 server/memory/keyspace/stats 过滤） |
| `TIME` | `() -> (Int64, Int64)` | 当前时间（秒, 微秒） |
| `COMMAND` | `() -> Array[String]` | 支持的命令列表 |
| `OBJECT_ENCODING` | `(key) -> String?` | 对象内部编码（embstr/listpack/hashtable/skiplist/quicklist） |
| `COMMAND_INFO` | `(commands) -> Array[String]` | 查询命令详情（未收录返回空串） |
| `OBJECT_REFCOUNT` | `(key) -> Int?` | 对象引用计数（当前恒为 1） |
| `OBJECT_IDLETIME` | `(key) -> Int?` | 对象空转秒数（有 TTL 时按剩余时间） |
| `MULTI` | `() -> String` | 开启事务（返回 "OK"） |
| `EXEC` | `() -> String` | 执行事务队列（返回 "OK"） |
| `DISCARD` | `() -> String` | 清空事务队列（返回 "OK"） |
| `WATCH` | `(key) -> Unit` | 监视 key（EXEC 前被修改则事务中止；差异：官方 EXEC 冲突返回 nil 回复，本库返回 TransactionAborted 错误） |
| `UNWATCH` | `() -> Unit` | 取消所有监视 |
| `AUTH` | `(password) -> String` | 认证（始终返回 "OK"） |
| `CLIENT` | `(subcommand) -> String` | 客户端信息（INFO/LIST 连接描述、ID 返回 "1"、GETNAME/Setname 存取客户端名、KILL 占位） |
| `CONFIG` | `(subcommand, parameter[, value]) -> Array[String]` | 配置查询/设置（GET 返回 [参数, 值] 键值对，SET 存储 override 并返回 "OK"） |
| `SLOWLOG` | `(subcommand) -> Array[String]` | 慢查询日志（LEN 返回 ["0"]、RESET 返回 ["OK"]、GET 空数组） |
| `SELECT` | `(index) -> String` | 选择数据库（始终返回 "OK"） |
| `MOVE` | `(key, db_index) -> Bool` | 移动 key 到其他数据库（仅支持 db 0） |
| `DUMP` | `(key) -> String?` | 返回字符串值原样（简化语义，非 RDB 序列化；仅 String 类型） |
| `RESTORE` | `(key, ttl_ms, value) -> Bool` | 反序列化恢复 key（ttl_ms>0 时设置过期） |
| `MONITOR` | `() -> String` | 进入监控模式（返回 "OK"） |
| `OBJECT_FREQ` | `(key) -> Int?` | 对象访问频率（当前为 0） |
| `COMMAND_COUNT` | `() -> Int` | 命令总数 |
| `SAVE` | `() -> String` | 同步保存（内存库返回 "OK"） |
| `BGSAVE` | `() -> String` | 后台保存（返回 "Background saving started"） |
| `LASTSAVE` | `() -> Int64` | 最近保存时间戳（秒） |
| `RESET` | `() -> String` | 重置连接状态（清 WATCH 与事务队列，返回 "RESET"） |
| `MEMORY_USAGE` | `(key) -> Int?` | 单 key 内存估算（字节，含 key 与 value） |
| `SORT` | `(key, ALPHA?, DESC?, LIMIT offset count?, STORE dest?) -> Array[String]` | 对 List/Set/ZSet 排序（数值默认，非数字报 NotInteger；STORE 覆盖 dest 为 List） |

## 完整公共 API 文档

以下列出 `lib/` 中所有公共 API 的完整签名。通过 `@lib` 访问。

### 错误模型

需要区分“key 不存在 / 类型错误 / 非整数 / 参数非法”时，使用 `try_*` 系列方法，它们返回 `Result[T, RedisError]`：

| 类型 | 取值 | 说明 |
|------|------|------|
| `RedisError` | `WrongType` | 对错误类型的 key 执行操作 |
| `RedisError` | `NotInteger` | 值不是合法整数 |
| `RedisError` | `NotFound` | key 不存在或已过期 |
| `RedisError` | `InvalidArgument` | 参数非法（如负 offset、批量参数长度不一致） |
| `RedisError` | `OutOfRange` | 数值超出允许范围（如 LPOS COUNT < -1） |
| `RedisError` | `IndexOutOfRange` | 索引越界（如 LSET 越界，"ERR index out of range"） |
| `RedisError` | `SyntaxError` | 选项组合互斥（如 SET NX+XX，"ERR syntax error"） |
| `RedisError` | `WrongArgumentCount(cmd)` | 参数个数错误（"ERR wrong number of arguments for '\{cmd}' command"） |
| `RedisError` | `Overflow` | 整数自增/自减溢出 |
| `RedisError` | `TransactionAborted` | WATCH 监视的 key 被修改，事务中止 |

> 旧方法保持兼容：`get/incrby/...` 仍返回 `Option`，`hset/lpush/...` 仍返回 `-1`/`false` 哨兵值；读命令的 `try_*`（`try_hget`/`try_lpop`/`try_zscore` 等）可区分 `NotFound` 与 `WrongType`。需要精确错误时调用对应的 `try_*` 方法。`RedisError::message()` 可获取 Redis 风格错误文本。

```moonbit
match db.try_incrby("name", 1) {
  Ok(value) => println("new value: \{value}")
  Err(error) => println(@lib.RedisError::message(error))
}
```

### Database 结构体

#### 构造函数与辅助方法

| 方法 | 签名 | 说明 |
|------|------|------|
| `new` | `() -> Database` | 创建空数据库 |
| `set_time` | `(self : Database, time_ms : Int64) -> Unit` | 设置当前时间（毫秒） |
| `advance_time` | `(self : Database, ms : Int64) -> Unit` | 推进时间（毫秒） |
| `check_expired` | `(self : Database, key : String) -> Bool` | 检查 key 是否已过期（惰性清理） |

#### String 操作

| 方法 | 签名 | 说明 |
|------|------|------|
| `set` | `(self : Database, key : String, value : String) -> Unit` | 设置字符串值 |
| `get` | `(self : Database, key : String) -> String?` | 获取字符串值 |
| `try_get` | `(self : Database, key : String) -> Result[String, RedisError]` | 获取字符串值并区分错误 |
| `try_append` | `(self : Database, key : String, value : String) -> Result[Int, RedisError]` | 精确错误版追加字符串 |
| `try_incrby` | `(self : Database, key : String, increment : Int64) -> Result[Int64, RedisError]` | 精确错误版自增 |
| `try_decrby` | `(self : Database, key : String, decrement : Int64) -> Result[Int64, RedisError]` | 精确错误版自减 |
| `try_setrange` | `(self : Database, key : String, offset : Int, value : String) -> Result[Int, RedisError]` | 精确错误版覆盖写入 |
| `del` | `(self : Database, key : String) -> Bool` | 删除 key |
| `exists` | `(self : Database, key : String) -> Bool` | 检查 key 是否存在 |
| `exists_many` | `(self : Database, keys : Array[String]) -> Int` | 多 key 存在计数（EXISTS k1 k2 语义） |
| `unlink_many` | `(self : Database, keys : Array[String]) -> Int` | 多 key 删除，返回删除个数 |
| `memory_usage` | `(self : Database, key : String) -> Int?` | 单 key 内存估算（字节） |
| `try_lpop_count` | `(self : Database, key : String, count : Int) -> Result[Array[String], RedisError]` | 精确错误版 LPOP count（负数返回 OutOfRange） |
| `try_rpop_count` | `(self : Database, key : String, count : Int) -> Result[Array[String], RedisError]` | 精确错误版 RPOP count |
| `try_zrange_modifiers` | `(self : Database, key : String, start : String, end : String, by : String?, rev : Bool, with_scores : Bool, offset : Int, count : Int) -> Result[Array[String], RedisError]` | 统一 ZRANGE 6.2 修饰符分发（by 为 None/BYSCORE/BYLEX） |
| `append` | `(self : Database, key : String, value : String) -> Int` | 追加字符串，返回新长度 |
| `strlen` | `(self : Database, key : String) -> Int` | 字符串长度 |
| `getrange` | `(self : Database, key : String, start : Int, end : Int) -> String` | 获取子串（支持负数索引） |
| `setrange` | `(self : Database, key : String, offset : Int, value : String) -> Int` | 从 offset 覆盖写入，返回新长度 |
| `incr` | `(self : Database, key : String) -> Int64?` | 自增 1 |
| `decr` | `(self : Database, key : String) -> Int64?` | 自减 1 |
| `incrby` | `(self : Database, key : String, increment : Int64) -> Int64?` | 增加指定整数 |
| `decrby` | `(self : Database, key : String, decrement : Int64) -> Int64?` | 减少指定整数 |
| `setnx` | `(self : Database, key : String, value : String) -> Bool` | 仅当 key 不存在时设置 |
| `setxx` | `(self : Database, key : String, value : String) -> Bool` | 仅当 key 存在时设置 |
| `set_with_options` | `(self : Database, key : String, value : String, nx : Bool, xx : Bool, ex_seconds : Int?, px_milliseconds : Int64?, keep_ttl : Bool) -> Result[Bool, RedisError]` | 完整 SET 选项 |
| `setex` | `(self : Database, key : String, seconds : Int, value : String) -> Bool` | 设置值并指定过期时间 |
| `getdel` | `(self : Database, key : String) -> String?` | 获取值并删除 key |
| `try_getdel` | `(self : Database, key : String) -> Result[String?, RedisError]` | 精确错误版 GETDEL（类型错返回 WrongType，不删除） |
| `try_lmpop` | `(self : Database, keys : Array[String], left : Bool, count : Int) -> Result[(String, Array[String])?, RedisError]` | 精确错误版 LMPOP（count<=0 返回 InvalidArgument） |
| `try_zmpop` | `(self : Database, keys : Array[String], min : Bool, count : Int) -> Result[(String, Array[(String, Float)])?, RedisError]` | 精确错误版 ZMPOP |
| `client_getname` | `(self : Database) -> String` | CLIENT GETNAME |
| `client_setname` | `(self : Database, name : String) -> String` | CLIENT SETNAME |
| `client_kill` | `(self : Database, target : String) -> String` | CLIENT KILL（占位返回 "OK"） |
| `config_set` | `(self : Database, parameter : String, value : String) -> String` | CONFIG SET（覆盖默认配置） |
| `try_bitfield_get` | `(self : Database, key : String, offset : Int, width : Int, signed : Bool) -> Result[Int, RedisError]` | 位段读取（宽 1-32） |
| `try_bitfield_set` | `(self : Database, key : String, offset : Int, width : Int, signed : Bool, value : Int) -> Result[Int, RedisError]` | 位段写入，返回旧值 |
| `try_bitfield_incrby` | `(self : Database, key : String, offset : Int, width : Int, signed : Bool, increment : Int) -> Result[Int, RedisError]` | 位段自增（WRAP 溢出），返回新值 |
| `getset` | `(self : Database, key : String, value : String) -> String?` | 设置新值并返回旧值 |
| `getex` | `(self : Database, key : String, expire_ms : Int64?) -> String?` | 获取值并设置/移除 TTL |
| `getex_ex` | `(self : Database, key : String, seconds : Int) -> String?` | GETEX EX（秒） |
| `getex_exat` | `(self : Database, key : String, timestamp_ms : Int64) -> String?` | GETEX EXAT（毫秒时间戳） |
| `getex_pxat` | `(self : Database, key : String, timestamp_ms : Int64) -> String?` | GETEX PXAT（毫秒时间戳） |
| `try_getbit` | `(self : Database, key : String, offset : Int) -> Result[Int, RedisError]` | 精确错误版读位 |
| `try_setbit` | `(self : Database, key : String, offset : Int, value : Int) -> Result[Int, RedisError]` | 精确错误版写位，返回原位值 |
| `try_bitpos` | `(self : Database, key : String, bit : Int, start : Int, end : Int) -> Result[Int, RedisError]` | 精确错误版找位（start/end 为 code unit 索引） |
| `try_bitcount_range` | `(self : Database, key : String, start : Int, end : Int) -> Result[Int, RedisError]` | 精确错误版范围置位计数 |
| `bitcount_range` | `(self : Database, key : String, start : Int, end : Int) -> Int` | 范围置位计数（类型错返回 -1） |
| `try_bitop` | `(self : Database, op : String, dest : String, src_keys : Array[String]) -> Result[Int, RedisError]` | AND/OR/XOR/NOT 按位运算（返回 dest 长度） |
| `bitop` | `(self : Database, op : String, dest : String, src_keys : Array[String]) -> Int` | 按位运算便捷版（错误返回 -1） |
| `expiretime` | `(self : Database, key : String) -> Int64` | 绝对过期时间戳（秒） |
| `pexpiretime` | `(self : Database, key : String) -> Int64` | 绝对过期时间戳（毫秒） |
| `watch` | `(self : Database, key : String) -> Unit` | WATCH 监视 key |
| `unwatch` | `(self : Database) -> Unit` | UNWATCH 取消监视 |
| `try_exec` | `(self : Database) -> Result[String, RedisError]` | 精确错误版 EXEC（监视冲突返回 TransactionAborted） |
| `save` | `(self : Database) -> String` | 同步保存（返回 "OK"） |
| `bgsave` | `(self : Database) -> String` | 后台保存 |
| `lastsave` | `(self : Database) -> Int64` | 最近保存时间戳（秒） |
| `command_count` | `(self : Database) -> Int` | 命令总数 |
| `reset` | `(self : Database) -> String` | 重置连接状态（返回 "RESET"） |
| `info_section` | `(self : Database, section : String) -> String` | INFO 按节查询（server/memory/keyspace/stats） |
| `try_getex` | `(self : Database, key : String, expire_ms : Int64?) -> Result[String, RedisError]` | 精确错误版 GETEX |
| `set_get` | `(self : Database, key : String, value : String) -> String?` | 设置新值并返回旧值（SET ... GET） |
| `try_set_get` | `(self : Database, key : String, value : String) -> Result[String?, RedisError]` | 精确错误版 SET ... GET |

#### Key 操作

| 方法 | 签名 | 说明 |
|------|------|------|
| `keys` | `(self : Database) -> Array[String]` | 列出所有 key |
| `keys_pattern` | `(self : Database, pattern : String) -> Array[String]` | 模式匹配 key |
| `scan` | `(self : Database, cursor : Int, count : Int) -> (Int, Array[String])` | 游标遍历 key |
| `scan_match` | `(self : Database, cursor : Int, count : Int, pattern : String) -> (Int, Array[String])` | 带模式匹配的游标遍历 |
| `scan_type` | `(self : Database, cursor : Int, count : Int, type_name : String) -> (Int, Array[String])` | 带类型过滤的游标遍历 |
| `clear_scan_caches` | `(self : Database) -> Unit` | 释放 SCAN/HSCAN/SSCAN 快照缓存 |
| `type_of` | `(self : Database, key : String) -> String` | 返回值类型 |
| `expire` | `(self : Database, key : String, seconds : Int) -> Bool` | 设置过期时间（秒） |
| `pexpire` | `(self : Database, key : String, milliseconds : Int64) -> Bool` | 设置过期时间（毫秒） |
| `expireat` | `(self : Database, key : String, unix_seconds : Int64) -> Bool` | 设置过期时间戳（秒） |
| `pexpireat` | `(self : Database, key : String, unix_milliseconds : Int64) -> Bool` | 设置过期时间戳（毫秒） |
| `expire_with_options` | `(self : Database, key : String, seconds : Int, nx : Bool, xx : Bool, gt : Bool, lt : Bool) -> Bool` | 带 NX/XX/GT/LT 选项的过期设置 |
| `pexpire_with_options` | `(self : Database, key : String, milliseconds : Int64, nx : Bool, xx : Bool, gt : Bool, lt : Bool) -> Bool` | 毫秒版过期选项 |
| `ttl` | `(self : Database, key : String) -> Int64` | 剩余生存时间（秒） |
| `pttl` | `(self : Database, key : String) -> Int64` | 剩余生存时间（毫秒） |
| `persist` | `(self : Database, key : String) -> Bool` | 移除过期时间 |
| `rename` | `(self : Database, old_key : String, new_key : String) -> Bool` | 重命名 key |
| `renamenx` | `(self : Database, old_key : String, new_key : String) -> Bool` | 仅在新 key 不存在时重命名 |
| `randomkey` | `(self : Database) -> String?` | 随机返回一个 key |
| `copy` | `(self : Database, source : String, destination : String) -> Bool` | 复制 key（深拷贝，不复制 TTL） |
| `copy_with_replace` | `(self : Database, source : String, destination : String, replace : Bool) -> Bool` | 可选覆盖目标 key 的复制 |
| `touch` | `(self : Database, keys : Array[String]) -> Int` | 返回存在的 key 数量 |
| `unlink` | `(self : Database, key : String) -> Bool` | 删除 key（同 del） |

#### Hash 操作

| 方法 | 签名 | 说明 |
|------|------|------|
| `hset` | `(self : Database, key : String, field : String, value : String) -> Int` | 设置 hash 字段，返回新建字段数 |
| `try_hget` | `(self : Database, key : String, field : String) -> Result[String, RedisError]` | 精确错误版获取 hash 字段 |
| `try_hset` | `(self : Database, key : String, field : String, value : String) -> Result[Int, RedisError]` | 精确错误版设置字段 |
| `hsetnx` | `(self : Database, key : String, field : String, value : String) -> Bool` | 仅当字段不存在时设置 |
| `hscan` | `(self : Database, key : String, cursor : Int, count : Int) -> (Int, Array[(String, String)])` | 增量遍历 hash 字段（快照缓存） |
| `try_hscan` | `(self : Database, key : String, cursor : Int, count : Int) -> Result[(Int, Array[(String, String)]), RedisError]` | 精确错误版 HSCAN |
| `hrandfield` | `(self : Database, key : String, count : Int) -> Array[String]` | 随机获取 hash 字段 |
| `hget` | `(self : Database, key : String, field : String) -> String?` | 获取 hash 字段 |
| `hdel` | `(self : Database, key : String, field : String) -> Bool` | 删除 hash 字段 |
| `hgetall` | `(self : Database, key : String) -> Map[String, String]` | 获取所有字段和值 |
| `hgetall_entries` | `(self : Database, key : String) -> Array[(String, String)]` | 获取所有字段和值（免 Map 复制，遍历更快） |
| `hlen` | `(self : Database, key : String) -> Int` | 字段数量 |
| `hexists` | `(self : Database, key : String, field : String) -> Bool` | 检查 hash 字段是否存在 |
| `hkeys` | `(self : Database, key : String) -> Array[String]` | 获取 hash 所有字段名 |
| `hvals` | `(self : Database, key : String) -> Array[String]` | 获取 hash 所有字段值 |
| `hincrby` | `(self : Database, key : String, field : String, increment : Int64) -> Int64?` | hash 字段整数自增 |
| `try_hincrby` | `(self : Database, key : String, field : String, increment : Int64) -> Result[Int64, RedisError]` | 精确错误版字段自增 |
| `hstrlen` | `(self : Database, key : String, field : String) -> Int` | hash 字段值长度 |

#### List 操作

| 方法 | 签名 | 说明 |
|------|------|------|
| `lpush` | `(self : Database, key : String, value : String) -> Int` | 左端插入 |
| `try_lpush` | `(self : Database, key : String, value : String) -> Result[Int, RedisError]` | 精确错误版左端插入 |
| `try_lpop` | `(self : Database, key : String) -> Result[String, RedisError]` | 精确错误版左端弹出 |
| `try_rpop` | `(self : Database, key : String) -> Result[String, RedisError]` | 精确错误版右端弹出 |
| `lpush_all` | `(self : Database, key : String, values : Array[String]) -> Int` | 批量左端插入（按 Redis 顺序反转） |
| `rpush_all` | `(self : Database, key : String, values : Array[String]) -> Int` | 批量右端插入 |
| `rpush` | `(self : Database, key : String, value : String) -> Int` | 右端插入 |
| `try_rpush` | `(self : Database, key : String, value : String) -> Result[Int, RedisError]` | 精确错误版右端插入 |
| `lpop` | `(self : Database, key : String) -> String?` | 左端弹出 |
| `rpop` | `(self : Database, key : String) -> String?` | 右端弹出 |
| `llen` | `(self : Database, key : String) -> Int` | 列表长度 |
| `lrange` | `(self : Database, key : String, start : Int, end : Int) -> Array[String]` | 范围获取 |
| `lindex` | `(self : Database, key : String, index : Int) -> String?` | 索引获取 |
| `lset` | `(self : Database, key : String, index : Int, value : String) -> Bool` | 索引设置 |
| `try_lset` | `(self : Database, key : String, index : Int, value : String) -> Result[Bool, RedisError]` | 精确错误版 LSET（越界返回 InvalidArgument） |
| `expireat_with_options` | `(self : Database, key : String, unix_seconds : Int64, nx : Bool, xx : Bool, gt : Bool, lt : Bool) -> Bool` | 绝对时间戳版 EXPIRE NX/XX/GT/LT |
| `pexpireat_with_options` | `(self : Database, key : String, unix_milliseconds : Int64, nx : Bool, xx : Bool, gt : Bool, lt : Bool) -> Bool` | 毫秒版 EXPIREAT NX/XX/GT/LT |
| `lrem` | `(self : Database, key : String, count : Int, value : String) -> Int` | 按值删除 |
| `linsert` | `(self : Database, key : String, before : Bool, pivot : String, value : String) -> Int` | 在 pivot 前/后插入 |
| `lpushx` | `(self : Database, key : String, value : String) -> Int` | 仅当列表存在时左端插入 |
| `rpushx` | `(self : Database, key : String, value : String) -> Int` | 仅当列表存在时右端插入 |
| `rpoplpush` | `(self : Database, source : String, destination : String) -> String?` | source 右端弹出插入 dest 左端 |
| `ltrim` | `(self : Database, key : String, start : Int, end : Int) -> Bool` | 裁剪列表至指定区间 |
| `lmove` | `(self : Database, source : String, destination : String, from_left : Bool, to_left : Bool) -> String?` | 在列表间移动元素 |
| `try_lmove` | `(self : Database, source : String, destination : String, from_left : Bool, to_left : Bool) -> Result[String, RedisError]` | 精确错误版 LMOVE |
| `lpos` | `(self : Database, key : String, value : String) -> Int?` | 元素的首次索引 |
| `try_lpos` | `(self : Database, key : String, value : String) -> Result[Int, RedisError]` | 精确错误版 LPOS |
| `try_lpos_options` | `(self : Database, key : String, value : String, rank : Int, count : Int, maxlen : Int) -> Result[Array[Int], RedisError]` | LPOS RANK/COUNT/MAXLEN（count 0=单个、-1=全部） |
| `lpos_options` | `(self : Database, key : String, value : String, rank : Int, count : Int, maxlen : Int) -> Array[Int]` | LPOS 选项便捷版（错误返回空） |
| `lpop_count` | `(self : Database, key : String, count : Int) -> Array[String]` | 批量左端弹出 |
| `rpop_count` | `(self : Database, key : String, count : Int) -> Array[String]` | 批量右端弹出 |

#### Set 操作

| 方法 | 签名 | 说明 |
|------|------|------|
| `sadd` | `(self : Database, key : String, value : String) -> Bool` | 添加成员 |
| `try_sadd` | `(self : Database, key : String, value : String) -> Result[Bool, RedisError]` | 精确错误版添加成员 |
| `sadd_all` | `(self : Database, key : String, values : Array[String]) -> Int` | 批量添加，返回新增数量 |
| `srem_all` | `(self : Database, key : String, values : Array[String]) -> Int` | 批量删除，返回删除数量 |
| `smembers` | `(self : Database, key : String) -> Array[String]` | 所有成员 |
| `try_smembers` | `(self : Database, key : String) -> Result[Array[String], RedisError]` | 精确错误版获取所有成员 |
| `srem` | `(self : Database, key : String, value : String) -> Bool` | 删除成员 |
| `scard` | `(self : Database, key : String) -> Int` | 成员数量 |
| `sismember` | `(self : Database, key : String, value : String) -> Bool` | 检查成员是否存在 |
| `smismember` | `(self : Database, key : String, members : Array[String]) -> Array[Bool]` | 批量检查成员是否存在 |
| `try_smismember` | `(self : Database, key : String, members : Array[String]) -> Result[Array[Bool], RedisError]` | 精确错误版 SMISMEMBER |
| `try_sintercard` | `(self : Database, keys : Array[String], limit : Int) -> Result[Int, RedisError]` | 精确错误版 SINTERCARD |
| `srandmember_count` | `(self : Database, key : String, count : Int) -> Array[String]` | 随机获取多个成员 |
| `spop_count` | `(self : Database, key : String, count : Int) -> Array[String]` | 随机弹出多个成员（弹空自动删 key） |
| `sintercard` | `(self : Database, keys : Array[String], limit : Int) -> Int` | 交集基数 |
| `sscan` | `(self : Database, key : String, cursor : Int, count : Int) -> (Int, Array[String])` | 增量遍历集合成员（快照缓存） |
| `sinter` | `(self : Database, keys : Array[String]) -> Array[String]` | 多集合交集（类型错误返回空数组） |
| `try_sinter` | `(self : Database, keys : Array[String]) -> Result[Array[String], RedisError]` | 精确错误版多集合交集 |
| `sinterstore` | `(self : Database, destination : String, keys : Array[String]) -> Int` | 交集存储到 dest |
| `try_sinterstore` | `(self : Database, destination : String, keys : Array[String]) -> Result[Int, RedisError]` | 精确错误版交集存储 |
| `sunion` | `(self : Database, keys : Array[String]) -> Array[String]` | 多集合并集（类型错误返回空数组） |
| `try_sunion` | `(self : Database, keys : Array[String]) -> Result[Array[String], RedisError]` | 精确错误版多集合并集 |
| `sunionstore` | `(self : Database, destination : String, keys : Array[String]) -> Int` | 并集存储到 dest |
| `try_sunionstore` | `(self : Database, destination : String, keys : Array[String]) -> Result[Int, RedisError]` | 精确错误版并集存储 |
| `sdiff` | `(self : Database, keys : Array[String]) -> Array[String]` | 多集合差集（类型错误返回空数组） |
| `try_sdiff` | `(self : Database, keys : Array[String]) -> Result[Array[String], RedisError]` | 精确错误版多集合差集 |
| `sdiffstore` | `(self : Database, destination : String, keys : Array[String]) -> Int` | 差集存储到 dest |
| `try_sdiffstore` | `(self : Database, destination : String, keys : Array[String]) -> Result[Int, RedisError]` | 精确错误版差集存储 |
| `smove` | `(self : Database, source : String, destination : String, value : String) -> Bool` | 跨集合移动元素 |
| `spop` | `(self : Database, key : String) -> String?` | 随机弹出元素 |
| `srandmember` | `(self : Database, key : String) -> String?` | 随机返回一个成员（不删除） |

#### Sorted Set 操作

| 方法 | 签名 | 说明 |
|------|------|------|
| `zadd` | `(self : Database, key : String, score : Float, member_val : String) -> Bool` | 添加/更新成员 |
| `try_zadd` | `(self : Database, key : String, score : Float, member_val : String) -> Result[Bool, RedisError]` | 精确错误版添加/更新成员 |
| `try_zadd_with_options` | `(self : Database, key : String, score : Float, member_val : String, nx : Bool, xx : Bool, ch : Bool, gt : Bool, lt : Bool) -> Result[Int, RedisError]` | ZADD NX/XX/CH/GT/LT（返回新增或变更数） |
| `try_zadd_incr` | `(self : Database, key : String, increment : Float, member_val : String, nx : Bool, xx : Bool, gt : Bool, lt : Bool) -> Result[Float?, RedisError]` | ZADD INCR（None 表示未更新） |
| `zrange` | `(self : Database, key : String, start : Int, end : Int) -> Array[String]` | 按排名升序获取范围 |
| `zcard` | `(self : Database, key : String) -> Int` | 成员数量 |
| `zscore` | `(self : Database, key : String, member_val : String) -> Float?` | 获取成员分数 |
| `try_zscore` | `(self : Database, key : String, member_val : String) -> Result[Float, RedisError]` | 精确错误版获取成员分数 |
| `zrandmember` | `(self : Database, key : String) -> String?` | 随机返回一个成员 |
| `zunion` | `(self : Database, keys : Array[String]) -> Array[(String, Float)]` | 多集合并集（分数求和） |
| `zinter` | `(self : Database, keys : Array[String]) -> Array[(String, Float)]` | 多集合交集（分数求和） |
| `zdiff` | `(self : Database, keys : Array[String]) -> Array[(String, Float)]` | 多集合差集 |
| `zscan` | `(self : Database, key : String, cursor : Int, count : Int) -> (Int, Array[(String, Float)])` | 增量遍历成员与分数 |
| `try_zscan` | `(self : Database, key : String, cursor : Int, count : Int) -> Result[(Int, Array[(String, Float)]), RedisError]` | 精确错误版 ZSCAN |
| `zrandmember_count` | `(self : Database, key : String, count : Int) -> Array[String]` | 随机获取多个成员 |
| `zpopmin_count` | `(self : Database, key : String, count : Int) -> Array[(String, Float)]` | 批量弹出最小分数成员 |
| `zpopmax_count` | `(self : Database, key : String, count : Int) -> Array[(String, Float)]` | 批量弹出最大分数成员 |
| `try_zrandmember` | `(self : Database, key : String) -> Result[String, RedisError]` | 精确错误版 ZRANDMEMBER |
| `try_zunion` | `(self : Database, keys : Array[String]) -> Result[Array[(String, Float)], RedisError]` | 精确错误版 ZUNION |
| `try_zinter` | `(self : Database, keys : Array[String]) -> Result[Array[(String, Float)], RedisError]` | 精确错误版 ZINTER |
| `try_zdiff` | `(self : Database, keys : Array[String]) -> Result[Array[(String, Float)], RedisError]` | 精确错误版 ZDIFF |
| `zunionstore` | `(self : Database, destination : String, keys : Array[String]) -> Int` | 并集存储 |
| `zinterstore` | `(self : Database, destination : String, keys : Array[String]) -> Int` | 交集存储 |
| `zdiffstore` | `(self : Database, destination : String, keys : Array[String]) -> Int` | 差集存储 |
| `zintercard` | `(self : Database, keys : Array[String], limit : Int) -> Int` | 交集基数 |
| `zrangestore` | `(self : Database, destination : String, source : String, start : Int, end : Int) -> Int` | 范围复制 |
| `zrem` | `(self : Database, key : String, member_val : String) -> Bool` | 删除成员 |
| `zrangebyscore` | `(self : Database, key : String, min_score : Float, max_score : Float) -> Array[String]` | 按分数范围获取 |
| `zrangebylex` | `(self : Database, key : String, min_bound : String, max_bound : String) -> Array[String]` | 按成员字典序范围获取 |
| `zrevrangebylex` | `(self : Database, key : String, max_bound : String, min_bound : String) -> Array[String]` | 按成员字典序降序获取 |
| `zlexcount` | `(self : Database, key : String, min_bound : String, max_bound : String) -> Int` | 统计字典序范围内成员数 |
| `zremrangebylex` | `(self : Database, key : String, min_bound : String, max_bound : String) -> Int` | 按字典序范围删除 |
| `zrange_with_scores` | `(self : Database, key : String, start : Int, end : Int) -> Array[(String, Float)]` | 按排名升序返回成员与分数 |
| `zrevrange_with_scores` | `(self : Database, key : String, start : Int, end : Int) -> Array[(String, Float)]` | 按排名降序返回成员与分数 |
| `zrangebyscore_with_limit` | `(self : Database, key : String, min_score : Float, max_score : Float, offset : Int, count : Int) -> Array[String]` | 分数区间升序分页（负 offset 返回空） |
| `zrevrangebyscore_with_limit` | `(self : Database, key : String, max_score : Float, min_score : Float, offset : Int, count : Int) -> Array[String]` | 分数区间降序分页 |
| `try_zrangebylex_with_limit` | `(self : Database, key : String, min_bound : String, max_bound : String, offset : Int, count : Int) -> Result[Array[String], RedisError]` | 字典序升序分页（精确错误版） |
| `try_zrevrangebylex_with_limit` | `(self : Database, key : String, max_bound : String, min_bound : String, offset : Int, count : Int) -> Result[Array[String], RedisError]` | 字典序降序分页（精确错误版） |
| `try_zrangebyscore_by_bound` | `(self : Database, key : String, min_bound : String, max_bound : String, offset : Int, count : Int) -> Result[Array[String], RedisError]` | 分数区间查询，支持 `[`/`(` 边界与 `-inf`/`+inf` |
| `try_zrevrangebyscore_by_bound` | `(self : Database, key : String, max_bound : String, min_bound : String, offset : Int, count : Int) -> Result[Array[String], RedisError]` | 分数区间降序查询（同上边界语义） |
| `try_zcount_by_bound` | `(self : Database, key : String, min_bound : String, max_bound : String) -> Result[Int, RedisError]` | 边界版分数区间计数 |
| `try_zremrangebyscore_by_bound` | `(self : Database, key : String, min_bound : String, max_bound : String) -> Result[Int, RedisError]` | 边界版按分数区间删除 |
| `try_zrangestore_byscore` | `(self : Database, destination : String, source : String, min_bound : String, max_bound : String, offset : Int, count : Int) -> Result[Int, RedisError]` | ZRANGESTORE BYSCORE（含 LIMIT） |
| `try_zrangestore_bylex` | `(self : Database, destination : String, source : String, min_bound : String, max_bound : String) -> Result[Int, RedisError]` | ZRANGESTORE BYLEX |
| `zcount` | `(self : Database, key : String, min_score : Float, max_score : Float) -> Int` | 统计分数范围内成员数 |
| `zrevrange` | `(self : Database, key : String, start : Int, end : Int) -> Array[String]` | 按排名降序获取范围 |
| `zrevrangebyscore` | `(self : Database, key : String, max_score : Float, min_score : Float) -> Array[String]` | 按分数降序范围获取 |
| `zrank` | `(self : Database, key : String, member_val : String) -> Int?` | 升序排名 |
| `zrevrank` | `(self : Database, key : String, member_val : String) -> Int?` | 降序排名 |
| `zincrby` | `(self : Database, key : String, increment : Float, member_val : String) -> Float` | 增加分数 |
| `try_zincrby` | `(self : Database, key : String, increment : Float, member_val : String) -> Result[Float, RedisError]` | 精确错误版增加分数 |
| `zremrangebyrank` | `(self : Database, key : String, start : Int, end : Int) -> Int` | 按排名范围删除 |
| `zremrangebyscore` | `(self : Database, key : String, min_score : Float, max_score : Float) -> Int` | 按分数范围删除 |
| `zpopmin` | `(self : Database, key : String) -> (String, Float)?` | 弹出最小分数成员 |
| `zpopmax` | `(self : Database, key : String) -> (String, Float)?` | 弹出最大分数成员 |
| `zmscore` | `(self : Database, key : String, members : Array[String]) -> Array[Float?]` | 批量获取成员分数 |

#### 批量操作

| 方法 | 签名 | 说明 |
|------|------|------|
| `mset` | `(self : Database, keys : Array[String], values : Array[String]) -> Unit` | 批量设置（长度不一致时整体不写入） |
| `msetnx` | `(self : Database, keys : Array[String], values : Array[String]) -> Bool` | 仅当所有 key 不存在时批量设置 |
| `try_mset` | `(self : Database, keys : Array[String], values : Array[String]) -> Result[Unit, RedisError]` | 精确错误版批量设置 |
| `try_msetnx` | `(self : Database, keys : Array[String], values : Array[String]) -> Result[Bool, RedisError]` | 精确错误版 MSETNX |
| `mget` | `(self : Database, keys : Array[String]) -> Array[String?]` | 批量获取 |
| `mdel` | `(self : Database, keys : Array[String]) -> Int` | 批量删除 |
| `hmset` | `(self : Database, key : String, fields : Array[String], values : Array[String]) -> Unit` | 批量设置 hash 字段（长度不一致时整体不写入） |
| `try_hmset` | `(self : Database, key : String, fields : Array[String], values : Array[String]) -> Result[Unit, RedisError]` | 精确错误版批量设置 hash 字段 |
| `try_hset_all` | `(self : Database, key : String, fields : Array[String], values : Array[String]) -> Result[Int, RedisError]` | 精确错误版多字段 HSET，返回新建字段数 |
| `hset_all` | `(self : Database, key : String, fields : Array[String], values : Array[String]) -> Int` | 多字段 HSET（长度不一致/类型错返回 -1） |
| `hmget` | `(self : Database, key : String, fields : Array[String]) -> Array[String?]` | 批量获取 hash 字段 |

#### 服务器命令

| 方法 | 签名 | 说明 |
|------|------|------|
| `dbsize` | `(self : Database) -> Int` | 当前 key 数量 |
| `flushdb` | `(self : Database) -> String` | 清空当前数据库（返回 "OK"） |
| `flushall` | `(self : Database) -> String` | 清空所有数据（返回 "OK"） |
| `ping` | `() -> String` | 返回 "PONG" |
| `echo` | `(self : Database, message : String) -> String` | 回显消息 |
| `info` | `(self : Database) -> String` | 服务器信息 |
| `time` | `(self : Database) -> (Int64, Int64)` | 当前时间 |
| `command` | `() -> Array[String]` | 支持的命令列表 |
| `object_encoding` | `(self : Database, key : String) -> String?` | 对象内部编码 |

### Deque 结构体

双端队列数据结构，作为 List 类型的底层实现暴露为公共 API。

| 方法 | 签名 | 说明 |
|------|------|------|
| `new` | `() -> Deque` | 创建空双端队列 |
| `push_front` | `(self : Deque, value : String) -> Unit` | 前端插入 |
| `push_back` | `(self : Deque, value : String) -> Unit` | 后端插入 |
| `pop_front` | `(self : Deque) -> String?` | 前端弹出 |
| `pop_back` | `(self : Deque) -> String?` | 后端弹出 |
| `length` | `(self : Deque) -> Int` | 队列长度 |
| `to_array` | `(self : Deque) -> Array[String]` | 转换为数组 |
| `get_at` | `(self : Deque, index : Int) -> String?` | 按索引获取 |
| `set_at` | `(self : Deque, index : Int, value : String) -> Bool` | 按索引设置 |
| `remove_at` | `(self : Deque, index : Int) -> Bool` | 按索引删除 |
| `find_index` | `(self : Deque, value : String) -> Int` | 按值查找索引 |
| `insert_at` | `(self : Deque, index : Int, value : String) -> Bool` | 按索引插入 |

### 独立函数

| 函数 | 签名 | 说明 |
|------|------|------|
| `match_pattern` | `(pattern : String, str : String) -> Bool` | 通配符模式匹配（支持 `*` `?`） |
| `match_pattern_helper` | `(pattern : String, str : String, p_idx : Int, s_idx : Int) -> Bool` | 模式匹配迭代辅助函数 |
| `uint16_to_digit` | `(c : UInt16) -> Int?` | Unicode 字符转数字 |
| `digit_to_uint16` | `(n : Int) -> UInt16` | 数字转 Unicode 字符 |
| `parse_int` | `(s : String) -> Int?` | 字符串解析为整数 |
| `parse_int64` | `(s : String) -> Int64?` | 字符串解析为 64 位整数 |
| `int_to_string` | `(n : Int) -> String` | 整数转字符串 |
| `sort_by_score` | `(items : Array[(String, Float)]) -> Array[(String, Float)]` | 按分数排序 |
| `merge_sort` | `(arr : Array[(String, Float)]) -> Array[(String, Float)]` | 归并排序 |
| `merge` | `(left : Array[(String, Float)], right : Array[(String, Float)]) -> Array[(String, Float)]` | 归并合并 |

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
  HLEN user:1001 = 5

📌 3. List 任务队列
  LRANGE tasks 0 -1 = [紧急任务, 完成报告, 代码审查, 团队会议]

📌 4. Set 标签系统
  SCARD = 3 (自动去重)
  TRY_SINTER 遇到错误类型 = WRONGTYPE Operation against a key holding the wrong kind of value

📌 5. Sorted Set 排行榜
  TOP 3 (ZREVRANGE WITHSCORES):
    #1 玩家D - 3200分
    #2 玩家B - 2500分

📌 6. Key 过期与管理
  65秒后 EXISTS = false

📌 7. 批量操作与错误模型
  TRY_MSET 长度不一致 = ERR invalid argument
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

模拟 Redis 风格的命令行交互，自动执行 62 条演示命令并输出结果。

```bash
moon run examples/cli_repl --target native
```

参考 [examples/cli_repl/main.mbt](examples/cli_repl/main.mbt)

支持解析和执行 74 个命令，包括 SET/GET/HSET/LPUSH/SADD/ZADD/SCAN/COPY/SORT 等常用命令。

## 许可证

本项目采用 [Apache License 2.0](LICENSE) 许可证（SPDX 标识符：`Apache-2.0`）。

详见项目根目录 [LICENSE](LICENSE) 文件。

## Redis 参考范围说明

本项目参考 [Redis](https://redis.io/) 的 API 设计思路，基于 MoonBit 语言独立实现。本项目**不是** Redis 的移植或 fork，所有代码均为原创实现。

### 参考范围

本项目参考了 Redis 的以下功能设计：

**数据结构**（5 种）：
- String — 字符串值
- Hash — 哈希表（字段-值映射）
- List — 列表（双端队列）
- Set — 集合（无序唯一元素）
- Sorted Set — 有序集合（分数排序）

**命令语义**（8 大命令组，141 个命令）：

| 命令组 | 命令数量 | 代表命令 |
|--------|----------|----------|
| String | 24 | SET, GET, DEL, EXISTS, APPEND, STRLEN, GETRANGE, SUBSTR, SETRANGE, GETBIT, SETBIT, BITPOS, BITCOUNT, BITOP, BITFIELD, INCR, DECR, INCRBY, DECRBY, SETNX, SETEX, PSETEX, GETDEL, GETSET |
| Key | 19 | KEYS, KEYS_PATTERN, SCAN, TYPE, EXPIRE, PEXPIRE, EXPIREAT, PEXPIREAT, EXPIRETIME, PEXPIRETIME, TTL, PTTL, PERSIST, RENAME, RENAMENX, RANDOMKEY, COPY, TOUCH, UNLINK |
| Hash | 13 | HSET, HSETNX, HGET, HDEL, HGETALL, HLEN, HEXISTS, HKEYS, HVALS, HINCRBY, HSTRLEN, HSCAN, HRANDFIELD |
| List | 17 | LPUSH, RPUSH, LPOP, RPOP, LLEN, LRANGE, LINDEX, LSET, LREM, LINSERT, LPUSHX, RPUSHX, RPOPLPUSH, LMOVE, LPOS, LTRIM, LMPOP |
| Set | 17 | SADD, SMEMBERS, SREM, SCARD, SISMEMBER, SMISMEMBER, SINTER, SINTERCARD, SINTERSTORE, SUNION, SUNIONSTORE, SDIFF, SDIFFSTORE, SMOVE, SPOP, SRANDMEMBER, SSCAN |
| Sorted Set | 32 | ZADD, ZRANGE, ZCARD, ZSCORE, ZREM, ZRANGEBYSCORE, ZRANGEBYLEX, ZREVRANGEBYLEX, ZLEXCOUNT, ZCOUNT, ZREVRANGE, ZREVRANGEBYSCORE, ZRANK, ZREVRANK, ZINCRBY, ZREMRANGEBYRANK, ZREMRANGEBYSCORE, ZREMRANGEBYLEX, ZPOPMIN, ZPOPMAX, ZMSCORE, ZRANDMEMBER, ZUNION, ZUNIONSTORE, ZINTER, ZINTERSTORE, ZINTERCARD, ZDIFF, ZDIFFSTORE, ZRANGESTORE, ZSCAN, ZMPOP |
| 批量操作 | 6 | MSET, MSETNX, MGET, MDEL, HMSET, HMGET |
| 服务器 | 34 | DBSIZE, FLUSHDB, FLUSHALL, PING, ECHO, INFO, TIME, COMMAND, COMMAND_INFO, COMMAND_COUNT, OBJECT_ENCODING, OBJECT_REFCOUNT, OBJECT_IDLETIME, MULTI, EXEC, DISCARD, WATCH, UNWATCH, AUTH, CLIENT, CONFIG, SLOWLOG, SELECT, MOVE, DUMP, RESTORE, MONITOR, OBJECT_FREQ, SAVE, BGSAVE, LASTSAVE, RESET, SORT, MEMORY_USAGE |

**Key 过期机制**：
- EXPIRE / PEXPIRE / EXPIREAT / PEXPIREAT — 设置过期时间（秒/毫秒、相对/绝对时间戳）
- TTL / PTTL — 查询剩余生存时间
- PERSIST — 移除过期时间
- 惰性过期检查策略
- 容器删除最后成员后自动清理 key（对齐 Redis 语义）

**内部编码命名**（OBJECT_ENCODING 命令）：
- `embstr` — String 类型编码
- `listpack` — Hash/List/Set/ZSet 小规模编码
- `hashtable` — Hash/Set 大规模编码
- `skiplist` — ZSet 大规模编码
- `quicklist` — List 大规模编码

## 第三方依赖

| 依赖 | 版本 | 来源 | 许可证 |
|------|------|------|--------|
| https://github.com/redis/redis | 8.8.0| redis | Redis Source Available License 2.0 (RSALv2) |
