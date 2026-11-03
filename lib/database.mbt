pub enum RedisValue {
  String(String)
  Hash(Map[String, String])
  List(Deque)
  Set(Map[String, Bool])
  ZSet(Map[String, Float])
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
  ZAdd(String, Float, String)
  ZRem(String, String)
}

pub struct Database {
  mut data : Map[String, RedisValue]
  mut expires : Map[String, Int64]
  mut current_time : Int64
  mut rand_state : Int
  mut zset_sorted_cache : Map[String, Array[(String, Float)]]
  mut zset_rank_cache : Map[String, Map[String, Int]]
  mut zset_lex_cache : Map[String, Array[String]]
  mut scan_snapshot : Array[String]?
  mut hscan_snapshots : Map[String, Array[String]]
  mut sscan_snapshots : Map[String, Array[String]]
  mut expire_heap : Array[(String, Int64)]
  mut in_transaction : Bool
  mut transaction_queue : Array[TransactionOp]
}

pub fn Database::new() -> Database {
  Database::{ data: Map([], capacity=1000), expires: Map([], capacity=1000), current_time: 0, rand_state: 123456789, zset_sorted_cache: Map([], capacity=100), zset_rank_cache: Map([], capacity=100), zset_lex_cache: Map([], capacity=100), scan_snapshot: None, hscan_snapshots: Map([], capacity=10), sscan_snapshots: Map([], capacity=10), expire_heap: [], in_transaction: false, transaction_queue: [] }
}

pub fn Database::set_time(self : Database, time_ms : Int64) -> Unit {
  self.current_time = time_ms
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
}

fn Database::maybe_compact_expire_heap(self : Database) -> Unit {
  if self.expire_heap.length() > self.expires.length() * 2 + 16 {
    self.rebuild_expire_heap()
  }
}

fn Database::delete_key(self : Database, key : String) -> Unit {
  self.data.remove(key)
  self.expires.remove(key)
  self.invalidate_zset_cache(key)
  self.scan_snapshot = None
  self.hscan_snapshots.remove(key)
  self.sscan_snapshots.remove(key)
  self.maybe_compact_expire_heap()
}

fn Database::put(self : Database, key : String, value : RedisValue) -> Unit {
  self.invalidate_zset_cache(key)
  self.scan_snapshot = None
  self.hscan_snapshots.remove(key)
  self.sscan_snapshots.remove(key)
  self.data.set(key, value)
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
        match self.get(key) {
          Some(s) => {
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
          None => {
            self.put(key, RedisValue::String("1"))
          }
        }
      }
      TransactionOp::Decr(key) => {
        match self.get(key) {
          Some(s) => {
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
    let pool : Array[String] = []
    for item in items {
      pool.push(item)
    }
    for i = 0; i < take; i = i + 1 {
      let j = i + self.next_rand() % (len - i)
      let tmp = pool[i]
      pool[i] = pool[j]
      pool[j] = tmp
      result.push(pool[i])
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