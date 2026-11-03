pub fn Database::hmset(self : Database, key : String, fields : Array[String], values : Array[String]) -> Unit {
  self.check_expired(key) |> ignore
  let hash = match self.data.get(key) {
    Some(RedisValue::Hash(h)) => h
    None => Map([], capacity=10)
    Some(_) => return
  }
  for i = 0; i < fields.length() && i < values.length(); i = i + 1 {
    hash.set(fields[i], values[i])
  }
  self.put(key, RedisValue::Hash(hash))
}

pub fn Database::hmget(self : Database, key : String, fields : Array[String]) -> Array[String?] {
  if self.check_expired(key) {
    let result = []
    for _ in fields {
      result.push(None)
    }
    result
  } else {
    match self.data.get(key) {
      Some(RedisValue::Hash(h)) => {
        let result = []
        for field in fields {
          result.push(h.get(field))
        }
        result
      }
      _ => {
        let result = []
        for _ in fields {
          result.push(None)
        }
        result
      }
    }
  }
}

pub fn Database::hset(self : Database, key : String, field : String, value : String) -> Int {
  self.check_expired(key) |> ignore
  let hash = match self.data.get(key) {
    Some(RedisValue::Hash(h)) => h
    None => Map([], capacity=10)
    Some(_) => return -1
  }
  let existed = hash.contains(field)
  hash.set(field, value)
  self.put(key, RedisValue::Hash(hash))
  if existed {
    0
  } else {
    1
  }
}

pub fn Database::hget(self : Database, key : String, field : String) -> String? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::Hash(h)) => h.get(field)
      _ => None
    }
  }
}

pub fn Database::hdel(self : Database, key : String, field : String) -> Bool {
  if self.check_expired(key) {
    false
  } else {
    match self.data.get(key) {
      Some(RedisValue::Hash(h)) => {
        let existed = h.contains(field)
        if existed {
          h.remove(field)
          self.remove_if_empty(key, h.length())
        }
        existed
      }
      _ => false
    }
  }
}

pub fn Database::hgetall(self : Database, key : String) -> Map[String, String] {
  if self.check_expired(key) {
    Map([], capacity=0)
  } else {
    match self.data.get(key) {
      Some(RedisValue::Hash(h)) => {
        let copy = Map([], capacity=h.length())
        for field, value in h {
          copy.set(field, value)
        }
        copy
      }
      _ => Map([], capacity=0)
    }
  }
}

pub fn Database::hlen(self : Database, key : String) -> Int {
  if self.check_expired(key) {
    0
  } else {
    match self.data.get(key) {
      Some(RedisValue::Hash(h)) => h.length()
      _ => 0
    }
  }
}

pub fn Database::hexists(self : Database, key : String, field : String) -> Bool {
  if self.check_expired(key) {
    false
  } else {
    match self.data.get(key) {
      Some(RedisValue::Hash(h)) => h.contains(field)
      _ => false
    }
  }
}

pub fn Database::hkeys(self : Database, key : String) -> Array[String] {
  if self.check_expired(key) {
    []
  } else {
    match self.data.get(key) {
      Some(RedisValue::Hash(h)) => {
        let result = []
        for field, _ in h {
          result.push(field)
        }
        result
      }
      _ => []
    }
  }
}

pub fn Database::hvals(self : Database, key : String) -> Array[String] {
  if self.check_expired(key) {
    []
  } else {
    match self.data.get(key) {
      Some(RedisValue::Hash(h)) => {
        let result = []
        for _, value in h {
          result.push(value)
        }
        result
      }
      _ => []
    }
  }
}

pub fn Database::hstrlen(self : Database, key : String, field : String) -> Int {
  match self.hget(key, field) {
    Some(value) => value.length()
    None => 0
  }
}

pub fn Database::hincrby(self : Database, key : String, field : String, increment : Int64) -> Int64? {
  self.check_expired(key) |> ignore
  let hash = match self.data.get(key) {
    Some(RedisValue::Hash(h)) => h
    None => Map([], capacity=10)
    Some(_) => return None
  }
  let current = match hash.get(field) {
    Some(value) =>
      match parse_int64(value) {
        Some(n) => n
        None => return None
      }
    None => 0
  }
  hash.set(field, "\{(current + increment)}")
  self.put(key, RedisValue::Hash(hash))
  Some(current + increment)
}

pub fn Database::hsetnx(self : Database, key : String, field : String, value : String) -> Bool {
  self.check_expired(key) |> ignore
  let hash = match self.data.get(key) {
    Some(RedisValue::Hash(h)) => h
    None => Map([], capacity=10)
    Some(_) => return false
  }
  if hash.contains(field) {
    false
  } else {
    hash.set(field, value)
    self.put(key, RedisValue::Hash(hash))
    true
  }
}

