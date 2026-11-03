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

pub fn Database::incrby(self : Database, key : String, increment : Int64) -> Int64? {
  self.check_expired(key) |> ignore
  let current = match self.data.get(key) {
    Some(RedisValue::String(s)) => s
    None => "0"
    Some(_) => return None
  }
  match parse_int64(current) {
    Some(n) => {
      let new_value = n + increment
      self.put(key, RedisValue::String("\{new_value}"))
      Some(new_value)
    }
    None => None
  }
}

pub fn Database::decrby(self : Database, key : String, decrement : Int64) -> Int64? {
  self.incrby(key, -decrement)
}

pub fn Database::incr(self : Database, key : String) -> Int64? {
  self.incrby(key, 1)
}

pub fn Database::decr(self : Database, key : String) -> Int64? {
  self.incrby(key, -1)
}

pub fn Database::getrange(self : Database, key : String, start : Int, end : Int) -> String {
  match self.get(key) {
    Some(s) => {
      let len = s.length()
      let start_idx = if start < 0 { len + start } else { start }
      let end_idx = if end < 0 { len + end } else { end }
      let s0 = if start_idx < 0 { 0 } else { start_idx }
      let e0 = if end_idx >= len { len - 1 } else { end_idx }
      if len == 0 || s0 >= len || s0 > e0 {
        ""
      } else {
        s[s0:e0 + 1].to_owned()
      }
    }
    None => ""
  }
}

fn null_padding(count : Int) -> String {
  let chars : Array[Char] = []
  for i = 0; i < count; i = i + 1 {
    chars.push('\u{0}')
  }
  String::from_array(chars)
}

pub fn Database::setrange(self : Database, key : String, offset : Int, value : String) -> Int {
  if offset < 0 {
    return -1
  }
  if self.check_expired(key) {
    if value.length() == 0 {
      return 0
    }
    let new_val = null_padding(offset) + value
    self.put(key, RedisValue::String(new_val))
    new_val.length()
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(s)) => {
        if value.length() == 0 {
          return s.length()
        }
        let len = s.length()
        let new_val = if offset >= len {
          s + null_padding(offset - len) + value
        } else {
          let tail_start = offset + value.length()
          let tail = if tail_start < len {
            s[tail_start:len].to_owned()
          } else {
            ""
          }
          s[0:offset].to_owned() + value + tail
        }
        self.put(key, RedisValue::String(new_val))
        new_val.length()
      }
      Some(_) => -1
      None => {
        if value.length() == 0 {
          return 0
        }
        let new_val = null_padding(offset) + value
        self.put(key, RedisValue::String(new_val))
        new_val.length()
      }
    }
  }
}

