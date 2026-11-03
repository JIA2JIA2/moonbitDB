pub fn Database::try_hmset(
  self : Database,
  key : String,
  fields : Array[String],
  values : Array[String],
) -> Result[Unit, RedisError] {
  if fields.length() == 0 || fields.length() != values.length() {
    return Err(RedisError::InvalidArgument)
  }
  self.check_expired(key) |> ignore
  let hash = match self.data.get(key) {
    Some(RedisValue::Hash(h)) => h
    None => Map([], capacity=10)
    Some(_) => return Err(RedisError::WrongType)
  }
  for i = 0; i < fields.length(); i = i + 1 {
    hash.set(fields[i], values[i])
  }
  self.put(key, RedisValue::Hash(hash))
  Ok(())
}

pub fn Database::hmset(self : Database, key : String, fields : Array[String], values : Array[String]) -> Unit {
  self.try_hmset(key, fields, values) |> ignore
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

pub fn Database::try_hset(self : Database, key : String, field : String, value : String) -> Result[Int, RedisError] {
  self.check_expired(key) |> ignore
  let hash = match self.data.get(key) {
    Some(RedisValue::Hash(h)) => h
    None => Map([], capacity=10)
    Some(_) => return Err(RedisError::WrongType)
  }
  let existed = hash.contains(field)
  hash.set(field, value)
  self.put(key, RedisValue::Hash(hash))
  if existed {
    Ok(0)
  } else {
    Ok(1)
  }
}

pub fn Database::hset(self : Database, key : String, field : String, value : String) -> Int {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::HSet(key, field, value))
    -1
  } else {
    match self.try_hset(key, field, value) {
      Ok(created) => created
      Err(_) => -1
    }
  }
}

pub fn Database::try_hget(self : Database, key : String, field : String) -> Result[String, RedisError] {
  if self.check_expired(key) {
    Err(RedisError::NotFound)
  } else {
    match self.data.get(key) {
      Some(RedisValue::Hash(h)) =>
        match h.get(field) {
          Some(value) => Ok(value)
          None => Err(RedisError::NotFound)
        }
      Some(_) => Err(RedisError::WrongType)
      None => Err(RedisError::NotFound)
    }
  }
}

pub fn Database::hget(self : Database, key : String, field : String) -> String? {
  match self.try_hget(key, field) {
    Ok(value) => Some(value)
    Err(_) => None
  }
}

pub fn Database::hdel(self : Database, key : String, field : String) -> Bool {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::HDel(key, field))
    true
  } else {
    if self.check_expired(key) {
      false
    } else {
      match self.data.get(key) {
        Some(RedisValue::Hash(h)) => {
          let existed = h.contains(field)
          if existed {
            h.remove(field)
            self.hscan_snapshots.remove(key)
            self.remove_if_empty(key, h.length())
          }
          existed
        }
        _ => false
      }
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

pub fn Database::try_hincrby(
  self : Database,
  key : String,
  field : String,
  increment : Int64,
) -> Result[Int64, RedisError] {
  self.check_expired(key) |> ignore
  let hash = match self.data.get(key) {
    Some(RedisValue::Hash(h)) => h
    None => Map([], capacity=10)
    Some(_) => return Err(RedisError::WrongType)
  }
  let current = match hash.get(field) {
    Some(value) =>
      match parse_int64(value) {
        Some(n) => n
        None => return Err(RedisError::NotInteger)
      }
    None => 0
  }
  match checked_add_i64(current, increment) {
    Some(new_value) => {
      hash.set(field, "\{new_value}")
      self.put(key, RedisValue::Hash(hash))
      Ok(new_value)
    }
    None => Err(RedisError::Overflow)
  }
}

pub fn Database::hincrby(self : Database, key : String, field : String, increment : Int64) -> Int64? {
  match self.try_hincrby(key, field, increment) {
    Ok(value) => Some(value)
    Err(_) => None
  }
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

fn pattern_match(text : String, pattern : String?) -> Bool {
  match pattern {
    None => true
    Some(p) => {
      if p == "*" {
        true
      } else if p.has_prefix("*") {
        let suffix = p[1:p.length()]
        text.has_suffix(suffix)
      } else if p.has_suffix("*") {
        let prefix = p[0:p.length() - 1]
        text.has_prefix(prefix)
      } else {
        text == p
      }
    }
  }
}

pub fn Database::hscan_with_match(
  self : Database,
  key : String,
  cursor : Int,
  count : Int,
  pattern : String?,
) -> (Int, Array[(String, String)]) {
  if self.check_expired(key) {
    return (0, [])
  }
  match self.data.get(key) {
    Some(RedisValue::Hash(hash)) => {
      let fields = match self.hscan_snapshots.get(key) {
        Some(cached) => cached
        None => {
          let collected : Array[String] = []
          for field, _ in hash {
            collected.push(field)
          }
          self.hscan_snapshots.set(key, collected)
          collected
        }
      }
      let len = fields.length()
      let limit = if count <= 0 { 10 } else { count }
      let start = if cursor < 0 { 0 } else { cursor }
      if start >= len {
        self.hscan_snapshots.remove(key)
        return (0, [])
      }
      let result : Array[(String, String)] = []
      let mut i = start
      let mut taken = 0
      while i < len && taken < limit {
        let field = fields[i]
        if pattern_match(field, pattern) {
          match hash.get(field) {
            Some(value) => result.push((field, value))
            None => ()
          }
          taken = taken + 1
        }
        i = i + 1
      }
      let next = if i >= len { 0 } else { i }
      if next == 0 {
        self.hscan_snapshots.remove(key)
      }
      (next, result)
    }
    _ => (0, [])
  }
}

pub fn Database::hscan(
  self : Database,
  key : String,
  cursor : Int,
  count : Int,
) -> (Int, Array[(String, String)]) {
  self.hscan_with_match(key, cursor, count, None)
}

pub fn Database::try_hscan(
  self : Database,
  key : String,
  cursor : Int,
  count : Int,
) -> Result[(Int, Array[(String, String)]), RedisError] {
  if self.check_expired(key) {
    Ok((0, []))
  } else {
    match self.data.get(key) {
      Some(RedisValue::Hash(_)) => Ok(self.hscan(key, cursor, count))
      Some(_) => Err(RedisError::WrongType)
      None => Ok((0, []))
    }
  }
}

pub fn Database::hrandfield(self : Database, key : String, count : Int) -> Array[String] {
  if self.check_expired(key) {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::Hash(hash)) => {
      let fields : Array[String] = []
      for field, _ in hash {
        fields.push(field)
      }
      self.random_sample(fields, count)
    }
    _ => []
  }
}


pub fn Database::hrandfield_with_values(
  self : Database,
  key : String,
  count : Int,
) -> Array[(String, String)] {
  if self.check_expired(key) {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::Hash(hash)) => {
      let fields : Array[String] = []
      for field, _ in hash {
        fields.push(field)
      }
      let sampled = self.random_sample(fields, count)
      let result : Array[(String, String)] = []
      for field in sampled {
        match hash.get(field) {
          Some(value) => result.push((field, value))
          None => ()
        }
      }
      result
    }
    _ => []
  }
}
