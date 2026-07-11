pub enum RedisValue {
  String(String)
  Hash(Map[String, String])
  List(Array[String])
  Set(Map[String, Bool])
  ZSet(Map[String, Float])
}

pub struct Database {
  data : Map[String, RedisValue]
}

pub fn Database::new() -> Database {
  Database::{ data: Map([], capacity=1000) }
}

pub fn Database::set(self : Database, key : String, value : String) -> Unit {
  self.data.set(key, RedisValue::String(value))
}

pub fn Database::get(self : Database, key : String) -> String? {
  match self.data.get(key) {
    Some(RedisValue::String(s)) => Some(s)
    _ => None
  }
}

pub fn Database::del(self : Database, key : String) -> Bool {
  let existed = self.data.contains(key)
  if existed {
    self.data.remove(key)
  }
  existed
}

pub fn Database::exists(self : Database, key : String) -> Bool {
  self.data.contains(key)
}

pub fn Database::keys(self : Database) -> Array[String] {
  let result = []
  for key, _ in self.data {
    result.push(key)
  }
  result
}

pub fn match_pattern(pattern : String, str : String) -> Bool {
  let p_len = pattern.length()
  let s_len = str.length()
  let mut p_idx = 0
  let mut s_idx = 0
  
  while p_idx < p_len {
    if p_idx < p_len && s_idx < s_len && (pattern[p_idx] == str[s_idx] || pattern[p_idx] == '?') {
      p_idx = p_idx + 1
      s_idx = s_idx + 1
    } else if p_idx < p_len && pattern[p_idx] == '*' {
      if p_idx == p_len - 1 {
        return true
      }
      let mut i = s_idx
      while i <= s_len {
        if match_pattern(pattern.substring(start=p_idx + 1), str.substring(start=i)) {
          return true
        }
        i = i + 1
      }
      return false
    } else {
      return false
    }
  }
  
  s_idx == s_len
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
  match self.data.get(key) {
    Some(RedisValue::String(_)) => "string"
    Some(RedisValue::Hash(_)) => "hash"
    Some(RedisValue::List(_)) => "list"
    Some(RedisValue::Set(_)) => "set"
    Some(RedisValue::ZSet(_)) => "zset"
    None => "none"
  }
}

pub fn Database::append(self : Database, key : String, value : String) -> Int {
  let current = match self.data.get(key) {
    Some(RedisValue::String(s)) => s
    _ => ""
  }
  let new_val = current + value
  self.data.set(key, RedisValue::String(new_val))
  new_val.length()
}

pub fn Database::strlen(self : Database, key : String) -> Int {
  match self.data.get(key) {
    Some(RedisValue::String(s)) => s.length()
    _ => 0
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
    let chars : Array[UInt16] = []
    if num < 0 {
      num = -num
    }
    while num > 0 {
      chars.push(digit_to_uint16(num % 10))
      num = num / 10
    }
    if n < 0 {
      chars.push(45)
    }
    let mut result = ""
    for i = chars.length() - 1; i >= 0; i = i - 1 {
      match chars[i] {
        48 => result = result + "0"
        49 => result = result + "1"
        50 => result = result + "2"
        51 => result = result + "3"
        52 => result = result + "4"
        53 => result = result + "5"
        54 => result = result + "6"
        55 => result = result + "7"
        56 => result = result + "8"
        57 => result = result + "9"
        45 => result = result + "-"
        _ => result = result + "0"
      }
    }
    result
  }
}

pub fn Database::incr(self : Database, key : String) -> Int? {
  let current = match self.data.get(key) {
    Some(RedisValue::String(s)) => s
    _ => "0"
  }
  match parse_int(current) {
    Some(n) => {
      let new_val = int_to_string(n + 1)
      self.data.set(key, RedisValue::String(new_val))
      Some(n + 1)
    }
    None => None
  }
}

pub fn Database::decr(self : Database, key : String) -> Int? {
  let current = match self.data.get(key) {
    Some(RedisValue::String(s)) => s
    _ => "0"
  }
  match parse_int(current) {
    Some(n) => {
      let new_val = int_to_string(n - 1)
      self.data.set(key, RedisValue::String(new_val))
      Some(n - 1)
    }
    None => None
  }
}

pub fn Database::hset(self : Database, key : String, field : String, value : String) -> Unit {
  let hash = match self.data.get(key) {
    Some(RedisValue::Hash(h)) => h
    _ => Map([], capacity=10)
  }
  hash.set(field, value)
  self.data.set(key, RedisValue::Hash(hash))
}

pub fn Database::hget(self : Database, key : String, field : String) -> String? {
  match self.data.get(key) {
    Some(RedisValue::Hash(h)) => h.get(field)
    _ => None
  }
}

pub fn Database::hdel(self : Database, key : String, field : String) -> Bool {
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

pub fn Database::hgetall(self : Database, key : String) -> Map[String, String] {
  match self.data.get(key) {
    Some(RedisValue::Hash(h)) => h
    _ => Map([], capacity=0)
  }
}

pub fn Database::hlen(self : Database, key : String) -> Int {
  match self.data.get(key) {
    Some(RedisValue::Hash(h)) => h.length()
    _ => 0
  }
}

pub fn Database::lpush(self : Database, key : String, value : String) -> Int {
  let list = match self.data.get(key) {
    Some(RedisValue::List(l)) => l
    _ => []
  }
  let new_list = [value]
  for item in list {
    new_list.push(item)
  }
  self.data.set(key, RedisValue::List(new_list))
  new_list.length()
}

pub fn Database::rpush(self : Database, key : String, value : String) -> Int {
  let list = match self.data.get(key) {
    Some(RedisValue::List(l)) => l
    _ => []
  }
  let new_list = list
  new_list.push(value)
  self.data.set(key, RedisValue::List(new_list))
  new_list.length()
}

pub fn Database::lpop(self : Database, key : String) -> String? {
  match self.data.get(key) {
    Some(RedisValue::List(list)) => {
      if list.length() == 0 {
        None
      } else {
        let val = list[0]
        let new_list : Array[String] = []
        for i = 1; i < list.length(); i = i + 1 {
          new_list.push(list[i])
        }
        self.data.set(key, RedisValue::List(new_list))
        Some(val)
      }
    }
    _ => None
  }
}

pub fn Database::rpop(self : Database, key : String) -> String? {
  match self.data.get(key) {
    Some(RedisValue::List(list)) => {
      let len = list.length()
      if len == 0 {
        None
      } else {
        let val = list[len - 1]
        let new_list : Array[String] = []
        for i = 0; i < len - 1; i = i + 1 {
          new_list.push(list[i])
        }
        self.data.set(key, RedisValue::List(new_list))
        Some(val)
      }
    }
    _ => None
  }
}

pub fn Database::llen(self : Database, key : String) -> Int {
  match self.data.get(key) {
    Some(RedisValue::List(l)) => l.length()
    _ => 0
  }
}

pub fn Database::sadd(self : Database, key : String, value : String) -> Bool {
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

pub fn Database::srem(self : Database, key : String, value : String) -> Bool {
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

pub fn Database::scard(self : Database, key : String) -> Int {
  match self.data.get(key) {
    Some(RedisValue::Set(s)) => s.length()
    _ => 0
  }
}

pub fn Database::sismember(self : Database, key : String, value : String) -> Bool {
  match self.data.get(key) {
    Some(RedisValue::Set(s)) => s.contains(value)
    _ => false
  }
}

pub fn Database::zadd(self : Database, key : String, score : Float, member_val : String) -> Bool {
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

pub fn sort_by_score(items : Array[(String, Float)]) -> Array[(String, Float)] {
  let result = items
  for i = 0; i < result.length(); i = i + 1 {
    for j = i + 1; j < result.length(); j = j + 1 {
      if result[i].1 > result[j].1 {
        let temp = result[i]
        result[i] = result[j]
        result[j] = temp
      }
    }
  }
  result
}

pub fn Database::zcard(self : Database, key : String) -> Int {
  match self.data.get(key) {
    Some(RedisValue::ZSet(z)) => z.length()
    _ => 0
  }
}

pub fn Database::zscore(self : Database, key : String, member_val : String) -> Float? {
  match self.data.get(key) {
    Some(RedisValue::ZSet(z)) => z.get(member_val)
    _ => None
  }
}

pub fn Database::zrem(self : Database, key : String, member_val : String) -> Bool {
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