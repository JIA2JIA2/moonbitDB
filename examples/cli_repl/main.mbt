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
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.get(parts[1]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "DEL" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { if db.del(parts[1]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "EXISTS" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
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
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
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
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { [db.type_of(parts[1])] }
    }
    "APPEND" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.append(parts[1], parts[2])}"] }
    }
    "STRLEN" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.strlen(parts[1])}"] }
    }
    "GETRANGE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match (parse_int(parts[2]), parse_int(parts[3])) {
          (Some(start), Some(end)) => ["\"\{db.getrange(parts[1], start, end)}\""]
          _ => ["ERR value is not an integer"]
        }
      }
    }
    "SETRANGE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(offset) => ["(integer) \{db.setrange(parts[1], offset, parts[3])}"]
          None => ["ERR value is not an integer"]
        }
      }
    }
    "INCR" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.incr(parts[1]) {
          Some(v) => ["(integer) \{v}"]
          None => ["ERR value is not an integer"]
        }
      }
    }
    "DECR" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.decr(parts[1]) {
          Some(v) => ["(integer) \{v}"]
          None => ["ERR value is not an integer"]
        }
      }
    }
    "INCRBY" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
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
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
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
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(s) => if db.expire(parts[1], s) { ["(integer) 1"] } else { ["(integer) 0"] }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "TTL" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.ttl(parts[1])}"] }
    }
    "PERSIST" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { if db.persist(parts[1]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "COPY" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { if db.copy(parts[1], parts[2]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "TOUCH" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        let keys = []
        for i = 1; i < parts.length(); i = i + 1 {
          keys.push(parts[i])
        }
        ["(integer) \{db.touch(keys)}"]
      }
    }
    "UNLINK" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { if db.unlink(parts[1]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "HSET" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else { db.hset(parts[1], parts[2], parts[3]) |> ignore; ["OK"] }
    }
    "HSETNX" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        if db.hsetnx(parts[1], parts[2], parts[3]) { ["(integer) 1"] } else { ["(integer) 0"] }
      }
    }
    "HGET" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match db.hget(parts[1], parts[2]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "HDEL" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { if db.hdel(parts[1], parts[2]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "HGETALL" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
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
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.hlen(parts[1])}"] }
    }
    "LPUSH" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.lpush(parts[1], parts[2])}"] }
    }
    "RPUSH" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.rpush(parts[1], parts[2])}"] }
    }
    "LPOP" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.lpop(parts[1]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "RPOP" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.rpop(parts[1]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "LLEN" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.llen(parts[1])}"] }
    }
    "LRANGE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
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
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { if db.sadd(parts[1], parts[2]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "SMEMBERS" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
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
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { if db.srem(parts[1], parts[2]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "SCARD" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.scard(parts[1])}"] }
    }
    "SISMEMBER" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { if db.sismember(parts[1], parts[2]) { ["(integer) 1"] } else { ["(integer) 0"] } }
    }
    "ZADD" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match parse_float(parts[2]) {
          Some(s) => if db.zadd(parts[1], s, parts[3]) { ["(integer) 1"] } else { ["(integer) 0"] }
          None => ["ERR value is not a valid float"]
        }
      }
    }
    "ZRANGE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
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
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
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
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.zcard(parts[1])}"] }
    }
    "ZSCORE" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match db.zscore(parts[1], parts[2]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "ZRANK" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match db.zrank(parts[1], parts[2]) {
          Some(v) => ["(integer) \{v}"]
          None => ["(nil)"]
        }
      }
    }
    "ZINCRBY" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
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
    "FLUSHDB" => { [@lib.Database::flushdb(db)] }
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
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[1]) {
          Some(m) => { db.advance_time(m.to_int64()); ["OK (advanced \{m}ms)"] }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "HELP" => {
      [
        "  String: SET/GET/APPEND/STRLEN/GETRANGE/SETRANGE/SUBSTR | INCR 族 | SETNX/SETEX/PSETEX",
        "  String: GETDEL/GETSET/GETEX | 位图: GETBIT/SETBIT/BITPOS/BITCOUNT/BITOP/BITFIELD",
        "  Hash: HSET/HMSET/HMGET/HGETALL/HKEYS/HVALS/HEXISTS/HSTRLEN/HINCRBY/HINCRBYFLOAT/HSCAN",
        "  List: LPUSH/RPUSH/LPOP/RPOP/LRANGE/LINDEX/LSET/LREM/LINSERT/LTRIM/LPOS/LPUSHX/RPUSHX",
        "  List: RPOPLPUSH/LMOVE/LMPOP",
        "  Set: SADD/SREM/SMEMBERS/SISMEMBER/SMISMEMBER/SCARD/SRANDMEMBER | SINTER/SUNION/SDIFF",
        "  Set: *STORE/SINTERCARD/SMOVE/SPOP/SSCAN",
        "  ZSet: ZADD/ZRANGE(+修饰符)/ZREVRANGE/ZRANGEBYSCORE/ZRANK/ZREVRANK/ZSCORE/ZMSCORE",
        "  ZSet: ZINCRBY/ZCOUNT/ZLEXCOUNT/ZRANGEBYLEX/ZREVRANGEBYLEX/ZREMRANGE*/ZPOPMIN/ZPOPMAX",
        "  ZSet: ZUNION/ZINTER/ZDIFF(+STORE)/ZINTERCARD/ZRANGESTORE/ZSCAN/ZRANDMEMBER/ZMPOP",
        "  Key: EXPIRE 族/EXPIRETIME/TTL/PTTL/PERSIST/RENAME/RANDOMKEY/COPY/TOUCH/UNLINK/TYPE",
        "  服务器: INFO/DBSIZE/FLUSHDB/FLUSHALL/PING/ECHO/TIME/WAIT/SHUTDOWN/LOLWUT/SAVE/BGSAVE",
        "  服务器: OBJECT_*/MEMORY_USAGE/COMMAND 族/AUTH/CLIENT/CONFIG/SLOWLOG/SELECT/MOVE",
        "  服务器: DUMP/RESTORE/MONITOR/SORT(+BY-GET)/MULTI/EXEC/DISCARD/WATCH/UNWATCH/RESET",
        "  ADVANCE ms | HELP | QUIT"
      ]
    }
    "GETBIT" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(off) => ["(integer) \{db.getbit(parts[1], off)}"]
          None => ["ERR value is not an integer"]
        }
      }
    }
    "SETBIT" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match (parse_int(parts[2]), parse_int(parts[3])) {
          (Some(off), Some(v)) => ["(integer) \{db.setbit(parts[1], off, v)}"]
          _ => ["ERR value is not an integer"]
        }
      }
    }
    "BITPOS" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(bit) => ["(integer) \{db.bitpos(parts[1], bit)}"]
          None => ["ERR value is not an integer"]
        }
      }
    }
    "BITCOUNT" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.bitcount(parts[1])}"] }
    }
    "BITOP" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        let src_keys = []
        for i = 3; i < parts.length(); i = i + 1 {
          src_keys.push(parts[i])
        }
        ["(integer) \{db.bitop(parts[1], parts[2], src_keys)}"]
      }
    }
    "SINTER" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        let items = db.sinter([parts[1], parts[2]])
        let result = []
        for i = 0; i < items.length(); i = i + 1 {
          result.push("  \{i + 1}) \"\{items[i]}\"")
        }
        result
      }
    }
    "SUNION" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        let items = db.sunion([parts[1], parts[2]])
        let result = []
        for i = 0; i < items.length(); i = i + 1 {
          result.push("  \{i + 1}) \"\{items[i]}\"")
        }
        result
      }
    }
    "SDIFF" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        let items = db.sdiff([parts[1], parts[2]])
        let result = []
        for i = 0; i < items.length(); i = i + 1 {
          result.push("  \{i + 1}) \"\{items[i]}\"")
        }
        result
      }
    }
    "SMISMEMBER" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        let members = []
        for i = 2; i < parts.length(); i = i + 1 {
          members.push(parts[i])
        }
        let flags = db.smismember(parts[1], members)
        let result = []
        for i = 0; i < flags.length(); i = i + 1 {
          result.push("  \{i + 1}) \{flags[i]}")
        }
        result
      }
    }
    "SRANDMEMBER" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        if parts.length() >= 3 {
          match parse_int(parts[2]) {
            Some(n) => {
              let items = db.srandmember_count(parts[1], n)
              let result = []
              for i = 0; i < items.length(); i = i + 1 {
                result.push("  \{i + 1}) \"\{items[i]}\"")
              }
              result
            }
            None => ["ERR value is not an integer"]
          }
        } else {
          match db.srandmember(parts[1]) {
            Some(v) => ["\"\{v}\""]
            None => ["(nil)"]
          }
        }
      }
    }
    "ZREM" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        let mut removed = 0
        for i = 2; i < parts.length(); i = i + 1 {
          if db.zrem(parts[1], parts[i]) {
            removed = removed + 1
          }
        }
        ["(integer) \{removed}"]
      }
    }
    "ZCOUNT" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match (parse_float(parts[2]), parse_float(parts[3])) {
          (Some(lo), Some(hi)) => ["(integer) \{db.zcount(parts[1], lo, hi)}"]
          _ => ["ERR value is not a valid float"]
        }
      }
    }
    "ZREVRANGE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match (parse_int(parts[2]), parse_int(parts[3])) {
          (Some(sv), Some(ev)) => {
            let with_scores = parts.length() > 4 && parts[4] == "WITHSCORES"
            let items = db.zrevrange_with_scores(parts[1], sv, ev)
            let result = []
            for i = 0; i < items.length(); i = i + 1 {
              if with_scores {
                result.push("  \{i + 1}) \"\{items[i].0}\" \{items[i].1}")
              } else {
                result.push("  \{i + 1}) \"\{items[i].0}\"")
              }
            }
            result
          }
          _ => ["ERR value is not an integer"]
        }
      }
    }
    "ZPOPMAX" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.zpopmax(parts[1]) {
          Some((m, s)) => ["1) \"\{m}\"", "2) \"\{s}\""]
          None => ["(empty array)"]
        }
      }
    }
    "ZMSCORE" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        let members = []
        for i = 2; i < parts.length(); i = i + 1 {
          members.push(parts[i])
        }
        let scores = db.zmscore(parts[1], members)
        let result = []
        for i = 0; i < scores.length(); i = i + 1 {
          match scores[i] {
            Some(v) => result.push("  \{i + 1}) \{v}")
            None => result.push("  \{i + 1}) (nil)")
          }
        }
        result
      }
    }
    "MULTI" => { [db.multi()] }
    "EXEC" => { [db.exec()] }
    "DISCARD" => { [db.discard()] }
    "WATCH" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { db.watch(parts[1]); ["OK"] }
    }
    "UNWATCH" => { db.unwatch(); ["OK"] }
    "ECHO" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { [parts[1]] }
    }
    "GETSET" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match db.getset(parts[1], parts[2]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "PSETEX" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(ms) => {
            if db.psetex(parts[1], ms.to_int64(), parts[3]) {
              ["OK"]
            } else {
              ["(error) invalid expire time"]
            }
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "SUBSTR" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match (parse_int(parts[2]), parse_int(parts[3])) {
          (Some(sv), Some(ev)) => ["\"\{db.getrange(parts[1], sv, ev)}\""]
          _ => ["ERR value is not an integer"]
        }
      }
    }
    "MSETNX" => {
      if parts.length() < 3 || (parts.length() - 1) % 2 != 0 { ["ERR wrong number of arguments"] }
      else {
        let keys = []
        let values = []
        for i = 1; i < parts.length(); i = i + 2 {
          keys.push(parts[i])
          values.push(parts[i + 1])
        }
        ["(integer) \{if db.msetnx(keys, values) { 1 } else { 0 }}"]
      }
    }
    "PTTL" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.pttl(parts[1]).to_string()}"] }
    }
    "EXPIREAT" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(sec) => ["(integer) \{if db.expireat(parts[1], sec.to_int64()) { 1 } else { 0 }}"]
          None => ["ERR value is not an integer"]
        }
      }
    }
    "PEXPIRE" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(ms) => ["(integer) \{if db.pexpire(parts[1], ms.to_int64()) { 1 } else { 0 }}"]
          None => ["ERR value is not an integer"]
        }
      }
    }
    "PEXPIREAT" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(ms) => ["(integer) \{if db.pexpireat(parts[1], ms.to_int64()) { 1 } else { 0 }}"]
          None => ["ERR value is not an integer"]
        }
      }
    }
    "EXPIRETIME" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.expiretime(parts[1]).to_string()}"] }
    }
    "PEXPIRETIME" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.pexpiretime(parts[1]).to_string()}"] }
    }
    "RENAME" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        if db.rename(parts[1], parts[2]) { ["OK"] } else { ["ERR no such key"] }
      }
    }
    "RENAMENX" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{if db.renamenx(parts[1], parts[2]) { 1 } else { 0 }}"] }
    }
    "RANDOMKEY" => {
      match db.randomkey() {
        Some(k) => ["\"\{k}\""]
        None => ["(nil)"]
      }
    }
    "FLUSHALL" => { [@lib.Database::flushall(db)] }
    "HKEYS" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        let items = db.hkeys(parts[1])
        let result = []
        for i = 0; i < items.length(); i = i + 1 {
          result.push("  \{i + 1}) \"\{items[i]}\"")
        }
        result
      }
    }
    "HVALS" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        let items = db.hvals(parts[1])
        let result = []
        for i = 0; i < items.length(); i = i + 1 {
          result.push("  \{i + 1}) \"\{items[i]}\"")
        }
        result
      }
    }
    "HEXISTS" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{if db.hexists(parts[1], parts[2]) { 1 } else { 0 }}"] }
    }
    "HMGET" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        let fields = []
        for i = 2; i < parts.length(); i = i + 1 {
          fields.push(parts[i])
        }
        let values = db.hmget(parts[1], fields)
        let result = []
        for i = 0; i < values.length(); i = i + 1 {
          match values[i] {
            Some(v) => result.push("  \{i + 1}) \"\{v}\"")
            None => result.push("  \{i + 1}) (nil)")
          }
        }
        result
      }
    }
    "HSTRLEN" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.hstrlen(parts[1], parts[2])}"] }
    }
    "HMSET" => {
      if parts.length() < 3 || (parts.length() - 1) % 2 != 0 { ["ERR wrong number of arguments"] }
      else {
        let fields = []
        let values = []
        for i = 1; i < parts.length(); i = i + 2 {
          fields.push(parts[i])
          values.push(parts[i + 1])
        }
        if db.hmset(parts[1], fields, values) {
          ["OK"]
        } else {
          ["(error) WRONGTYPE"]
        }
      }
    }
    "HINCRBYFLOAT" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match parse_float(parts[3]) {
          Some(inc) => {
            match db.hincrbyfloat(parts[1], parts[2], inc) {
              Some(v) => ["\"\{v}\""]
              None => ["(nil)"]
            }
          }
          None => ["ERR value is not a valid float"]
        }
      }
    }
    "HSCAN" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(cursor) => {
            let r = db.hscan_with_match(parts[1], cursor, 10, None)
            let result = []
            for i = 0; i < r.1.length(); i = i + 1 {
              let pair = r.1[i]
              result.push("  \{i + 1}) \"\{pair.0}\" => \"\{pair.1}\"")
            }
            result.push("  (next cursor: \{r.0})")
            result
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "LINDEX" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(idx) => {
            match db.lindex(parts[1], idx) {
              Some(v) => ["\"\{v}\""]
              None => ["(nil)"]
            }
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "LSET" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(idx) => {
            if db.lset(parts[1], idx, parts[3]) { ["OK"] } else { ["ERR index out of range"] }
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "LPOS" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match db.lpos(parts[1], parts[2]) {
          Some(idx) => ["(integer) \{idx}"]
          None => ["(nil)"]
        }
      }
    }
    "LPUSHX" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.lpushx(parts[1], parts[2])}"] }
    }
    "RPUSHX" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.rpushx(parts[1], parts[2])}"] }
    }
    "RPOPLPUSH" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match db.rpoplpush(parts[1], parts[2]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "LTRIM" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match (parse_int(parts[2]), parse_int(parts[3])) {
          (Some(sv), Some(ev)) => {
            if db.ltrim(parts[1], sv, ev) { ["OK"] } else { ["(error) WRONGTYPE"] }
          }
          _ => ["ERR value is not an integer"]
        }
      }
    }
    "LMPOP" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        let keys = []
        let mut left = true
        let mut count = 1
        for i = 1; i < parts.length(); i = i + 1 {
          if parts[i] == "LEFT" { left = true }
          else if parts[i] == "RIGHT" { left = false }
          else if parts[i] == "COUNT" && i + 1 < parts.length() {
            match parse_int(parts[i + 1]) {
              Some(c) => { count = c }
              None => ()
            }
            break
          } else {
            keys.push(parts[i])
          }
        }
        match db.lmpop(keys, left, count) {
          Some((k, vals)) => {
            let result = ["\"\{k}\""]
            for i = 0; i < vals.length(); i = i + 1 {
              result.push("  \{i + 1}) \"\{vals[i]}\"")
            }
            result
          }
          None => ["(nil)"]
        }
      }
    }
    "SINTERCARD" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.sintercard([parts[1], parts[2]], 0)}"] }
    }
    "SINTERSTORE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.sinterstore(parts[1], [parts[2], parts[3]])}"] }
    }
    "SUNIONSTORE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.sunionstore(parts[1], [parts[2], parts[3]])}"] }
    }
    "SDIFFSTORE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.sdiffstore(parts[1], [parts[2], parts[3]])}"] }
    }
    "SSCAN" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(cursor) => {
            let r = db.sscan(parts[1], cursor, 10)
            let result = []
            for i = 0; i < r.1.length(); i = i + 1 {
              result.push("  \{i + 1}) \"\{r.1[i]}\"")
            }
            result.push("  (next cursor: \{r.0})")
            result
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "ZREVRANK" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match db.zrevrank(parts[1], parts[2]) {
          Some(r) => ["(integer) \{r}"]
          None => ["(nil)"]
        }
      }
    }
    "ZRANGEBYSCORE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match (parse_float(parts[2]), parse_float(parts[3])) {
          (Some(lo), Some(hi)) => {
            let items = db.zrangebyscore(parts[1], lo, hi)
            let result = []
            for i = 0; i < items.length(); i = i + 1 {
              result.push("  \{i + 1}) \"\{items[i]}\"")
            }
            result
          }
          _ => ["ERR value is not a valid float"]
        }
      }
    }
    "ZRANDMEMBER" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        if parts.length() >= 3 {
          match parse_int(parts[2]) {
            Some(n) => {
              let items = db.zrandmember_count(parts[1], n)
              let result = []
              for i = 0; i < items.length(); i = i + 1 {
                result.push("  \{i + 1}) \"\{items[i]}\"")
              }
              result
            }
            None => ["ERR value is not an integer"]
          }
        } else {
          match db.zrandmember(parts[1]) {
            Some(m) => ["\"\{m}\""]
            None => ["(nil)"]
          }
        }
      }
    }
    "ZUNION" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        let items = db.zunion([parts[1], parts[2]])
        let result = []
        for i = 0; i < items.length(); i = i + 1 {
          result.push("  \{i + 1}) \"\{items[i].0}\" \{items[i].1}")
        }
        result
      }
    }
    "ZINTER" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        let items = db.zinter([parts[1], parts[2]])
        let result = []
        for i = 0; i < items.length(); i = i + 1 {
          result.push("  \{i + 1}) \"\{items[i].0}\" \{items[i].1}")
        }
        result
      }
    }
    "ZDIFF" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        let items = db.zdiff([parts[1], parts[2]])
        let result = []
        for i = 0; i < items.length(); i = i + 1 {
          result.push("  \{i + 1}) \"\{items[i]}\"")
        }
        result
      }
    }
    "ZINTERSTORE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.zinterstore(parts[1], [parts[2], parts[3]])}"] }
    }
    "ZDIFFSTORE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.zdiffstore(parts[1], [parts[2], parts[3]])}"] }
    }
    "ZRANGESTORE" => {
      if parts.length() < 5 { ["ERR wrong number of arguments"] }
      else {
        match (parse_int(parts[3]), parse_int(parts[4])) {
          (Some(sv), Some(ev)) => ["(integer) \{db.zrangestore(parts[1], parts[2], sv, ev)}"]
          _ => ["ERR value is not an integer"]
        }
      }
    }
    "ZSCAN" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(cursor) => {
            let r = db.zscan(parts[1], cursor, 10)
            let result = []
            for i = 0; i < r.1.length(); i = i + 1 {
              result.push("  \{i + 1}) \"\{r.1[i].0}\" \{r.1[i].1}")
            }
            result.push("  (next cursor: \{r.0})")
            result
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "ZREMRANGEBYRANK" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match (parse_int(parts[2]), parse_int(parts[3])) {
          (Some(sv), Some(ev)) => ["(integer) \{db.zremrangebyrank(parts[1], sv, ev)}"]
          _ => ["ERR value is not an integer"]
        }
      }
    }
    "ZREMRANGEBYSCORE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match (parse_float(parts[2]), parse_float(parts[3])) {
          (Some(lo), Some(hi)) => ["(integer) \{db.zremrangebyscore(parts[1], lo, hi)}"]
          _ => ["ERR value is not a valid float"]
        }
      }
    }
    "ZREVRANGEBYLEX" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        let items = db.zrevrangebylex(parts[1], parts[2], parts[3])
        let result = []
        for i = 0; i < items.length(); i = i + 1 {
          result.push("  \{i + 1}) \"\{items[i]}\"")
        }
        result
      }
    }
    "ZLEXCOUNT" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.zlexcount(parts[1], parts[2], parts[3])}"] }
    }
    "ZREMRANGEBYLEX" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{db.zremrangebylex(parts[1], parts[2], parts[3])}"] }
    }
    "ZMPOP" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        let keys = []
        let mut min = true
        let mut count = 1
        for i = 1; i < parts.length(); i = i + 1 {
          if parts[i] == "MIN" { min = true }
          else if parts[i] == "MAX" { min = false }
          else if parts[i] == "COUNT" && i + 1 < parts.length() {
            match parse_int(parts[i + 1]) {
              Some(c) => { count = c }
              None => ()
            }
            break
          } else {
            keys.push(parts[i])
          }
        }
        match db.zmpop(keys, min, count) {
          Some((k, vals)) => {
            let result = ["\"\{k}\""]
            for i = 0; i < vals.length(); i = i + 1 {
              result.push("  \{i + 1}) \"\{vals[i].0}\" \{vals[i].1}")
            }
            result
          }
          None => ["(nil)"]
        }
      }
    }
    "OBJECT_ENCODING" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.object_encoding(parts[1]) {
          Some(enc) => ["\"\{enc}\""]
          None => ["(nil)"]
        }
      }
    }
    "OBJECT_REFCOUNT" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.object_refcount(parts[1]) {
          Some(n) => ["(integer) \{n}"]
          None => ["(nil)"]
        }
      }
    }
    "OBJECT_IDLETIME" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.object_idletime(parts[1]) {
          Some(n) => ["(integer) \{n}"]
          None => ["(nil)"]
        }
      }
    }
    "OBJECT_FREQ" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.object_freq(parts[1]) {
          Some(n) => ["(integer) \{n}"]
          None => ["(nil)"]
        }
      }
    }
    "MEMORY_USAGE" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.memory_usage(parts[1]) {
          Some(b) => ["(integer) \{b}"]
          None => ["(nil)"]
        }
      }
    }
    "BITFIELD" => {
      if parts.length() < 5 { ["ERR wrong number of arguments"] }
      else {
        let op = parts[2].to_upper()
        match parse_int(parts[4]) {
          Some(offset) => {
            if op == "GET" {
              let signed = parts[3].has_prefix("i")
              match db.try_bitfield_get(parts[1], offset, 8, signed) {
                Ok(v) => ["(integer) \{v}"]
                Err(e) => [@lib.RedisError::message(e)]
              }
            } else if op == "SET" && parts.length() >= 6 {
              match parse_int(parts[5]) {
                Some(v) => {
                  match db.try_bitfield_set(parts[1], offset, 8, false, v) {
                    Ok(old) => ["(integer) \{old}"]
                    Err(e) => [@lib.RedisError::message(e)]
                  }
                }
                None => ["ERR value is not an integer"]
              }
            } else if op == "INCRBY" && parts.length() >= 6 {
              match parse_int(parts[5]) {
                Some(v) => {
                  match db.try_bitfield_incrby(parts[1], offset, 8, false, v) {
                    Ok(nv) => ["(integer) \{nv}"]
                    Err(e) => [@lib.RedisError::message(e)]
                  }
                }
                None => ["ERR value is not an integer"]
              }
            } else {
              ["ERR syntax error"]
            }
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "RESTORE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(ttl) => {
            let replace = parts.contains("REPLACE")
            match db.try_restore(parts[1], ttl.to_int64(), parts[3], replace, false) {
              Ok(_) => ["OK"]
              Err(e) => [@lib.RedisError::message(e)]
            }
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "WAIT" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { ["(integer) 0"] }
    }
    "SHUTDOWN" => { [db.shutdown()] }
    "CONFIG_RESETSTAT" => { [db.config_resetstat()] }
    "LOLWUT" => { [db.lolwut()] }
    "COMMAND_COUNT" => { ["(integer) \{db.command_count()}"] }
    "SAVE" => { [db.save()] }
    "BGSAVE" => { [db.bgsave()] }
    "LASTSAVE" => { ["(integer) \{db.lastsave().to_string()}"] }
    "GETDEL" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.try_getdel(parts[1]) {
          Ok(Some(v)) => ["\"\{v}\""]
          Ok(None) => ["(nil)"]
          Err(_) => ["(error) WRONGTYPE"]
        }
      }
    }
    "GETEX" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(ms) => {
            match db.getex(parts[1], Some(ms.to_int64())) {
              Some(v) => ["\"\{v}\""]
              None => ["(nil)"]
            }
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "SETNX" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{if db.setnx(parts[1], parts[2]) { 1 } else { 0 }}"] }
    }
    "SETEX" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(sec) => {
            if db.setex(parts[1], sec, parts[3]) {
              ["OK"]
            } else {
              ["(error)"]
            }
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "HINCRBY" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[3]) {
          Some(n) => {
            match db.hincrby(parts[1], parts[2], n.to_int64()) {
              Some(v) => ["(integer) \{v.to_string()}"]
              None => ["(error) not an integer"]
            }
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "LINSERT" => {
      if parts.length() < 5 { ["ERR wrong number of arguments"] }
      else {
        let before = parts[2].to_upper() == "BEFORE"
        ["(integer) \{db.linsert(parts[1], before, parts[3], parts[4])}"]
      }
    }
    "LMOVE" => {
      if parts.length() < 5 { ["ERR wrong number of arguments"] }
      else {
        let src_left = parts[3].to_upper() == "LEFT"
        let dst_left = parts[4].to_upper() == "LEFT"
        match db.lmove(parts[1], parts[2], src_left, dst_left) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "SMOVE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else { ["(integer) \{if db.smove(parts[1], parts[2], parts[3]) { 1 } else { 0 }}"] }
    }
    "SPOP" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.spop(parts[1]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "ZPOPMIN" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match db.zpopmin(parts[1]) {
          Some((m, s)) => ["1) \"\{m}\"", "2) \"\{s}\""]
          None => ["(empty array)"]
        }
      }
    }
    "ZUNIONSTORE" => {
      if parts.length() < 4 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(n) => {
            let keys = Array::make(n, "")
            for i = 0; i < n; i = i + 1 {
              keys[i] = parts[3 + i]
            }
            ["(integer) \{db.zunionstore(parts[1], keys)}"]
          }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "SORT" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        let alpha = parts.contains("ALPHA")
        let desc = parts.contains("DESC")
        let items = db.sort(parts[1], alpha, desc, 0, 0, None)
        let result = []
        for i = 0; i < items.length(); i = i + 1 {
          result.push("  \{i + 1}) \"\{items[i]}\"")
        }
        result
      }
    }
    "AUTH" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { [@lib.Database::auth(db, parts[1])] }
    }
    "CLIENT" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else { [@lib.Database::client_list(db, parts[1])] }
    }
    "CONFIG" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        let pair = @lib.Database::config_get(db, parts[2])
        ["  \{pair[0]} = \"\{pair[1]}\""]
      }
    }
    "SLOWLOG" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        let entries = @lib.Database::slowlog(db, parts[1])
        if entries.length() == 0 { ["(empty)"] } else { entries }
      }
    }
    "SELECT" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[1]) {
          Some(idx) => [@lib.Database::select(db, idx)]
          None => ["ERR value is not an integer"]
        }
      }
    }
    "MOVE" => {
      if parts.length() < 3 { ["ERR wrong number of arguments"] }
      else {
        match parse_int(parts[2]) {
          Some(idx) => { ["(integer) \{@lib.Database::move_key(db, parts[1], idx)}"] }
          None => ["ERR value is not an integer"]
        }
      }
    }
    "DUMP" => {
      if parts.length() < 2 { ["ERR wrong number of arguments"] }
      else {
        match @lib.Database::dump(db, parts[1]) {
          Some(v) => ["\"\{v}\""]
          None => ["(nil)"]
        }
      }
    }
    "MONITOR" => { [@lib.Database::monitor(db)] }
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
