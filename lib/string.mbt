pub fn Database::set(self : Database, key : String, value : String) -> Unit {
  self.put(key, RedisValue::String(value))
  self.expires.remove(key)
}

pub fn Database::setnx(self : Database, key : String, value : String) -> Bool {
  if self.check_expired(key) {
    self.put(key, RedisValue::String(value))
    return true
  }
  if self.data.contains(key) {
    false
  } else {
    self.put(key, RedisValue::String(value))
    true
  }
}

pub fn Database::setex(self : Database, key : String, seconds : Int, value : String) -> Bool {
  if seconds <= 0 {
    false
  } else {
    self.put(key, RedisValue::String(value))
    self.expires.set(key, self.current_time + seconds.to_int64() * 1000)
    true
  }
}

pub fn Database::getdel(self : Database, key : String) -> String? {
  let value = self.get(key)
  match value {
    Some(_) => self.delete_key(key)
    None => ()
  }
  value
}

pub fn Database::getset(self : Database, key : String, value : String) -> String? {
  if self.check_expired(key) {
    self.put(key, RedisValue::String(value))
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(s)) => {
        self.put(key, RedisValue::String(value))
        self.expires.remove(key)
        Some(s)
      }
      Some(_) => None
      None => {
        self.put(key, RedisValue::String(value))
        self.expires.remove(key)
        None
      }
    }
  }
}

pub fn Database::mset(self : Database, keys : Array[String], values : Array[String]) -> Unit {
  for i = 0; i < keys.length() && i < values.length(); i = i + 1 {
    self.put(keys[i], RedisValue::String(values[i]))
    self.expires.remove(keys[i])
  }
}

pub fn Database::mget(self : Database, keys : Array[String]) -> Array[String?] {
  let result = []
  for key in keys {
    if self.check_expired(key) {
      result.push(None)
    } else {
      match self.data.get(key) {
        Some(RedisValue::String(s)) => result.push(Some(s))
        _ => result.push(None)
      }
    }
  }
  result
}

pub fn Database::mdel(self : Database, keys : Array[String]) -> Int {
  let mut count = 0
  for key in keys {
    if !self.check_expired(key) {
      let existed = self.data.contains(key)
      if existed {
        self.delete_key(key)
        count = count + 1
      }
    }
  }
  count
}

pub fn Database::get(self : Database, key : String) -> String? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(s)) => Some(s)
      _ => None
    }
  }
}

pub fn Database::append(self : Database, key : String, value : String) -> Int {
  if self.check_expired(key) {
    let new_val = value
    self.put(key, RedisValue::String(new_val))
    new_val.length()
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(s)) => {
        let new_val = s + value
        self.put(key, RedisValue::String(new_val))
        new_val.length()
      }
      Some(_) => -1
      None => {
        let new_val = value
        self.put(key, RedisValue::String(new_val))
        new_val.length()
      }
    }
  }
}

pub fn Database::strlen(self : Database, key : String) -> Int {
  if self.check_expired(key) {
    0
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(s)) => s.length()
      _ => 0
    }
  }
}

pub fn Database::incr(self : Database, key : String) -> Int64? {
  self.check_expired(key) |> ignore
  let current = match self.data.get(key) {
    Some(RedisValue::String(s)) => s
    None => "0"
    Some(_) => return None
  }
  match parse_int64(current) {
    Some(n) => {
      let new_val = "\{(n + 1)}"
      self.put(key, RedisValue::String(new_val))
      Some(n + 1)
    }
    None => None
  }
}

pub fn Database::decr(self : Database, key : String) -> Int64? {
  self.check_expired(key) |> ignore
  let current = match self.data.get(key) {
    Some(RedisValue::String(s)) => s
    None => "0"
    Some(_) => return None
  }
  match parse_int64(current) {
    Some(n) => {
      let new_val = "\{(n - 1)}"
      self.put(key, RedisValue::String(new_val))
      Some(n - 1)
    }
    None => None
  }
}

