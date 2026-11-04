pub enum RedisValue {
  String(String)
  Hash(Map[String, String])
  List(Deque)
  Set(Map[String, Bool])
  ZSet(Map[String, Double])
}

pub enum TransactionOp {
  Set(String, String)
  Incr(String)
  Decr(String)
  HSet(String, String, String)
  HDel(String, String)
  LPush(String, String)
  RPush(String, String)
  SAdd(String, String)
  SRem(String, String)
  ZAdd(String, Double, String)
  ZRem(String, String)
  Del(String)
  Expire(String, Int)
  Persist(String)
  LPop(String)
  RPop(String)
  GetSet(String, String)
  Append(String, String)
  IncrBy(String, Int64)
  DecrBy(String, Int64)
  SetNx(String, String)
  GetDel(String)
  HSetNx(String, String, String)
  HIncrBy(String, String, Int64)
}

const MAX_CACHE_SIZE : Int = 100
const DEFAULT_SCAN_COUNT : Int = 10
const EMBSTR_MAX_SIZE : Int = 44
const HASHTABLE_MAX_ENTRIES : Int = 512
const LISTPACK_MAX_ENTRIES : Int = 128

pub struct Database {
  mut data : Map[String, RedisValue]
  mut expires : Map[String, Int64]
  mut current_time : Int64
  mut rand_state : Int
  mut zset_sorted_cache : Map[String, Array[(String, Double)]]
  mut zset_rank_cache : Map[String, Map[String, Int]]
  mut zset_lex_cache : Map[String, Array[String]]
  mut zset_cache_lru : Array[String]
  mut scan_snapshot : Array[String]?
  mut hscan_snapshots : Map[String, Array[String]]
  mut sscan_snapshots : Map[String, Array[String]]
  mut zscan_snapshots : Map[String, Array[(String, Double)]]
  mut expire_heap : Array[(String, Int64)]
  mut in_transaction : Bool
  mut transaction_queue : Array[TransactionOp]
  mut watched_keys : Map[String, Bool]
  mut watched_dirty : Map[String, Bool]
  mut start_time : Int64
  mut client_name : String
  mut config_overrides : Map[String, String]
}

pub fn Database::new() -> Database {
  Database::{ data: Map([], capacity=1000), expires: Map([], capacity=1000), current_time: 0, rand_state: 123456789, zset_sorted_cache: Map([], capacity=100), zset_rank_cache: Map([], capacity=100), zset_lex_cache: Map([], capacity=100), zset_cache_lru: [], scan_snapshot: None, hscan_snapshots: Map([], capacity=10), sscan_snapshots: Map([], capacity=10), zscan_snapshots: Map([], capacity=10), expire_heap: [], in_transaction: false, transaction_queue: [], watched_keys: Map([], capacity=10), watched_dirty: Map([], capacity=10), start_time: 0, client_name: "", config_overrides: Map([], capacity=10) }
}

pub fn Database::set_time(self : Database, time_ms : Int64) -> Unit {
  self.current_time = time_ms
  self.start_time = time_ms
}

pub fn Database::advance_time(self : Database, ms : Int64) -> Unit {
  match checked_add_i64(self.current_time, ms) {
    Some(next) => self.current_time = next
    None => ()
  }
}

fn Database::invalidate_zset_cache(self : Database, key : String) -> Unit {
  self.zset_sorted_cache.remove(key)
  self.zset_rank_cache.remove(key)
  self.zset_lex_cache.remove(key)
  let new_lru : Array[String] = []
  for k in self.zset_cache_lru {
    if k != key {
      new_lru.push(k)
    }
  }
  self.zset_cache_lru = new_lru
}

fn Database::touch_zset_cache(self : Database, key : String) -> Unit {
  let mut new_lru : Array[String] = []
  for k in self.zset_cache_lru {
    if k != key {
      new_lru.push(k)
    }
  }
  new_lru.push(key)
  if new_lru.length() > MAX_CACHE_SIZE {
    let oldest = new_lru[0]
    self.zset_sorted_cache.remove(oldest)
    self.zset_rank_cache.remove(oldest)
    self.zset_lex_cache.remove(oldest)
    let remaining : Array[String] = []
    for i = 1; i < new_lru.length(); i = i + 1 {
      remaining.push(new_lru[i])
    }
    new_lru = remaining
  }
  self.zset_cache_lru = new_lru
}

fn Database::maybe_compact_expire_heap(self : Database) -> Unit {
  if self.expire_heap.length() > self.expires.length() * 2 + 16 {
    self.rebuild_expire_heap()
  }
}

fn Database::mark_dirty(self : Database, key : String) -> Unit {
  if self.watched_keys.contains(key) {
    self.watched_dirty.set(key, true)
  }
}

fn Database::delete_key(self : Database, key : String) -> Unit {
  self.mark_dirty(key)
  self.data.remove(key)
  self.expires.remove(key)
  self.invalidate_zset_cache(key)
  self.scan_snapshot = None
  self.hscan_snapshots.remove(key)
  self.sscan_snapshots.remove(key)
  self.zscan_snapshots.remove(key)
  self.maybe_compact_expire_heap()
}

fn Database::put(self : Database, key : String, value : RedisValue) -> Unit {
  self.mark_dirty(key)
  self.invalidate_zset_cache(key)
  self.scan_snapshot = None
  self.hscan_snapshots.remove(key)
  self.sscan_snapshots.remove(key)
  self.zscan_snapshots.remove(key)
  self.data.set(key, value)
}

pub fn Database::watch(self : Database, key : String) -> Unit {
  self.check_expired(key) |> ignore
  self.watched_keys.set(key, true)
  self.watched_dirty.remove(key)
}

pub fn Database::unwatch(self : Database) -> Unit {
  self.watched_keys = Map([], capacity=10)
  self.watched_dirty = Map([], capacity=10)
}

fn Database::watched_keys_conflict(self : Database) -> Bool {
  for _key, dirty in self.watched_dirty {
    if dirty {
      return true
    }
  }
  false
}

fn Database::expire_heap_push(self : Database, key : String, expire_time : Int64) -> Unit {
  self.expire_heap.push((key, expire_time))
  let mut index = self.expire_heap.length() - 1
  while index > 0 {
    let parent = (index - 1) / 2
    if self.expire_heap[parent].1 <= self.expire_heap[index].1 {
      break
    }
    let tmp = self.expire_heap[parent]
    self.expire_heap[parent] = self.expire_heap[index]
    self.expire_heap[index] = tmp
    index = parent
  }
}

fn Database::expire_heap_pop(self : Database) -> (String, Int64)? {
  let len = self.expire_heap.length()
  if len == 0 {
    return None
  }
  let top = self.expire_heap[0]
  let last = self.expire_heap.pop().unwrap()
  let size = self.expire_heap.length()
  if size > 0 {
    self.expire_heap[0] = last
    let mut index = 0
    while true {
      let left = index * 2 + 1
      let right = index * 2 + 2
      let mut smallest = index
      if left < size && self.expire_heap[left].1 < self.expire_heap[smallest].1 {
        smallest = left
      }
      if right < size && self.expire_heap[right].1 < self.expire_heap[smallest].1 {
        smallest = right
      }
      if smallest == index {
        break
      }
      let tmp = self.expire_heap[index]
      self.expire_heap[index] = self.expire_heap[smallest]
      self.expire_heap[smallest] = tmp
      index = smallest
    }
  }
  Some(top)
}

fn Database::rebuild_expire_heap(self : Database) -> Unit {
  self.expire_heap = []
  for key, expire_time in self.expires {
    self.expire_heap_push(key, expire_time)
  }
}

fn Database::set_expire(self : Database, key : String, expire_time : Int64) -> Unit {
  self.mark_dirty(key)
  self.expires.set(key, expire_time)
  self.expire_heap_push(key, expire_time)
  self.maybe_compact_expire_heap()
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
  while self.expire_heap.length() > 0 && self.expire_heap[0].1 <= self.current_time {
    match self.expire_heap_pop() {
      Some((key, expire_time)) =>
        match self.expires.get(key) {
          Some(current) =>
            if current == expire_time {
              self.delete_key(key)
            }
          None => ()
        }
      None => break
    }
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

fn Database::begin_transaction(self : Database) -> Unit {
  self.in_transaction = true
  self.transaction_queue = []
}

fn Database::run_transaction(self : Database) -> Bool {
  if !self.in_transaction {
    return false
  }
  self.in_transaction = false
  let queue = self.transaction_queue
  self.transaction_queue = []
  for op in queue {
    match op {
      TransactionOp::Set(key, value) => {
        self.put(key, RedisValue::String(value))
        self.expires.remove(key)
      }
      TransactionOp::Incr(key) => {
        match self.data.get(key) {
          Some(RedisValue::String(s)) => {
            match parse_int64(s) {
              Some(n) => {
                match checked_add_i64(n, 1) {
                  Some(new_value) => {
                    self.put(key, RedisValue::String(int_to_string(new_value.to_int())))
                  }
                  None => ()
                }
              }
              None => ()
            }
          }
          Some(_) => ()
          None => {
            self.put(key, RedisValue::String("1"))
          }
        }
      }
      TransactionOp::Decr(key) => {
        match self.data.get(key) {
          Some(RedisValue::String(s)) => {
            match parse_int64(s) {
              Some(n) => {
                match checked_add_i64(n, -1) {
                  Some(new_value) => {
                    self.put(key, RedisValue::String(int_to_string(new_value.to_int())))
                  }
                  None => ()
                }
              }
              None => ()
            }
          }
          Some(_) => ()
          None => {
            self.put(key, RedisValue::String("-1"))
          }
        }
      }
      TransactionOp::HSet(key, field, value) => {
        self.hset(key, field, value) |> ignore
      }
      TransactionOp::HDel(key, field) => {
        self.hdel(key, field) |> ignore
      }
      TransactionOp::LPush(key, value) => {
        self.lpush(key, value) |> ignore
      }
      TransactionOp::RPush(key, value) => {
        self.rpush(key, value) |> ignore
      }
      TransactionOp::SAdd(key, member_val) => {
        self.sadd(key, member_val) |> ignore
      }
      TransactionOp::SRem(key, member_val) => {
        self.srem(key, member_val) |> ignore
      }
      TransactionOp::ZAdd(key, score, member_val) => {
        self.zadd(key, score, member_val) |> ignore
      }
      TransactionOp::ZRem(key, member_val) => {
        self.zrem(key, member_val) |> ignore
      }
      TransactionOp::Del(key) => {
        self.delete_key(key)
      }
      TransactionOp::Expire(key, seconds) => {
        match checked_add_i64(self.current_time, seconds.to_int64() * 1000) {
          Some(expire_time) => {
            if expire_time <= self.current_time {
              self.delete_key(key)
            } else {
              self.set_expire(key, expire_time)
            }
          }
          None => ()
        }
      }
      TransactionOp::Persist(key) => {
        self.expires.remove(key)
        self.maybe_compact_expire_heap()
      }
      TransactionOp::LPop(key) => {
        match self.data.get(key) {
          Some(RedisValue::List(deque)) => {
            ignore(deque.pop_front())
            self.remove_if_empty(key, deque.length())
          }
          _ => ()
        }
      }
      TransactionOp::RPop(key) => {
        match self.data.get(key) {
          Some(RedisValue::List(deque)) => {
            ignore(deque.pop_back())
            self.remove_if_empty(key, deque.length())
          }
          _ => ()
        }
      }
      TransactionOp::GetSet(key, value) => {
        match self.data.get(key) {
          Some(RedisValue::String(_)) | None => {
            self.put(key, RedisValue::String(value))
            self.expires.remove(key)
          }
          Some(_) => ()
        }
      }
      TransactionOp::Append(key, value) => {
        ignore(self.try_append(key, value))
      }
      TransactionOp::IncrBy(key, n) => {
        ignore(self.try_incrby(key, n))
      }
      TransactionOp::DecrBy(key, n) => {
        ignore(self.try_decrby(key, n))
      }
      TransactionOp::SetNx(key, value) => {
        ignore(self.try_setnx(key, value))
      }
      TransactionOp::GetDel(key) => {
        ignore(self.try_getdel(key))
      }
      TransactionOp::HSetNx(key, field, value) => {
        match self.data.get(key) {
          Some(RedisValue::Hash(h)) => {
            if !h.contains(field) {
              h.set(field, value)
              self.put(key, RedisValue::Hash(h))
            }
          }
          None => {
            let h : Map[String, String] = Map([], capacity=10)
            h.set(field, value)
            self.put(key, RedisValue::Hash(h))
          }
          Some(_) => ()
        }
      }
      TransactionOp::HIncrBy(key, field, n) => {
        match self.data.get(key) {
          Some(RedisValue::Hash(h)) => {
            let current : Int64 = match h.get(field) {
              Some(v) => match parse_int64(v) {
                Some(nv) => nv
                None => 0
              }
              None => 0
            }
            match checked_add_i64(current, n) {
              Some(new_value) => {
                h.set(field, new_value.to_string())
                self.put(key, RedisValue::Hash(h))
              }
              None => ()
            }
          }
          Some(_) => ()
          None => ()
        }
      }
    }
  }
  true
}

fn Database::clear_transaction(self : Database) -> Bool {
  if !self.in_transaction {
    return false
  }
  self.in_transaction = false
  self.transaction_queue = []
  true
}

fn Database::random_sample(self : Database, items : Array[String], count : Int) -> Array[String] {
  let len = items.length()
  let result : Array[String] = []
  if len == 0 || count == 0 {
    result
  } else if count > 0 {
    let take = if count >= len { len } else { count }
    for i = 0; i < take; i = i + 1 {
      let j = i + self.next_rand() % (len - i)
      let tmp = items[i]
      items[i] = items[j]
      items[j] = tmp
      result.push(items[i])
    }
    result
  } else {
    let take = -count
    for _i = 0; _i < take; _i = _i + 1 {
      result.push(items[self.next_rand() % len])
    }
    result
  }
}