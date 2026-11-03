pub fn Database::try_sadd(self : Database, key : String, value : String) -> Result[Bool, RedisError] {
  self.check_expired(key) |> ignore
  let set = match self.data.get(key) {
    Some(RedisValue::Set(s)) => s
    None => Map([], capacity=10)
    Some(_) => return Err(RedisError::WrongType)
  }
  if set.contains(value) {
    Ok(false)
  } else {
    set.set(value, true)
    self.put(key, RedisValue::Set(set))
    Ok(true)
  }
}

pub fn Database::sadd(self : Database, key : String, value : String) -> Bool {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::SAdd(key, value))
    false
  } else {
    match self.try_sadd(key, value) {
      Ok(added) => added
      Err(_) => false
    }
  }
}

pub fn Database::try_sadd_all(self : Database, key : String, values : Array[String]) -> Result[Int, RedisError] {
  self.check_expired(key) |> ignore
  if values.length() == 0 {
    return Err(RedisError::InvalidArgument)
  }
  let set = match self.data.get(key) {
    Some(RedisValue::Set(s)) => s
    None => Map([], capacity=10)
    Some(_) => return Err(RedisError::WrongType)
  }
  let mut added = 0
  for value in values {
    if !set.contains(value) {
      set.set(value, true)
      added = added + 1
    }
  }
  if added > 0 {
    self.put(key, RedisValue::Set(set))
  }
  Ok(added)
}

pub fn Database::sadd_all(self : Database, key : String, values : Array[String]) -> Int {
  match self.try_sadd_all(key, values) {
    Ok(added) => added
    Err(_) => -1
  }
}

pub fn Database::try_srem_all(self : Database, key : String, values : Array[String]) -> Result[Int, RedisError] {
  if values.length() == 0 {
    return Err(RedisError::InvalidArgument)
  }
  if self.check_expired(key) {
    return Ok(0)
  }
  match self.data.get(key) {
    Some(RedisValue::Set(set)) => {
      let mut removed = 0
      for value in values {
        if set.contains(value) {
          set.remove(value)
          removed = removed + 1
        }
      }
      if removed > 0 {
        if set.length() == 0 {
          self.delete_key(key)
        } else {
          self.put(key, RedisValue::Set(set))
        }
      }
      Ok(removed)
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok(0)
  }
}

pub fn Database::srem_all(self : Database, key : String, values : Array[String]) -> Int {
  match self.try_srem_all(key, values) {
    Ok(removed) => removed
    Err(_) => -1
  }
}

pub fn Database::try_smembers(self : Database, key : String) -> Result[Array[String], RedisError] {
  if self.check_expired(key) {
    Ok([])
  } else {
    match self.data.get(key) {
      Some(RedisValue::Set(s)) => {
        let result = []
        for m, _ in s {
          result.push(m)
        }
        Ok(result)
      }
      Some(_) => Err(RedisError::WrongType)
      None => Ok([])
    }
  }
}

pub fn Database::smembers(self : Database, key : String) -> Array[String] {
  match self.try_smembers(key) {
    Ok(values) => values
    Err(_) => []
  }
}

pub fn Database::smismember(
  self : Database,
  key : String,
  members : Array[String],
) -> Array[Bool] {
  let result : Array[Bool] = []
  if self.check_expired(key) {
    for _ in members {
      result.push(false)
    }
  } else {
    match self.data.get(key) {
      Some(RedisValue::Set(set)) => {
        for member_val in members {
          result.push(set.contains(member_val))
        }
      }
      _ => {
        for _ in members {
          result.push(false)
        }
      }
    }
  }
  result
}

pub fn Database::srem(self : Database, key : String, value : String) -> Bool {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::SRem(key, value))
    true
  } else {
    if self.check_expired(key) {
      false
    } else {
      match self.data.get(key) {
        Some(RedisValue::Set(set)) => {
          let existed = set.contains(value)
          if existed {
            set.remove(value)
            self.sscan_snapshots.remove(key)
            self.remove_if_empty(key, set.length())
          }
          existed
        }
        _ => false
      }
    }
  }
}

pub fn Database::scard(self : Database, key : String) -> Int {
  if self.check_expired(key) {
    0
  } else {
    match self.data.get(key) {
      Some(RedisValue::Set(s)) => s.length()
      _ => 0
    }
  }
}

pub fn Database::sismember(self : Database, key : String, value : String) -> Bool {
  if self.check_expired(key) {
    false
  } else {
    match self.data.get(key) {
      Some(RedisValue::Set(s)) => s.contains(value)
      _ => false
    }
  }
}

fn Database::get_set_members(self : Database, key : String) -> Map[String, Bool] {
  if self.check_expired(key) {
    Map([], capacity=0)
  } else {
    match self.data.get(key) {
      Some(RedisValue::Set(s)) => s
      _ => Map([], capacity=0)
    }
  }
}

pub fn Database::try_sinter(self : Database, keys : Array[String]) -> Result[Array[String], RedisError] {
  if !self.sets_valid(keys) {
    Err(RedisError::WrongType)
  } else {
    Ok(self.sinter(keys))
  }
}

pub fn Database::sinter(self : Database, keys : Array[String]) -> Array[String] {
  if keys.length() == 0 {
    return []
  }
  let first_set = self.get_set_members(keys[0])
  if first_set.length() == 0 {
    return []
  }
  let result = Map([], capacity=first_set.length())
  for m, _ in first_set {
    result.set(m, true)
  }
  for i = 1; i < keys.length(); i = i + 1 {
    let current_set = self.get_set_members(keys[i])
    let to_remove = []
    for m, _ in result {
      if !current_set.contains(m) {
        to_remove.push(m)
      }
    }
    for m in to_remove {
      result.remove(m)
    }
  }
  let arr = []
  for m, _ in result {
    arr.push(m)
  }
  arr
}

pub fn Database::sintercard(self : Database, keys : Array[String], limit : Int) -> Int {
  if !self.sets_valid(keys) {
    return -1
  }
  if keys.length() == 0 {
    return 0
  }
  let sets : Array[Map[String, Bool]] = []
  for key in keys {
    sets.push(self.get_set_members(key))
  }
  let first = sets[0]
  let cap = if limit <= 0 { 2147483647 } else { limit }
  let mut count = 0
  for member_val, _ in first {
    let mut present = true
    for i = 1; i < sets.length(); i = i + 1 {
      if !sets[i].contains(member_val) {
        present = false
        break
      }
    }
    if present {
      count = count + 1
      if count >= cap {
        break
      }
    }
  }
  count
}

pub fn Database::try_sinterstore(self : Database, destination : String, keys : Array[String]) -> Result[Int, RedisError] {
  if !self.sets_valid(keys) {
    return Err(RedisError::WrongType)
  }
  let result = self.sinter(keys)
  let new_set = Map([], capacity=result.length())
  for item in result {
    new_set.set(item, true)
  }
  self.put(destination, RedisValue::Set(new_set))
  self.expires.remove(destination)
  Ok(result.length())
}

pub fn Database::sinterstore(self : Database, destination : String, keys : Array[String]) -> Int {
  match self.try_sinterstore(destination, keys) {
    Ok(count) => count
    Err(_) => -1
  }
}

pub fn Database::try_sunion(self : Database, keys : Array[String]) -> Result[Array[String], RedisError] {
  if !self.sets_valid(keys) {
    Err(RedisError::WrongType)
  } else {
    Ok(self.sunion(keys))
  }
}

pub fn Database::sunion(self : Database, keys : Array[String]) -> Array[String] {
  let result = Map([], capacity=10)
  for key in keys {
    let current_set = self.get_set_members(key)
    for m, _ in current_set {
      result.set(m, true)
    }
  }
  let arr = []
  for m, _ in result {
    arr.push(m)
  }
  arr
}

pub fn Database::try_sunionstore(self : Database, destination : String, keys : Array[String]) -> Result[Int, RedisError] {
  if !self.sets_valid(keys) {
    return Err(RedisError::WrongType)
  }
  let result = self.sunion(keys)
  let new_set = Map([], capacity=result.length())
  for item in result {
    new_set.set(item, true)
  }
  self.put(destination, RedisValue::Set(new_set))
  self.expires.remove(destination)
  Ok(result.length())
}

pub fn Database::sunionstore(self : Database, destination : String, keys : Array[String]) -> Int {
  match self.try_sunionstore(destination, keys) {
    Ok(count) => count
    Err(_) => -1
  }
}

pub fn Database::try_sdiff(self : Database, keys : Array[String]) -> Result[Array[String], RedisError] {
  if !self.sets_valid(keys) {
    Err(RedisError::WrongType)
  } else {
    Ok(self.sdiff(keys))
  }
}

pub fn Database::sdiff(self : Database, keys : Array[String]) -> Array[String] {
  if keys.length() == 0 {
    return []
  }
  let first_set = self.get_set_members(keys[0])
  let result = Map([], capacity=first_set.length())
  for m, _ in first_set {
    result.set(m, true)
  }
  for i = 1; i < keys.length(); i = i + 1 {
    let current_set = self.get_set_members(keys[i])
    let to_remove = []
    for m, _ in result {
      if current_set.contains(m) {
        to_remove.push(m)
      }
    }
    for m in to_remove {
      result.remove(m)
    }
  }
  let arr = []
  for m, _ in result {
    arr.push(m)
  }
  arr
}

pub fn Database::try_sdiffstore(self : Database, destination : String, keys : Array[String]) -> Result[Int, RedisError] {
  if !self.sets_valid(keys) {
    return Err(RedisError::WrongType)
  }
  let result = self.sdiff(keys)
  let new_set = Map([], capacity=result.length())
  for item in result {
    new_set.set(item, true)
  }
  self.put(destination, RedisValue::Set(new_set))
  self.expires.remove(destination)
  Ok(result.length())
}

pub fn Database::sdiffstore(self : Database, destination : String, keys : Array[String]) -> Int {
  match self.try_sdiffstore(destination, keys) {
    Ok(count) => count
    Err(_) => -1
  }
}

pub fn Database::smove(self : Database, source : String, destination : String, value : String) -> Bool {
  if self.check_expired(source) || self.check_expired(destination) {
    return false
  }
  if source == destination {
    return match self.data.get(source) {
      Some(RedisValue::Set(s)) => s.contains(value)
      _ => false
    }
  }
  match self.data.get(source) {
    Some(RedisValue::Set(src_set)) => {
      let dest_set = match self.data.get(destination) {
        Some(RedisValue::Set(d)) => d
        None => Map([], capacity=10)
        Some(_) => return false
      }
      if !src_set.contains(value) {
        return false
      }
      src_set.remove(value)
      self.sscan_snapshots.remove(source)
      self.remove_if_empty(source, src_set.length())
      dest_set.set(value, true)
      self.put(destination, RedisValue::Set(dest_set))
      true
    }
    _ => false
  }
}

pub fn Database::spop(self : Database, key : String) -> String? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::Set(set)) => {
        if set.length() == 0 {
          return None
        }
        let target = self.next_rand() % set.length()
        let mut i = 0
        let mut popped : String? = None
        for m, _ in set {
          if i == target {
            popped = Some(m)
            break
          }
          i = i + 1
        }
        match popped {
          Some(v) => {
            set.remove(v)
            self.sscan_snapshots.remove(key)
            self.remove_if_empty(key, set.length())
            Some(v)
          }
          None => None
        }
      }
      _ => None
    }
  }
}

pub fn Database::srandmember(self : Database, key : String) -> String? {
  if self.check_expired(key) {
    return None
  }
  match self.data.get(key) {
    Some(RedisValue::Set(s)) => {
      if s.length() == 0 {
        None
      } else {
        let target = self.next_rand() % s.length()
        let mut i = 0
        let mut result : String? = None
        for m, _ in s {
          if i == target {
            result = Some(m)
            break
          }
          i = i + 1
        }
        result
      }
    }
    _ => None
  }
}

pub fn Database::sscan(
  self : Database,
  key : String,
  cursor : Int,
  count : Int,
) -> (Int, Array[String]) {
  if self.check_expired(key) {
    return (0, [])
  }
  match self.data.get(key) {
    Some(RedisValue::Set(set)) => {
      let members = match self.sscan_snapshots.get(key) {
        Some(cached) => cached
        None => {
          let collected : Array[String] = []
          for member_val, _ in set {
            collected.push(member_val)
          }
          self.sscan_snapshots.set(key, collected)
          collected
        }
      }
      let len = members.length()
      let limit = if count <= 0 { 10 } else { count }
      let start = if cursor < 0 { 0 } else { cursor }
      if start >= len {
        self.sscan_snapshots.remove(key)
        return (0, [])
      }
      let end = if start + limit > len { len } else { start + limit }
      let result : Array[String] = []
      for i = start; i < end; i = i + 1 {
        result.push(members[i])
      }
      let next = if end >= len { 0 } else { end }
      if next == 0 {
        self.sscan_snapshots.remove(key)
      }
      (next, result)
    }
    _ => (0, [])
  }
}

pub fn Database::try_sscan(
  self : Database,
  key : String,
  cursor : Int,
  count : Int,
) -> Result[(Int, Array[String]), RedisError] {
  if self.check_expired(key) {
    Ok((0, []))
  } else {
    match self.data.get(key) {
      Some(RedisValue::Set(_)) => Ok(self.sscan(key, cursor, count))
      Some(_) => Err(RedisError::WrongType)
      None => Ok((0, []))
    }
  }
}

pub fn Database::try_sintercard(
  self : Database,
  keys : Array[String],
  limit : Int,
) -> Result[Int, RedisError] {
  if !self.sets_valid(keys) {
    Err(RedisError::WrongType)
  } else {
    Ok(self.sintercard(keys, limit))
  }
}

pub fn Database::try_smismember(
  self : Database,
  key : String,
  members : Array[String],
) -> Result[Array[Bool], RedisError] {
  if self.check_expired(key) {
    let result : Array[Bool] = []
    for _ in members {
      result.push(false)
    }
    Ok(result)
  } else {
    match self.data.get(key) {
      Some(RedisValue::Set(_)) => Ok(self.smismember(key, members))
      Some(_) => Err(RedisError::WrongType)
      None => {
        let result : Array[Bool] = []
        for _ in members {
          result.push(false)
        }
        Ok(result)
      }
    }
  }
}

pub fn Database::srandmember_count(self : Database, key : String, count : Int) -> Array[String] {
  if self.check_expired(key) {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::Set(set)) => {
      let members : Array[String] = []
      for member_val, _ in set {
        members.push(member_val)
      }
      self.random_sample(members, count)
    }
    _ => []
  }
}

