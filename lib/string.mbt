pub fn Database::set(self : Database, key : String, value : String) -> Unit {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::Set(key, value))
  } else {
    self.put(key, RedisValue::String(value))
    self.expires.remove(key)
  }
}

pub fn Database::try_setnx(self : Database, key : String, value : String) -> Result[Bool, RedisError] {
  self.check_expired(key) |> ignore
  match self.data.get(key) {
    Some(RedisValue::String(_)) => Ok(false)
    Some(_) => Err(RedisError::WrongType)
    None => {
      self.put(key, RedisValue::String(value))
      Ok(true)
    }
  }
}

pub fn Database::setnx(self : Database, key : String, value : String) -> Bool {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::SetNx(key, value))
    return true
  }
  match self.try_setnx(key, value) {
    Ok(set) => set
    Err(_) => false
  }
}

pub fn Database::try_setex(self : Database, key : String, seconds : Int, value : String) -> Result[Bool, RedisError] {
  if seconds <= 0 {
    return Err(RedisError::InvalidArgument)
  }
  self.check_expired(key) |> ignore
  match self.data.get(key) {
    Some(RedisValue::String(_)) | None => ()
    Some(_) => return Err(RedisError::WrongType)
  }
  match checked_add_i64(self.current_time, seconds.to_int64() * 1000) {
    Some(expire_time) => {
      self.put(key, RedisValue::String(value))
      self.set_expire(key, expire_time)
      Ok(true)
    }
    None => Err(RedisError::Overflow)
  }
}

pub fn Database::setex(self : Database, key : String, seconds : Int, value : String) -> Bool {
  match self.try_setex(key, seconds, value) {
    Ok(done) => done
    Err(_) => false
  }
}

pub fn Database::try_psetex(self : Database, key : String, milliseconds : Int64, value : String) -> Result[Bool, RedisError] {
  if milliseconds <= 0 {
    return Err(RedisError::InvalidArgument)
  }
  self.check_expired(key) |> ignore
  match self.data.get(key) {
    Some(RedisValue::String(_)) | None => ()
    Some(_) => return Err(RedisError::WrongType)
  }
  match checked_add_i64(self.current_time, milliseconds) {
    Some(expire_time) => {
      self.put(key, RedisValue::String(value))
      self.set_expire(key, expire_time)
      Ok(true)
    }
    None => Err(RedisError::Overflow)
  }
}

pub fn Database::psetex(self : Database, key : String, milliseconds : Int64, value : String) -> Bool {
  match self.try_psetex(key, milliseconds, value) {
    Ok(done) => done
    Err(_) => false
  }
}

pub fn Database::try_getdel(self : Database, key : String) -> Result[String?, RedisError] {
  if self.check_expired(key) {
    return Ok(None)
  }
  match self.data.get(key) {
    Some(RedisValue::String(s)) => {
      self.delete_key(key)
      Ok(Some(s))
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok(None)
  }
}

pub fn Database::getdel(self : Database, key : String) -> String? {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::GetDel(key))
    return None
  }
  match self.try_getdel(key) {
    Ok(value) => value
    Err(_) => None
  }
}

pub fn Database::getset(self : Database, key : String, value : String) -> String? {
  match self.try_set_get(key, value) {
    Ok(old_value) => old_value
    Err(_) => None
  }
}

pub fn Database::try_set_get(
  self : Database,
  key : String,
  value : String,
) -> Result[String?, RedisError] {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::GetSet(key, value))
    return Ok(None)
  }
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
    return Err(RedisError::WrongArgumentCount("mset"))
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
  exat : Int64?,
  pxat : Int64?,
  keep_ttl : Bool,
) -> Result[String?, RedisError] {
  if nx && xx {
    return Err(RedisError::SyntaxError)
  }
  let has_ex = match ex_seconds {
    Some(_) => true
    None => false
  }
  let has_px = match px_milliseconds {
    Some(_) => true
    None => false
  }
  let has_exat = match exat {
    Some(_) => true
    None => false
  }
  let has_pxat = match pxat {
    Some(_) => true
    None => false
  }
  let has_expiry = has_ex || has_px || has_exat || has_pxat
  if has_ex && has_px {
    return Err(RedisError::SyntaxError)
  }
  if has_ex && has_exat {
    return Err(RedisError::SyntaxError)
  }
  if has_ex && has_pxat {
    return Err(RedisError::SyntaxError)
  }
  if has_px && has_exat {
    return Err(RedisError::SyntaxError)
  }
  if has_px && has_pxat {
    return Err(RedisError::SyntaxError)
  }
  if has_exat && has_pxat {
    return Err(RedisError::SyntaxError)
  }
  if keep_ttl && has_expiry {
    return Err(RedisError::SyntaxError)
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
  } else if has_exat {
    let timestamp = exat.unwrap()
    if timestamp <= self.current_time {
      self.put(key, RedisValue::String(value))
      self.delete_key(key)
      return Ok(old_value)
    }
    Some(timestamp)
  } else if has_pxat {
    let timestamp = pxat.unwrap()
    if timestamp <= self.current_time {
      self.put(key, RedisValue::String(value))
      self.delete_key(key)
      return Ok(old_value)
    }
    Some(timestamp)
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
  match self.set_with_options(key, value, false, true, false, None, None, None, None, false) {
    Ok(_) => existed
    Err(_) => false
  }
}

pub fn Database::set_get(self : Database, key : String, value : String) -> String? {
  match self.set_with_options(key, value, false, false, true, None, None, None, None, false) {
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
      self.mark_dirty(key)
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

pub fn Database::getex_ex(self : Database, key : String, seconds : Int) -> String? {
  match self.try_getex_ex(key, seconds) {
    Ok(value) => Some(value)
    Err(_) => None
  }
}

pub fn Database::getex_exat(self : Database, key : String, timestamp_ms : Int64) -> String? {
  match self.try_getex_exat(key, timestamp_ms) {
    Ok(value) => Some(value)
    Err(_) => None
  }
}

pub fn Database::getex_pxat(self : Database, key : String, timestamp_ms : Int64) -> String? {
  match self.try_getex_pxat(key, timestamp_ms) {
    Ok(value) => Some(value)
    Err(_) => None
  }
}

pub fn Database::try_getbit(self : Database, key : String, offset : Int) -> Result[Int, RedisError] {
  if offset < 0 {
    return Err(RedisError::InvalidArgument)
  }
  if self.check_expired(key) {
    return Ok(0)
  }
  match self.data.get(key) {
    Some(RedisValue::String(s)) => {
      let cu = offset / 16
      let bit = offset % 16
      if cu >= s.length() {
        Ok(0)
      } else {
        Ok((s[cu].to_int() >> bit) & 1)
      }
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok(0)
  }
}

pub fn Database::getbit(self : Database, key : String, offset : Int) -> Int {
  match self.try_getbit(key, offset) {
    Ok(bit) => bit
    Err(_) => 0
  }
}

pub fn Database::try_setbit(self : Database, key : String, offset : Int, value : Int) -> Result[Int, RedisError] {
  if offset < 0 {
    return Err(RedisError::InvalidArgument)
  }
  if value != 0 && value != 1 {
    return Err(RedisError::InvalidArgument)
  }
  self.check_expired(key) |> ignore
  match self.data.get(key) {
    Some(RedisValue::String(s)) => {
      let cu = offset / 16
      let bit = offset % 16
      let old_unit : Int = if cu < s.length() { s[cu].to_int() } else { 0 }
      let old_bit = (old_unit >> bit) & 1
      let new_unit = if value == 1 { old_unit | (1 << bit) } else { old_unit & (0xFFFF ^ (1 << bit)) }
      let new_char = Int::unsafe_to_char(new_unit)
      let new_str = if cu < s.length() {
        s[0:cu].to_owned() + "\{new_char}" + s[cu + 1:s.length()].to_owned()
      } else {
        let pad = String::make(cu - s.length(), Int::unsafe_to_char(0))
        s + pad + "\{new_char}"
      }
      self.put(key, RedisValue::String(new_str))
      Ok(old_bit)
    }
    Some(_) => Err(RedisError::WrongType)
    None => {
      let cu = offset / 16
      let bit = offset % 16
      let new_unit = if value == 1 { 1 << bit } else { 0 }
      let pad = String::make(cu, Int::unsafe_to_char(0))
      let new_str = pad + "\{Int::unsafe_to_char(new_unit)}"
      self.put(key, RedisValue::String(new_str))
      Ok(0)
    }
  }
}

pub fn Database::setbit(self : Database, key : String, offset : Int, value : Int) -> Int {
  match self.try_setbit(key, offset, value) {
    Ok(bit) => bit
    Err(_) => -1
  }
}

fn setbit_in_string(s : String, offset : Int, bit : Int) -> String {
  let cu = offset / 16
  let b = offset % 16
  let old_unit : Int = if cu < s.length() { s[cu].to_int() } else { 0 }
  let new_unit = if bit == 1 { old_unit | (1 << b) } else { old_unit & (0xFFFF ^ (1 << b)) }
  let new_char = Int::unsafe_to_char(new_unit)
  if cu < s.length() {
    s[0:cu].to_owned() + "\{new_char}" + s[cu + 1:s.length()].to_owned()
  } else {
    let pad = String::make(cu - s.length(), Int::unsafe_to_char(0))
    s + pad + "\{new_char}"
  }
}

fn bitfield_read_units(s : String, offset : Int, width : Int, signed : Bool) -> Int {
  let mut value = 0
  for i = 0; i < width; i = i + 1 {
    let cu = (offset + i) / 16
    let b = (offset + i) % 16
    let bit : Int = if cu < s.length() { (s[cu].to_int() >> b) & 1 } else { 0 }
    value = value | (bit << i)
  }
  if signed && width < 32 {
    let sign_bit = 1 << (width - 1)
    if (value & sign_bit) != 0 {
      value = value - (1 << width)
    }
  }
  value
}

fn bitfield_write_units(s : String, offset : Int, width : Int, raw : Int) -> String {
  let mut out = s
  for i = 0; i < width; i = i + 1 {
    out = setbit_in_string(out, offset + i, (raw >> i) & 1)
  }
  out
}

pub fn Database::try_bitfield_get(
  self : Database,
  key : String,
  offset : Int,
  width : Int,
  signed : Bool,
) -> Result[Int, RedisError] {
  if offset < 0 || width < 1 || width > 32 {
    return Err(RedisError::InvalidArgument)
  }
  if self.check_expired(key) {
    return Ok(0)
  }
  match self.data.get(key) {
    Some(RedisValue::String(s)) => Ok(bitfield_read_units(s, offset, width, signed))
    Some(_) => Err(RedisError::WrongType)
    None => Ok(0)
  }
}

pub fn Database::try_bitfield_set(
  self : Database,
  key : String,
  offset : Int,
  width : Int,
  signed : Bool,
  value : Int,
) -> Result[Int, RedisError] {
  if offset < 0 || width < 1 || width > 32 {
    return Err(RedisError::InvalidArgument)
  }
  self.check_expired(key) |> ignore
  match self.data.get(key) {
    Some(RedisValue::String(s)) => {
      let old = bitfield_read_units(s, offset, width, signed)
      let mask = if width >= 32 { -1 } else { (1 << width) - 1 }
      let new_s = bitfield_write_units(s, offset, width, value & mask)
      self.put(key, RedisValue::String(new_s))
      Ok(old)
    }
    Some(_) => Err(RedisError::WrongType)
    None => {
      let mask = if width >= 32 { -1 } else { (1 << width) - 1 }
      let new_s = bitfield_write_units("", offset, width, value & mask)
      self.put(key, RedisValue::String(new_s))
      Ok(0)
    }
  }
}

pub fn Database::try_bitfield_incrby(
  self : Database,
  key : String,
  offset : Int,
  width : Int,
  signed : Bool,
  increment : Int,
) -> Result[Int, RedisError] {
  if offset < 0 || width < 1 || width > 32 {
    return Err(RedisError::InvalidArgument)
  }
  self.check_expired(key) |> ignore
  let current = match self.data.get(key) {
    Some(RedisValue::String(s)) => bitfield_read_units(s, offset, width, false)
    Some(_) => return Err(RedisError::WrongType)
    None => 0
  }
  let mask = if width >= 32 { -1 } else { (1 << width) - 1 }
  let new_raw = (current + increment) & mask
  let old_str = match self.data.get(key) {
    Some(RedisValue::String(s)) => s
    _ => ""
  }
  let new_s = bitfield_write_units(old_str, offset, width, new_raw)
  self.put(key, RedisValue::String(new_s))
  Ok(bitfield_read_units(new_s, offset, width, signed))
}

pub fn Database::try_bitfield_incrby_mode(
  self : Database,
  key : String,
  offset : Int,
  width : Int,
  signed : Bool,
  increment : Int,
  overflow : String?,
) -> Result[Int?, RedisError] {
  if offset < 0 || width < 1 || width > 32 {
    return Err(RedisError::InvalidArgument)
  }
  let mode = match overflow {
    Some(m) => m.to_upper()
    None => "WRAP"
  }
  if mode != "WRAP" && mode != "SAT" && mode != "FAIL" {
    return Err(RedisError::InvalidArgument)
  }
  self.check_expired(key) |> ignore
  let current = match self.data.get(key) {
    Some(RedisValue::String(s)) => bitfield_read_units(s, offset, width, false)
    Some(_) => return Err(RedisError::WrongType)
    None => 0
  }
  let mask = if width >= 32 { -1 } else { (1 << width) - 1 }
  let max_val = if signed {
    (1 << (width - 1)) - 1
  } else if width >= 32 {
    2147483647
  } else {
    mask
  }
  let min_val = if signed {
    -(1 << (width - 1))
  } else {
    0
  }
  let raw_sum = current + increment
  let overflowed = raw_sum > max_val || raw_sum < min_val
  let new_raw = if overflowed {
    if mode == "FAIL" {
      return Ok(None)
    } else if mode == "SAT" {
      if raw_sum > max_val {
        max_val
      } else {
        min_val
      }
    } else {
      raw_sum & mask
    }
  } else {
    raw_sum
  }
  let old_str = match self.data.get(key) {
    Some(RedisValue::String(s)) => s
    _ => ""
  }
  let new_s = bitfield_write_units(old_str, offset, width, new_raw)
  self.put(key, RedisValue::String(new_s))
  Ok(Some(bitfield_read_units(new_s, offset, width, signed)))
}

pub fn Database::try_bitpos(self : Database, key : String, bit : Int, start : Int, end : Int) -> Result[Int, RedisError] {
  if bit != 0 && bit != 1 {
    return Err(RedisError::InvalidArgument)
  }
  if self.check_expired(key) {
    return Ok(-1)
  }
  match self.data.get(key) {
    Some(RedisValue::String(s)) => {
      let len = s.length()
      let s_idx = if start < 0 { len + start } else { start }
      let e_idx = if end < 0 { len + end + 1 } else { end + 1 }
      let from = if s_idx < 0 { 0 } else { s_idx }
      let to = if e_idx > len { len } else { e_idx }
      if from >= to {
        return Ok(-1)
      }
      for cu = from; cu < to; cu = cu + 1 {
        let unit = s[cu].to_int()
        for b = 0; b < 16; b = b + 1 {
          if ((unit >> b) & 1) == bit {
            return Ok(cu * 16 + b)
          }
        }
      }
      Ok(-1)
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok(-1)
  }
}

pub fn Database::bitpos(self : Database, key : String, bit : Int) -> Int {
  match self.try_bitpos(key, bit, 0, -1) {
    Ok(pos) => pos
    Err(_) => -1
  }
}

pub fn Database::try_bitop(
  self : Database,
  op : String,
  dest : String,
  src_keys : Array[String],
) -> Result[Int, RedisError] {
  let cmd = op.to_upper()
  if cmd != "NOT" && cmd != "AND" && cmd != "OR" && cmd != "XOR" {
    return Err(RedisError::InvalidArgument)
  }
  if cmd == "NOT" {
    if src_keys.length() != 1 {
      return Err(RedisError::InvalidArgument)
    }
  } else if src_keys.length() == 0 {
    return Err(RedisError::InvalidArgument)
  }
  let units : Array[Array[Int]] = []
  for skey in src_keys {
    self.check_expired(skey) |> ignore
    match self.data.get(skey) {
      Some(RedisValue::String(s)) => {
        let arr : Array[Int] = []
        for i = 0; i < s.length(); i = i + 1 {
          arr.push(s[i].to_int())
        }
        units.push(arr)
      }
      Some(_) => return Err(RedisError::WrongType)
      None => units.push([])
    }
  }
  let max_len = if cmd == "NOT" {
    units[0].length()
  } else {
    let mut max_v = 0
    for arr in units {
      if arr.length() > max_v {
        max_v = arr.length()
      }
    }
    max_v
  }
  let out : Array[String] = []
  for i = 0; i < max_len; i = i + 1 {
    let first = if i < units[0].length() { units[0][i] } else { 0 }
    let unit = if cmd == "NOT" {
      0xFFFF ^ first
    } else {
      let mut acc = first
      for k = 1; k < units.length(); k = k + 1 {
        let other = if i < units[k].length() { units[k][i] } else { 0 }
        if cmd == "AND" {
          acc = acc & other
        } else if cmd == "OR" {
          acc = acc | other
        } else {
          acc = acc ^ other
        }
      }
      acc
    }
    out.push("\{Int::unsafe_to_char(unit)}")
  }
  let result = out.join("")
  match self.data.get(dest) {
    Some(RedisValue::String(_)) | None => ()
    Some(_) => return Err(RedisError::WrongType)
  }
  self.put(dest, RedisValue::String(result))
  Ok(max_len)
}

pub fn Database::bitop(self : Database, op : String, dest : String, src_keys : Array[String]) -> Int {
  match self.try_bitop(op, dest, src_keys) {
    Ok(len) => len
    Err(_) => -1
  }
}

pub fn Database::try_msetnx(
  self : Database,
  keys : Array[String],
  values : Array[String],
) -> Result[Bool, RedisError] {
  if keys.length() == 0 || keys.length() != values.length() {
    return Err(RedisError::WrongArgumentCount("msetnx"))
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
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::Append(key, value))
    return 0
  }
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
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::IncrBy(key, increment))
    return None
  }
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
    None => {
      let new_value = increment
      if new_value != new_value {
        Err(RedisError::Overflow)
      } else {
        self.put(key, RedisValue::String(format_float(new_value)))
        Ok(new_value)
      }
    }
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
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::DecrBy(key, decrement))
    return None
  }
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

