pub fn Database::try_lpush(self : Database, key : String, value : String) -> Result[Int, RedisError] {
  self.check_expired(key) |> ignore
  let deque = match self.data.get(key) {
    Some(RedisValue::List(d)) => d
    None => Deque::new()
    Some(_) => return Err(RedisError::WrongType)
  }
  deque.push_front(value)
  self.put(key, RedisValue::List(deque))
  Ok(deque.length())
}

pub fn Database::lpush(self : Database, key : String, value : String) -> Int {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::LPush(key, value))
    -1
  } else {
    match self.try_lpush(key, value) {
      Ok(length) => length
      Err(_) => -1
    }
  }
}

pub fn Database::try_rpush(self : Database, key : String, value : String) -> Result[Int, RedisError] {
  self.check_expired(key) |> ignore
  let deque = match self.data.get(key) {
    Some(RedisValue::List(d)) => d
    None => Deque::new()
    Some(_) => return Err(RedisError::WrongType)
  }
  deque.push_back(value)
  self.put(key, RedisValue::List(deque))
  Ok(deque.length())
}

pub fn Database::rpush(self : Database, key : String, value : String) -> Int {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::RPush(key, value))
    -1
  } else {
    match self.try_rpush(key, value) {
      Ok(length) => length
      Err(_) => -1
    }
  }
}

pub fn Database::try_lpush_all(self : Database, key : String, values : Array[String]) -> Result[Int, RedisError] {
  self.check_expired(key) |> ignore
  if values.length() == 0 {
    return Err(RedisError::InvalidArgument)
  }
  let deque = match self.data.get(key) {
    Some(RedisValue::List(d)) => d
    None => Deque::new()
    Some(_) => return Err(RedisError::WrongType)
  }
  for value in values {
    deque.push_front(value)
  }
  self.put(key, RedisValue::List(deque))
  Ok(deque.length())
}

pub fn Database::lpush_all(self : Database, key : String, values : Array[String]) -> Int {
  match self.try_lpush_all(key, values) {
    Ok(length) => length
    Err(_) => -1
  }
}

pub fn Database::try_rpush_all(self : Database, key : String, values : Array[String]) -> Result[Int, RedisError] {
  self.check_expired(key) |> ignore
  if values.length() == 0 {
    return Err(RedisError::InvalidArgument)
  }
  let deque = match self.data.get(key) {
    Some(RedisValue::List(d)) => d
    None => Deque::new()
    Some(_) => return Err(RedisError::WrongType)
  }
  for value in values {
    deque.push_back(value)
  }
  self.put(key, RedisValue::List(deque))
  Ok(deque.length())
}

pub fn Database::rpush_all(self : Database, key : String, values : Array[String]) -> Int {
  match self.try_rpush_all(key, values) {
    Ok(length) => length
    Err(_) => -1
  }
}

pub fn Database::try_lpop(self : Database, key : String) -> Result[String, RedisError] {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::LPop(key))
    return Err(RedisError::NotFound)
  }
  if self.check_expired(key) {
    Err(RedisError::NotFound)
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(deque)) =>
        match deque.pop_front() {
          Some(value) => {
            if deque.length() == 0 {
              self.remove_if_empty(key, 0)
            } else {
              self.put(key, RedisValue::List(deque))
            }
            Ok(value)
          }
          None => Err(RedisError::NotFound)
        }
      Some(_) => Err(RedisError::WrongType)
      None => Err(RedisError::NotFound)
    }
  }
}

pub fn Database::lpop(self : Database, key : String) -> String? {
  match self.try_lpop(key) {
    Ok(value) => Some(value)
    Err(_) => None
  }
}

pub fn Database::try_rpop(self : Database, key : String) -> Result[String, RedisError] {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::RPop(key))
    return Err(RedisError::NotFound)
  }
  if self.check_expired(key) {
    Err(RedisError::NotFound)
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(deque)) =>
        match deque.pop_back() {
          Some(value) => {
            if deque.length() == 0 {
              self.remove_if_empty(key, 0)
            } else {
              self.put(key, RedisValue::List(deque))
            }
            Ok(value)
          }
          None => Err(RedisError::NotFound)
        }
      Some(_) => Err(RedisError::WrongType)
      None => Err(RedisError::NotFound)
    }
  }
}

pub fn Database::rpop(self : Database, key : String) -> String? {
  match self.try_rpop(key) {
    Ok(value) => Some(value)
    Err(_) => None
  }
}

pub fn Database::llen(self : Database, key : String) -> Int {
  if self.check_expired(key) {
    0
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(d)) => d.length()
      _ => 0
    }
  }
}

pub fn Database::lrange(self : Database, key : String, start : Int, end : Int) -> Array[String] {
  if self.check_expired(key) {
    []
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(deque)) => {
        let arr = deque.to_array()
        let len = arr.length()
        let start_idx = if start < 0 { len + start } else { start }
        let end_idx = if end < 0 { len + end } else { end }
        let s = if start_idx < 0 { 0 } else { start_idx }
        let result = []
        for i = s; i <= end_idx && i < len; i = i + 1 {
          result.push(arr[i])
        }
        result
      }
      _ => []
    }
  }
}

pub fn Database::lindex(self : Database, key : String, index : Int) -> String? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(deque)) => {
        if index < 0 {
          deque.get_at(deque.length() + index)
        } else {
          deque.get_at(index)
        }
      }
      _ => None
    }
  }
}

pub fn Database::try_lset(
  self : Database,
  key : String,
  index : Int,
  value : String,
) -> Result[Bool, RedisError] {
  if self.check_expired(key) {
    return Err(RedisError::NotFound)
  }
  match self.data.get(key) {
    Some(RedisValue::List(deque)) => {
      let adjusted_idx = if index < 0 { deque.length() + index } else { index }
      if adjusted_idx < 0 || adjusted_idx >= deque.length() {
        return Err(RedisError::IndexOutOfRange)
      }
      ignore(deque.set_at(adjusted_idx, value))
      self.put(key, RedisValue::List(deque))
      Ok(true)
    }
    Some(_) => Err(RedisError::WrongType)
    None => Err(RedisError::NotFound)
  }
}

pub fn Database::lset(self : Database, key : String, index : Int, value : String) -> Bool {
  match self.try_lset(key, index, value) {
    Ok(_) => true
    Err(_) => false
  }
}

pub fn Database::lrem(self : Database, key : String, count : Int, value : String) -> Int {
  if self.check_expired(key) {
    0
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(deque)) => {
        let arr = deque.to_array()
        let len = arr.length()
        let result = []
        let mut removed_count = 0
        if count > 0 {
          for i = 0; i < len; i = i + 1 {
            if removed_count < count && arr[i] == value {
              removed_count = removed_count + 1
            } else {
              result.push(arr[i])
            }
          }
        } else if count < 0 {
          let limit = -count
          let mut seen = 0
          let mut cut = -1
          for i = len - 1; i >= 0; i = i - 1 {
            if arr[i] == value {
              seen = seen + 1
              if seen == limit {
                cut = i
                break
              }
            }
          }
          if cut == -1 && seen > 0 {
            cut = 0
          }
          for i = 0; i < len; i = i + 1 {
            if cut != -1 && i >= cut && arr[i] == value {
              removed_count = removed_count + 1
            } else {
              result.push(arr[i])
            }
          }
        } else {
          for i = 0; i < len; i = i + 1 {
            if arr[i] == value {
              removed_count = removed_count + 1
            } else {
              result.push(arr[i])
            }
          }
        }

        let new_deque = Deque::new()
        for item in result {
          new_deque.push_back(item)
        }
        if new_deque.length() == 0 {
          self.remove_if_empty(key, 0)
        } else {
          self.put(key, RedisValue::List(new_deque))
        }
        removed_count
      }
      _ => 0
    }
  }
}

pub fn Database::linsert(self : Database, key : String, before : Bool, pivot : String, value : String) -> Int {
  if self.check_expired(key) {
    -1
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(deque)) => {
        let idx = deque.find_index(pivot)
        if idx == -1 {
          -1
        } else {
          let insert_idx = if before { idx } else { idx + 1 }
          deque.insert_at(insert_idx, value) |> ignore
          self.put(key, RedisValue::List(deque))
          deque.length()
        }
      }
      _ => -1
    }
  }
}

pub fn Database::lpushx(self : Database, key : String, value : String) -> Int {
  if self.check_expired(key) {
    0
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(deque)) => {
        deque.push_front(value)
        self.put(key, RedisValue::List(deque))
        deque.length()
      }
      None => 0
      Some(_) => -1
    }
  }
}

pub fn Database::rpushx(self : Database, key : String, value : String) -> Int {
  if self.check_expired(key) {
    0
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(deque)) => {
        deque.push_back(value)
        self.put(key, RedisValue::List(deque))
        deque.length()
      }
      None => 0
      Some(_) => -1
    }
  }
}

pub fn Database::rpoplpush(self : Database, source : String, destination : String) -> String? {
  if self.check_expired(source) {
    return None
  }
  if source == destination {
    match self.data.get(source) {
      Some(RedisValue::List(deque)) =>
        match deque.pop_back() {
          Some(val) => {
            deque.push_front(val)
            self.put(source, RedisValue::List(deque))
            Some(val)
          }
          None => None
        }
      _ => None
    }
  } else {
    self.check_expired(destination) |> ignore
    let dest_deque = match self.data.get(destination) {
      Some(RedisValue::List(d)) => d
      None => Deque::new()
      Some(_) => return None
    }
    match self.data.get(source) {
      Some(RedisValue::List(source_deque)) =>
        match source_deque.pop_back() {
          Some(val) => {
            if source_deque.length() == 0 {
              self.delete_key(source)
            } else {
              self.put(source, RedisValue::List(source_deque))
            }
            dest_deque.push_front(val)
            self.put(destination, RedisValue::List(dest_deque))
            Some(val)
          }
          None => None
        }
      _ => None
    }
  }
}

pub fn Database::try_lmove(
  self : Database,
  source : String,
  destination : String,
  from_left : Bool,
  to_left : Bool,
) -> Result[String, RedisError] {
  if self.check_expired(source) {
    return Err(RedisError::NotFound)
  }
  match self.data.get(source) {
    Some(RedisValue::List(_)) => ()
    Some(_) => return Err(RedisError::WrongType)
    None => return Err(RedisError::NotFound)
  }
  if source != destination {
    self.check_expired(destination) |> ignore
    match self.data.get(destination) {
      Some(RedisValue::List(_)) => ()
      Some(_) => return Err(RedisError::WrongType)
      None => ()
    }
  }
  match self.lmove(source, destination, from_left, to_left) {
    Some(value) => Ok(value)
    None => Err(RedisError::NotFound)
  }
}

pub fn Database::lmove(
  self : Database,
  source : String,
  destination : String,
  from_left : Bool,
  to_left : Bool,
) -> String? {
  if self.check_expired(source) {
    return None
  }
  if source == destination {
    match self.data.get(source) {
      Some(RedisValue::List(deque)) => {
        let value = if from_left {
          deque.pop_front()
        } else {
          deque.pop_back()
        }
        match value {
          Some(val) => {
            if to_left {
              deque.push_front(val)
            } else {
              deque.push_back(val)
            }
            self.put(source, RedisValue::List(deque))
            Some(val)
          }
          None => None
        }
      }
      _ => None
    }
  } else {
    self.check_expired(destination) |> ignore
    let dest_deque = match self.data.get(destination) {
      Some(RedisValue::List(d)) => d
      None => Deque::new()
      Some(_) => return None
    }
    match self.data.get(source) {
      Some(RedisValue::List(source_deque)) => {
        let value = if from_left {
          source_deque.pop_front()
        } else {
          source_deque.pop_back()
        }
        match value {
          Some(val) => {
            if source_deque.length() == 0 {
              self.delete_key(source)
            } else {
              self.put(source, RedisValue::List(source_deque))
            }
            if to_left {
              dest_deque.push_front(val)
            } else {
              dest_deque.push_back(val)
            }
            self.put(destination, RedisValue::List(dest_deque))
            Some(val)
          }
          None => None
        }
      }
      _ => None
    }
  }
}

pub fn Database::ltrim(self : Database, key : String, start : Int, end : Int) -> Bool {
  if self.check_expired(key) {
    return false
  }
  match self.data.get(key) {
    Some(RedisValue::List(deque)) => {
      let arr = deque.to_array()
      let len = arr.length()
      let start_idx = if start < 0 { len + start } else { start }
      let end_idx = if end < 0 { len + end } else { end }
      let s = if start_idx < 0 { 0 } else { start_idx }
      let e = if end_idx > len - 1 { len - 1 } else { end_idx }
      let new_deque = Deque::new()
      if s <= e && s < len {
        for i = s; i <= e; i = i + 1 {
          new_deque.push_back(arr[i])
        }
      }
      if new_deque.length() == 0 {
        self.remove_if_empty(key, 0)
      } else {
        self.put(key, RedisValue::List(new_deque))
      }
      true
    }
    _ => false
  }
}

pub fn Database::try_lpos(self : Database, key : String, value : String) -> Result[Int, RedisError] {
  if self.check_expired(key) {
    Err(RedisError::NotFound)
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(deque)) => {
        let arr = deque.to_array()
        let mut result = Err(RedisError::NotFound)
        for i = 0; i < arr.length(); i = i + 1 {
          if arr[i] == value {
            result = Ok(i)
            break
          }
        }
        result
      }
      Some(_) => Err(RedisError::WrongType)
      None => Err(RedisError::NotFound)
    }
  }
}

pub fn Database::lpos(self : Database, key : String, value : String) -> Int? {
  match self.try_lpos(key, value) {
    Ok(index) => Some(index)
    Err(_) => None
  }
}

pub fn Database::try_lpos_options(
  self : Database,
  key : String,
  value : String,
  rank : Int,
  count : Int,
  maxlen : Int,
) -> Result[Array[Int], RedisError] {
  if rank == 0 {
    return Err(RedisError::InvalidArgument)
  }
  if count < -1 {
    return Err(RedisError::OutOfRange)
  }
  if self.check_expired(key) {
    return Err(RedisError::NotFound)
  }
  match self.data.get(key) {
    Some(RedisValue::List(deque)) => {
      let arr = deque.to_array()
      let len = arr.length()
      let scan_limit = if maxlen > 0 && maxlen < len { maxlen } else { len }
      let result : Array[Int] = []
      if rank > 0 {
        let mut matches = 0
        for i = 0; i < scan_limit; i = i + 1 {
          if arr[i] == value {
            matches = matches + 1
            if matches >= rank {
              result.push(i)
              if count == 0 || (count > 0 && result.length() >= count) {
                break
              }
            }
          }
        }
      } else {
        let target = -rank
        let mut matches = 0
        let mut i = len - 1
        let stop = if scan_limit < len { len - scan_limit } else { -1 }
        while i > stop {
          if arr[i] == value {
            matches = matches + 1
            if matches >= target {
              result.push(i)
              if count == 0 || (count > 0 && result.length() >= count) {
                break
              }
            }
          }
          i = i - 1
        }
      }
      if result.length() == 0 {
        return Err(RedisError::NotFound)
      }
      Ok(result)
    }
    Some(_) => Err(RedisError::WrongType)
    None => Err(RedisError::NotFound)
  }
}

pub fn Database::lpos_options(
  self : Database,
  key : String,
  value : String,
  rank : Int,
  count : Int,
  maxlen : Int,
) -> Array[Int] {
  match self.try_lpos_options(key, value, rank, count, maxlen) {
    Ok(indices) => indices
    Err(_) => []
  }
}

pub fn Database::lpop_count(self : Database, key : String, count : Int) -> Array[String] {
  if self.check_expired(key) || count <= 0 {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::List(deque)) => {
      let take = if count > deque.length() { deque.length() } else { count }
      let result : Array[String] = []
      for _i = 0; _i < take; _i = _i + 1 {
        match deque.pop_front() {
          Some(value) => result.push(value)
          None => break
        }
      }
      if deque.length() == 0 {
        self.remove_if_empty(key, 0)
      } else {
        self.put(key, RedisValue::List(deque))
      }
      result
    }
    _ => []
  }
}

pub fn Database::rpop_count(self : Database, key : String, count : Int) -> Array[String] {
  if self.check_expired(key) || count <= 0 {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::List(deque)) => {
      let take = if count > deque.length() { deque.length() } else { count }
      let result : Array[String] = []
      for _i = 0; _i < take; _i = _i + 1 {
        match deque.pop_back() {
          Some(value) => result.push(value)
          None => break
        }
      }
      if deque.length() == 0 {
        self.remove_if_empty(key, 0)
      } else {
        self.put(key, RedisValue::List(deque))
      }
      result
    }
    _ => []
  }
}

pub fn Database::try_lmpop(
  self : Database,
  keys : Array[String],
  left : Bool,
  count : Int,
) -> Result[(String, Array[String])?, RedisError] {
  if keys.length() == 0 || count <= 0 {
    return Err(RedisError::InvalidArgument)
  }
  for key in keys {
    if self.check_expired(key) {
      continue
    }
    match self.data.get(key) {
      Some(RedisValue::List(deque)) => {
        let take = if count > deque.length() { deque.length() } else { count }
        if take == 0 {
          continue
        }
        let result : Array[String] = []
        if left {
          for _i = 0; _i < take; _i = _i + 1 {
            match deque.pop_front() {
              Some(value) => result.push(value)
              None => break
            }
          }
        } else {
          for _i = 0; _i < take; _i = _i + 1 {
            match deque.pop_back() {
              Some(value) => result.push(value)
              None => break
            }
          }
        }
        if deque.length() == 0 {
          self.remove_if_empty(key, 0)
        } else {
          self.put(key, RedisValue::List(deque))
        }
        return Ok(Some((key, result)))
      }
      _ => ()
    }
  }
  Ok(None)
}

pub fn Database::try_lpop_count(self : Database, key : String, count : Int) -> Result[Array[String], RedisError] {
  if count < 0 {
    return Err(RedisError::OutOfRange)
  }
  Ok(self.lpop_count(key, count))
}

pub fn Database::try_rpop_count(self : Database, key : String, count : Int) -> Result[Array[String], RedisError] {
  if count < 0 {
    return Err(RedisError::OutOfRange)
  }
  Ok(self.rpop_count(key, count))
}

pub fn Database::lmpop(
  self : Database,
  keys : Array[String],
  left : Bool,
  count : Int,
) -> (String, Array[String])? {
  match self.try_lmpop(keys, left, count) {
    Ok(result) => result
    Err(_) => None
  }
}

