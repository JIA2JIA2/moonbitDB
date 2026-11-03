pub fn Database::try_zadd(self : Database, key : String, score : Float, member_val : String) -> Result[Bool, RedisError] {
  self.check_expired(key) |> ignore
  let zset = match self.data.get(key) {
    Some(RedisValue::ZSet(z)) => z
    None => Map([], capacity=10)
    Some(_) => return Err(RedisError::WrongType)
  }
  let existed = zset.contains(member_val)
  zset.set(member_val, score)
  self.data.set(key, RedisValue::ZSet(zset))
  self.zset_rank_cache.remove(key)
  self.zset_lex_cache.remove(key)
  match self.zset_sorted_cache.get(key) {
    Some(cached) => self.zset_sorted_cache.set(key, update_sorted_items(cached, member_val, Some(score)))
    None => ()
  }
  Ok(!existed)
}

pub fn Database::zadd(self : Database, key : String, score : Float, member_val : String) -> Bool {
  match self.try_zadd(key, score, member_val) {
    Ok(added) => added
    Err(_) => false
  }
}

pub fn Database::zrange(self : Database, key : String, start : Int, end : Int) -> Array[String] {
  if self.check_expired(key) {
    []
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(_)) => {
        let sorted = self.get_sorted_zset(key)
        let len = sorted.length()
        let result : Array[String] = []
        let start_idx = if start < 0 { len + start } else { start }
        let end_idx = if end < 0 { len + end } else { end }
        let s = if start_idx < 0 { 0 } else { start_idx }
        for i = s; i <= end_idx && i < len; i = i + 1 {
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
    if left[i].1 < right[j].1 || (left[i].1 == right[j].1 && left[i].0.compare(right[j].0) <= 0) {
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

pub fn Database::try_zscore(self : Database, key : String, member_val : String) -> Result[Float, RedisError] {
  if self.check_expired(key) {
    Err(RedisError::NotFound)
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(z)) =>
        match z.get(member_val) {
          Some(score) => Ok(score)
          None => Err(RedisError::NotFound)
        }
      Some(_) => Err(RedisError::WrongType)
      None => Err(RedisError::NotFound)
    }
  }
}

pub fn Database::zscore(self : Database, key : String, member_val : String) -> Float? {
  match self.try_zscore(key, member_val) {
    Ok(score) => Some(score)
    Err(_) => None
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
          self.invalidate_zset_cache(key)
          self.remove_if_empty(key, zset.length())
        }
        existed
      }
      _ => false
    }
  }
}

fn Database::get_sorted_zset(self : Database, key : String) -> Array[(String, Float)] {
  if self.check_expired(key) {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(zset)) =>
      match self.zset_sorted_cache.get(key) {
        Some(cached) => cached
        None => {
          let items : Array[(String, Float)] = []
          for m, s in zset {
            items.push((m, s))
          }
          let sorted = sort_by_score(items)
          self.zset_sorted_cache.set(key, sorted)
          let ranks = Map([], capacity=sorted.length())
          for i = 0; i < sorted.length(); i = i + 1 {
            ranks.set(sorted[i].0, i)
          }
          self.zset_rank_cache.set(key, ranks)
          sorted
        }
      }
    _ => []
  }
}

fn Database::get_rank_map(self : Database, key : String) -> Map[String, Int] {
  match self.zset_rank_cache.get(key) {
    Some(ranks) => ranks
    None => {
      let sorted = self.get_sorted_zset(key)
      let ranks = Map([], capacity=sorted.length())
      for i = 0; i < sorted.length(); i = i + 1 {
        ranks.set(sorted[i].0, i)
      }
      self.zset_rank_cache.set(key, ranks)
      ranks
    }
  }
}

fn Database::get_sorted_lex(self : Database, key : String) -> Array[String] {
  match self.zset_lex_cache.get(key) {
    Some(cached) => cached
    None => {
      let members : Array[String] = []
      match self.data.get(key) {
        Some(RedisValue::ZSet(zset)) =>
          for member_val, _ in zset {
            members.push(member_val)
          }
        _ => ()
      }
      members.sort()
      self.zset_lex_cache.set(key, members)
      members
    }
  }
}

fn update_sorted_items(
  items : Array[(String, Float)],
  member_val : String,
  new_score : Float?,
) -> Array[(String, Float)] {
  let base : Array[(String, Float)] = []
  for item in items {
    if item.0 != member_val {
      base.push(item)
    }
  }
  match new_score {
    Some(score) => {
      let result : Array[(String, Float)] = []
      let mut inserted = false
      for item in base {
        if !inserted && (score < item.1 || (score == item.1 && member_val.compare(item.0) <= 0)) {
          result.push((member_val, score))
          inserted = true
        }
        result.push(item)
      }
      if !inserted {
        result.push((member_val, score))
      }
      result
    }
    None => base
  }
}

fn lower_bound(sorted : Array[(String, Float)], score : Float) -> Int {
  let mut lo = 0
  let mut hi = sorted.length()
  while lo < hi {
    let mid = (lo + hi) / 2
    if sorted[mid].1 < score {
      lo = mid + 1
    } else {
      hi = mid
    }
  }
  lo
}

fn upper_bound(sorted : Array[(String, Float)], score : Float) -> Int {
  let mut lo = 0
  let mut hi = sorted.length()
  while lo < hi {
    let mid = (lo + hi) / 2
    if sorted[mid].1 <= score {
      lo = mid + 1
    } else {
      hi = mid
    }
  }
  lo
}

pub fn Database::zrangebyscore(self : Database, key : String, min_score : Float, max_score : Float) -> Array[String] {
  let sorted = self.get_sorted_zset(key)
  let lo = lower_bound(sorted, min_score)
  let hi = upper_bound(sorted, max_score)
  let result = []
  for i = lo; i < hi; i = i + 1 {
    result.push(sorted[i].0)
  }
  result
}

pub fn Database::zcount(self : Database, key : String, min_score : Float, max_score : Float) -> Int {
  let sorted = self.get_sorted_zset(key)
  let lo = lower_bound(sorted, min_score)
  let hi = upper_bound(sorted, max_score)
  if hi > lo {
    hi - lo
  } else {
    0
  }
}

pub fn Database::zrevrange(self : Database, key : String, start : Int, end : Int) -> Array[String] {
  let sorted = self.get_sorted_zset(key)
  let len = sorted.length()
  let result = []
  let start_idx = if start < 0 { len + start } else { start }
  let end_idx = if end < 0 { len + end } else { end }
  let s = if start_idx < 0 { 0 } else { start_idx }
  for i = s; i <= end_idx && i < len; i = i + 1 {
    let rev_idx = len - 1 - i
    if rev_idx >= 0 {
      result.push(sorted[rev_idx].0)
    }
  }
  result
}

pub fn Database::zrevrangebyscore(self : Database, key : String, max_score : Float, min_score : Float) -> Array[String] {
  let sorted = self.get_sorted_zset(key)
  let lo = lower_bound(sorted, min_score)
  let hi = upper_bound(sorted, max_score)
  let result = []
  for i = hi - 1; i >= lo; i = i - 1 {
    result.push(sorted[i].0)
  }
  result
}

pub fn Database::zrank(self : Database, key : String, member_val : String) -> Int? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) => {
        if !zset.contains(member_val) {
          None
        } else {
          self.get_rank_map(key).get(member_val)
        }
      }
      _ => None
    }
  }
}

pub fn Database::zrevrank(self : Database, key : String, member_val : String) -> Int? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) => {
        if !zset.contains(member_val) {
          None
        } else {
          match self.get_rank_map(key).get(member_val) {
            Some(rank) => Some(zset.length() - 1 - rank)
            None => None
          }
        }
      }
      _ => None
    }
  }
}

pub fn Database::try_zincrby(self : Database, key : String, increment : Float, member_val : String) -> Result[Float, RedisError] {
  self.check_expired(key) |> ignore
  let zset = match self.data.get(key) {
    Some(RedisValue::ZSet(z)) => z
    None => Map([], capacity=10)
    Some(_) => return Err(RedisError::WrongType)
  }
  let current_score = match zset.get(member_val) {
    Some(s) => s
    None => 0.0
  }
  let new_score = current_score + increment
  zset.set(member_val, new_score)
  self.data.set(key, RedisValue::ZSet(zset))
  self.zset_rank_cache.remove(key)
  self.zset_lex_cache.remove(key)
  match self.zset_sorted_cache.get(key) {
    Some(cached) => self.zset_sorted_cache.set(key, update_sorted_items(cached, member_val, Some(new_score)))
    None => ()
  }
  Ok(new_score)
}

pub fn Database::zincrby(self : Database, key : String, increment : Float, member_val : String) -> Float {
  match self.try_zincrby(key, increment, member_val) {
    Ok(score) => score
    Err(_) => 0.0
  }
}

pub fn Database::zremrangebyrank(self : Database, key : String, start : Int, end : Int) -> Int {
  if self.check_expired(key) {
    0
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) => {
        let sorted = self.get_sorted_zset(key)
        let len = sorted.length()
        let start_idx = if start < 0 { len + start } else { start }
        let end_idx = if end < 0 { len + end } else { end }
        let s = if start_idx < 0 { 0 } else { start_idx }
        let mut removed = 0
        for i = s; i <= end_idx && i < len; i = i + 1 {
          zset.remove(sorted[i].0)
          removed = removed + 1
        }
        if removed > 0 {
          self.invalidate_zset_cache(key)
          self.remove_if_empty(key, zset.length())
        }
        removed
      }
      _ => 0
    }
  }
}

pub fn Database::zremrangebyscore(self : Database, key : String, min_score : Float, max_score : Float) -> Int {
  if self.check_expired(key) {
    0
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) => {
        let to_remove = []
        for m, s in zset {
          if s >= min_score && s <= max_score {
            to_remove.push(m)
          }
        }
        for m in to_remove {
          zset.remove(m)
        }
        if to_remove.length() > 0 {
          self.invalidate_zset_cache(key)
          self.remove_if_empty(key, zset.length())
        }
        to_remove.length()
      }
      _ => 0
    }
  }
}

pub fn Database::zpopmin(self : Database, key : String) -> (String, Float)? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) => {
        let sorted = self.get_sorted_zset(key)
        if sorted.length() == 0 {
          None
        } else {
          let min_item = sorted[0]
          zset.remove(min_item.0)
          self.invalidate_zset_cache(key)
          self.remove_if_empty(key, zset.length())
          Some(min_item)
        }
      }
      _ => None
    }
  }
}

pub fn Database::zpopmax(self : Database, key : String) -> (String, Float)? {
  if self.check_expired(key) {
    None
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) => {
        let sorted = self.get_sorted_zset(key)
        let len = sorted.length()
        if len == 0 {
          None
        } else {
          let max_item = sorted[len - 1]
          zset.remove(max_item.0)
          self.invalidate_zset_cache(key)
          self.remove_if_empty(key, zset.length())
          Some(max_item)
        }
      }
      _ => None
    }
  }
}

pub fn Database::zmscore(self : Database, key : String, members : Array[String]) -> Array[Float?] {
  if self.check_expired(key) {
    let result = []
    for _ in members {
      result.push(None)
    }
    result
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(z)) => {
        let result = []
        for m in members {
          result.push(z.get(m))
        }
        result
      }
      _ => {
        let result = []
        for _ in members {
          result.push(None)
        }
        result
      }
    }
  }
}

pub fn Database::zrange_with_scores(self : Database, key : String, start : Int, end : Int) -> Array[(String, Float)] {
  if self.check_expired(key) {
    []
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(_)) => {
        let sorted = self.get_sorted_zset(key)
        let len = sorted.length()
        let result : Array[(String, Float)] = []
        let start_idx = if start < 0 { len + start } else { start }
        let end_idx = if end < 0 { len + end } else { end }
        let s = if start_idx < 0 { 0 } else { start_idx }
        for i = s; i <= end_idx && i < len; i = i + 1 {
          result.push(sorted[i])
        }
        result
      }
      _ => []
    }
  }
}

pub fn Database::zrevrange_with_scores(self : Database, key : String, start : Int, end : Int) -> Array[(String, Float)] {
  if self.check_expired(key) {
    []
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(_)) => {
        let sorted = self.get_sorted_zset(key)
        let len = sorted.length()
        let result : Array[(String, Float)] = []
        let start_idx = if start < 0 { len + start } else { start }
        let end_idx = if end < 0 { len + end } else { end }
        let s = if start_idx < 0 { 0 } else { start_idx }
        for i = s; i <= end_idx && i < len; i = i + 1 {
          let rev_idx = len - 1 - i
          if rev_idx >= 0 {
            result.push(sorted[rev_idx])
          }
        }
        result
      }
      _ => []
    }
  }
}

fn lex_bound_valid(bound : String) -> Bool {
  if bound == "-" || bound == "+" {
    true
  } else {
    bound.length() > 0 && (bound[0] == '[' || bound[0] == '(')
  }
}

fn lex_lower_ok(member_val : String, bound : String) -> Bool {
  if bound == "-" {
    true
  } else if bound == "+" {
    false
  } else {
    let inclusive = bound[0] == '['
    let target = bound[1:bound.length()].to_owned()
    let cmp = member_val.compare(target)
    if inclusive {
      cmp >= 0
    } else {
      cmp > 0
    }
  }
}

fn lex_upper_ok(member_val : String, bound : String) -> Bool {
  if bound == "+" {
    true
  } else if bound == "-" {
    false
  } else {
    let inclusive = bound[0] == '['
    let target = bound[1:bound.length()].to_owned()
    let cmp = member_val.compare(target)
    if inclusive {
      cmp <= 0
    } else {
      cmp < 0
    }
  }
}

pub fn Database::zrangebylex(self : Database, key : String, min_bound : String, max_bound : String) -> Array[String] {
  if !lex_bound_valid(min_bound) || !lex_bound_valid(max_bound) {
    return []
  }
  if self.check_expired(key) {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(_)) => {
      let members = self.get_sorted_lex(key)
      let result = []
      for member_val in members {
        if lex_lower_ok(member_val, min_bound) && lex_upper_ok(member_val, max_bound) {
          result.push(member_val)
        }
      }
      result
    }
    _ => []
  }
}

