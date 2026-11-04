pub fn Database::try_zadd(self : Database, key : String, score : Float, member_val : String) -> Result[Bool, RedisError] {
  if score != score {
    return Err(RedisError::InvalidArgument)
  }
  if score > 1.7976931348623157e308 || score < -1.7976931348623157e308 {
    return Err(RedisError::InvalidArgument)
  }
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
  if !existed {
    match self.zset_lex_cache.get(key) {
      Some(cached) => self.zset_lex_cache.set(key, add_lex_member(cached, member_val))
      None => ()
    }
  }
  match self.zset_sorted_cache.get(key) {
    Some(cached) => self.zset_sorted_cache.set(key, update_sorted_items(cached, member_val, Some(score)))
    None => ()
  }
  Ok(!existed)
}

pub fn Database::zadd(self : Database, key : String, score : Float, member_val : String) -> Bool {
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::ZAdd(key, score, member_val))
    false
  } else {
    match self.try_zadd(key, score, member_val) {
      Ok(added) => added
      Err(_) => false
    }
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

fn insertion_sort(arr : Array[(String, Float)]) -> Array[(String, Float)] {
  let mut result : Array[(String, Float)] = []
  for item in arr {
    let mut i = result.length()
    while i > 0 {
      let prev = result[i - 1]
      if prev.1 < item.1 || (prev.1 == item.1 && prev.0.lexical_compare(item.0) <= 0) {
        break
      }
      i = i - 1
    }
    if i == result.length() {
      result.push(item)
    } else {
      let new_result : Array[(String, Float)] = []
      for j = 0; j < i; j = j + 1 {
        new_result.push(result[j])
      }
      new_result.push(item)
      for j = i; j < result.length(); j = j + 1 {
        new_result.push(result[j])
      }
      result = new_result
    }
  }
  result
}

pub fn sort_by_score(items : Array[(String, Float)]) -> Array[(String, Float)] {
  if items.length() <= 16 {
    insertion_sort(items)
  } else {
    merge_sort(items)
  }
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
    if left[i].1 < right[j].1 || (left[i].1 == right[j].1 && left[i].0.lexical_compare(right[j].0) <= 0) {
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
  if self.in_transaction {
    self.transaction_queue.push(TransactionOp::ZRem(key, member_val))
    true
  } else {
    if self.check_expired(key) {
      false
    } else {
      match self.data.get(key) {
        Some(RedisValue::ZSet(zset)) => {
          let existed = zset.contains(member_val)
          if existed {
            zset.remove(member_val)
            if zset.length() == 0 {
              self.remove_if_empty(key, 0)
            } else {
              self.write_through_remove_zset_members(key, [member_val])
            }
          }
          existed
        }
        _ => false
      }
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
        Some(cached) => {
          self.touch_zset_cache(key)
          cached
        }
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
          self.touch_zset_cache(key)
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
      members.sort_by((a, b) => a.lexical_compare(b))
      self.zset_lex_cache.set(key, members)
      members
    }
  }
}

fn find_insert_position(
  items : Array[(String, Float)],
  score : Float,
  member_val : String,
) -> Int {
  let mut left = 0
  let mut right = items.length()
  while left < right {
    let mid = (left + right) / 2
    let mid_item = items.get(mid).unwrap()
    if mid_item.1 < score || (mid_item.1 == score && mid_item.0.lexical_compare(member_val) < 0) {
      left = mid + 1
    } else {
      right = mid
    }
  }
  left
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
      let pos = find_insert_position(base, score, member_val)
      let result : Array[(String, Float)] = []
      for i = 0; i < pos; i = i + 1 {
        result.push(base.get(i).unwrap())
      }
      result.push((member_val, score))
      for i = pos; i < base.length(); i = i + 1 {
        result.push(base.get(i).unwrap())
      }
      result
    }
    None => base
  }
}

fn remove_sorted_members(
  items : Array[(String, Float)],
  removed : Map[String, Bool],
) -> Array[(String, Float)] {
  let result : Array[(String, Float)] = []
  for item in items {
    if !removed.contains(item.0) {
      result.push(item)
    }
  }
  result
}

fn remove_lex_members(items : Array[String], removed : Map[String, Bool]) -> Array[String] {
  let result : Array[String] = []
  for item in items {
    if !removed.contains(item) {
      result.push(item)
    }
  }
  result
}

fn add_lex_member(items : Array[String], member_val : String) -> Array[String] {
  for item in items {
    if item == member_val {
      return items
    }
  }
  let result : Array[String] = []
  let mut inserted = false
  for item in items {
    if !inserted && member_val.lexical_compare(item) <= 0 {
      result.push(member_val)
      inserted = true
    }
    result.push(item)
  }
  if !inserted {
    result.push(member_val)
  }
  result
}

fn Database::write_through_remove_zset_members(
  self : Database,
  key : String,
  members : Array[String],
) -> Unit {
  let removed = Map([], capacity=members.length())
  for member_val in members {
    removed.set(member_val, true)
  }
  match self.zset_sorted_cache.get(key) {
    Some(items) => self.zset_sorted_cache.set(key, remove_sorted_members(items, removed))
    None => ()
  }
  match self.zset_lex_cache.get(key) {
    Some(items) => self.zset_lex_cache.set(key, remove_lex_members(items, removed))
    None => ()
  }
  self.zset_rank_cache.remove(key)
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

pub fn Database::zrangebyscore_with_limit(
  self : Database,
  key : String,
  min_score : Float,
  max_score : Float,
  offset : Int,
  count : Int,
) -> Array[String] {
  let sorted = self.get_sorted_zset(key)
  let lo = lower_bound(sorted, min_score)
  let hi = upper_bound(sorted, max_score)
  let result = []
  let mut i = lo
  if offset > 0 {
    i = i + offset
    if i >= hi {
      return result
    }
  }
  let mut taken = 0
  while i < hi && (count <= 0 || taken < count) {
    result.push(sorted[i].0)
    i = i + 1
    taken = taken + 1
  }
  result
}

pub fn Database::zrangebyscore(self : Database, key : String, min_score : Float, max_score : Float) -> Array[String] {
  self.zrangebyscore_with_limit(key, min_score, max_score, 0, 0)
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
  let existed = zset.contains(member_val)
  let current_score = match zset.get(member_val) {
    Some(s) => s
    None => 0.0
  }
  let new_score = current_score + increment
  zset.set(member_val, new_score)
  self.data.set(key, RedisValue::ZSet(zset))
  self.zset_rank_cache.remove(key)
  if !existed {
    match self.zset_lex_cache.get(key) {
      Some(cached) => self.zset_lex_cache.set(key, add_lex_member(cached, member_val))
      None => ()
    }
  }
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
        let removed_members : Array[String] = []
        for i = s; i <= end_idx && i < len; i = i + 1 {
          removed_members.push(sorted[i].0)
          zset.remove(sorted[i].0)
        }
        if removed_members.length() > 0 {
          if zset.length() == 0 {
            self.remove_if_empty(key, 0)
          } else {
            self.write_through_remove_zset_members(key, removed_members)
          }
        }
        removed_members.length()
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
        let sorted = self.get_sorted_zset(key)
        let lo = lower_bound(sorted, min_score)
        let hi = upper_bound(sorted, max_score)
        if hi <= lo {
          0
        } else {
          let to_remove = []
          for i = lo; i < hi; i = i + 1 {
            to_remove.push(sorted[i].0)
          }
          for m in to_remove {
            zset.remove(m)
          }
          if to_remove.length() > 0 {
            if zset.length() == 0 {
              self.remove_if_empty(key, 0)
            } else {
              self.write_through_remove_zset_members(key, to_remove)
            }
          }
          to_remove.length()
        }
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
          if zset.length() == 0 {
            self.remove_if_empty(key, 0)
          } else {
            self.write_through_remove_zset_members(key, [min_item.0])
          }
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
          if zset.length() == 0 {
            self.remove_if_empty(key, 0)
          } else {
            self.write_through_remove_zset_members(key, [max_item.0])
          }
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

fn Database::zsets_valid(self : Database, keys : Array[String]) -> Bool {
  for key in keys {
    if self.check_expired(key) {
      continue
    }
    match self.data.get(key) {
      Some(RedisValue::ZSet(_)) => ()
      None => ()
      Some(_) => return false
    }
  }
  true
}

fn lex_bound_valid(bound : String) -> Bool {
  if bound == "-" || bound == "+" {
    true
  } else {
    bound.length() > 0 && (bound[0] == '[' || bound[0] == '(')
  }
}

fn lex_lower_index(members : Array[String], bound : String) -> Int {
  if bound == "-" {
    0
  } else if bound == "+" {
    members.length()
  } else {
    let inclusive = bound[0] == '['
    let target = bound[1:bound.length()].to_owned()
    let mut lo = 0
    let mut hi = members.length()
    while lo < hi {
      let mid = (lo + hi) / 2
      let cmp = members[mid].lexical_compare(target)
      if cmp < 0 || (!inclusive && cmp == 0) {
        lo = mid + 1
      } else {
        hi = mid
      }
    }
    lo
  }
}

fn lex_upper_index(members : Array[String], bound : String) -> Int {
  if bound == "+" {
    members.length()
  } else if bound == "-" {
    0
  } else {
    let inclusive = bound[0] == '['
    let target = bound[1:bound.length()].to_owned()
    let mut lo = 0
    let mut hi = members.length()
    while lo < hi {
      let mid = (lo + hi) / 2
      let cmp = members[mid].lexical_compare(target)
      if cmp < 0 || (inclusive && cmp == 0) {
        lo = mid + 1
      } else {
        hi = mid
      }
    }
    lo
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
      let start = lex_lower_index(members, min_bound)
      let end = lex_upper_index(members, max_bound)
      let result : Array[String] = []
      for i = start; i < end; i = i + 1 {
        result.push(members[i])
      }
      result
    }
    _ => []
  }
}

pub fn Database::zrevrangebylex(
  self : Database,
  key : String,
  max_bound : String,
  min_bound : String,
) -> Array[String] {
  if !lex_bound_valid(min_bound) || !lex_bound_valid(max_bound) {
    return []
  }
  if self.check_expired(key) {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(_)) => {
      let members = self.get_sorted_lex(key)
      let start = lex_lower_index(members, min_bound)
      let end = lex_upper_index(members, max_bound)
      let result : Array[String] = []
      let mut index = end - 1
      while index >= start {
        result.push(members[index])
        index = index - 1
      }
      result
    }
    _ => []
  }
}

pub fn Database::zlexcount(
  self : Database,
  key : String,
  min_bound : String,
  max_bound : String,
) -> Int {
  if !lex_bound_valid(min_bound) || !lex_bound_valid(max_bound) {
    return 0
  }
  if self.check_expired(key) {
    return 0
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(_)) => {
      let members = self.get_sorted_lex(key)
      let start = lex_lower_index(members, min_bound)
      let end = lex_upper_index(members, max_bound)
      if end > start {
        end - start
      } else {
        0
      }
    }
    _ => 0
  }
}

pub fn Database::zremrangebylex(
  self : Database,
  key : String,
  min_bound : String,
  max_bound : String,
) -> Int {
  if !lex_bound_valid(min_bound) || !lex_bound_valid(max_bound) {
    return 0
  }
  if self.check_expired(key) {
    return 0
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(zset)) => {
      let members = self.get_sorted_lex(key)
      let start = lex_lower_index(members, min_bound)
      let end = lex_upper_index(members, max_bound)
      let to_remove : Array[String] = []
      for i = start; i < end; i = i + 1 {
        to_remove.push(members[i])
      }
      for member_val in to_remove {
        zset.remove(member_val)
      }
      if to_remove.length() > 0 {
        if zset.length() == 0 {
          self.remove_if_empty(key, 0)
        } else {
          self.write_through_remove_zset_members(key, to_remove)
        }
      }
      to_remove.length()
    }
    _ => 0
  }
}

pub fn Database::zrandmember(self : Database, key : String) -> String? {
  if self.check_expired(key) {
    return None
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(zset)) => {
      if zset.length() == 0 {
        None
      } else {
        let target = self.next_rand() % zset.length()
        let mut index = 0
        let mut result : String? = None
        for member_val, _ in zset {
          if index == target {
            result = Some(member_val)
            break
          }
          index = index + 1
        }
        result
      }
    }
    _ => None
  }
}

pub fn Database::zunion(self : Database, keys : Array[String]) -> Array[(String, Float)] {
  let scores : Map[String, Float] = Map([], capacity=10)
  for key in keys {
    if self.check_expired(key) {
      continue
    }
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) =>
        for member_val, score in zset {
          let current = match scores.get(member_val) {
            Some(value) => value
            None => 0.0
          }
          scores.set(member_val, current + score)
        }
      _ => ()
    }
  }
  let items : Array[(String, Float)] = []
  for member_val, score in scores {
    items.push((member_val, score))
  }
  sort_by_score(items)
}

pub fn Database::zinter(self : Database, keys : Array[String]) -> Array[(String, Float)] {
  if keys.length() == 0 {
    return []
  }
  let first = if self.check_expired(keys[0]) {
    return []
  } else {
    match self.data.get(keys[0]) {
      Some(RedisValue::ZSet(zset)) => zset
      _ => return []
    }
  }
  let scores : Map[String, Float] = Map([], capacity=10)
  for member_val, first_score in first {
    let mut total = first_score
    let mut present = true
    for i = 1; i < keys.length(); i = i + 1 {
      if self.check_expired(keys[i]) {
        present = false
        break
      }
      match self.data.get(keys[i]) {
        Some(RedisValue::ZSet(zset)) =>
          match zset.get(member_val) {
            Some(score) => total = total + score
            None => {
              present = false
              break
            }
          }
        _ => {
          present = false
          break
        }
      }
    }
    if present {
      scores.set(member_val, total)
    }
  }
  let items : Array[(String, Float)] = []
  for member_val, score in scores {
    items.push((member_val, score))
  }
  sort_by_score(items)
}

pub fn Database::zdiff_with_scores(self : Database, keys : Array[String]) -> Array[(String, Float)] {
  if keys.length() == 0 {
    return []
  }
  let first = if self.check_expired(keys[0]) {
    return []
  } else {
    match self.data.get(keys[0]) {
      Some(RedisValue::ZSet(zset)) => zset
      _ => return []
    }
  }
  let items : Array[(String, Float)] = []
  for member_val, score in first {
    let mut keep = true
    for i = 1; i < keys.length(); i = i + 1 {
      if self.check_expired(keys[i]) {
        continue
      }
      match self.data.get(keys[i]) {
        Some(RedisValue::ZSet(zset)) =>
          if zset.contains(member_val) {
            keep = false
            break
          }
        _ => ()
      }
    }
    if keep {
      items.push((member_val, score))
    }
  }
  sort_by_score(items)
}

pub fn Database::zdiff(self : Database, keys : Array[String]) -> Array[String] {
  let items = self.zdiff_with_scores(keys)
  let result : Array[String] = []
  for item in items {
    result.push(item.0)
  }
  result
}

pub fn Database::zscan_with_match(
  self : Database,
  key : String,
  cursor : Int,
  count : Int,
  pattern : String?,
) -> (Int, Array[(String, Float)]) {
  if self.check_expired(key) {
    return (0, [])
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(_)) => {
      let items = self.get_sorted_zset(key)
      let len = items.length()
      let limit = if count <= 0 { 10 } else { count }
      let start = if cursor < 0 { 0 } else { cursor }
      if start >= len {
        return (0, [])
      }
      let result : Array[(String, Float)] = []
      let mut i = start
      let mut taken = 0
      while i < len && taken < limit {
        let item = items[i]
        if pattern_match(item.0, pattern) {
          result.push(item)
          taken = taken + 1
        }
        i = i + 1
      }
      let next = if i >= len { 0 } else { i }
      (next, result)
    }
    _ => (0, [])
  }
}

pub fn Database::zscan(
  self : Database,
  key : String,
  cursor : Int,
  count : Int,
) -> (Int, Array[(String, Float)]) {
  self.zscan_with_match(key, cursor, count, None)
}

pub fn Database::try_zscan(
  self : Database,
  key : String,
  cursor : Int,
  count : Int,
) -> Result[(Int, Array[(String, Float)]), RedisError] {
  if self.check_expired(key) {
    Ok((0, []))
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(_)) => Ok(self.zscan(key, cursor, count))
      Some(_) => Err(RedisError::WrongType)
      None => Ok((0, []))
    }
  }
}

pub fn Database::try_zrandmember(self : Database, key : String) -> Result[String, RedisError] {
  if self.check_expired(key) {
    Err(RedisError::NotFound)
  } else {
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) =>
        if zset.length() == 0 {
          Err(RedisError::NotFound)
        } else {
          match self.zrandmember(key) {
            Some(value) => Ok(value)
            None => Err(RedisError::NotFound)
          }
        }
      Some(_) => Err(RedisError::WrongType)
      None => Err(RedisError::NotFound)
    }
  }
}

pub fn Database::try_zunion(self : Database, keys : Array[String]) -> Result[Array[(String, Float)], RedisError] {
  if !self.zsets_valid(keys) {
    Err(RedisError::WrongType)
  } else {
    Ok(self.zunion(keys))
  }
}

pub fn Database::try_zinter(self : Database, keys : Array[String]) -> Result[Array[(String, Float)], RedisError] {
  if !self.zsets_valid(keys) {
    Err(RedisError::WrongType)
  } else {
    Ok(self.zinter(keys))
  }
}

pub fn Database::try_zdiff(self : Database, keys : Array[String]) -> Result[Array[(String, Float)], RedisError] {
  if !self.zsets_valid(keys) {
    Err(RedisError::WrongType)
  } else {
    Ok(self.zdiff_with_scores(keys))
  }
}

pub fn Database::try_zunionstore(
  self : Database,
  destination : String,
  keys : Array[String],
) -> Result[Int, RedisError] {
  if !self.zsets_valid(keys) {
    return Err(RedisError::WrongType)
  }
  let result = self.zunion(keys)
  let scores : Map[String, Float] = Map([], capacity=result.length())
  for pair in result {
    scores.set(pair.0, pair.1)
  }
  self.put(destination, RedisValue::ZSet(scores))
  self.expires.remove(destination)
  Ok(result.length())
}

pub fn Database::zunionstore(self : Database, destination : String, keys : Array[String]) -> Int {
  match self.try_zunionstore(destination, keys) {
    Ok(count) => count
    Err(_) => -1
  }
}

pub fn Database::try_zinterstore(
  self : Database,
  destination : String,
  keys : Array[String],
) -> Result[Int, RedisError] {
  if !self.zsets_valid(keys) {
    return Err(RedisError::WrongType)
  }
  let result = self.zinter(keys)
  let scores : Map[String, Float] = Map([], capacity=result.length())
  for pair in result {
    scores.set(pair.0, pair.1)
  }
  self.put(destination, RedisValue::ZSet(scores))
  self.expires.remove(destination)
  Ok(result.length())
}

pub fn Database::zinterstore(self : Database, destination : String, keys : Array[String]) -> Int {
  match self.try_zinterstore(destination, keys) {
    Ok(count) => count
    Err(_) => -1
  }
}

pub fn Database::try_zdiffstore(
  self : Database,
  destination : String,
  keys : Array[String],
) -> Result[Int, RedisError] {
  if !self.zsets_valid(keys) {
    return Err(RedisError::WrongType)
  }
  let result = self.zdiff_with_scores(keys)
  let scores : Map[String, Float] = Map([], capacity=result.length())
  for pair in result {
    scores.set(pair.0, pair.1)
  }
  self.put(destination, RedisValue::ZSet(scores))
  self.expires.remove(destination)
  Ok(result.length())
}

pub fn Database::zdiffstore(self : Database, destination : String, keys : Array[String]) -> Int {
  match self.try_zdiffstore(destination, keys) {
    Ok(count) => count
    Err(_) => -1
  }
}

pub fn Database::try_zintercard(
  self : Database,
  keys : Array[String],
  limit : Int,
) -> Result[Int, RedisError] {
  if !self.zsets_valid(keys) {
    return Err(RedisError::WrongType)
  }
  let count = self.zinter(keys).length()
  Ok(if limit > 0 && count > limit { limit } else { count })
}

pub fn Database::zintercard(self : Database, keys : Array[String], limit : Int) -> Int {
  match self.try_zintercard(keys, limit) {
    Ok(count) => count
    Err(_) => -1
  }
}

pub fn Database::try_zrangestore(
  self : Database,
  destination : String,
  source : String,
  start : Int,
  end : Int,
) -> Result[Int, RedisError] {
  if self.check_expired(source) {
    return Ok(0)
  }
  match self.data.get(source) {
    Some(RedisValue::ZSet(_)) => {
      let result = self.zrange_with_scores(source, start, end)
      let scores : Map[String, Float] = Map([], capacity=result.length())
      for pair in result {
        scores.set(pair.0, pair.1)
      }
      self.put(destination, RedisValue::ZSet(scores))
      self.expires.remove(destination)
      Ok(result.length())
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok(0)
  }
}

pub fn Database::zrangestore(
  self : Database,
  destination : String,
  source : String,
  start : Int,
  end : Int,
) -> Int {
  match self.try_zrangestore(destination, source, start, end) {
    Ok(count) => count
    Err(_) => -1
  }
}

pub fn Database::zrandmember_count(self : Database, key : String, count : Int) -> Array[String] {
  if self.check_expired(key) {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(zset)) => {
      let members : Array[String] = []
      for member_val, _ in zset {
        members.push(member_val)
      }
      self.random_sample(members, count)
    }
    _ => []
  }
}

pub fn Database::zpopmin_count(
  self : Database,
  key : String,
  count : Int,
) -> Array[(String, Float)] {
  if self.check_expired(key) || count <= 0 {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(zset)) => {
      let sorted = self.get_sorted_zset(key)
      let take = if count > sorted.length() { sorted.length() } else { count }
      let result : Array[(String, Float)] = []
      let removed : Array[String] = []
      for i = 0; i < take; i = i + 1 {
        result.push(sorted[i])
        removed.push(sorted[i].0)
      }
      for member_val in removed {
        zset.remove(member_val)
      }
      if take > 0 {
        if zset.length() == 0 {
          self.remove_if_empty(key, 0)
        } else {
          self.write_through_remove_zset_members(key, removed)
        }
      }
      result
    }
    _ => []
  }
}

pub fn Database::zpopmax_count(
  self : Database,
  key : String,
  count : Int,
) -> Array[(String, Float)] {
  if self.check_expired(key) || count <= 0 {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(zset)) => {
      let sorted = self.get_sorted_zset(key)
      let len = sorted.length()
      let take = if count > len { len } else { count }
      let result : Array[(String, Float)] = []
      let removed : Array[String] = []
      for i = 0; i < take; i = i + 1 {
        let item = sorted[len - 1 - i]
        result.push(item)
        removed.push(item.0)
      }
      for member_val in removed {
        zset.remove(member_val)
      }
      if take > 0 {
        if zset.length() == 0 {
          self.remove_if_empty(key, 0)
        } else {
          self.write_through_remove_zset_members(key, removed)
        }
      }
      result
    }
    _ => []
  }
}


pub fn Database::zmpop(
  self : Database,
  keys : Array[String],
  min : Bool,
  count : Int,
) -> (String, Array[(String, Float)])? {
  if keys.length() == 0 || count <= 0 {
    return None
  }
  for key in keys {
    if self.check_expired(key) {
      continue
    }
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) => {
        let sorted = self.get_sorted_zset(key)
        let len = sorted.length()
        if len == 0 {
          continue
        }
        let take = if count > len { len } else { count }
        let result : Array[(String, Float)] = []
        let removed : Array[String] = []
        if min {
          for i = 0; i < take; i = i + 1 {
            result.push(sorted[i])
            removed.push(sorted[i].0)
          }
        } else {
          for i = 0; i < take; i = i + 1 {
            let item = sorted[len - 1 - i]
            result.push(item)
            removed.push(item.0)
          }
        }
        for member_val in removed {
          zset.remove(member_val)
        }
        if take > 0 {
          if zset.length() == 0 {
            self.remove_if_empty(key, 0)
          } else {
            self.write_through_remove_zset_members(key, removed)
          }
        }
        return Some((key, result))
      }
      _ => ()
    }
  }
  None
}

pub fn Database::zrandmember_with_scores(
  self : Database,
  key : String,
  count : Int,
) -> Array[(String, Float)] {
  if self.check_expired(key) {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(zset)) => {
      let members : Array[String] = []
      for member_val, _ in zset {
        members.push(member_val)
      }
      let sampled = self.random_sample(members, count)
      let result : Array[(String, Float)] = []
      for member_val in sampled {
        match zset.get(member_val) {
          Some(score) => result.push((member_val, score))
          None => ()
        }
      }
      result
    }
    _ => []
  }
}
