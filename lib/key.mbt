pub fn Database::expire(self : Database, key : String, seconds : Int) -> Bool {
  if self.check_expired(key) {
    return false
  }
  if self.data.contains(key) {
    match checked_add_i64(self.current_time, seconds.to_int64() * 1000) {
      Some(expire_time) => {
        self.set_expire(key, expire_time)
        true
      }
      None => false
    }
  } else {
    false
  }
}

pub fn Database::pexpire(self : Database, key : String, milliseconds : Int64) -> Bool {
  if self.check_expired(key) {
    return false
  }
  if self.data.contains(key) {
    match checked_add_i64(self.current_time, milliseconds) {
      Some(expire_time) => {
        self.set_expire(key, expire_time)
        true
      }
      None => false
    }
  } else {
    false
  }
}

pub fn Database::expireat(self : Database, key : String, unix_seconds : Int64) -> Bool {
  if self.check_expired(key) {
    return false
  }
  let max_seconds = I64_MAX / 1000
  let min_seconds = I64_MIN / 1000
  if unix_seconds > max_seconds || unix_seconds < min_seconds {
    return false
  }
  if self.data.contains(key) {
    self.set_expire(key, unix_seconds * 1000)
    true
  } else {
    false
  }
}

pub fn Database::pexpireat(self : Database, key : String, unix_milliseconds : Int64) -> Bool {
  if self.check_expired(key) {
    return false
  }
  if self.data.contains(key) {
    self.set_expire(key, unix_milliseconds)
    true
  } else {
    false
  }
}

fn Database::set_expire_with_options(
  self : Database,
  key : String,
  expire_time : Int64,
  nx : Bool,
  xx : Bool,
  gt : Bool,
  lt : Bool,
) -> Bool {
  if (nx && (xx || gt || lt)) || (gt && lt) {
    return false
  }
  if self.check_expired(key) || !self.data.contains(key) {
    return false
  }
  match self.expires.get(key) {
    Some(current) => {
      if nx {
        return false
      }
      if gt && expire_time <= current {
        return false
      }
      if lt && expire_time >= current {
        return false
      }
    }
    None => {
      if nx {
        self.set_expire(key, expire_time)
        return true
      }
      if xx || gt {
        return false
      }
    }
  }
  self.set_expire(key, expire_time)
  true
}

pub fn Database::expire_with_options(
  self : Database,
  key : String,
  seconds : Int,
  nx : Bool,
  xx : Bool,
  gt : Bool,
  lt : Bool,
) -> Bool {
  match checked_add_i64(self.current_time, seconds.to_int64() * 1000) {
    Some(expire_time) =>
      self.set_expire_with_options(key, expire_time, nx, xx, gt, lt)
    None => false
  }
}

pub fn Database::pexpire_with_options(
  self : Database,
  key : String,
  milliseconds : Int64,
  nx : Bool,
  xx : Bool,
  gt : Bool,
  lt : Bool,
) -> Bool {
  match checked_add_i64(self.current_time, milliseconds) {
    Some(expire_time) =>
      self.set_expire_with_options(key, expire_time, nx, xx, gt, lt)
    None => false
  }
}

pub fn Database::ttl(self : Database, key : String) -> Int64 {
  if !self.data.contains(key) {
    -2
  } else if self.expires.contains(key) {
    let remaining = self.expires.get(key).unwrap() - self.current_time
    if remaining <= 0 {
      self.delete_key(key)
      -2
    } else {
      (remaining + 500) / 1000
    }
  } else {
    -1
  }
}

pub fn Database::pttl(self : Database, key : String) -> Int64 {
  if !self.data.contains(key) {
    -2
  } else if self.expires.contains(key) {
    let remaining = self.expires.get(key).unwrap() - self.current_time
    if remaining <= 0 {
      self.delete_key(key)
      -2
    } else {
      remaining
    }
  } else {
    -1
  }
}

pub fn Database::persist(self : Database, key : String) -> Bool {
  if self.check_expired(key) {
    return false
  }
  if self.data.contains(key) && self.expires.contains(key) {
    self.expires.remove(key)
    self.maybe_compact_expire_heap()
    true
  } else {
    false
  }
}

pub fn Database::del(self : Database, key : String) -> Bool {
  if self.check_expired(key) {
    false
  } else {
    let existed = self.data.contains(key)
    if existed {
      self.delete_key(key)
    }
    existed
  }
}

pub fn Database::exists(self : Database, key : String) -> Bool {
  if self.check_expired(key) {
    false
  } else {
    self.data.contains(key)
  }
}

pub fn Database::keys(self : Database) -> Array[String] {
  self.collect_expired()
  let result = []
  for key, _ in self.data {
    result.push(key)
  }
  result
}

fn Database::sorted_keys_snapshot(self : Database) -> Array[String] {
  match self.scan_snapshot {
    Some(keys) => keys
    None => {
      self.collect_expired()
      let keys : Array[String] = []
      for key, _ in self.data {
        keys.push(key)
      }
      keys.sort_by((a, b) => a.lexical_compare(b))
      self.scan_snapshot = Some(keys)
      keys
    }
  }
}

fn literal_prefix(pattern : String) -> String {
  let mut index = 0
  while index < pattern.length() && pattern[index] != '*' && pattern[index] != '?' {
    index = index + 1
  }
  pattern[0:index].to_owned()
}

fn lower_bound_keys(keys : Array[String], target : String) -> Int {
  let mut lo = 0
  let mut hi = keys.length()
  while lo < hi {
    let mid = (lo + hi) / 2
    if keys[mid].lexical_compare(target) < 0 {
      lo = mid + 1
    } else {
      hi = mid
    }
  }
  lo
}

pub fn Database::keys_pattern(self : Database, pattern : String) -> Array[String] {
  let prefix = literal_prefix(pattern)
  if prefix.length() == 0 {
    self.collect_expired()
    let result = []
    for key, _ in self.data {
      if match_pattern(pattern, key) {
        result.push(key)
      }
    }
    result
  } else {
    let keys = self.sorted_keys_snapshot()
    let start = lower_bound_keys(keys, prefix)
    let result = []
    let mut index = start
    while index < keys.length() && keys[index].has_prefix(prefix) {
      if match_pattern(pattern, keys[index]) {
        result.push(keys[index])
      }
      index = index + 1
    }
    result
  }
}

pub fn Database::type_of(self : Database, key : String) -> String {
  if self.check_expired(key) {
    "none"
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(_)) => "string"
      Some(RedisValue::Hash(_)) => "hash"
      Some(RedisValue::List(_)) => "list"
      Some(RedisValue::Set(_)) => "set"
      Some(RedisValue::ZSet(_)) => "zset"
      None => "none"
    }
  }
}

pub fn Database::randomkey(self : Database) -> String? {
  self.collect_expired()
  let mut chosen : String? = None
  let mut seen = 0
  for key, _ in self.data {
    seen = seen + 1
    if self.next_rand() % seen == 0 {
      chosen = Some(key)
    }
  }
  chosen
}

pub fn Database::rename(self : Database, old_key : String, new_key : String) -> Bool {
  if self.check_expired(old_key) {
    return false
  }
  match self.data.get(old_key) {
    Some(value) => {
      if old_key == new_key {
        return true
      }
      self.put(new_key, value)
      self.data.remove(old_key)
      self.invalidate_zset_cache(old_key)
      self.hscan_snapshots.remove(old_key)
      self.sscan_snapshots.remove(old_key)
      match self.expires.get(old_key) {
        Some(expire_time) => {
          self.set_expire(new_key, expire_time)
          self.expires.remove(old_key)
        }
        None => self.expires.remove(new_key)
      }
      self.maybe_compact_expire_heap()
      true
    }
    None => false
  }
}

pub fn Database::renamenx(self : Database, old_key : String, new_key : String) -> Bool {
  if self.check_expired(old_key) {
    return false
  }
  self.check_expired(new_key) |> ignore
  if self.data.contains(new_key) {
    return false
  }
  self.rename(old_key, new_key)
}

pub fn Database::scan(self : Database, cursor : Int, count : Int) -> (Int, Array[String]) {
  let all_keys = if cursor == 0 {
    self.collect_expired()
    let keys : Array[String] = []
    for key, _ in self.data {
      keys.push(key)
    }
    keys.sort_by((a, b) => a.lexical_compare(b))
    self.scan_snapshot = Some(keys)
    keys
  } else {
    match self.scan_snapshot {
      Some(keys) => keys
      None => {
        self.collect_expired()
        let keys : Array[String] = []
        for key, _ in self.data {
          keys.push(key)
        }
        keys.sort_by((a, b) => a.lexical_compare(b))
        self.scan_snapshot = Some(keys)
        keys
      }
    }
  }
  let len = all_keys.length()
  let limit = if count <= 0 { 10 } else { count }
  let start = if cursor < 0 { 0 } else { cursor }
  if start >= len {
    self.scan_snapshot = None
    return (0, [])
  }
  let end = if start + limit > len { len } else { start + limit }
  let result : Array[String] = []
  for i = start; i < end; i = i + 1 {
    result.push(all_keys[i])
  }
  let next = if end >= len { 0 } else { end }
  if next == 0 {
    self.scan_snapshot = None
  }
  (next, result)
}

fn clone_redis_value(value : RedisValue) -> RedisValue {
  match value {
    RedisValue::String(s) => RedisValue::String(s)
    RedisValue::Hash(h) => {
      let copy = Map([], capacity=h.length())
      for field, item in h {
        copy.set(field, item)
      }
      RedisValue::Hash(copy)
    }
    RedisValue::List(d) => {
      let copy = Deque::new()
      for item in d.to_array() {
        copy.push_back(item)
      }
      RedisValue::List(copy)
    }
    RedisValue::Set(s) => {
      let copy = Map([], capacity=s.length())
      for member_val, flag in s {
        copy.set(member_val, flag)
      }
      RedisValue::Set(copy)
    }
    RedisValue::ZSet(z) => {
      let copy = Map([], capacity=z.length())
      for member_val, score in z {
        copy.set(member_val, score)
      }
      RedisValue::ZSet(copy)
    }
  }
}

pub fn Database::copy_with_replace(
  self : Database,
  source : String,
  destination : String,
  replace : Bool,
) -> Bool {
  if source == destination {
    return false
  }
  if self.check_expired(source) {
    return false
  }
  self.check_expired(destination) |> ignore
  if !replace && self.data.contains(destination) {
    return false
  }
  match self.data.get(source) {
    Some(value) => {
      if replace {
        self.expires.remove(destination)
      }
      self.put(destination, clone_redis_value(value))
      true
    }
    None => false
  }
}

pub fn Database::copy(self : Database, source : String, destination : String) -> Bool {
  self.copy_with_replace(source, destination, false)
}

pub fn Database::scan_match(
  self : Database,
  cursor : Int,
  count : Int,
  pattern : String,
) -> (Int, Array[String]) {
  let (next, keys) = self.scan(cursor, count)
  let result : Array[String] = []
  for key in keys {
    if match_pattern(pattern, key) {
      result.push(key)
    }
  }
  (next, result)
}

pub fn Database::scan_type(
  self : Database,
  cursor : Int,
  count : Int,
  type_name : String,
) -> (Int, Array[String]) {
  let (next, keys) = self.scan(cursor, count)
  let result : Array[String] = []
  for key in keys {
    if self.type_of(key) == type_name {
      result.push(key)
    }
  }
  (next, result)
}

pub fn Database::clear_scan_caches(self : Database) -> Unit {
  self.scan_snapshot = None
  self.hscan_snapshots = Map([], capacity=10)
  self.sscan_snapshots = Map([], capacity=10)
}

pub fn Database::touch(self : Database, keys : Array[String]) -> Int {
  let mut count = 0
  for key in keys {
    if !self.check_expired(key) && self.data.contains(key) {
      count = count + 1
    }
  }
  count
}

pub fn Database::unlink(self : Database, key : String) -> Bool {
  self.del(key)
}

