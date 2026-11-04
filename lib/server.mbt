pub fn Database::dbsize(self : Database) -> Int {
  self.collect_expired()
  self.data.length()
}

pub fn Database::flushdb(self : Database) -> String {
  self.data = Map([], capacity=1000)
  self.expires = Map([], capacity=1000)
  self.zset_sorted_cache = Map([], capacity=100)
  self.zset_rank_cache = Map([], capacity=100)
  self.zset_lex_cache = Map([], capacity=100)
  self.scan_snapshot = None
  self.hscan_snapshots = Map([], capacity=10)
  self.sscan_snapshots = Map([], capacity=10)
  self.zscan_snapshots = Map([], capacity=10)
  self.expire_heap = []
  "OK"
}

pub fn Database::flushall(self : Database) -> String {
  self.flushdb()
}

pub fn Database::ping() -> String {
  "PONG"
}

pub fn Database::echo(_ : Database, message : String) -> String {
  message
}

fn Database::info_server_section(self : Database) -> Array[String] {
  [
    "# Server",
    "moonbit_db_version:0.1.4",
    "uptime_in_ms:\{self.current_time - self.start_time}"
  ]
}

fn Database::info_memory_section(self : Database) -> Array[String] {
  let mut value_bytes : Int = 0
  for key, value in self.data {
    value_bytes = value_bytes + key.length() * 2
    match value {
      RedisValue::String(s) => value_bytes = value_bytes + s.length() * 2
      RedisValue::Hash(h) =>
        for f, v in h {
          value_bytes = value_bytes + (f.length() + v.length()) * 2
        }
      RedisValue::Set(s) =>
        for m, _ in s {
          value_bytes = value_bytes + m.length() * 2
        }
      RedisValue::ZSet(z) =>
        for m, _ in z {
          value_bytes = value_bytes + m.length() * 2 + 8
        }
      RedisValue::List(d) => value_bytes = value_bytes + d.length() * 16
    }
  }
  [
    "# Memory",
    "used_memory:\{value_bytes}",
    "keys:\{self.data.length()}",
    "expires:\{self.expires.length()}"
  ]
}

fn Database::info_keyspace_section(self : Database) -> Array[String] {
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
  ["# Keyspace", "db0:keys=\{key_count},expires=\{expire_count},avg_ttl=\{avg_ttl}"]
}

fn Database::info_stats_section(self : Database) -> Array[String] {
  let mut zset_cached = 0
  for _key, _items in self.zset_sorted_cache {
    zset_cached = zset_cached + 1
  }
  ["# Stats", "zset_sorted_cache_entries:\{zset_cached}", "expire_heap_size:\{self.expire_heap.length()}"]
}

pub fn Database::info_section(self : Database, section : String) -> String {
  self.collect_expired()
  let cmd = section.to_lower()
  let sections : Array[String] = if cmd == "server" {
    self.info_server_section()
  } else if cmd == "memory" {
    self.info_memory_section()
  } else if cmd == "keyspace" {
    self.info_keyspace_section()
  } else if cmd == "stats" {
    self.info_stats_section()
  } else {
    []
  }
  sections.join("\n")
}

pub fn Database::info(self : Database) -> String {
  self.collect_expired()
  let parts : Array[Array[String]] = [
    self.info_server_section(),
    self.info_memory_section(),
    self.info_keyspace_section(),
    self.info_stats_section()
  ]
  let joined : Array[String] = []
  for part in parts {
    for line in part {
      joined.push(line)
    }
    joined.push("")
  }
  if joined.length() > 0 {
    ignore(joined.remove(joined.length() - 1))
  }
  joined.join("\n")
}

pub fn Database::time(self : Database) -> (Int64, Int64) {
  let seconds = self.current_time / 1000
  let microseconds = (self.current_time % 1000) * 1000
  (seconds, microseconds)
}

pub fn Database::command() -> Array[String] {
  [
    "SET", "GET", "DEL", "EXISTS", "KEYS", "KEYS_PATTERN", "SCAN", "TYPE", "APPEND", "STRLEN",
    "INCR", "DECR", "INCRBY", "DECRBY", "SETNX", "SETEX", "PSETEX", "GETDEL", "GETSET", "GETEX",
    "GETRANGE", "SUBSTR", "SETRANGE", "GETBIT", "SETBIT", "BITPOS", "BITCOUNT", "BITOP",
    "EXPIRE", "PEXPIRE", "EXPIREAT", "PEXPIREAT", "EXPIRETIME", "PEXPIRETIME", "TTL", "PTTL", "PERSIST",
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
    "MULTI", "EXEC", "DISCARD", "WATCH", "UNWATCH",
    "LMPOP", "ZMPOP",
    "PING", "ECHO", "INFO", "TIME", "COMMAND", "COMMAND_INFO", "COMMAND_COUNT", "OBJECT_ENCODING", "OBJECT_REFCOUNT", "OBJECT_IDLETIME",
    "AUTH", "CLIENT", "CONFIG", "SLOWLOG",
    "SELECT", "MOVE", "DUMP", "RESTORE", "MONITOR",
    "OBJECT_FREQ",
    "SAVE", "BGSAVE", "LASTSAVE", "RESET"
  ]
}

pub fn Database::object_encoding(self : Database, key : String) -> String? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(s)) => if s.length() > EMBSTR_MAX_SIZE { Some("raw") } else { Some("embstr") }
      Some(RedisValue::Hash(h)) => if h.length() > HASHTABLE_MAX_ENTRIES { Some("hashtable") } else { Some("listpack") }
      Some(RedisValue::List(d)) => if d.length() > LISTPACK_MAX_ENTRIES { Some("quicklist") } else { Some("listpack") }
      Some(RedisValue::Set(s)) => if s.length() > LISTPACK_MAX_ENTRIES { Some("hashtable") } else { Some("listpack") }
      Some(RedisValue::ZSet(z)) => if z.length() > LISTPACK_MAX_ENTRIES { Some("skiplist") } else { Some("listpack") }
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

pub fn Database::client_list(_self : Database, subcommand : String) -> String {
  let cmd = subcommand.to_upper()
  if cmd == "ID" {
    "1"
  } else if cmd == "GETNAME" {
    ""
  } else {
    "id=1 addr=127.0.0.1:12345 fd=1 name= age=0 flags=N db=0 sub=0 psub=0 multi=-1 qbuf=0 qbuf-free=0 obl=0 oll=0 omem=0 events=r cmd=client"
  }
}

pub fn Database::config_get(_self : Database, parameter : String) -> Array[String] {
  let param = parameter.to_lower()
  let value = if param == "maxmemory" {
    "0"
  } else if param == "databases" {
    "16"
  } else if param == "appendonly" {
    "no"
  } else if param == "save" {
    ""
  } else if param == "timeout" {
    "0"
  } else {
    ""
  }
  [param, value]
}

pub fn Database::slowlog(_self : Database, subcommand : String) -> Array[String] {
  let cmd = subcommand.to_upper()
  if cmd == "LEN" {
    ["0"]
  } else if cmd == "RESET" {
    ["OK"]
  } else {
    []
  }
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

pub fn Database::try_exec(self : Database) -> Result[String, RedisError] {
  if !self.in_transaction {
    return Err(RedisError::InvalidArgument)
  }
  if self.watched_keys_conflict() {
    ignore(self.clear_transaction())
    self.unwatch()
    return Err(RedisError::TransactionAborted)
  }
  if self.run_transaction() {
    self.unwatch()
    Ok("OK")
  } else {
    Err(RedisError::InvalidArgument)
  }
}

pub fn Database::exec(self : Database) -> String {
  if !self.in_transaction {
    "ERR EXEC without MULTI"
  } else {
    match self.try_exec() {
      Ok(msg) => msg
      Err(RedisError::TransactionAborted) => "ERR EXEC aborted (watched key modified)"
      Err(_) => "ERR EXEC failed"
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

pub fn Database::monitor(_ : Database) -> String {
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

pub fn Database::try_bitcount_range(self : Database, key : String, start : Int, end : Int) -> Result[Int, RedisError] {
  if self.check_expired(key) {
    return Ok(0)
  }
  match self.data.get(key) {
    Some(RedisValue::String(s)) => {
      let len = s.length()
      let s_idx = if start < 0 { len + start } else { start }
      let e_idx = if end < 0 { len + end + 1 } else { end + 1 }
      let from = if s_idx < 0 { 0 } else { s_idx }
      let to = if e_idx > len { len } else { e_idx }
      let mut count = 0
      let mut i = from
      while i < to {
        let unit = s[i].to_int()
        let mut b = 0
        while b < 16 {
          if (unit & (1 << b)) != 0 {
            count = count + 1
          }
          b = b + 1
        }
        i = i + 1
      }
      Ok(count)
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok(0)
  }
}

pub fn Database::bitcount_range(self : Database, key : String, start : Int, end : Int) -> Int {
  match self.try_bitcount_range(key, start, end) {
    Ok(count) => count
    Err(_) => -1
  }
}

pub fn Database::bitcount(self : Database, key : String) -> Int {
  match self.try_bitcount_range(key, 0, -1) {
    Ok(count) => count
    Err(_) => 0
  }
}

pub fn Database::save(_self : Database) -> String {
  "OK"
}

pub fn Database::bgsave(_self : Database) -> String {
  "Background saving started"
}

pub fn Database::lastsave(self : Database) -> Int64 {
  self.current_time / 1000
}

pub fn Database::command_count(_self : Database) -> Int {
  Database::command().length()
}

pub fn Database::reset(self : Database) -> String {
  self.unwatch()
  ignore(self.clear_transaction())
  "RESET"
}


