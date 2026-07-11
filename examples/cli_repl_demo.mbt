fn main() -> Unit {
  println("========================================")
  println("  MoonBitDB - 命令行交互演示")
  println("========================================")
  println()
  println("  支持的命令：")
  println("    SET key value       设置字符串值")
  println("    GET key             获取字符串值")
  println("    DEL key             删除key")
  println("    EXISTS key          检查key是否存在")
  println("    KEYS                列出所有key")
  println("    TYPE key            获取值类型")
  println("    APPEND key value    追加字符串")
  println("    STRLEN key          字符串长度")
  println("    INCR key            自增1")
  println("    DECR key            自减1")
  println("    EXPIRE key seconds  设置过期时间")
  println("    TTL key             查看剩余时间")
  println("    PERSIST key         移除过期时间")
  println("    HSET key f v        设置hash字段")
  println("    HGET key f          获取hash字段")
  println("    HDEL key f          删除hash字段")
  println("    HGETALL key         获取所有hash字段")
  println("    HLEN key            hash字段数")
  println("    LPUSH key value     左端push")
  println("    RPUSH key value     右端push")
  println("    LPOP key            左端pop")
  println("    RPOP key            右端pop")
  println("    LLEN key            列表长度")
  println("    LRANGE key s e      列表范围")
  println("    SADD key m          添加集合成员")
  println("    SMEMBERS key        所有集合成员")
  println("    SREM key m          删除集合成员")
  println("    SCARD key           集合大小")
  println("    SISMEMBER key m     检查成员")
  println("    ZADD key s m        添加有序集合")
  println("    ZRANGE key s e      有序集合范围")
  println("    ZCARD key           有序集合大小")
  println("    ZSCORE key m        成员分数")
  println("    ZRANK key m         成员排名")
  println("    MSET k1 k2.. v1 v2.. 批量设置")
  println("    MGET k1 k2..        批量获取")
  println("    MDEL k1 k2..        批量删除")
  println("    DBSIZE              数据库大小")
  println("    FLUSHDB             清空数据库")
  println("    PING                测试连接")
  println("    INFO                服务器信息")
  println("    TIME                当前时间")
  println("    ADVANCE ms          推进时间")
  println("    HELP                显示帮助")
  println("    QUIT                退出")
  println()
  
  let db = Database::new()
  
  let demo_commands : Array[String] = [
    "SET name Alice",
    "SET age 25",
    "GET name",
    "GET age",
    "INCR age",
    "GET age",
    "APPEND name ' Smith'",
    "GET name",
    "STRLEN name",
    "EXPIRE name 60",
    "TTL name",
    "HSET user:1 username alice",
    "HSET user:1 email alice@test.com",
    "HGET user:1 username",
    "HLEN user:1",
    "LPUSH tasks 'Task 1'",
    "LPUSH tasks 'Task 2'",
    "RPUSH tasks 'Task 3'",
    "LRANGE tasks 0 -1",
    "LLEN tasks",
    "SADD tags tech",
    "SADD tags programming",
    "SADD tags moonbit",
    "SMEMBERS tags",
    "SCARD tags",
    "SISMEMBER tags moonbit",
    "ZADD scores 100 alice",
    "ZADD scores 200 bob",
    "ZADD scores 150 charlie",
    "ZRANGE scores 0 -1",
    "ZRANK scores bob",
    "ZINCRBY scores 50 alice",
    "ZRANGE scores 0 -1",
    "MSET a 1 b 2 c 3",
    "MGET a b c d",
    "KEYS",
    "TYPE name",
    "TYPE user:1",
    "TYPE tasks",
    "TYPE scores",
    "DBSIZE",
    "ADVANCE 70000",
    "EXISTS name",
    "TTL name",
    "INFO",
    "PING"
  ]
  
  println("  🎬 开始演示命令执行...")
  println("----------------------------------------")
  for cmd in demo_commands {
    println()
    println("  > \{cmd}")
    let result = execute_command(db, cmd)
    for line in result {
      println("  \{line}")
    }
  }
  
  println()
  println("========================================")
  println("  ✅ 命令行交互演示完成！")
  println("========================================")
}

fn execute_command(db : Database, cmd : String) -> Array[String] {
  let parts = split_command(cmd)
  if parts.length() == 0 {
    return ["(empty)"]
  }
  
  let command = parts[0].to_upper()
  
  match command {
    "SET" => {
      if parts.length() < 3 {
        ["ERR wrong number of arguments for 'set' command"]
      } else {
        db.set(parts[1], parts[2])
        ["OK"]
      }
    }
    "GET" => {
      if parts.length() < 2 {
        ["ERR wrong number of arguments for 'get' command"]
      } else {
        match db.get(parts[1]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "DEL" => {
      if parts.length() < 2 {
        ["ERR wrong number of arguments"]
      } else {
        if db.del(parts[1]) { ["(integer) 1"] } else { ["(integer) 0"] }
      }
    }
    "EXISTS" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { if db.exists(parts[1]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "KEYS" => {
      let keys = db.keys()
      let result = []
      for i = 0; i < keys.length(); i = i + 1 {
        result.push("  \{i + 1}) \"\{keys[i]}\"")
      }
      result
    }
    "TYPE" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { [db.type_of(parts[1])] }
    }
    "APPEND" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else {
        let len = db.append(parts[1], parts[2])
        ["(integer) \{len}"]
      }
    }
    "STRLEN" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { ["(integer) \{db.strlen(parts[1])}"] }
    }
    "INCR" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        match db.incr(parts[1]) {
          Some(v) => ["(integer) \{v}"]
          None => ["ERR value is not an integer"]
        }
      }
    }
    "DECR" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        match db.decr(parts[1]) {
          Some(v) => ["(integer) \{v}"]
          None => ["ERR value is not an integer"]
        }
      }
    }
    "EXPIRE" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else {
        let sec = string_to_int(parts[2])
        match sec {
          Some(s) => if db.expire(parts[1], s) { ["(integer) 1"] } else { ["(integer) 0"] }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "TTL" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { ["(integer) \{db.ttl(parts[1])}"] }
    }
    "PERSIST" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { if db.persist(parts[1]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "HSET" => {
      if parts.length() < 4 { ["ERR wrong number"] }
      else {
        db.hset(parts[1], parts[2], parts[3]) |> ignore
        ["OK"]
      }
    }
    "HGET" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else {
        match db.hget(parts[1], parts[2]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "HDEL" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else { if db.hdel(parts[1], parts[2]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "HGETALL" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        let all = db.hgetall(parts[1])
        let result = []
        for i = 0; i < all.length(); i = i + 1 {
          let (f, v) = all[i]
          result.push("  \{i * 2 + 1}) \"\{f}\"")
          result.push("  \{i * 2 + 2}) \"\{v}\"")
        }
        result
      }
    }
    "HLEN" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { ["(integer) \{db.hlen(parts[1])}"] }
    }
    "LPUSH" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else {
        let len = db.lpush(parts[1], parts[2])
        ["(integer) \{len}"]
      }
    }
    "RPUSH" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else {
        let len = db.rpush(parts[1], parts[2])
        ["(integer) \{len}"]
      }
    }
    "LPOP" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        match db.lpop(parts[1]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "RPOP" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        match db.rpop(parts[1]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "LLEN" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { ["(integer) \{db.llen(parts[1])}"] }
    }
    "LRANGE" => {
      if parts.length() < 4 { ["ERR wrong number"] }
      else {
        let s = string_to_int(parts[2])
        let e = string_to_int(parts[3])
        match (s, e) {
          (Some(sv), Some(ev)) => {
            let items = db.lrange(parts[1], sv, ev)
            let result = []
            for i = 0; i < items.length(); i = i + 1 {
              result.push("  \{i + 1}) \"\{items[i]}\"")
            }
            result
          }
          _ => ["ERR value is not an integer"]
        }
      }
    }
    "SADD" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else { if db.sadd(parts[1], parts[2]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "SMEMBERS" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        let members = db.smembers(parts[1])
        let result = []
        for i = 0; i < members.length(); i = i + 1 {
          result.push("  \{i + 1}) \"\{members[i]}\"")
        }
        result
      }
    }
    "SREM" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else { if db.srem(parts[1], parts[2]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "SCARD" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { ["(integer) \{db.scard(parts[1])}"] }
    }
    "SISMEMBER" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else { if db.sismember(parts[1], parts[2]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "ZADD" => {
      if parts.length() < 4 { ["ERR wrong number"] }
      else {
        let score = string_to_float(parts[2])
        match score {
          Some(s) => if db.zadd(parts[1], s, parts[3]) { ["(integer) 1"] } else { ["(integer) 0"] }
          None => ["ERR value is not a valid float"]
        }
      }
    }
    "ZRANGE" => {
      if parts.length() < 4 { ["ERR wrong number"] }
      else {
        let s = string_to_int(parts[2])
        let e = string_to_int(parts[3])
        match (s, e) {
          (Some(sv), Some(ev)) => {
            let items = db.zrange(parts[1], sv, ev)
            let result = []
            for i = 0; i < items.length(); i = i + 1 {
              result.push("  \{i * 2 + 1}) \"\{items[i]}\"")
            }
            result
          }
          _ => ["ERR value is not an integer"]
        }
      }
    }
    "ZCARD" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { ["(integer) \{db.zcard(parts[1])}"] }
    }
    "ZSCORE" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else {
        match db.zscore(parts[1], parts[2]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "ZRANK" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else {
        match db.zrank(parts[1], parts[2]) {
          Some(v) => ["(integer) \{v}"]
          None => ["(nil)"]
        }
      }
    }
    "ZINCRBY" => {
      if parts.length() < 4 { ["ERR wrong number"] }
      else {
        let inc = string_to_float(parts[2])
        match inc {
          Some(i) => {
            let new_score = db.zincrby(parts[1], i, parts[3])
            ["\"\{new_score}\""]
          }
          None => ["ERR value is not a valid float"]
        }
      }
    }
    "MSET" => {
      let mid = parts.length() / 2
      let keys = []
      let vals = []
      for i = 1; i < parts.length() && i <= mid; i = i + 1 {
        keys.push(parts[i])
      }
      for i = mid + 1; i < parts.length(); i = i + 1 {
        vals.push(parts[i])
      }
      db.mset(keys, vals)
      ["OK"]
    }
    "MGET" => {
      let keys = []
      for i = 1; i < parts.length(); i = i + 1 {
        keys.push(parts[i])
      }
      let vals = db.mget(keys)
      let result = []
      for i = 0; i < vals.length(); i = i + 1 {
        match vals[i] {
          Some(v) => result.push("  \{i + 1}) \"\{v}\"")
          None => result.push("  \{i + 1}) (nil)")
        }
      }
      result
    }
    "MDEL" => {
      let keys = []
      for i = 1; i < parts.length(); i = i + 1 {
        keys.push(parts[i])
      }
      ["(integer) \{db.mdel(keys)}"]
    }
    "DBSIZE" => {
      ["(integer) \{db.dbsize()}"]
    }
    "FLUSHDB" => {
      db.flushdb()
      ["OK"]
    }
    "PING" => {
      [Database::ping()]
    }
    "INFO" => {
      let info = db.info()
      info.split("\n")
    }
    "TIME" => {
      let (s, us) = db.time()
      ["\{s}", "\{us}"]
    }
    "ADVANCE" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        let ms = string_to_int(parts[1])
        match ms {
          Some(m) => {
            db.advance_time(m)
            ["OK (advanced \{m}ms)"]
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "HELP" => {
      [
        "SET key value | GET key | DEL key | EXISTS key",
        "KEYS | TYPE key | APPEND key value | STRLEN key",
        "INCR key | DECR key | EXPIRE key s | TTL key",
        "HSET key f v | HGET key f | HGETALL key | HLEN key",
        "LPUSH/RPUSH key v | LPOP/RPOP key | LRANGE key s e",
        "SADD key m | SMEMBERS key | SREM key m | SCARD key",
        "ZADD key s m | ZRANGE key s e | ZRANK key m",
        "MSET/MGET/MDEL ... | DBSIZE | FLUSHDB | PING | INFO",
        "ADVANCE ms | HELP | QUIT"
      ]
    }
    _ => ["ERR unknown command '\{command}'"]
  }
}

fn split_command(cmd : String) -> Array[String] {
  let parts : Array[String] = []
  let mut current = ""
  let mut i = 0
  let mut in_quote = false
  while i < cmd.length() {
    let c = cmd[i]
    if c == '\'' || c == '"' {
      in_quote = !in_quote
    } else if c == ' ' && !in_quote {
      if current.length() > 0 {
        parts.push(current)
        current = ""
      }
    } else {
      current = current + c.to_string()
    }
    i = i + 1
  }
  if current.length() > 0 {
    parts.push(current)
  }
  parts
}
