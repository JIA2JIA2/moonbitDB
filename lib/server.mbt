pub fn Database::dbsize(self : Database) -> Int {
  self.collect_expired()
  self.data.length()
}

pub fn Database::flushdb(self : Database) -> Unit {
  self.data = Map([], capacity=1000)
  self.expires = Map([], capacity=1000)
  self.zset_sorted_cache = Map([], capacity=100)
  self.zset_rank_cache = Map([], capacity=100)
  self.zset_lex_cache = Map([], capacity=100)
  self.scan_snapshot = None
  self.hscan_snapshots = Map([], capacity=10)
  self.sscan_snapshots = Map([], capacity=10)
  self.expire_heap = []
}

pub fn Database::flushall(self : Database) -> Unit {
  self.flushdb()
}

pub fn Database::ping() -> String {
  "PONG"
}

pub fn Database::echo(_ : Database, message : String) -> String {
  message
}

pub fn Database::info(self : Database) -> String {
  self.collect_expired()
  let key_count = self.data.length()
  let mut expire_count = 0
  let mut total_ttl : Int64 = 0
  for _, expire_time in self.expires {
    expire_count = expire_count + 1
    let remaining = expire_time - self.current_time
    if remaining > 0 {
      total_ttl = total_ttl + remaining
    }
  }
  let avg_ttl : Int64 = if expire_count == 0 {
    0
  } else {
    total_ttl / expire_count.to_int64()
  }
  let info_arr : Array[String] = [
    "# Server",
    "moonbit_db_version:0.2.0",
    "uptime_in_ms:\{self.current_time}",
    "",
    "# Keyspace",
    "db0:keys=\{key_count},expires=\{expire_count},avg_ttl=\{avg_ttl}"
  ]
  info_arr.join("\n")
}

pub fn Database::time(self : Database) -> (Int64, Int64) {
  let seconds = self.current_time / 1000
  let microseconds = (self.current_time % 1000) * 1000
  (seconds, microseconds)
}

pub fn Database::command() -> Array[String] {
  [
    "SET", "GET", "DEL", "EXISTS", "KEYS", "KEYS_PATTERN", "SCAN", "TYPE", "APPEND", "STRLEN",
    "INCR", "DECR", "INCRBY", "DECRBY", "SETNX", "SETEX", "GETDEL", "GETSET", "GETEX",
    "GETRANGE", "SETRANGE",
    "EXPIRE", "PEXPIRE", "EXPIREAT", "PEXPIREAT", "TTL", "PTTL", "PERSIST",
    "MSET", "MSETNX", "MGET", "MDEL",
    "HSET", "HSETNX", "HGET", "HDEL", "HGETALL", "HLEN", "HEXISTS", "HKEYS", "HVALS",
    "HINCRBY", "HSTRLEN", "HMSET", "HMGET", "HSCAN", "HRANDFIELD",
    "LPUSH", "RPUSH", "LPOP", "RPOP", "LLEN", "LRANGE", "LINDEX", "LSET", "LREM", "LINSERT",
    "LPUSHX", "RPUSHX", "RPOPLPUSH", "LMOVE", "LPOS", "LTRIM",
    "SADD", "SMEMBERS", "SREM", "SCARD", "SISMEMBER", "SMISMEMBER",
    "SINTER", "SINTERCARD", "SINTERSTORE", "SUNION", "SUNIONSTORE", "SDIFF", "SDIFFSTORE", "SMOVE", "SPOP",
    "SRANDMEMBER", "SSCAN",
    "ZADD", "ZRANGE", "ZCARD", "ZSCORE", "ZREM",
    "ZRANGEBYSCORE", "ZRANGEBYLEX", "ZREVRANGEBYLEX", "ZLEXCOUNT", "ZCOUNT", "ZREVRANGE", "ZREVRANGEBYSCORE",
    "ZRANK", "ZREVRANK", "ZINCRBY", "ZREMRANGEBYRANK", "ZREMRANGEBYSCORE", "ZREMRANGEBYLEX",
    "ZPOPMIN", "ZPOPMAX", "ZMSCORE", "ZRANDMEMBER", "ZDIFF", "ZDIFFSTORE", "ZUNION", "ZUNIONSTORE", "ZINTER", "ZINTERSTORE", "ZINTERCARD", "ZRANGESTORE", "ZSCAN",
    "DBSIZE", "FLUSHDB", "FLUSHALL", "RANDOMKEY", "RENAME", "RENAMENX", "COPY", "TOUCH", "UNLINK",
    "MULTI", "EXEC", "DISCARD",
    "LMPOP", "ZMPOP",
    "PING", "ECHO", "INFO", "TIME", "COMMAND", "OBJECT_ENCODING"
  ]
}

pub fn Database::object_encoding(self : Database, key : String) -> String? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(_)) => Some("embstr")
      Some(RedisValue::Hash(h)) => if h.length() > 512 { Some("hashtable") } else { Some("listpack") }
      Some(RedisValue::List(d)) => if d.length() > 128 { Some("quicklist") } else { Some("listpack") }
      Some(RedisValue::Set(s)) => if s.length() > 128 { Some("hashtable") } else { Some("listpack") }
      Some(RedisValue::ZSet(z)) => if z.length() > 128 { Some("skiplist") } else { Some("listpack") }
      None => None
    }
  }
}

pub fn Database::multi(self : Database) -> String {
  if self.in_transaction {
    "ERR MULTI calls can not be nested"
  } else {
    self.begin_transaction()
    "OK"
  }
}

pub fn Database::exec(self : Database) -> String {
  if !self.in_transaction {
    "ERR EXEC without MULTI"
  } else {
    if self.run_transaction() {
      "OK"
    } else {
      "ERR EXEC failed"
    }
  }
}

pub fn Database::discard(self : Database) -> String {
  if !self.in_transaction {
    "ERR DISCARD without MULTI"
  } else {
    if self.clear_transaction() {
      "OK"
    } else {
      "ERR DISCARD failed"
    }
  }
}


pub fn Database::command_info(_ : Database, commands : Array[String]) -> Array[String] {
  let all_commands = Database::command()
  let result : Array[String] = []
  for cmd in commands {
    let mut found = false
    for all_cmd in all_commands {
      if all_cmd == cmd {
        result.push(cmd)
        found = true
        break
      }
    }
    if !found {
      result.push("")
    }
  }
  result
}
