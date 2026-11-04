fn parse_int(s : String) -> Int? {
  let mut result = 0
  let mut i = 0
  let mut neg = false
  if s.length() > 0 && s[0] == '-' {
    neg = true
    i = 1
  }
  if i >= s.length() {
    return None
  }
  while i < s.length() {
    let c = s[i]
    if c >= '0' && c <= '9' {
      result = result * 10 + (c.to_int() - '0'.to_int())
    } else {
      return None
    }
    i = i + 1
  }
  if neg { Some(-result) } else { Some(result) }
}

fn parse_float(s : String) -> Float? {
  let mut result : Float = 0.0
  let mut i = 0
  let mut neg = false
  if s.length() > 0 && s[0] == '-' {
    neg = true
    i = 1
  }
  if i >= s.length() {
    return None
  }
  let mut has_dot = false
  let mut decimal_pos : Float = 10.0
  let mut has_digit = false
  while i < s.length() {
    let c = s[i]
    if c >= '0' && c <= '9' {
      has_digit = true
      let digit = Float::from_int(c.to_int() - '0'.to_int())
      if has_dot {
        result = result + digit / decimal_pos
        decimal_pos = decimal_pos * 10.0
      } else {
        result = result * 10.0 + digit
      }
    } else if c == '.' && !has_dot {
      has_dot = true
    } else {
      return None
    }
    i = i + 1
  }
  if !has_digit {
    return None
  }
  if neg { Some(-result) } else { Some(result) }
}

fn split_command(cmd : String) -> Array[String] {
  let parts : Array[String] = []
  let mut start = -1
  let mut i = 0
  let mut in_quote = false
  while i < cmd.length() {
    let c = cmd[i]
    if c == '\'' || c == '"' {
      if in_quote {
        // closing quote: extract token between quotes
        if start >= 0 {
          parts.push(cmd[start:i].to_owned())
          start = -1
        }
        in_quote = false
      } else {
        // opening quote: start after the quote char
        in_quote = true
        start = i + 1
      }
    } else if c == ' ' && !in_quote {
      if start >= 0 {
        parts.push(cmd[start:i].to_owned())
        start = -1
      }
    } else {
      if start == -1 && !in_quote {
        start = i
      }
    }
    i = i + 1
  }
  // handle last token
  if start >= 0 && start <= cmd.length() {
    if start == cmd.length() {
      parts.push("")
    } else {
      parts.push(cmd[start:].to_owned())
    }
  }
  parts
}

fn execute_command(db : @lib.Database, cmd : String) -> Array[String] {
  let parts = split_command(cmd)
  if parts.length() == 0 {
    return ["(empty)"]
  }
  let command = parts[0].to_upper()
  match command {
    "SET" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { db.set(parts[1], parts[2]); ["OK"] }
    }
    "GET" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        match db.get(parts[1]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "DEL" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { if db.del(parts[1]) { ["(integer) 1"] } else { ["(integer) 0"] } }
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
    "SCAN" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else {
        match (parse_int(parts[1]), parse_int(parts[2])) {
          (Some(cursor), Some(count)) => {
            let (next, keys) = db.scan(cursor, count)
            let result = ["(cursor) \{next}"]
            for i = 0; i < keys.length(); i = i + 1 {
              result.push("  \{i + 1}) \"\{keys[i]}\"")
            }
            result
          }
          _ => ["ERR value is not an integer"]
        }
      }
    }
    "TYPE" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { [db.type_of(parts[1])] }
    }
    "APPEND" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else { ["(integer) \{db.append(parts[1], parts[2])}"] }
    }
    "STRLEN" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { ["(integer) \{db.strlen(parts[1])}"] }
    }
    "GETRANGE" => {
      if parts.length() < 4 { ["ERR wrong number"] }
      else {
        match (parse_int(parts[2]), parse_int(parts[3])) {
          (Some(start), Some(end)) => ["\"\{db.getrange(parts[1], start, end)}\""]
          _ => ["ERR value is not an integer"]
        }
      }
    }
    "SETRANGE" => {
      if parts.length() < 4 { ["ERR wrong number"] }
      else {
        match parse_int(parts[2]) {
          Some(offset) => ["(integer) \{db.setrange(parts[1], offset, parts[3])}"]
          None => ["ERR value is not an integer"]
        }
      }
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
    "INCRBY" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else {
        match parse_int(parts[2]) {
          Some(delta) =>
            match db.incrby(parts[1], delta.to_int64()) {
              Some(v) => ["(integer) \{v}"]
              None => ["ERR value is not an integer or out of range"]
            }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "DECRBY" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else {
        match parse_int(parts[2]) {
          Some(delta) =>
            match db.decrby(parts[1], delta.to_int64()) {
              Some(v) => ["(integer) \{v}"]
              None => ["ERR value is not an integer or out of range"]
            }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "EXPIRE" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else {
        match parse_int(parts[2]) {
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
    "COPY" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else { if db.copy(parts[1], parts[2]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "TOUCH" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        let keys = []
        for i = 1; i < parts.length(); i = i + 1 {
          keys.push(parts[i])
        }
        ["(integer) \{db.touch(keys)}"]
      }
    }
    "UNLINK" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { if db.unlink(parts[1]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "HSET" => {
      if parts.length() < 4 { ["ERR wrong number"] }
      else { db.hset(parts[1], parts[2], parts[3]) |> ignore; ["OK"] }
    }
    "HSETNX" => {
      if parts.length() < 4 { ["ERR wrong number"] }
      else {
        if db.hsetnx(parts[1], parts[2], parts[3]) { ["(integer) 1"] } else { ["(integer) 0"] }
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
        let mut idx = 1
        for f, v in all {
          result.push("  \{idx}) \"\{f}\"")
          result.push("  \{idx + 1}) \"\{v}\"")
          idx = idx + 2
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
      else { ["(integer) \{db.lpush(parts[1], parts[2])}"] }
    }
    "RPUSH" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else { ["(integer) \{db.rpush(parts[1], parts[2])}"] }
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
        match (parse_int(parts[2]), parse_int(parts[3])) {
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
        match parse_float(parts[2]) {
          Some(s) => if db.zadd(parts[1], s, parts[3]) { ["(integer) 1"] } else { ["(integer) 0"] }
          None => ["ERR value is not a valid float"]
        }
      }
    }
    "ZRANGE" => {
      if parts.length() < 4 { ["ERR wrong number"] }
      else {
        match (parse_int(parts[2]), parse_int(parts[3])) {
          (Some(sv), Some(ev)) => {
            let with_scores = parts.length() > 4 && parts[4] == "WITHSCORES"
            if with_scores {
              let items = db.zrange_with_scores(parts[1], sv, ev)
              let result = []
              for i = 0; i < items.length(); i = i + 1 {
                result.push("  \{i + 1}) \"\{items[i].0}\" \{items[i].1}")
              }
              result
            } else {
              let items = db.zrange(parts[1], sv, ev)
              let result = []
              for i = 0; i < items.length(); i = i + 1 {
                result.push("  \{i + 1}) \"\{items[i]}\"")
              }
              result
            }
          }
          _ => ["ERR value is not an integer"]
        }
      }
    }
    "ZRANGEBYLEX" => {
      if parts.length() < 4 { ["ERR wrong number"] }
      else {
        let items = db.zrangebylex(parts[1], parts[2], parts[3])
        if parts.length() >= 7 && parts[4] == "LIMIT" {
          match (parse_int(parts[5]), parse_int(parts[6])) {
            (Some(off), Some(cnt)) => {
              if off < 0 {
                ["ERR offset is out of range"]
              } else {
                let apply_off = if off > items.length() { items.length() } else { off }
                let apply_cnt = if cnt <= 0 { items.length() - apply_off } else { cnt }
                let apply_end = if apply_off + apply_cnt > items.length() { items.length() } else { apply_off + apply_cnt }
                let result = []
                for i = apply_off; i < apply_end; i = i + 1 {
                  result.push("  \{i - apply_off + 1}) \"\{items[i]}\"")
                }
                result
              }
            }
            _ => ["ERR value is not an integer"]
          }
        } else {
          let result = []
          for i = 0; i < items.length(); i = i + 1 {
            result.push("  \{i + 1}) \"\{items[i]}\"")
          }
          result
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
        match parse_float(parts[2]) {
          Some(inc) => {
            let new_score = db.zincrby(parts[1], inc, parts[3])
            ["\"\{new_score}\""]
          }
          None => ["ERR value is not a valid float"]
        }
      }
    }
    "MSET" => {
      if parts.length() < 3 || (parts.length() - 1) % 2 != 0 {
        ["ERR wrong number of arguments"]
      } else {
        let keys = []
        let vals = []
        let mut i = 1
        while i + 1 < parts.length() {
          keys.push(parts[i])
          vals.push(parts[i + 1])
          i = i + 2
        }
        db.mset(keys, vals)
        ["OK"]
      }
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
    "DBSIZE" => { ["(integer) \{db.dbsize()}"] }
    "FLUSHDB" => { db.flushdb(); ["OK"] }
    "PING" => { [@lib.Database::ping()] }
    "INFO" => {
      let info = db.info()
      let lines = info.split("\n")
      let result = []
      for line in lines {
        result.push("  \{line}")
      }
      result
    }
    "TIME" => {
      let (s, us) = db.time()
      ["  seconds: \{s}", "  microseconds: \{us}"]
    }
    "ADVANCE" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        match parse_int(parts[1]) {
          Some(m) => { db.advance_time(m.to_int64()); ["OK (advanced \{m}ms)"] }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "HELP" => {
      [
        "  SET/GET/DEL/EXISTS | KEYS/SCAN | TYPE | APPEND/STRLEN",
        "  GETRANGE/SETRANGE | INCR/DECR/INCRBY/DECRBY | EXPIRE/TTL",
        "  HSET/HSETNX/HGET/HGETALL/HLEN",
        "  LPUSH/RPUSH/LPOP/RPOP/LRANGE",
        "  SADD/SMEMBERS/SREM/SCARD/SISMEMBER",
        "  ZADD/ZRANGE/ZRANGEBYLEX/ZRANK/ZINCRBY",
        "  MSET/MGET/MDEL | COPY/TOUCH/UNLINK | DBSIZE/FLUSHDB/PING/INFO",
        "  AUTH/CLIENT/CONFIG/SLOWLOG | SELECT/MOVE/DUMP/RESTORE/MONITOR",
        "  ADVANCE ms | HELP | QUIT"
      ]
    }
    "AUTH" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { [@lib.Database::auth(db, parts[1])] }
    }
    "CLIENT" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else { [@lib.Database::client_list(db)] }
    }
    "CONFIG" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else { [@lib.Database::config_get(db, parts[2])] }
    }
    "SLOWLOG" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        let entries = @lib.Database::slowlog(db, parts[1])
        if entries.length() == 0 { ["(empty)"] } else { entries }
      }
    }
    "SELECT" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        match parse_int(parts[1]) {
          Some(idx) => [@lib.Database::select(db, idx)]
          None => ["ERR value is not an integer"]
        }
      }
    }
    "MOVE" => {
      if parts.length() < 3 { ["ERR wrong number"] }
      else {
        match parse_int(parts[2]) {
          Some(idx) => { ["(integer) \{@lib.Database::move_key(db, parts[1], idx)}"] }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "DUMP" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        match @lib.Database::dump(db, parts[1]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "RESTORE" => {
      if parts.length() < 4 { ["ERR wrong number"] }
      else {
        match parse_int(parts[2]) {
          Some(ttl) => {
            ["(integer) \{@lib.Database::restore(db, parts[1], ttl.to_int64(), parts[3])}"]
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "MONITOR" => { [@lib.Database::monitor(db)] }
    "OBJECT_FREQ" => {
      if parts.length() < 2 { ["ERR wrong number"] }
      else {
        match @lib.Database::object_freq(db, parts[1]) {
          Some(freq) => ["(integer) \{freq}"]
          None => ["(nil)"]
        }
      }
    }
    _ => ["ERR unknown command '\{command}'"]
  }
}

fn main {
  println("========================================")
  println("  MoonBitDB - 命令行交互演示")
  println("========================================")
  println("")

  let db = @lib.Database::new()

  let demo_commands : Array[String] = [
    "SET name Alice",
    "SET age 25",
    "GET name",
    "GET age",
    "INCR age",
    "GET age",
    "INCRBY age 5",
    "DECRBY age 2",
    "APPEND name ' Smith'",
    "GET name",
    "STRLEN name",
    "GETRANGE name 0 4",
    "SETRANGE name 0 A",
    "EXPIRE name 60",
    "TTL name",
    "HSET user:1 username alice",
    "HSET user:1 email alice@test.com",
    "HSETNX user:1 username bob",
    "HSETNX user:1 city Beijing",
    "HGET user:1 username",
    "HGET user:1 city",
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
    "ZADD names 0 alice",
    "ZADD names 0 bob",
    "ZADD names 0 carol",
    "ZADD names 0 dave",
    "ZRANGEBYLEX names [b (d",
    "MSET a 1 b 2 c 3",
    "MGET a b c d",
    "COPY name name:copy",
    "TOUCH name user:1 name:copy",
    "UNLINK name:copy",
    "SCAN 0 5",
    "KEYS",
    "TYPE name",
    "TYPE user:1",
    "TYPE tasks",
    "TYPE scores",
    "DBSIZE",
    "ADVANCE 70000",
    "EXISTS name",
    "TTL name",
    "PING",
    "INFO"
  ]

  println("  🎬 开始演示命令执行...")
  println("----------------------------------------")
  for cmd in demo_commands {
    println("")
    println("  > \{cmd}")
    let result = execute_command(db, cmd)
    for line in result {
      println("  \{line}")
    }
  }

  println("")
  println("========================================")
  println("  ✅ 命令行交互演示完成！")
  println("========================================")
}
