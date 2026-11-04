pub fn Database::set(self : Database, key : String, value : String) -> Unit {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::Set(key, value))
  } else {
    self.put(key, RedisValue::String(value))
    self.expires.remove(key)
  }
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
    match checked_add_i64(self.current_time, seconds.to_int64() * 1000) {
      Some(expire_time) => {
        self.put(key, RedisValue::String(value))
        self.set_expire(key, expire_time)
        true
      }
      None => false
    }
  }
}

pub fn Database::psetex(self : Database, key : String, milliseconds : Int64, value : String) -> Bool {
  if milliseconds <= 0 {
    false
  } else {
    match checked_add_i64(self.current_time, milliseconds) {
      Some(expire_time) => {
        self.put(key, RedisValue::String(value))
        self.set_expire(key, expire_time)
        true
      }
      None => false
    }
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

pub fn Database::try_set_get(
  self : Database,
  key : String,
  value : String,
) -> Result[String?, RedisError] {
  if self.check_expired(key) {
    self.put(key, RedisValue::String(value))
    Ok(None)
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(s)) => {
        self.put(key, RedisValue::String(value))
        self.expires.remove(key)
        Ok(Some(s))
      }
      Some(_) => Err(RedisError::WrongType)
      None => {
        self.put(key, RedisValue::String(value))
        self.expires.remove(key)
        Ok(None)
      }
    }
  }
}

pub fn Database::try_mset(
  self : Database,
  keys : Array[String],
  values : Array[String],
) -> Result[Unit, RedisError] {
  if keys.length() == 0 || keys.length() != values.length() {
    return Err(RedisError::InvalidArgument)
  }
  for i = 0; i < keys.length(); i = i + 1 {
    self.put(keys[i], RedisValue::String(values[i]))
    self.expires.remove(keys[i])
  }
  Ok(())
}

pub fn Database::mset(self : Database, keys : Array[String], values : Array[String]) -> Unit {
  self.try_mset(keys, values) |> ignore
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

pub fn Database::try_get(self : Database, key : String) -> Result[String, RedisError] {
  if self.check_expired(key) {
    Err(RedisError::NotFound)
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(s)) => Ok(s)
      Some(_) => Err(RedisError::WrongType)
      None => Err(RedisError::NotFound)
    }
  }
}

pub fn Database::get(self : Database, key : String) -> String? {
  match self.try_get(key) {
    Ok(value) => Some(value)
    Err(_) => None
  }
}

pub fn Database::set_with_options(
  self : Database,
  key : String,
  value : String,
  nx : Bool,
  xx : Bool,
  get : Bool,
  ex_seconds : Int?,
  px_milliseconds : Int64?,
  keep_ttl : Bool,
) -> Result[String?, RedisError] {
  if nx && xx {
    return Err(RedisError::InvalidArgument)
  }
  let has_ex = match ex_seconds {
    Some(_) => true
    None => false
  }
  let has_px = match px_milliseconds {
    Some(_) => true
    None => false
  }
  if has_ex && has_px {
    return Err(RedisError::InvalidArgument)
  }
  if keep_ttl && (has_ex || has_px) {
    return Err(RedisError::InvalidArgument)
  }
  self.check_expired(key) |> ignore
  let exists = self.data.contains(key)
  if nx && exists {
    return Ok(None)
  }
  if xx && !exists {
    return Ok(None)
  }
  if exists {
    match self.data.get(key) {
      Some(RedisValue::String(_)) => ()
      Some(_) => return Err(RedisError::WrongType)
      None => ()
    }
  }
  let old_value = if get {
    match self.try_get(key) {
      Ok(v) => Some(v)
      Err(_) => None
    }
  } else {
    None
  }
  let ttl : Int64? = if keep_ttl {
    if exists {
      self.expires.get(key)
    } else {
      None
    }
  } else if has_ex {
    let seconds = ex_seconds.unwrap()
    if seconds <= 0 {
      return Err(RedisError::InvalidArgument)
    }
    match checked_add_i64(self.current_time, seconds.to_int64() * 1000) {
      Some(expire_time) => Some(expire_time)
      None => return Err(RedisError::InvalidArgument)
    }
  } else if has_px {
    let milliseconds = px_milliseconds.unwrap()
    if milliseconds <= 0 {
      return Err(RedisError::InvalidArgument)
    }
    match checked_add_i64(self.current_time, milliseconds) {
      Some(expire_time) => Some(expire_time)
      None => return Err(RedisError::InvalidArgument)
    }
  } else {
    None
  }
  self.put(key, RedisValue::String(value))
  match ttl {
    Some(expire_time) => self.set_expire(key, expire_time)
    None => self.expires.remove(key)
  }
  Ok(old_value)
}

pub fn Database::setxx(self : Database, key : String, value : String) -> Bool {
  let existed = !self.check_expired(key) && self.data.contains(key)
  match self.set_with_options(key, value, false, true, false, None, None, false) {
    Ok(_) => existed
    Err(_) => false
  }
}

pub fn Database::set_get(self : Database, key : String, value : String) -> String? {
  match self.set_with_options(key, value, false, false, true, None, None, false) {
    Ok(old_value) => old_value
    Err(_) => None
  }
}

pub fn Database::try_getex_persist(
  self : Database,
  key : String,
) -> Result[String, RedisError] {
  match self.try_get(key) {
    Err(error) => Err(error)
    Ok(value) => {
      self.expires.remove(key)
      self.maybe_compact_expire_heap()
      Ok(value)
    }
  }
}

pub fn Database::try_getex_ex(
  self : Database,
  key : String,
  seconds : Int,
) -> Result[String, RedisError] {
  match self.try_get(key) {
    Err(error) => Err(error)
    Ok(value) => {
      if seconds <= 0 {
        Err(RedisError::InvalidArgument)
      } else {
        match checked_add_i64(self.current_time, seconds.to_int64() * 1000) {
          Some(expire_time) => {
            self.set_expire(key, expire_time)
            Ok(value)
          }
          None => Err(RedisError::InvalidArgument)
        }
      }
    }
  }
}

pub fn Database::try_getex_px(
  self : Database,
  key : String,
  milliseconds : Int64,
) -> Result[String, RedisError] {
  match self.try_get(key) {
    Err(error) => Err(error)
    Ok(value) => {
      if milliseconds <= 0 {
        Err(RedisError::InvalidArgument)
      } else {
        match checked_add_i64(self.current_time, milliseconds) {
          Some(expire_time) => {
            self.set_expire(key, expire_time)
            Ok(value)
          }
          None => Err(RedisError::InvalidArgument)
        }
      }
    }
  }
}

pub fn Database::try_getex_exat(
  self : Database,
  key : String,
  timestamp_ms : Int64,
) -> Result[String, RedisError] {
  match self.try_get(key) {
    Err(error) => Err(error)
    Ok(value) => {
      if timestamp_ms <= self.current_time {
        Err(RedisError::InvalidArgument)
      } else {
        self.set_expire(key, timestamp_ms)
        Ok(value)
      }
    }
  }
}

pub fn Database::try_getex_pxat(
  self : Database,
  key : String,
  timestamp_ms : Int64,
) -> Result[String, RedisError] {
  match self.try_get(key) {
    Err(error) => Err(error)
    Ok(value) => {
      if timestamp_ms <= self.current_time {
        Err(RedisError::InvalidArgument)
      } else {
        self.set_expire(key, timestamp_ms)
        Ok(value)
      }
    }
  }
}

pub fn Database::getex(self : Database, key : String, expire_ms : Int64?) -> String? {
  match expire_ms {
    Some(ms) => match self.try_getex_px(key, ms) {
      Ok(value) => Some(value)
      Err(_) => None
    }
    None => match self.try_getex_persist(key) {
      Ok(value) => Some(value)
      Err(_) => None
    }
  }
}

pub fn Database::try_msetnx(
  self : Database,
  keys : Array[String],
  values : Array[String],
) -> Result[Bool, RedisError] {
  if keys.length() == 0 || keys.length() != values.length() {
    return Err(RedisError::InvalidArgument)
  }
  for key in keys {
    if self.check_expired(key) || self.data.contains(key) {
      return Ok(false)
    }
  }
  for i = 0; i < keys.length(); i = i + 1 {
    self.put(keys[i], RedisValue::String(values[i]))
    self.expires.remove(keys[i])
  }
  Ok(true)
}

pub fn Database::msetnx(self : Database, keys : Array[String], values : Array[String]) -> Bool {
  match self.try_msetnx(keys, values) {
    Ok(applied) => applied
    Err(_) => false
  }
}

pub fn Database::try_append(self : Database, key : String, value : String) -> Result[Int, RedisError] {
  self.check_expired(key) |> ignore
  let current = match self.data.get(key) {
    Some(RedisValue::String(s)) => s
    None => ""
    Some(_) => return Err(RedisError::WrongType)
  }
  if current.length().to_int64() + value.length().to_int64() > MAX_STRING_SIZE {
    return Err(RedisError::InvalidArgument)
  }
  let new_val = current + value
  self.put(key, RedisValue::String(new_val))
  Ok(new_val.length())
}

pub fn Database::append(self : Database, key : String, value : String) -> Int {
  match self.try_append(key, value) {
    Ok(length) => length
    Err(_) => -1
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

pub fn Database::try_incrby(self : Database, key : String, increment : Int64) -> Result[Int64, RedisError] {
  self.check_expired(key) |> ignore
  let current = match self.data.get(key) {
    Some(RedisValue::String(s)) => s
    None => "0"
    Some(_) => return Err(RedisError::WrongType)
  }
  match parse_int64(current) {
    Some(n) =>
      match checked_add_i64(n, increment) {
        Some(new_value) => {
          self.put(key, RedisValue::String("\{new_value}"))
          Ok(new_value)
        }
        None => Err(RedisError::Overflow)
      }
    None => Err(RedisError::NotInteger)
  }
}

pub fn Database::incrby(self : Database, key : String, increment : Int64) -> Int64? {
  match self.try_incrby(key, increment) {
    Ok(value) => Some(value)
    Err(_) => None
  }
}

pub fn Database::try_incrbyfloat(
  self : Database,
  key : String,
  increment : Float,
) -> Result[Float, RedisError] {
  if self.check_expired(key) {
    return Err(RedisError::NotFound)
  }
  match self.data.get(key) {
    Some(RedisValue::String(s)) => {
      match parse_float(s) {
        Some(current) => {
          let new_value = current + increment
          if new_value != new_value {
            Err(RedisError::Overflow)
          } else {
            self.put(key, RedisValue::String(format_float(new_value)))
            Ok(new_value)
          }
        }
        None => Err(RedisError::NotInteger)
      }
    }
    Some(_) => Err(RedisError::WrongType)
    None => Err(RedisError::NotFound)
  }
}

pub fn Database::incrbyfloat(self : Database, key : String, increment : Float) -> Float? {
  match self.try_incrbyfloat(key, increment) {
    Ok(value) => Some(value)
    Err(_) => None
  }
}

pub fn Database::try_decrby(self : Database, key : String, decrement : Int64) -> Result[Int64, RedisError] {
  let min_value : Int64 = -9223372036854775807L - 1L
  if decrement == min_value {
    Err(RedisError::Overflow)
  } else {
    self.try_incrby(key, -decrement)
  }
}

pub fn Database::decrby(self : Database, key : String, decrement : Int64) -> Int64? {
  match self.try_decrby(key, decrement) {
    Ok(value) => Some(value)
    Err(_) => None
  }
}

pub fn Database::incr(self : Database, key : String) -> Int64? {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::Incr(key))
    None
  } else {
    self.incrby(key, 1)
  }
}

pub fn Database::decr(self : Database, key : String) -> Int64? {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::Decr(key))
    None
  } else {
    self.incrby(key, -1)
  }
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

const MAX_STRING_SIZE : Int64 = 536870912L

pub fn Database::try_setrange(self : Database, key : String, offset : Int, value : String) -> Result[Int, RedisError] {
  if offset < 0 {
    return Err(RedisError::InvalidArgument)
  }
  if value.length() > 0 && offset.to_int64() + value.length().to_int64() > MAX_STRING_SIZE {
    return Err(RedisError::InvalidArgument)
  }
  if self.check_expired(key) {
    if value.length() == 0 {
      return Ok(0)
    }
    let new_val = null_padding(offset) + value
    self.put(key, RedisValue::String(new_val))
    Ok(new_val.length())
  } else {
    match self.data.get(key) {
      Some(RedisValue::String(s)) => {
        if value.length() == 0 {
          return Ok(s.length())
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
        Ok(new_val.length())
      }
      Some(_) => Err(RedisError::WrongType)
      None => {
        if value.length() == 0 {
          return Ok(0)
        }
        let new_val = null_padding(offset) + value
        self.put(key, RedisValue::String(new_val))
        Ok(new_val.length())
      }
    }
  }
}

pub fn Database::setrange(self : Database, key : String, offset : Int, value : String) -> Int {
  match self.try_setrange(key, offset, value) {
    Ok(length) => length
    Err(_) => -1
  }
}

