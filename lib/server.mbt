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
    "PING", "ECHO", "INFO", "TIME", "COMMAND", "OBJECT_ENCODING",
    "AUTH", "CLIENT", "CONFIG", "SLOWLOG",
    "SELECT", "MOVE", "DUMP", "RESTORE", "MONITOR",
    "OBJECT_FREQ"
  ]
}

pub fn Database::object_encoding(self : Database, key : String) -> String? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(s)) => if s.length() > 44 { Some("raw") } else { Some("embstr") }
      Some(RedisValue::Hash(h)) => if h.length() > 512 { Some("hashtable") } else { Some("listpack") }
      Some(RedisValue::List(d)) => if d.length() > 128 { Some("quicklist") } else { Some("listpack") }
      Some(RedisValue::Set(s)) => if s.length() > 128 { Some("hashtable") } else { Some("listpack") }
      Some(RedisValue::ZSet(z)) => if z.length() > 128 { Some("skiplist") } else { Some("listpack") }
      None => None
    }
  }
}

pub fn Database::object_refcount(self : Database, key : String) -> Int? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(_)) => Some(1)
      Some(RedisValue::Hash(h)) => Some(h.length())
      Some(RedisValue::List(d)) => Some(d.length())
      Some(RedisValue::Set(s)) => Some(s.length())
      Some(RedisValue::ZSet(z)) => Some(z.length())
      None => None
    }
  }
}

pub fn Database::auth(_ : Database, _password : String) -> String {
  "OK"
}

pub fn Database::client_list(_ : Database) -> String {
  "id=1 addr=127.0.0.1:12345 fd=1 name= age=0 flags=N db=0 sub=0 psub=0 multi=-1 qbuf=0 qbuf-free=0 obl=0 oll=0 omem=0 events=r cmd=client"
}

pub fn Database::config_get(_ : Database, _parameter : String) -> String {
  "0"
}

pub fn Database::slowlog(_ : Database, _subcommand : String) -> Array[String] {
  []
}

pub fn Database::object_idletime(self : Database, key : String) -> Int? {
  if self.check_expired(key) {
    None
  } else {
    match self.expires.get(key) {
      Some(expire_time) => {
        let remaining = expire_time - self.current_time
        if remaining > 0 {
          Some((remaining / 1000).to_int())
        } else {
          Some(0)
        }
      }
      None => Some(0)
    }
  }
}

pub fn Database::object_freq(self : Database, key : String) -> Int? {
  if self.check_expired(key) {
    None
  } else if self.data.contains(key) {
    Some(0)
  } else {
    None
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

pub fn Database::select(_ : Database, _index : Int) -> String {
  "OK"
}

pub fn Database::move_key(self : Database, key : String, db_index : Int) -> Bool {
  if self.check_expired(key) {
    false
  } else if db_index == 0 {
    self.data.contains(key)
  } else {
    false
  }
}

pub fn Database::dump(self : Database, key : String) -> String? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(s)) => Some(s)
      _ => None
    }
  }
}

pub fn Database::restore(self : Database, key : String, ttl_ms : Int64, value : String) -> Bool {
  self.put(key, RedisValue::String(value))
  if ttl_ms > 0 {
    match checked_add_i64(self.current_time, ttl_ms) {
      Some(expire_time) => self.set_expire(key, expire_time)
      None => ()
    }
  }
  true
}

pub fn Database::bitcount(self : Database, key : String) -> Int {
  if self.check_expired(key) {
    0
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(s)) => {
        let mut count = 0
        for i = 0; i < s.length(); i = i + 1 {
          let c = s[i]
          let mut b = 0
          while b < 8 {
            if (c.to_int() & (1 << b)) != 0 {
              count = count + 1
            }
            b = b + 1
          }
        }
        count
      }
      _ => 0
    }
  }
}


