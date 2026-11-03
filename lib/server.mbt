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
  for _, _ in self.expires {
    expire_count = expire_count + 1
  }
  let info_arr : Array[String] = [
    "# Server",
    "moonbit_db_version:0.1.3",
    "uptime_in_ms:\{self.current_time}",
    "",
    "# Keyspace",
    "db0:keys=\{key_count},expires=\{expire_count},avg_ttl=0"
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
    "HINCRBY", "HSTRLEN", "HMSET", "HMGET", "HSCAN",
    "LPUSH", "RPUSH", "LPOP", "RPOP", "LLEN", "LRANGE", "LINDEX", "LSET", "LREM", "LINSERT",
    "LPUSHX", "RPUSHX", "RPOPLPUSH", "LMOVE", "LTRIM",
    "SADD", "SMEMBERS", "SREM", "SCARD", "SISMEMBER", "SMISMEMBER",
    "SINTER", "SINTERCARD", "SINTERSTORE", "SUNION", "SUNIONSTORE", "SDIFF", "SDIFFSTORE", "SMOVE", "SPOP",
    "SRANDMEMBER", "SSCAN",
    "ZADD", "ZRANGE", "ZCARD", "ZSCORE", "ZREM",
    "ZRANGEBYSCORE", "ZRANGEBYLEX", "ZREVRANGEBYLEX", "ZLEXCOUNT", "ZCOUNT", "ZREVRANGE", "ZREVRANGEBYSCORE",
    "ZRANK", "ZREVRANK", "ZINCRBY", "ZREMRANGEBYRANK", "ZREMRANGEBYSCORE", "ZREMRANGEBYLEX",
    "ZPOPMIN", "ZPOPMAX", "ZMSCORE", "ZRANDMEMBER", "ZDIFF", "ZUNION", "ZINTER", "ZSCAN",
    "DBSIZE", "FLUSHDB", "FLUSHALL", "RANDOMKEY", "RENAME", "RENAMENX", "COPY", "TOUCH", "UNLINK",
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

