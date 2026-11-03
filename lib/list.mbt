pub fn Database::lpush(self : Database, key : String, value : String) -> Int {
  self.check_expired(key) |> ignore
  let deque = match self.data.get(key) {
    Some(RedisValue::List(d)) => d
    None => Deque::new()
    Some(_) => return -1
  }
  deque.push_front(value)
  self.put(key, RedisValue::List(deque))
  deque.length()
}

pub fn Database::rpush(self : Database, key : String, value : String) -> Int {
  self.check_expired(key) |> ignore
  let deque = match self.data.get(key) {
    Some(RedisValue::List(d)) => d
    None => Deque::new()
    Some(_) => return -1
  }
  deque.push_back(value)
  self.put(key, RedisValue::List(deque))
  deque.length()
}

pub fn Database::lpop(self : Database, key : String) -> String? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(deque)) => {
        let val = deque.pop_front()
        if deque.length() == 0 {
          self.remove_if_empty(key, 0)
        } else {
          self.put(key, RedisValue::List(deque))
        }
        val
      }
      _ => None
    }
  }
}

pub fn Database::rpop(self : Database, key : String) -> String? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(deque)) => {
        let val = deque.pop_back()
        if deque.length() == 0 {
          self.remove_if_empty(key, 0)
        } else {
          self.put(key, RedisValue::List(deque))
        }
        val
      }
      _ => None
    }
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

pub fn Database::lset(self : Database, key : String, index : Int, value : String) -> Bool {
  if self.check_expired(key) {
    false
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(deque)) => {
        let adjusted_idx = if index < 0 { deque.length() + index } else { index }
        let result = deque.set_at(adjusted_idx, value)
        self.put(key, RedisValue::List(deque))
        result
      }
      _ => false
    }
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
      _ => 0
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
      _ => 0
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

