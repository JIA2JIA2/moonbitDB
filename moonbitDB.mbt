pub struct Deque {
  mut front : Array[String]
  mut back : Array[String]
}

pub fn Deque::new() -> Deque {
  Deque::{ front: [], back: [] }
}

pub fn Deque::push_front(self : Deque, value : String) -> Unit {
  self.front.push(value)
}

pub fn Deque::push_back(self : Deque, value : String) -> Unit {
  self.back.push(value)
}

pub fn Deque::pop_front(self : Deque) -> String? {
  if self.front.length() == 0 {
    while self.back.length() > 0 {
      let item = self.back.pop()
      match item {
        Some(v) => self.front.push(v)
        None => break
      }
    }
  }
  self.front.pop()
}

pub fn Deque::pop_back(self : Deque) -> String? {
  if self.back.length() == 0 {
    while self.front.length() > 0 {
      let item = self.front.pop()
      match item {
        Some(v) => self.back.push(v)
        None => break
      }
    }
  }
  self.back.pop()
}

pub fn Deque::length(self : Deque) -> Int {
  self.front.length() + self.back.length()
}

pub fn Deque::to_array(self : Deque) -> Array[String] {
  let result = []
  for i = self.front.length() - 1; i >= 0; i = i - 1 {
    result.push(self.front[i])
  }
  for item in self.back {
    result.push(item)
  }
  result
}

pub fn Deque::get_at(self : Deque, index : Int) -> String? {
  let len = self.length()
  if index < 0 || index >= len {
    None
  } else {
    let front_len = self.front.length()
    if index < front_len {
      Some(self.front[front_len - 1 - index])
    } else {
      Some(self.back[index - front_len])
    }
  }
}

pub fn Deque::set_at(self : Deque, index : Int, value : String) -> Bool {
  let len = self.length()
  if index < 0 || index >= len {
    false
  } else {
    let front_len = self.front.length()
    if index < front_len {
      self.front[front_len - 1 - index] = value
    } else {
      self.back[index - front_len] = value
    }
    true
  }
}

pub fn Deque::remove_at(self : Deque, index : Int) -> Bool {
  let len = self.length()
  if index < 0 || index >= len {
    false
  } else {
    let front_len = self.front.length()
    if index < front_len {
      let idx = front_len - 1 - index
      let new_front : Array[String] = []
      for i = 0; i < idx; i = i + 1 {
        new_front.push(self.front[i])
      }
      for i = idx + 1; i < front_len; i = i + 1 {
        new_front.push(self.front[i])
      }
      self.front = new_front
    } else {
      let idx = index - front_len
      let new_back : Array[String] = []
      for i = 0; i < idx; i = i + 1 {
        new_back.push(self.back[i])
      }
      for i = idx + 1; i < self.back.length(); i = i + 1 {
        new_back.push(self.back[i])
      }
      self.back = new_back
    }
    true
  }
}

pub fn Deque::find_index(self : Deque, value : String) -> Int {
  let arr = self.to_array()
  for i = 0; i < arr.length(); i = i + 1 {
    if arr[i] == value {
      return i
    }
  }
  -1
}

pub fn Deque::insert_at(self : Deque, index : Int, value : String) -> Bool {
  let len = self.length()
  if index < 0 || index > len {
    false
  } else {
    let front_len = self.front.length()
    if index <= front_len {
      let idx = front_len - index
      let new_front : Array[String] = []
      for i = 0; i < idx; i = i + 1 {
        new_front.push(self.front[i])
      }
      new_front.push(value)
      for i = idx; i < front_len; i = i + 1 {
        new_front.push(self.front[i])
      }
      self.front = new_front
    } else {
      let idx = index - front_len
      let new_back : Array[String] = []
      for i = 0; i < idx; i = i + 1 {
        new_back.push(self.back[i])
      }
      new_back.push(value)
      for i = idx; i < self.back.length(); i = i + 1 {
        new_back.push(self.back[i])
      }
      self.back = new_back
    }
    true
  }
}

pub enum RedisValue {
  String(String)
  Hash(Map[String, String])
  List(Deque)
  Set(Map[String, Bool])
  ZSet(Map[String, Float])
}

pub struct Database {
  data : Map[String, RedisValue]
  expires : Map[String, Int]
  mut current_time : Int
}

pub fn Database::new() -> Database {
  Database::{ data: Map([], capacity=1000), expires: Map([], capacity=1000), current_time: 0 }
}

pub fn Database::set_time(self : Database, time_ms : Int) -> Unit {
  self.current_time = time_ms
}

pub fn Database::advance_time(self : Database, ms : Int) -> Unit {
  self.current_time = self.current_time + ms
}

pub fn Database::check_expired(self : Database, key : String) -> Bool {
  match self.expires.get(key) {
    Some(expire_time) => {
      if expire_time <= self.current_time {
        self.data.remove(key)
        self.expires.remove(key)
        true
      } else {
        false
      }
    }
    None => false
  }
}

pub fn Database::set(self : Database, key : String, value : String) -> Unit {
  self.data.set(key, RedisValue::String(value))
  self.expires.remove(key)
}

pub fn Database::expire(self : Database, key : String, seconds : Int) -> Bool {
  if self.data.contains(key) {
    self.expires.set(key, self.current_time + seconds * 1000)
    true
  } else {
    false
  }
}

pub fn Database::pexpire(self : Database, key : String, milliseconds : Int) -> Bool {
  if self.data.contains(key) {
    self.expires.set(key, self.current_time + milliseconds)
    true
  } else {
    false
  }
}

pub fn Database::ttl(self : Database, key : String) -> Int {
  if !self.data.contains(key) {
    -2
  } else if self.expires.contains(key) {
    let remaining = self.expires.get(key).unwrap() - self.current_time
    if remaining <= 0 {
      self.data.remove(key)
      self.expires.remove(key)
      -2
    } else {
      remaining / 1000
    }
  } else {
    -1
  }
}

pub fn Database::pttl(self : Database, key : String) -> Int {
  if !self.data.contains(key) {
    -2
  } else if self.expires.contains(key) {
    let remaining = self.expires.get(key).unwrap() - self.current_time
    if remaining <= 0 {
      self.data.remove(key)
      self.expires.remove(key)
      -2
    } else {
      remaining
    }
  } else {
    -1
  }
}

pub fn Database::persist(self : Database, key : String) -> Bool {
  if self.data.contains(key) && self.expires.contains(key) {
    self.expires.remove(key)
    true
  } else {
    false
  }
}

pub fn Database::mset(self : Database, keys : Array[String], values : Array[String]) -> Unit {
  for i = 0; i < keys.length() && i < values.length(); i = i + 1 {
    self.data.set(keys[i], RedisValue::String(values[i]))
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
        self.data.remove(key)
        self.expires.remove(key)
        count = count + 1
      }
    }
  }
  count
}

pub fn Database::hmset(self : Database, key : String, fields : Array[String], values : Array[String]) -> Unit {
  self.check_expired(key) |> ignore
  let hash = match self.data.get(key) {
    Some(RedisValue::Hash(h)) => h
    _ => Map([], capacity=10)
  }
  for i = 0; i < fields.length() && i < values.length(); i = i + 1 {
    hash.set(fields[i], values[i])
  }
  self.data.set(key, RedisValue::Hash(hash))
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

pub fn Database::del(self : Database, key : String) -> Bool {
  if self.check_expired(key) {
    false
  } else {
    let existed = self.data.contains(key)
    if existed {
      self.data.remove(key)
      self.expires.remove(key)
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
  let result = []
  for key, _ in self.data {
    if !self.check_expired(key) {
      result.push(key)
    }
  }
  result
}

pub fn match_pattern(pattern : String, str : String) -> Bool {
  match_pattern_helper(pattern, str, 0, 0)
}

pub fn match_pattern_helper(pattern : String, str : String, p_idx : Int, s_idx : Int) -> Bool {
  let p_len = pattern.length()
  let s_len = str.length()
  
  if p_idx == p_len {
    return s_idx == s_len
  }
  
  if p_idx < p_len && s_idx < s_len && (pattern[p_idx] == str[s_idx] || pattern[p_idx] == '?') {
    match_pattern_helper(pattern, str, p_idx + 1, s_idx + 1)
  } else if p_idx < p_len && pattern[p_idx] == '*' {
    if p_idx == p_len - 1 {
      true
    } else {
      let mut i = s_idx
      while i <= s_len {
        if match_pattern_helper(pattern, str, p_idx + 1, i) {
          return true
        }
        i = i + 1
      }
      false
    }
  } else {
    false
  }
}

pub fn Database::keys_pattern(self : Database, pattern : String) -> Array[String] {
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

pub fn Database::append(self : Database, key : String, value : String) -> Int {
  if self.check_expired(key) {
    let new_val = value
    self.data.set(key, RedisValue::String(new_val))
    new_val.length()
  } else {
    let current = match self.data.get(key) {
      Some(RedisValue::String(s)) => s
      _ => ""
    }
    let new_val = current + value
    self.data.set(key, RedisValue::String(new_val))
    new_val.length()
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

pub fn uint16_to_digit(c : UInt16) -> Int? {
  match c {
    48 => Some(0)
    49 => Some(1)
    50 => Some(2)
    51 => Some(3)
    52 => Some(4)
    53 => Some(5)
    54 => Some(6)
    55 => Some(7)
    56 => Some(8)
    57 => Some(9)
    _ => None
  }
}

pub fn digit_to_uint16(n : Int) -> UInt16 {
  match n {
    0 => 48
    1 => 49
    2 => 50
    3 => 51
    4 => 52
    5 => 53
    6 => 54
    7 => 55
    8 => 56
    9 => 57
    _ => 48
  }
}

pub fn parse_int(s : String) -> Int? {
  if s.length() == 0 {
    None
  } else {
    let mut result = 0
    let mut sign = 1
    let mut start = 0
    if s[0] == 45 {
      sign = -1
      start = 1
    } else if s[0] == 43 {
      start = 1
    }
    for i = start; i < s.length(); i = i + 1 {
      match uint16_to_digit(s[i]) {
        Some(digit) => result = result * 10 + digit
        None => return None
      }
    }
    Some(result * sign)
  }
}

pub fn int_to_string(n : Int) -> String {
  if n == 0 {
    "0"
  } else {
    let mut num = n
    let chars : Array[String] = []
    if num < 0 {
      num = -num
    }
    while num > 0 {
      match num % 10 {
        0 => chars.push("0")
        1 => chars.push("1")
        2 => chars.push("2")
        3 => chars.push("3")
        4 => chars.push("4")
        5 => chars.push("5")
        6 => chars.push("6")
        7 => chars.push("7")
        8 => chars.push("8")
        9 => chars.push("9")
        _ => chars.push("0")
      }
      num = num / 10
    }
    if n < 0 {
      chars.push("-")
    }
    let result : Array[String] = []
    for i = chars.length() - 1; i >= 0; i = i - 1 {
      result.push(chars[i])
    }
    result.join("")
  }
}

pub fn Database::incr(self : Database, key : String) -> Int? {
  self.check_expired(key) |> ignore
  let current = match self.data.get(key) {
    Some(RedisValue::String(s)) => s
    _ => "0"
  }
  match parse_int(current) {
    Some(n) => {
      let new_val = int_to_string(n + 1)
      self.data.set(key, RedisValue::String(new_val))
      self.expires.remove(key)
      Some(n + 1)
    }
    None => None
  }
}

pub fn Database::decr(self : Database, key : String) -> Int? {
  self.check_expired(key) |> ignore
  let current = match self.data.get(key) {
    Some(RedisValue::String(s)) => s
    _ => "0"
  }
  match parse_int(current) {
    Some(n) => {
      let new_val = int_to_string(n - 1)
      self.data.set(key, RedisValue::String(new_val))
      self.expires.remove(key)
      Some(n - 1)
    }
    None => None
  }
}

pub fn Database::hset(self : Database, key : String, field : String, value : String) -> Unit {
  self.check_expired(key) |> ignore
  let hash = match self.data.get(key) {
    Some(RedisValue::Hash(h)) => h
    _ => Map([], capacity=10)
  }
  hash.set(field, value)
  self.data.set(key, RedisValue::Hash(hash))
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
          self.data.set(key, RedisValue::Hash(h))
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
      Some(RedisValue::Hash(h)) => h
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

pub fn Database::lpush(self : Database, key : String, value : String) -> Int {
  self.check_expired(key) |> ignore
  let deque = match self.data.get(key) {
    Some(RedisValue::List(d)) => d
    _ => Deque::new()
  }
  deque.push_front(value)
  self.data.set(key, RedisValue::List(deque))
  deque.length()
}

pub fn Database::rpush(self : Database, key : String, value : String) -> Int {
  self.check_expired(key) |> ignore
  let deque = match self.data.get(key) {
    Some(RedisValue::List(d)) => d
    _ => Deque::new()
  }
  deque.push_back(value)
  self.data.set(key, RedisValue::List(deque))
  deque.length()
}

pub fn Database::lpop(self : Database, key : String) -> String? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::List(deque)) => {
        let val = deque.pop_front()
        self.data.set(key, RedisValue::List(deque))
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
        self.data.set(key, RedisValue::List(deque))
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
        let result = []
        for i = start_idx; i <= end_idx && i >= 0 && i < len; i = i + 1 {
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
        self.data.set(key, RedisValue::List(deque))
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
        let removed = Map([], capacity=len)
        let mut count_remaining = count
        
        if count > 0 {
          for i = 0; i < len; i = i + 1 {
            if count_remaining > 0 && arr[i] == value {
              removed.set(int_to_string(i), true)
              count_remaining = count_remaining - 1
            }
          }
        } else if count < 0 {
          for i = len - 1; i >= 0; i = i - 1 {
            if count_remaining < 0 && arr[i] == value {
              removed.set(int_to_string(i), true)
              count_remaining = count_remaining + 1
            }
          }
        } else {
          for i = 0; i < len; i = i + 1 {
            if arr[i] == value {
              removed.set(int_to_string(i), true)
            }
          }
        }
        
        let result = []
        for i = 0; i < len; i = i + 1 {
          if !removed.contains(int_to_string(i)) {
            result.push(arr[i])
          }
        }
        
        let new_deque = Deque::new()
        for item in result {
          new_deque.push_back(item)
        }
        self.data.set(key, RedisValue::List(new_deque))
        removed.keys().length()
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
          self.data.set(key, RedisValue::List(deque))
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
        self.data.set(key, RedisValue::List(deque))
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
        self.data.set(key, RedisValue::List(deque))
        deque.length()
      }
      _ => 0
    }
  }
}

pub fn Database::rpoplpush(self : Database, source : String, destination : String) -> String? {
  if self.check_expired(source) {
    None
  } else {
    match self.data.get(source) {
      Some(RedisValue::List(source_deque)) => {
        match source_deque.pop_back() {
          Some(val) => {
            self.data.set(source, RedisValue::List(source_deque))
            let dest_deque = match self.data.get(destination) {
              Some(RedisValue::List(d)) => d
              _ => Deque::new()
            }
            dest_deque.push_front(val)
            self.data.set(destination, RedisValue::List(dest_deque))
            Some(val)
          }
          None => None
        }
      }
      _ => None
    }
  }
}

pub fn Database::sadd(self : Database, key : String, value : String) -> Bool {
  self.check_expired(key) |> ignore
  let set = match self.data.get(key) {
    Some(RedisValue::Set(s)) => s
    _ => Map([], capacity=10)
  }
  if set.contains(value) {
    false
  } else {
    set.set(value, true)
    self.data.set(key, RedisValue::Set(set))
    true
  }
}

pub fn Database::smembers(self : Database, key : String) -> Array[String] {
  if self.check_expired(key) {
    []
  } else {
    match self.data.get(key) {
      Some(RedisValue::Set(s)) => {
        let result = []
        for m, _ in s {
          result.push(m)
        }
        result
      }
      _ => []
    }
  }
}

pub fn Database::srem(self : Database, key : String, value : String) -> Bool {
  if self.check_expired(key) {
    false
  } else {
    match self.data.get(key) {
      Some(RedisValue::Set(set)) => {
        let existed = set.contains(value)
        if existed {
          set.remove(value)
          self.data.set(key, RedisValue::Set(set))
        }
        existed
      }
      _ => false
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

pub fn Database::sinterstore(self : Database, destination : String, keys : Array[String]) -> Int {
  let result = self.sinter(keys)
  let new_set = Map([], capacity=result.length())
  for item in result {
    new_set.set(item, true)
  }
  self.data.set(destination, RedisValue::Set(new_set))
  result.length()
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

pub fn Database::sunionstore(self : Database, destination : String, keys : Array[String]) -> Int {
  let result = self.sunion(keys)
  let new_set = Map([], capacity=result.length())
  for item in result {
    new_set.set(item, true)
  }
  self.data.set(destination, RedisValue::Set(new_set))
  result.length()
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

pub fn Database::sdiffstore(self : Database, destination : String, keys : Array[String]) -> Int {
  let result = self.sdiff(keys)
  let new_set = Map([], capacity=result.length())
  for item in result {
    new_set.set(item, true)
  }
  self.data.set(destination, RedisValue::Set(new_set))
  result.length()
}

pub fn Database::smove(self : Database, source : String, destination : String, value : String) -> Bool {
  if self.check_expired(source) || self.check_expired(destination) {
    return false
  }
  match self.data.get(source) {
    Some(RedisValue::Set(src_set)) => {
      if !src_set.contains(value) {
        return false
      }
      src_set.remove(value)
      self.data.set(source, RedisValue::Set(src_set))
      let dest_set = match self.data.get(destination) {
        Some(RedisValue::Set(d)) => d
        _ => Map([], capacity=10)
      }
      dest_set.set(value, true)
      self.data.set(destination, RedisValue::Set(dest_set))
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
        let mut popped : String? = None
        for m, _ in set {
          popped = Some(m)
          break
        }
        match popped {
          Some(v) => {
            set.remove(v)
            self.data.set(key, RedisValue::Set(set))
            Some(v)
          }
          None => None
        }
      }
      _ => None
    }
  }
}

pub fn Database::zadd(self : Database, key : String, score : Float, member_val : String) -> Bool {
  self.check_expired(key) |> ignore
  let zset = match self.data.get(key) {
    Some(RedisValue::ZSet(z)) => z
    _ => Map([], capacity=10)
  }
  let existed = zset.contains(member_val)
  zset.set(member_val, score)
  self.data.set(key, RedisValue::ZSet(zset))
  !existed
}

pub fn Database::zrange(self : Database, key : String, start : Int, end : Int) -> Array[String] {
  if self.check_expired(key) {
    []
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) => {
        let items : Array[(String, Float)] = []
        for m, s in zset {
          items.push((m, s))
        }
        let sorted = sort_by_score(items)
        let len = sorted.length()
        let result : Array[String] = []
        let start_idx = if start < 0 { len + start } else { start }
        let end_idx = if end < 0 { len + end } else { end }
        for i = start_idx; i <= end_idx && i < len; i = i + 1 {
          result.push(sorted[i].0)
        }
        result
      }
      _ => []
    }
  }
}

pub fn sort_by_score(items : Array[(String, Float)]) -> Array[(String, Float)] {
  merge_sort(items)
}

pub fn merge_sort(arr : Array[(String, Float)]) -> Array[(String, Float)] {
  let len = arr.length()
  if len <= 1 {
    arr
  } else {
    let mid = len / 2
    let left : Array[(String, Float)] = []
    let right : Array[(String, Float)] = []
    for i = 0; i < mid; i = i + 1 {
      left.push(arr[i])
    }
    for i = mid; i < len; i = i + 1 {
      right.push(arr[i])
    }
    merge(merge_sort(left), merge_sort(right))
  }
}

pub fn merge(left : Array[(String, Float)], right : Array[(String, Float)]) -> Array[(String, Float)] {
  let result : Array[(String, Float)] = []
  let mut i = 0
  let mut j = 0
  while i < left.length() && j < right.length() {
    if left[i].1 <= right[j].1 {
      result.push(left[i])
      i = i + 1
    } else {
      result.push(right[j])
      j = j + 1
    }
  }
  while i < left.length() {
    result.push(left[i])
    i = i + 1
  }
  while j < right.length() {
    result.push(right[j])
    j = j + 1
  }
  result
}

pub fn Database::zcard(self : Database, key : String) -> Int {
  if self.check_expired(key) {
    0
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(z)) => z.length()
      _ => 0
    }
  }
}

pub fn Database::zscore(self : Database, key : String, member_val : String) -> Float? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(z)) => z.get(member_val)
      _ => None
    }
  }
}

pub fn Database::zrem(self : Database, key : String, member_val : String) -> Bool {
  if self.check_expired(key) {
    false
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) => {
        let existed = zset.contains(member_val)
        if existed {
          zset.remove(member_val)
          self.data.set(key, RedisValue::ZSet(zset))
        }
        existed
      }
      _ => false
    }
  }
}

test "cross_type_operations" {
  let db = Database::new()
  
  db.set("string_key", "hello")
  db.hset("hash_key", "field", "value")
  db.lpush("list_key", "item") |> ignore
  db.sadd("set_key", "member") |> ignore
  db.zadd("zset_key", 100.0, "zmember") |> ignore
  
  debug_inspect(db.type_of("string_key"), content="\"string\"")
  debug_inspect(db.type_of("hash_key"), content="\"hash\"")
  debug_inspect(db.type_of("list_key"), content="\"list\"")
  debug_inspect(db.type_of("set_key"), content="\"set\"")
  debug_inspect(db.type_of("zset_key"), content="\"zset\"")
  debug_inspect(db.type_of("nonexistent"), content="\"none\"")
  
  let all_keys = db.keys()
  debug_inspect(all_keys.length(), content="5")
  
  debug_inspect(db.exists("string_key"), content="true")
  db.del("string_key") |> ignore
  debug_inspect(db.exists("string_key"), content="false")
  
  let remaining = db.keys()
  debug_inspect(remaining.length(), content="4")
}

test "string_operations" {
  let db = Database::new()
  
  db.set("name", "Alice")
  debug_inspect(db.get("name"), content="Some(\"Alice\")")
  
  db.set("age", "30")
  debug_inspect(db.get("age"), content="Some(\"30\")")
  
  db.append("name", " Smith") |> ignore
  debug_inspect(db.get("name"), content="Some(\"Alice Smith\")")
  
  debug_inspect(db.strlen("name"), content="11")
}

test "string_boundary_cases" {
  let db = Database::new()
  
  debug_inspect(db.get("nonexistent"), content="None")
  debug_inspect(db.strlen("nonexistent"), content="0")
  
  db.set("empty", "")
  debug_inspect(db.get("empty"), content="Some(\"\")")
  debug_inspect(db.strlen("empty"), content="0")
  
  db.set("key1", "value1")
  db.set("key1", "value2")
  debug_inspect(db.get("key1"), content="Some(\"value2\")")
  
  db.append("non_string_key", "test") |> ignore
  debug_inspect(db.get("non_string_key"), content="Some(\"test\")")
  
  debug_inspect(db.exists("key1"), content="true")
  debug_inspect(db.exists("nonexistent"), content="false")
  
  db.del("key1") |> ignore
  debug_inspect(db.exists("key1"), content="false")
  debug_inspect(db.del("nonexistent"), content="false")
}

test "incr_decr_operations" {
  let db = Database::new()
  
  debug_inspect(db.incr("counter"), content="Some(1)")
  debug_inspect(db.incr("counter"), content="Some(2)")
  debug_inspect(db.decr("counter"), content="Some(1)")
  debug_inspect(db.decr("counter"), content="Some(0)")
  debug_inspect(db.decr("counter"), content="Some(-1)")
  
  db.set("invalid", "abc")
  debug_inspect(db.incr("invalid"), content="None")
  
  debug_inspect(db.get("counter"), content="Some(\"-1\")")
}

test "keys_pattern_operations" {
  let db = Database::new()
  
  db.set("user:1", "Alice")
  db.set("user:2", "Bob")
  db.set("user:3", "Charlie")
  db.set("product:100", "Laptop")
  db.set("config", "default")
  
  let users = db.keys_pattern("user:*")
  debug_inspect(users.length(), content="3")
  
  let user1 = db.keys_pattern("user:1")
  debug_inspect(user1.length(), content="1")
  
  let any = db.keys_pattern("*")
  debug_inspect(any.length(), content="5")
  
  let product = db.keys_pattern("product:*")
  debug_inspect(product.length(), content="1")
  
  let user_any = db.keys_pattern("user:?")
  debug_inspect(user_any.length(), content="3")
  
  let no_match = db.keys_pattern("nonexistent:*")
  debug_inspect(no_match.length(), content="0")
}

test "hash_operations" {
  let db = Database::new()
  
  db.hset("user:1", "name", "Bob")
  db.hset("user:1", "email", "bob@example.com")
  
  debug_inspect(db.hget("user:1", "name"), content="Some(\"Bob\")")
  debug_inspect(db.hget("user:1", "email"), content="Some(\"bob@example.com\")")
  
  debug_inspect(db.hlen("user:1"), content="2")
  
  db.hdel("user:1", "email") |> ignore
  debug_inspect(db.hget("user:1", "email"), content="None")
}

test "hash_boundary_cases" {
  let db = Database::new()
  
  debug_inspect(db.hget("nonexistent", "field"), content="None")
  debug_inspect(db.hlen("nonexistent"), content="0")
  debug_inspect(db.hdel("nonexistent", "field"), content="false")
  
  let empty_hash = db.hgetall("nonexistent")
  debug_inspect(empty_hash.length(), content="0")
  
  db.hset("hash1", "field1", "value1")
  db.hset("hash1", "field2", "value2")
  
  let hash_all = db.hgetall("hash1")
  debug_inspect(hash_all.length(), content="2")
  
  db.hset("hash1", "field1", "updated")
  debug_inspect(db.hget("hash1", "field1"), content="Some(\"updated\")")
  debug_inspect(db.hlen("hash1"), content="2")
  
  db.hdel("hash1", "field1") |> ignore
  debug_inspect(db.hlen("hash1"), content="1")
  
  debug_inspect(db.hdel("hash1", "nonexistent_field"), content="false")
  
  db.hset("hash2", "", "empty_field")
  debug_inspect(db.hget("hash2", ""), content="Some(\"empty_field\")")
}

test "list_operations" {
  let db = Database::new()
  
  db.lpush("mylist", "a") |> ignore
  db.lpush("mylist", "b") |> ignore
  db.rpush("mylist", "c") |> ignore
  
  debug_inspect(db.llen("mylist"), content="3")
  debug_inspect(db.lpop("mylist"), content="Some(\"b\")")
  debug_inspect(db.rpop("mylist"), content="Some(\"c\")")
  debug_inspect(db.llen("mylist"), content="1")
}

test "list_boundary_cases" {
  let db = Database::new()
  
  debug_inspect(db.llen("nonexistent"), content="0")
  debug_inspect(db.lpop("nonexistent"), content="None")
  debug_inspect(db.rpop("nonexistent"), content="None")
  
  db.lpush("empty_list", "first") |> ignore
  debug_inspect(db.llen("empty_list"), content="1")
  debug_inspect(db.lpop("empty_list"), content="Some(\"first\")")
  debug_inspect(db.llen("empty_list"), content="0")
  debug_inspect(db.lpop("empty_list"), content="None")
  
  db.rpush("single_list", "item") |> ignore
  debug_inspect(db.rpop("single_list"), content="Some(\"item\")")
  debug_inspect(db.llen("single_list"), content="0")
  
  db.lpush("multi", "x") |> ignore
  db.lpush("multi", "y") |> ignore
  db.lpush("multi", "z") |> ignore
  debug_inspect(db.llen("multi"), content="3")
  
  debug_inspect(db.lpop("multi"), content="Some(\"z\")")
  debug_inspect(db.lpop("multi"), content="Some(\"y\")")
  debug_inspect(db.lpop("multi"), content="Some(\"x\")")
  debug_inspect(db.lpop("multi"), content="None")
  
  db.rpush("reverse", "a") |> ignore
  db.rpush("reverse", "b") |> ignore
  db.rpush("reverse", "c") |> ignore
  debug_inspect(db.rpop("reverse"), content="Some(\"c\")")
  debug_inspect(db.rpop("reverse"), content="Some(\"b\")")
  debug_inspect(db.rpop("reverse"), content="Some(\"a\")")
}

test "set_operations" {
  let db = Database::new()
  
  db.sadd("myset", "apple") |> ignore
  db.sadd("myset", "banana") |> ignore
  db.sadd("myset", "apple") |> ignore
  
  debug_inspect(db.scard("myset"), content="2")
  debug_inspect(db.sismember("myset", "apple"), content="true")
  debug_inspect(db.sismember("myset", "orange"), content="false")
  
  db.srem("myset", "banana") |> ignore
  debug_inspect(db.scard("myset"), content="1")
}

test "set_boundary_cases" {
  let db = Database::new()
  
  debug_inspect(db.scard("nonexistent"), content="0")
  debug_inspect(db.sismember("nonexistent", "item"), content="false")
  debug_inspect(db.srem("nonexistent", "item"), content="false")
  
  let empty_members = db.smembers("nonexistent")
  debug_inspect(empty_members.length(), content="0")
  
  debug_inspect(db.sadd("set1", "a"), content="true")
  debug_inspect(db.sadd("set1", "b"), content="true")
  debug_inspect(db.sadd("set1", "a"), content="false")
  
  debug_inspect(db.scard("set1"), content="2")
  
  debug_inspect(db.srem("set1", "c"), content="false")
  debug_inspect(db.srem("set1", "a"), content="true")
  
  debug_inspect(db.scard("set1"), content="1")
  debug_inspect(db.sismember("set1", "a"), content="false")
  debug_inspect(db.sismember("set1", "b"), content="true")
  
  let members = db.smembers("set1")
  debug_inspect(members.length(), content="1")
  
  db.srem("set1", "b") |> ignore
  debug_inspect(db.scard("set1"), content="0")
  
  db.sadd("empty_item", "") |> ignore
  debug_inspect(db.sismember("empty_item", ""), content="true")
}

test "sorted_set_operations" {
  let db = Database::new()
  
  db.zadd("scores", 100.0, "Alice") |> ignore
  db.zadd("scores", 90.0, "Bob") |> ignore
  db.zadd("scores", 95.0, "Charlie") |> ignore
  
  debug_inspect(db.zcard("scores"), content="3")
  debug_inspect(db.zscore("scores", "Alice"), content="Some(100)")
  
  let top_two = db.zrange("scores", 1, 2)
  debug_inspect(top_two.length(), content="2")
  
  db.zrem("scores", "Bob") |> ignore
  debug_inspect(db.zcard("scores"), content="2")
}

test "sorted_set_boundary_cases" {
  let db = Database::new()
  
  debug_inspect(db.zcard("nonexistent"), content="0")
  debug_inspect(db.zscore("nonexistent", "member"), content="None")
  debug_inspect(db.zrem("nonexistent", "member"), content="false")
  
  let empty_range = db.zrange("nonexistent", 0, -1)
  debug_inspect(empty_range.length(), content="0")
  
  debug_inspect(db.zadd("zset1", 10.0, "a"), content="true")
  debug_inspect(db.zadd("zset1", 20.0, "b"), content="true")
  debug_inspect(db.zadd("zset1", 10.0, "a"), content="false")
  
  debug_inspect(db.zcard("zset1"), content="2")
  debug_inspect(db.zscore("zset1", "a"), content="Some(10)")
  
  debug_inspect(db.zadd("zset1", 15.0, "a"), content="false")
  debug_inspect(db.zscore("zset1", "a"), content="Some(15)")
  
  let all_range = db.zrange("zset1", 0, -1)
  debug_inspect(all_range.length(), content="2")
  
  let first = db.zrange("zset1", 0, 0)
  debug_inspect(first.length(), content="1")
  
  let last = db.zrange("zset1", -1, -1)
  debug_inspect(last.length(), content="1")
  
  debug_inspect(db.zrem("zset1", "c"), content="false")
  debug_inspect(db.zrem("zset1", "a"), content="true")
  
  debug_inspect(db.zcard("zset1"), content="1")
  debug_inspect(db.zscore("zset1", "a"), content="None")
  
  db.zadd("neg_scores", -5.0, "low") |> ignore
  db.zadd("neg_scores", -10.0, "lower") |> ignore
  db.zadd("neg_scores", 0.0, "zero") |> ignore
  
  let sorted_neg = db.zrange("neg_scores", 0, -1)
  debug_inspect(sorted_neg.length(), content="3")
}

test "expire_operations" {
  let db = Database::new()
  
  db.set("key1", "value1")
  debug_inspect(db.exists("key1"), content="true")
  debug_inspect(db.ttl("key1"), content="-1")
  debug_inspect(db.pttl("key1"), content="-1")
  
  debug_inspect(db.expire("key1", 60), content="true")
  debug_inspect(db.ttl("key1") >= 0, content="true")
  debug_inspect(db.pttl("key1") >= 0, content="true")
  
  debug_inspect(db.expire("nonexistent", 60), content="false")
  
  debug_inspect(db.persist("key1"), content="true")
  debug_inspect(db.ttl("key1"), content="-1")
  debug_inspect(db.pttl("key1"), content="-1")
  
  debug_inspect(db.persist("key1"), content="false")
}

test "expire_after_timeout" {
  let db = Database::new()
  
  db.set("temp_key", "temp_value")
  debug_inspect(db.get("temp_key"), content="Some(\"temp_value\")")
  
  debug_inspect(db.pexpire("temp_key", 10), content="true")
  
  db.advance_time(20)
  
  debug_inspect(db.get("temp_key"), content="None")
  debug_inspect(db.exists("temp_key"), content="false")
  debug_inspect(db.ttl("temp_key"), content="-2")
  debug_inspect(db.pttl("temp_key"), content="-2")
}

test "expire_type_check" {
  let db = Database::new()
  
  db.set("string_key", "value")
  db.hset("hash_key", "field", "value")
  db.lpush("list_key", "item") |> ignore
  db.sadd("set_key", "member") |> ignore
  db.zadd("zset_key", 100.0, "member") |> ignore
  
  debug_inspect(db.expire("string_key", 60), content="true")
  debug_inspect(db.expire("hash_key", 60), content="true")
  debug_inspect(db.expire("list_key", 60), content="true")
  debug_inspect(db.expire("set_key", 60), content="true")
  debug_inspect(db.expire("zset_key", 60), content="true")
  
  debug_inspect(db.ttl("string_key") >= 0, content="true")
  debug_inspect(db.ttl("hash_key") >= 0, content="true")
  debug_inspect(db.ttl("list_key") >= 0, content="true")
  debug_inspect(db.ttl("set_key") >= 0, content="true")
  debug_inspect(db.ttl("zset_key") >= 0, content="true")
}

test "expire_refresh_on_write" {
  let db = Database::new()
  
  db.set("key", "value")
  db.expire("key", 1) |> ignore
  debug_inspect(db.ttl("key") >= 0, content="true")
  
  db.set("key", "new_value")
  debug_inspect(db.ttl("key"), content="-1")
  
  db.set("key2", "100")
  db.expire("key2", 1) |> ignore
  debug_inspect(db.ttl("key2") >= 0, content="true")
  
  db.incr("key2") |> ignore
  debug_inspect(db.ttl("key2"), content="-1")
}

test "mset_mget_operations" {
  let db = Database::new()
  
  db.mset(["key1", "key2", "key3"], ["value1", "value2", "value3"])
  
  debug_inspect(db.get("key1"), content="Some(\"value1\")")
  debug_inspect(db.get("key2"), content="Some(\"value2\")")
  debug_inspect(db.get("key3"), content="Some(\"value3\")")
  
  let values = db.mget(["key1", "key2", "key3"])
  debug_inspect(values.length(), content="3")
  debug_inspect(values[0], content="Some(\"value1\")")
  debug_inspect(values[1], content="Some(\"value2\")")
  debug_inspect(values[2], content="Some(\"value3\")")
  
  let mixed = db.mget(["key1", "nonexistent", "key3"])
  debug_inspect(mixed.length(), content="3")
  debug_inspect(mixed[0], content="Some(\"value1\")")
  debug_inspect(mixed[1], content="None")
  debug_inspect(mixed[2], content="Some(\"value3\")")
  
  db.mset(["key2", "key4"], ["updated", "value4"])
  debug_inspect(db.get("key2"), content="Some(\"updated\")")
  debug_inspect(db.get("key4"), content="Some(\"value4\")")
}

test "mdel_operations" {
  let db = Database::new()
  
  db.mset(["a", "b", "c", "d"], ["1", "2", "3", "4"])
  
  debug_inspect(db.exists("a"), content="true")
  debug_inspect(db.exists("b"), content="true")
  debug_inspect(db.exists("c"), content="true")
  debug_inspect(db.exists("d"), content="true")
  
  let deleted = db.mdel(["a", "c", "nonexistent"])
  debug_inspect(deleted, content="2")
  
  debug_inspect(db.exists("a"), content="false")
  debug_inspect(db.exists("b"), content="true")
  debug_inspect(db.exists("c"), content="false")
  debug_inspect(db.exists("d"), content="true")
  
  let deleted_all = db.mdel(["b", "d"])
  debug_inspect(deleted_all, content="2")
  
  let remaining = db.keys()
  debug_inspect(remaining.length(), content="0")
}

test "hmset_hmget_operations" {
  let db = Database::new()
  
  db.hmset("user:1", ["name", "email", "age"], ["Alice", "alice@example.com", "30"])
  
  debug_inspect(db.hget("user:1", "name"), content="Some(\"Alice\")")
  debug_inspect(db.hget("user:1", "email"), content="Some(\"alice@example.com\")")
  debug_inspect(db.hget("user:1", "age"), content="Some(\"30\")")
  
  let fields = db.hmget("user:1", ["name", "email", "age"])
  debug_inspect(fields.length(), content="3")
  debug_inspect(fields[0], content="Some(\"Alice\")")
  debug_inspect(fields[1], content="Some(\"alice@example.com\")")
  debug_inspect(fields[2], content="Some(\"30\")")
  
  let mixed = db.hmget("user:1", ["name", "nonexistent", "age"])
  debug_inspect(mixed.length(), content="3")
  debug_inspect(mixed[0], content="Some(\"Alice\")")
  debug_inspect(mixed[1], content="None")
  debug_inspect(mixed[2], content="Some(\"30\")")
  
  db.hmset("user:1", ["age", "city"], ["31", "Beijing"])
  debug_inspect(db.hget("user:1", "age"), content="Some(\"31\")")
  debug_inspect(db.hget("user:1", "city"), content="Some(\"Beijing\")")
}

test "batch_with_expire" {
  let db = Database::new()
  
  db.mset(["temp1", "temp2"], ["value1", "value2"])
  db.expire("temp1", 1) |> ignore
  
  let before_expire = db.mget(["temp1", "temp2"])
  debug_inspect(before_expire.length(), content="2")
  debug_inspect(before_expire[0], content="Some(\"value1\")")
  debug_inspect(before_expire[1], content="Some(\"value2\")")
  
  db.advance_time(2000)
  
  let after_expire = db.mget(["temp1", "temp2"])
  debug_inspect(after_expire.length(), content="2")
  debug_inspect(after_expire[0], content="None")
  debug_inspect(after_expire[1], content="Some(\"value2\")")
  
  let deleted = db.mdel(["temp1", "temp2"])
  debug_inspect(deleted, content="1")
  
  db.hmset("temp_hash", ["a", "b"], ["1", "2"])
  db.expire("temp_hash", 1) |> ignore
  
  db.advance_time(2000)
  
  let hash_fields = db.hmget("temp_hash", ["a", "b"])
  debug_inspect(hash_fields.length(), content="2")
  debug_inspect(hash_fields[0], content="None")
  debug_inspect(hash_fields[1], content="None")
}

test "lrange_operations" {
  let db = Database::new()
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "b") |> ignore
  db.rpush("list1", "c") |> ignore
  db.rpush("list1", "d") |> ignore
  db.rpush("list1", "e") |> ignore
  
  let range1 = db.lrange("list1", 0, -1)
  debug_inspect(range1.length(), content="5")
  
  let range2 = db.lrange("list1", 0, 2)
  debug_inspect(range2.length(), content="3")
  
  let range3 = db.lrange("list1", -3, -1)
  debug_inspect(range3.length(), content="3")
  
  let range4 = db.lrange("list1", 10, 20)
  debug_inspect(range4.length(), content="0")
  
  let range5 = db.lrange("nonexistent", 0, -1)
  debug_inspect(range5.length(), content="0")
}

test "lindex_lset_operations" {
  let db = Database::new()
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "b") |> ignore
  db.rpush("list1", "c") |> ignore
  
  debug_inspect(db.lindex("list1", 0), content="Some(\"a\")")
  debug_inspect(db.lindex("list1", 1), content="Some(\"b\")")
  debug_inspect(db.lindex("list1", -1), content="Some(\"c\")")
  debug_inspect(db.lindex("list1", 10), content="None")
  debug_inspect(db.lindex("nonexistent", 0), content="None")
  
  debug_inspect(db.lset("list1", 1, "new_b"), content="true")
  debug_inspect(db.lindex("list1", 1), content="Some(\"new_b\")")
  
  debug_inspect(db.lset("list1", -1, "new_c"), content="true")
  debug_inspect(db.lindex("list1", -1), content="Some(\"new_c\")")
  
  debug_inspect(db.lset("list1", 10, "invalid"), content="false")
  debug_inspect(db.lset("nonexistent", 0, "value"), content="false")
}

test "lrem_operations" {
  let db = Database::new()
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "b") |> ignore
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "c") |> ignore
  db.rpush("list1", "a") |> ignore
  
  let removed1 = db.lrem("list1", 2, "a")
  debug_inspect(removed1, content="2")
  debug_inspect(db.llen("list1"), content="3")
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "a") |> ignore
  
  let removed2 = db.lrem("list1", -2, "a")
  debug_inspect(removed2, content="2")
  debug_inspect(db.llen("list1"), content="3")
  
  let removed3 = db.lrem("list1", 0, "a")
  debug_inspect(removed3, content="1")
  debug_inspect(db.llen("list1"), content="2")
  
  let removed4 = db.lrem("nonexistent", 1, "a")
  debug_inspect(removed4, content="0")
}

test "linsert_operations" {
  let db = Database::new()
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "b") |> ignore
  db.rpush("list1", "c") |> ignore
  
  let len1 = db.linsert("list1", true, "b", "new")
  debug_inspect(len1, content="4")
  debug_inspect(db.lindex("list1", 1), content="Some(\"new\")")
  
  let len2 = db.linsert("list1", false, "b", "after")
  debug_inspect(len2, content="5")
  debug_inspect(db.lindex("list1", 3), content="Some(\"after\")")
  
  let len3 = db.linsert("list1", true, "nonexistent", "value")
  debug_inspect(len3, content="-1")
  
  let len4 = db.linsert("nonexistent", true, "a", "value")
  debug_inspect(len4, content="-1")
}

test "lpushx_rpushx_operations" {
  let db = Database::new()
  
  let len1 = db.lpushx("nonexistent", "a")
  debug_inspect(len1, content="0")
  
  let len2 = db.rpushx("nonexistent", "a")
  debug_inspect(len2, content="0")
  
  db.rpush("list1", "a") |> ignore
  
  let len3 = db.lpushx("list1", "b")
  debug_inspect(len3, content="2")
  debug_inspect(db.lindex("list1", 0), content="Some(\"b\")")
  
  let len4 = db.rpushx("list1", "c")
  debug_inspect(len4, content="3")
  debug_inspect(db.lindex("list1", 2), content="Some(\"c\")")
}

test "rpoplpush_operations" {
  let db = Database::new()
  
  db.rpush("source", "a") |> ignore
  db.rpush("source", "b") |> ignore
  db.rpush("source", "c") |> ignore
  
  let val1 = db.rpoplpush("source", "dest")
  debug_inspect(val1, content="Some(\"c\")")
  debug_inspect(db.llen("source"), content="2")
  debug_inspect(db.llen("dest"), content="1")
  debug_inspect(db.lindex("dest", 0), content="Some(\"c\")")
  
  let val2 = db.rpoplpush("source", "dest")
  debug_inspect(val2, content="Some(\"b\")")
  debug_inspect(db.llen("source"), content="1")
  debug_inspect(db.llen("dest"), content="2")
  debug_inspect(db.lindex("dest", 0), content="Some(\"b\")")
  
  let val3 = db.rpoplpush("empty", "dest")
  debug_inspect(val3, content="None")
  
  db.rpush("rotate", "1") |> ignore
  db.rpush("rotate", "2") |> ignore
  db.rpush("rotate", "3") |> ignore
  db.rpush("rotate", "4") |> ignore
  
  let val4 = db.rpoplpush("rotate", "rotate")
  debug_inspect(val4, content="Some(\"4\")")
  debug_inspect(db.lindex("rotate", 0), content="Some(\"4\")")
  debug_inspect(db.lindex("rotate", 1), content="Some(\"1\")")
  debug_inspect(db.lindex("rotate", 2), content="Some(\"2\")")
  debug_inspect(db.lindex("rotate", 3), content="Some(\"3\")")
}

test "sinter_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  db.sadd("set1", "d") |> ignore
  
  db.sadd("set2", "b") |> ignore
  db.sadd("set2", "c") |> ignore
  db.sadd("set2", "e") |> ignore
  db.sadd("set2", "f") |> ignore
  
  db.sadd("set3", "c") |> ignore
  db.sadd("set3", "d") |> ignore
  db.sadd("set3", "g") |> ignore
  
  let inter1 = db.sinter(["set1", "set2"])
  debug_inspect(inter1.length(), content="2")
  debug_inspect(inter1.contains("b"), content="true")
  debug_inspect(inter1.contains("c"), content="true")
  
  let inter2 = db.sinter(["set1", "set2", "set3"])
  debug_inspect(inter2.length(), content="1")
  debug_inspect(inter2.contains("c"), content="true")
  
  let inter3 = db.sinter(["set1", "nonexistent"])
  debug_inspect(inter3.length(), content="0")
  
  let inter4 = db.sinter([])
  debug_inspect(inter4.length(), content="0")
}

test "sinterstore_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  
  db.sadd("set2", "b") |> ignore
  db.sadd("set2", "c") |> ignore
  db.sadd("set2", "d") |> ignore
  
  let count = db.sinterstore("dest", ["set1", "set2"])
  debug_inspect(count, content="2")
  debug_inspect(db.scard("dest"), content="2")
  debug_inspect(db.sismember("dest", "b"), content="true")
  debug_inspect(db.sismember("dest", "c"), content="true")
  debug_inspect(db.sismember("dest", "a"), content="false")
}

test "sunion_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  
  db.sadd("set2", "b") |> ignore
  db.sadd("set2", "c") |> ignore
  
  db.sadd("set3", "d") |> ignore
  
  let union1 = db.sunion(["set1", "set2"])
  debug_inspect(union1.length(), content="3")
  debug_inspect(union1.contains("a"), content="true")
  debug_inspect(union1.contains("b"), content="true")
  debug_inspect(union1.contains("c"), content="true")
  
  let union2 = db.sunion(["set1", "set2", "set3"])
  debug_inspect(union2.length(), content="4")
  
  let union3 = db.sunion(["set1", "nonexistent"])
  debug_inspect(union3.length(), content="2")
  
  let union4 = db.sunion([])
  debug_inspect(union4.length(), content="0")
}

test "sunionstore_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  
  db.sadd("set2", "c") |> ignore
  db.sadd("set2", "d") |> ignore
  
  let count = db.sunionstore("dest", ["set1", "set2"])
  debug_inspect(count, content="4")
  debug_inspect(db.scard("dest"), content="4")
  debug_inspect(db.sismember("dest", "a"), content="true")
  debug_inspect(db.sismember("dest", "d"), content="true")
}

test "sdiff_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  db.sadd("set1", "d") |> ignore
  
  db.sadd("set2", "b") |> ignore
  db.sadd("set2", "c") |> ignore
  
  db.sadd("set3", "d") |> ignore
  
  let diff1 = db.sdiff(["set1", "set2"])
  debug_inspect(diff1.length(), content="2")
  debug_inspect(diff1.contains("a"), content="true")
  debug_inspect(diff1.contains("d"), content="true")
  debug_inspect(diff1.contains("b"), content="false")
  debug_inspect(diff1.contains("c"), content="false")
  
  let diff2 = db.sdiff(["set1", "set2", "set3"])
  debug_inspect(diff2.length(), content="1")
  debug_inspect(diff2.contains("a"), content="true")
  
  let diff3 = db.sdiff(["set1", "nonexistent"])
  debug_inspect(diff3.length(), content="4")
  
  let diff4 = db.sdiff([])
  debug_inspect(diff4.length(), content="0")
}

test "sdiffstore_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  
  db.sadd("set2", "b") |> ignore
  
  let count = db.sdiffstore("dest", ["set1", "set2"])
  debug_inspect(count, content="2")
  debug_inspect(db.scard("dest"), content="2")
  debug_inspect(db.sismember("dest", "a"), content="true")
  debug_inspect(db.sismember("dest", "c"), content="true")
  debug_inspect(db.sismember("dest", "b"), content="false")
}

test "smove_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  
  db.sadd("set2", "x") |> ignore
  db.sadd("set2", "y") |> ignore
  
  debug_inspect(db.smove("set1", "set2", "b"), content="true")
  debug_inspect(db.sismember("set1", "b"), content="false")
  debug_inspect(db.sismember("set2", "b"), content="true")
  debug_inspect(db.scard("set1"), content="2")
  debug_inspect(db.scard("set2"), content="3")
  
  debug_inspect(db.smove("set1", "set2", "nonexistent"), content="false")
  
  debug_inspect(db.smove("nonexistent", "set2", "a"), content="false")
  
  debug_inspect(db.smove("set1", "newset", "a"), content="true")
  debug_inspect(db.sismember("newset", "a"), content="true")
}

test "spop_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  
  let popped = db.spop("set1")
  debug_inspect(popped is None, content="false")
  debug_inspect(db.scard("set1"), content="2")
  
  db.spop("set1") |> ignore
  db.spop("set1") |> ignore
  
  debug_inspect(db.scard("set1"), content="0")
  
  let empty_pop = db.spop("set1")
  debug_inspect(empty_pop is None, content="true")
  
  let nonexistent_pop = db.spop("nonexistent")
  debug_inspect(nonexistent_pop is None, content="true")
}

test "set_operations_with_expire" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  
  db.sadd("set2", "b") |> ignore
  db.sadd("set2", "c") |> ignore
  db.sadd("set2", "d") |> ignore
  
  db.expire("set2", 1) |> ignore
  
  db.advance_time(2000)
  
  let inter = db.sinter(["set1", "set2"])
  debug_inspect(inter.length(), content="0")
  
  let union = db.sunion(["set1", "set2"])
  debug_inspect(union.length(), content="3")
  
  let diff = db.sdiff(["set1", "set2"])
  debug_inspect(diff.length(), content="3")
}