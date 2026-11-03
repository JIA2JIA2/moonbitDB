pub fn Database::expire(self : Database, key : String, seconds : Int) -> Bool {
  if self.check_expired(key) {
    return false
  }
  if self.data.contains(key) {
    self.expires.set(key, self.current_time + seconds.to_int64() * 1000)
    true
  } else {
    false
  }
}

pub fn Database::pexpire(self : Database, key : String, milliseconds : Int64) -> Bool {
  if self.check_expired(key) {
    return false
  }
  if self.data.contains(key) {
    self.expires.set(key, self.current_time + milliseconds)
    true
  } else {
    false
  }
}

pub fn Database::expireat(self : Database, key : String, unix_seconds : Int64) -> Bool {
  if self.check_expired(key) {
    return false
  }
  if self.data.contains(key) {
    self.expires.set(key, unix_seconds * 1000)
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
    self.expires.set(key, unix_milliseconds)
    true
  } else {
    false
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
      (remaining + 999) / 1000
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

pub fn Database::keys_pattern(self : Database, pattern : String) -> Array[String] {
  self.collect_expired()
  let result = []
  for key, _ in self.data {
    if match_pattern(pattern, key) {
      result.push(key)
    }
  }
  result
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
  let all_keys : Array[String] = []
  for key, _ in self.data {
    all_keys.push(key)
  }
  if all_keys.length() == 0 {
    None
  } else {
    Some(all_keys[self.next_rand() % all_keys.length()])
  }
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
      self.zset_sorted_cache.remove(old_key)
      self.zset_sorted_cache.remove(new_key)
      match self.expires.get(old_key) {
        Some(expire_time) => {
          self.expires.set(new_key, expire_time)
          self.expires.remove(old_key)
        }
        None => self.expires.remove(new_key)
      }
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

