pub enum RedisValue {
  String(String)
  Hash(Map[String, String])
  List(Deque)
  Set(Map[String, Bool])
  ZSet(Map[String, Float])
}

pub struct Database {
  mut data : Map[String, RedisValue]
  mut expires : Map[String, Int64]
  mut current_time : Int64
  mut rand_state : Int
  mut zset_sorted_cache : Map[String, Array[(String, Float)]]
}

pub fn Database::new() -> Database {
  Database::{ data: Map([], capacity=1000), expires: Map([], capacity=1000), current_time: 0, rand_state: 123456789, zset_sorted_cache: Map([], capacity=100) }
}

pub fn Database::set_time(self : Database, time_ms : Int64) -> Unit {
  self.current_time = time_ms
}

pub fn Database::advance_time(self : Database, ms : Int64) -> Unit {
  self.current_time = self.current_time + ms
}

fn Database::delete_key(self : Database, key : String) -> Unit {
  self.data.remove(key)
  self.expires.remove(key)
  self.zset_sorted_cache.remove(key)
}

fn Database::put(self : Database, key : String, value : RedisValue) -> Unit {
  self.zset_sorted_cache.remove(key)
  self.data.set(key, value)
}

fn Database::sets_valid(self : Database, keys : Array[String]) -> Bool {
  for key in keys {
    if self.check_expired(key) {
      continue
    }
    match self.data.get(key) {
      Some(RedisValue::Set(_)) => ()
      None => ()
      Some(_) => return false
    }
  }
  true
}

pub fn Database::check_expired(self : Database, key : String) -> Bool {
  match self.expires.get(key) {
    Some(expire_time) => {
      if expire_time <= self.current_time {
        self.delete_key(key)
        true
      } else {
        false
      }
    }
    None => false
  }
}

fn Database::collect_expired(self : Database) -> Unit {
  let expired : Array[String] = []
  for key, expire_time in self.expires {
    if expire_time <= self.current_time {
      expired.push(key)
    }
  }
  for key in expired {
    self.delete_key(key)
  }
}

fn Database::remove_if_empty(self : Database, key : String, count : Int) -> Unit {
  if count == 0 {
    self.delete_key(key)
  }
}

fn Database::next_rand(self : Database) -> Int {
  let mut x = self.rand_state & 0x7fffffff
  if x == 0 {
    x = 1
  }
  x = x ^ (x << 13)
  x = x ^ (x >> 17)
  x = x ^ (x << 5)
  x = x & 0x7fffffff
  if x == 0 {
    x = 1
  }
  self.rand_state = x
  x
}

