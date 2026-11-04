pub fn Database::try_zadd(self : Database, key : String, score : Float, member_val : String) -> Result[Bool, RedisError] {
  if score != score {
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
  self.mark_dirty(key)
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

pub fn Database::try_zadd_with_options(
  self : Database,
  key : String,
  score : Float,
  member_val : String,
  nx : Bool,
  xx : Bool,
  ch : Bool,
  gt : Bool,
  lt : Bool,
) -> Result[Int, RedisError] {
  if score != score {
    return Err(RedisError::InvalidArgument)
  }
  if (nx && (xx || gt || lt)) || (gt && lt) {
    return Err(RedisError::InvalidArgument)
  }
  self.check_expired(key) |> ignore
  let zset = match self.data.get(key) {
    Some(RedisValue::ZSet(z)) => z
    None => Map([], capacity=10)
    Some(_) => return Err(RedisError::WrongType)
  }
  let mut added = 0
  let mut changed = 0
  match zset.get(member_val) {
    Some(current) => {
      if nx {
        return Ok(0)
      }
      if gt && score <= current {
        return Ok(0)
      }
      if lt && score >= current {
        return Ok(0)
      }
      if score != current {
        zset.set(member_val, score)
        changed = 1
      }
    }
    None => {
      if xx {
        return Ok(0)
      }
      zset.set(member_val, score)
      added = 1
    }
  }
  if added + changed > 0 {
    self.mark_dirty(key)
    self.data.set(key, RedisValue::ZSet(zset))
    self.zset_rank_cache.remove(key)
    if added > 0 {
      match self.zset_lex_cache.get(key) {
        Some(cached) => self.zset_lex_cache.set(key, add_lex_member(cached, member_val))
        None => ()
      }
    }
    match self.zset_sorted_cache.get(key) {
      Some(cached) => self.zset_sorted_cache.set(key, update_sorted_items(cached, member_val, Some(score)))
      None => ()
    }
  }
  Ok(if ch { added + changed } else { added })
}

pub fn Database::try_zadd_incr(
  self : Database,
  key : String,
  increment : Float,
  member_val : String,
  nx : Bool,
  xx : Bool,
  gt : Bool,
  lt : Bool,
) -> Result[Float?, RedisError] {
  if increment != increment {
    return Err(RedisError::InvalidArgument)
  }
  if (nx && (xx || gt || lt)) || (gt && lt) {
    return Err(RedisError::InvalidArgument)
  }
  self.check_expired(key) |> ignore
  let zset = match self.data.get(key) {
    Some(RedisValue::ZSet(z)) => z
    None => Map([], capacity=10)
    Some(_) => return Err(RedisError::WrongType)
  }
  match zset.get(member_val) {
    Some(current) => {
      if nx {
        return Ok(None)
      }
      let new_score = current + increment
      if gt && new_score <= current {
        return Ok(None)
      }
      if lt && new_score >= current {
        return Ok(None)
      }
      zset.set(member_val, new_score)
      self.mark_dirty(key)
      self.data.set(key, RedisValue::ZSet(zset))
      self.zset_rank_cache.remove(key)
      match self.zset_sorted_cache.get(key) {
        Some(cached) => self.zset_sorted_cache.set(key, update_sorted_items(cached, member_val, Some(new_score)))
        None => ()
      }
      Ok(Some(new_score))
    }
    None => {
      if xx {
        return Ok(None)
      }
      zset.set(member_val, increment)
      self.mark_dirty(key)
      self.data.set(key, RedisValue::ZSet(zset))
      self.zset_rank_cache.remove(key)
      match self.zset_lex_cache.get(key) {
        Some(cached) => self.zset_lex_cache.set(key, add_lex_member(cached, member_val))
        None => ()
      }
      Ok(Some(increment))
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
  for i = 1; i < arr.length(); i = i + 1 {
    let item = arr[i]
    let mut j = i - 1
    while j >= 0 {
      let prev = arr[j]
      if prev.1 < item.1 || (prev.1 == item.1 && prev.0.lexical_compare(item.0) <= 0) {
        break
      }
      arr[j + 1] = arr[j]
      j = j - 1
    }
    arr[j + 1] = item
  }
  arr
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
    return arr
  }
  let tmp : Array[(String, Float)] = []
  for i = 0; i < len; i = i + 1 {
    tmp.push(arr[i])
  }
  merge_sort_range(arr, tmp, 0, len)
  arr
}

fn merge_sort_range(arr : Array[(String, Float)], tmp : Array[(String, Float)], lo : Int, hi : Int) -> Unit {
  if hi - lo <= 1 {
    return
  }
  let mid = (lo + hi) / 2
  merge_sort_range(arr, tmp, lo, mid)
  merge_sort_range(arr, tmp, mid, hi)
  let mut i = lo
  let mut j = mid
  let mut k = lo
  while i < mid && j < hi {
    if arr[i].1 < arr[j].1 || (arr[i].1 == arr[j].1 && arr[i].0.lexical_compare(arr[j].0) <= 0) {
      tmp[k] = arr[i]
      i = i + 1
    } else {
      tmp[k] = arr[j]
      j = j + 1
    }
    k = k + 1
  }
  while i < mid {
    tmp[k] = arr[i]
    i = i + 1
    k = k + 1
  }
  while j < hi {
    tmp[k] = arr[j]
    j = j + 1
    k = k + 1
  }
  for idx = lo; idx < hi; idx = idx + 1 {
    arr[idx] = tmp[idx]
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

pub fn Database::try_zrange(
  self : Database,
  key : String,
  start : Int,
  end : Int,
  with_scores : Bool,
) -> Result[Array[String], RedisError] {
  if self.check_expired(key) {
    return Ok([])
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(_)) => {
      if with_scores {
        let items = self.zrange_with_scores(key, start, end)
        let result : Array[String] = []
        for (m, s) in items {
          result.push(m)
          result.push(format_float(s))
        }
        Ok(result)
      } else {
        Ok(self.zrange(key, start, end))
      }
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok([])
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

fn Database::ensure_rank_cache(self : Database, key : String) -> Unit {
  match self.zset_rank_cache.get(key) {
    Some(_) => ()
    None => {
      let sorted = self.get_sorted_zset(key)
      let ranks = Map([], capacity=sorted.length())
      for i = 0; i < sorted.length(); i = i + 1 {
        ranks.set(sorted[i].0, i)
      }
      self.zset_rank_cache.set(key, ranks)
    }
  }
}

fn Database::get_rank_map(self : Database, key : String) -> Map[String, Int] {
  self.ensure_rank_cache(key)
  match self.zset_rank_cache.get(key) {
    Some(ranks) => ranks
    None => Map([], capacity=0)
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

fn update_sorted_items(
  items : Array[(String, Float)],
  member_val : String,
  new_score : Float?,
) -> Array[(String, Float)] {
  let result : Array[(String, Float)] = []
  let mut inserted = match new_score {
    Some(_) => false
    None => true
  }
  let insert_score = match new_score {
    Some(s) => s
    None => 0.0
  }
  for item in items {
    if item.0 == member_val {
      continue
    }
    if !inserted {
      if item.1 > insert_score || (item.1 == insert_score && item.0.lexical_compare(member_val) >= 0) {
        result.push((member_val, insert_score))
        inserted = true
      }
    }
    result.push(item)
  }
  if !inserted {
    result.push((member_val, insert_score))
  }
  result
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
  let mut lo = 0
  let mut hi = items.length()
  while lo < hi {
    let mid = (lo + hi) / 2
    let cmp = member_val.lexical_compare(items[mid])
    if cmp == 0 {
      return items
    } else if cmp < 0 {
      hi = mid
    } else {
      lo = mid + 1
    }
  }
  let pos = lo
  let result : Array[String] = []
  for i = 0; i < items.length(); i = i + 1 {
    if i == pos {
      result.push(member_val)
    }
    result.push(items[i])
  }
  if pos >= items.length() {
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
  lower_bound_incl(sorted, score, true)
}

fn upper_bound(sorted : Array[(String, Float)], score : Float) -> Int {
  upper_bound_incl(sorted, score, true)
}

fn lower_bound_incl(sorted : Array[(String, Float)], score : Float, inclusive : Bool) -> Int {
  let mut lo = 0
  let mut hi = sorted.length()
  while lo < hi {
    let mid = (lo + hi) / 2
    if sorted[mid].1 < score || (sorted[mid].1 == score && !inclusive) {
      lo = mid + 1
    } else {
      hi = mid
    }
  }
  lo
}

fn upper_bound_incl(sorted : Array[(String, Float)], score : Float, inclusive : Bool) -> Int {
  let mut lo = 0
  let mut hi = sorted.length()
  while lo < hi {
    let mid = (lo + hi) / 2
    if sorted[mid].1 < score || (sorted[mid].1 == score && inclusive) {
      lo = mid + 1
    } else {
      hi = mid
    }
  }
  lo
}

fn float_inf(positive : Bool) -> Float {
  let zero : Float = 0.0
  let one : Float = 1.0
  if positive {
    one / zero
  } else {
    -(one / zero)
  }
}

fn parse_score_bound(bound : String) -> Result[(Float, Bool), RedisError] {
  if bound.length() == 0 {
    return Err(RedisError::InvalidArgument)
  }
  let mut exclusive = false
  let mut body = bound
  if bound[0] == 40 {
    exclusive = true
    body = bound[1:bound.length()].to_owned()
  } else if bound[0] == 91 {
    body = bound[1:bound.length()].to_owned()
  }
  if body.length() == 0 {
    return Err(RedisError::InvalidArgument)
  }
  if body == "-inf" {
    Ok((float_inf(false), exclusive))
  } else if body == "+inf" || body == "inf" {
    Ok((float_inf(true), exclusive))
  } else {
    match parse_float(body) {
      Some(value) => Ok((value, exclusive))
      None => Err(RedisError::InvalidArgument)
    }
  }
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
  if offset < 0 {
    return result
  }
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

pub fn Database::zrangebyscore_with_scores(
  self : Database,
  key : String,
  min_score : Float,
  max_score : Float,
) -> Array[(String, Float)] {
  let sorted = self.get_sorted_zset(key)
  let lo = lower_bound(sorted, min_score)
  let hi = upper_bound(sorted, max_score)
  let result : Array[(String, Float)] = []
  for i = lo; i < hi; i = i + 1 {
    result.push(sorted[i])
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

pub fn Database::zrevrangebyscore_with_limit(
  self : Database,
  key : String,
  max_score : Float,
  min_score : Float,
  offset : Int,
  count : Int,
) -> Array[String] {
  let sorted = self.get_sorted_zset(key)
  let lo = lower_bound(sorted, min_score)
  let hi = upper_bound(sorted, max_score)
  let result = []
  let total = hi - lo
  if total <= 0 || offset < 0 {
    return result
  }
  let mut i = hi - 1
  if offset > 0 {
    if offset >= total {
      return result
    }
    i = hi - 1 - offset
  }
  let mut taken = 0
  while i >= lo && (count <= 0 || taken < count) {
    result.push(sorted[i].0)
    i = i - 1
    taken = taken + 1
  }
  result
}

pub fn Database::try_zrangebyscore_by_bound(
  self : Database,
  key : String,
  min_bound : String,
  max_bound : String,
  offset : Int,
  count : Int,
) -> Result[Array[String], RedisError] {
  let min_parsed = match parse_score_bound(min_bound) {
    Ok(p) => p
    Err(e) => return Err(e)
  }
  let max_parsed = match parse_score_bound(max_bound) {
    Ok(p) => p
    Err(e) => return Err(e)
  }
  if self.check_expired(key) {
    return Ok([])
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(_)) => {
      let sorted = self.get_sorted_zset(key)
      let lo = lower_bound_incl(sorted, min_parsed.0, !min_parsed.1)
      let hi = upper_bound_incl(sorted, max_parsed.0, !max_parsed.1)
      let result = []
      let total = hi - lo
      if total <= 0 || offset < 0 {
        return Ok([])
      }
      let mut i = lo
      if offset > 0 {
        if offset >= total {
          return Ok([])
        }
        i = lo + offset
      }
      let mut taken = 0
      while i < hi && (count <= 0 || taken < count) {
        result.push(sorted[i].0)
        i = i + 1
        taken = taken + 1
      }
      Ok(result)
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok([])
  }
}

pub fn Database::try_zrevrangebyscore_by_bound(
  self : Database,
  key : String,
  max_bound : String,
  min_bound : String,
  offset : Int,
  count : Int,
) -> Result[Array[String], RedisError] {
  let min_parsed = match parse_score_bound(min_bound) {
    Ok(p) => p
    Err(e) => return Err(e)
  }
  let max_parsed = match parse_score_bound(max_bound) {
    Ok(p) => p
    Err(e) => return Err(e)
  }
  if self.check_expired(key) {
    return Ok([])
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(_)) => {
      let sorted = self.get_sorted_zset(key)
      let lo = lower_bound_incl(sorted, min_parsed.0, !min_parsed.1)
      let hi = upper_bound_incl(sorted, max_parsed.0, !max_parsed.1)
      let result = []
      let total = hi - lo
      if total <= 0 || offset < 0 {
        return Ok([])
      }
      let mut i = hi - 1
      if offset > 0 {
        if offset >= total {
          return Ok([])
        }
        i = hi - 1 - offset
      }
      let mut taken = 0
      while i >= lo && (count <= 0 || taken < count) {
        result.push(sorted[i].0)
        i = i - 1
        taken = taken + 1
      }
      Ok(result)
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok([])
  }
}

pub fn Database::try_zcount_by_bound(
  self : Database,
  key : String,
  min_bound : String,
  max_bound : String,
) -> Result[Int, RedisError] {
  let min_parsed = match parse_score_bound(min_bound) {
    Ok(p) => p
    Err(e) => return Err(e)
  }
  let max_parsed = match parse_score_bound(max_bound) {
    Ok(p) => p
    Err(e) => return Err(e)
  }
  if self.check_expired(key) {
    return Ok(0)
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(_)) => {
      let sorted = self.get_sorted_zset(key)
      let lo = lower_bound_incl(sorted, min_parsed.0, !min_parsed.1)
      let hi = upper_bound_incl(sorted, max_parsed.0, !max_parsed.1)
      if hi > lo {
        Ok(hi - lo)
      } else {
        Ok(0)
      }
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok(0)
  }
}

pub fn Database::try_zremrangebyscore_by_bound(
  self : Database,
  key : String,
  min_bound : String,
  max_bound : String,
) -> Result[Int, RedisError] {
  let min_parsed = match parse_score_bound(min_bound) {
    Ok(p) => p
    Err(e) => return Err(e)
  }
  let max_parsed = match parse_score_bound(max_bound) {
    Ok(p) => p
    Err(e) => return Err(e)
  }
  if self.check_expired(key) {
    return Ok(0)
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(zset)) => {
      let sorted = self.get_sorted_zset(key)
      let lo = lower_bound_incl(sorted, min_parsed.0, !min_parsed.1)
      let hi = upper_bound_incl(sorted, max_parsed.0, !max_parsed.1)
      if hi <= lo {
        return Ok(0)
      }
      let to_remove = []
      for i = lo; i < hi; i = i + 1 {
        to_remove.push(sorted[i].0)
      }
      ignore(self.remove_zset_collected(key, zset, to_remove))
      Ok(to_remove.length())
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok(0)
  }
}

pub fn Database::try_zrangestore_byscore(
  self : Database,
  destination : String,
  source : String,
  min_bound : String,
  max_bound : String,
  offset : Int,
  count : Int,
) -> Result[Int, RedisError] {
  let min_parsed = match parse_score_bound(min_bound) {
    Ok(p) => p
    Err(e) => return Err(e)
  }
  let max_parsed = match parse_score_bound(max_bound) {
    Ok(p) => p
    Err(e) => return Err(e)
  }
  if self.check_expired(source) {
    return Ok(0)
  }
  match self.data.get(source) {
    Some(RedisValue::ZSet(_)) => {
      let sorted = self.get_sorted_zset(source)
      let lo = lower_bound_incl(sorted, min_parsed.0, !min_parsed.1)
      let hi = upper_bound_incl(sorted, max_parsed.0, !max_parsed.1)
      let scores : Map[String, Float] = Map([], capacity=hi - lo)
      let mut taken = 0
      let mut skipped = 0
      for i = lo; i < hi; i = i + 1 {
        if offset > 0 && skipped < offset {
          skipped = skipped + 1
          continue
        }
        if count > 0 && taken >= count {
          break
        }
        scores.set(sorted[i].0, sorted[i].1)
        taken = taken + 1
      }
      self.put(destination, RedisValue::ZSet(scores))
      self.expires.remove(destination)
      Ok(scores.length())
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok(0)
  }
}

pub fn Database::try_zrangestore_bylex(
  self : Database,
  destination : String,
  source : String,
  min_bound : String,
  max_bound : String,
) -> Result[Int, RedisError] {
  if !lex_bound_valid(min_bound) || !lex_bound_valid(max_bound) {
    return Err(RedisError::InvalidArgument)
  }
  if self.check_expired(source) {
    return Ok(0)
  }
  match self.data.get(source) {
    Some(RedisValue::ZSet(_)) => {
      let members = self.get_sorted_lex(source)
      let start = lex_lower_index(members, min_bound)
      let end = lex_upper_index(members, max_bound)
      let scores : Map[String, Float] = Map([], capacity=end - start)
      for i = start; i < end; i = i + 1 {
        match self.zscore(source, members[i]) {
          Some(score) => scores.set(members[i], score)
          None => ()
        }
      }
      self.put(destination, RedisValue::ZSet(scores))
      self.expires.remove(destination)
      Ok(scores.length())
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok(0)
  }
}

pub fn Database::zrevrangebyscore_with_scores(
  self : Database,
  key : String,
  max_score : Float,
  min_score : Float,
) -> Array[(String, Float)] {
  let sorted = self.get_sorted_zset(key)
  let lo = lower_bound(sorted, min_score)
  let hi = upper_bound(sorted, max_score)
  let result : Array[(String, Float)] = []
  for i = hi - 1; i >= lo; i = i - 1 {
    result.push(sorted[i])
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
  self.mark_dirty(key)
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

fn Database::remove_zset_collected(
  self : Database,
  key : String,
  zset : Map[String, Float],
  members : Array[String],
) -> Int {
  for m in members {
    zset.remove(m)
  }
  if members.length() > 0 {
    if zset.length() == 0 {
      self.remove_if_empty(key, 0)
    } else {
      self.write_through_remove_zset_members(key, members)
    }
  }
  members.length()
}

fn Database::zrangebyscore_by_bound_with_scores_impl(
  self : Database,
  key : String,
  min_bound : String,
  max_bound : String,
  rev : Bool,
  offset : Int,
  count : Int,
) -> Result[Array[(String, Float)], RedisError] {
  let min_parsed = match parse_score_bound(min_bound) {
    Ok(p) => p
    Err(e) => return Err(e)
  }
  let max_parsed = match parse_score_bound(max_bound) {
    Ok(p) => p
    Err(e) => return Err(e)
  }
  if self.check_expired(key) {
    return Ok([])
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(_)) => {
      let sorted = self.get_sorted_zset(key)
      let lo = lower_bound_incl(sorted, min_parsed.0, !min_parsed.1)
      let hi = upper_bound_incl(sorted, max_parsed.0, !max_parsed.1)
      let result : Array[(String, Float)] = []
      let total = hi - lo
      if total <= 0 || offset < 0 {
        return Ok(result)
      }
      if rev {
        let mut i = hi - 1
        if offset > 0 {
          if offset >= total {
            return Ok(result)
          }
          i = hi - 1 - offset
        }
        let mut taken = 0
        while i >= lo && (count <= 0 || taken < count) {
          result.push(sorted[i])
          i = i - 1
          taken = taken + 1
        }
      } else {
        let mut i = lo
        if offset > 0 {
          if offset >= total {
            return Ok(result)
          }
          i = lo + offset
        }
        let mut taken = 0
        while i < hi && (count <= 0 || taken < count) {
          result.push(sorted[i])
          i = i + 1
          taken = taken + 1
        }
      }
      Ok(result)
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok([])
  }
}

fn Database::zrangebylex_with_scores_impl(
  self : Database,
  key : String,
  min_bound : String,
  max_bound : String,
  rev : Bool,
  offset : Int,
  count : Int,
) -> Result[Array[(String, Float)], RedisError] {
  if !lex_bound_valid(min_bound) || !lex_bound_valid(max_bound) {
    return Ok([])
  }
  if self.check_expired(key) {
    return Ok([])
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(zset)) => {
      let members = self.get_sorted_lex(key)
      let start = lex_lower_index(members, min_bound)
      let end = lex_upper_index(members, max_bound)
      let total = end - start
      if total <= 0 || offset < 0 {
        return Ok([])
      }
      let apply_offset = if offset > total { total } else { offset }
      let apply_count = if count <= 0 { total - apply_offset } else { count }
      let apply_end = if apply_offset + apply_count > total { total } else { apply_offset + apply_count }
      let result : Array[(String, Float)] = []
      if rev {
        let mut i = end - 1 - apply_offset
        let stop = end - apply_end
        while i >= stop {
          let m = members[i]
          match zset.get(m) {
            Some(s) => result.push((m, s))
            None => ()
          }
          i = i - 1
        }
      } else {
        for i = start + apply_offset; i < start + apply_end; i = i + 1 {
          let m = members[i]
          match zset.get(m) {
            Some(s) => result.push((m, s))
            None => ()
          }
        }
      }
      Ok(result)
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok([])
  }
}

fn pairs_to_flat(pairs : Array[(String, Float)], with_scores : Bool) -> Array[String] {
  let out : Array[String] = []
  for p in pairs {
    out.push(p.0)
    if with_scores {
      out.push(format_float(p.1))
    }
  }
  out
}

pub fn Database::try_zrange_modifiers(
  self : Database,
  key : String,
  start : String,
  end : String,
  by : String?,
  rev : Bool,
  with_scores : Bool,
  offset : Int,
  count : Int,
) -> Result[Array[String], RedisError] {
  match by {
    None => {
      let s_idx = match parse_int(start) {
        Some(v) => v
        None => return Err(RedisError::InvalidArgument)
      }
      let e_idx = match parse_int(end) {
        Some(v) => v
        None => return Err(RedisError::InvalidArgument)
      }
      if self.check_expired(key) {
        return Ok([])
      }
      match self.data.get(key) {
        Some(RedisValue::ZSet(_)) => {
          let pairs = if rev {
            self.zrevrange_with_scores(key, s_idx, e_idx)
          } else {
            self.zrange_with_scores(key, s_idx, e_idx)
          }
          Ok(pairs_to_flat(pairs, with_scores))
        }
        Some(_) => Err(RedisError::WrongType)
        None => Ok([])
      }
    }
    Some(b) => {
      let by_kind = b.to_lower()
      if by_kind == "byscore" {
        match self.zrangebyscore_by_bound_with_scores_impl(key, start, end, rev, offset, count) {
          Ok(pairs) => Ok(pairs_to_flat(pairs, with_scores))
          Err(e) => Err(e)
        }
      } else if by_kind == "bylex" {
        match self.zrangebylex_with_scores_impl(key, start, end, rev, offset, count) {
          Ok(pairs) => Ok(pairs_to_flat(pairs, with_scores))
          Err(e) => Err(e)
        }
      } else {
        Err(RedisError::InvalidArgument)
      }
    }
  }
}

pub fn Database::zrange_modifiers(
  self : Database,
  key : String,
  start : String,
  end : String,
  by : String?,
  rev : Bool,
  with_scores : Bool,
  offset : Int,
  count : Int,
) -> Array[String] {
  match self.try_zrange_modifiers(key, start, end, by, rev, with_scores, offset, count) {
    Ok(items) => items
    Err(_) => []
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
        }
        self.remove_zset_collected(key, zset, removed_members)
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
          self.remove_zset_collected(key, zset, to_remove)
        }
      }
      _ => 0
    }
  }
}

fn Database::zpop_extreme(self : Database, key : String, from_min : Bool) -> (String, Float)? {
  if self.check_expired(key) {
    return None
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(zset)) => {
      let sorted = self.get_sorted_zset(key)
      let len = sorted.length()
      if len == 0 {
        return None
      }
      let item = if from_min { sorted[0] } else { sorted[len - 1] }
      ignore(self.remove_zset_collected(key, zset, [item.0]))
      Some(item)
    }
    _ => None
  }
}

pub fn Database::zpopmin(self : Database, key : String) -> (String, Float)? {
  self.zpop_extreme(key, true)
}

pub fn Database::zpopmax(self : Database, key : String) -> (String, Float)? {
  self.zpop_extreme(key, false)
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

pub fn Database::try_zrangebylex_with_limit(
  self : Database,
  key : String,
  min_bound : String,
  max_bound : String,
  offset : Int,
  count : Int,
) -> Result[Array[String], RedisError] {
  if !lex_bound_valid(min_bound) || !lex_bound_valid(max_bound) {
    return Ok([])
  }
  if self.check_expired(key) {
    return Ok([])
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(_)) => {
      let members = self.get_sorted_lex(key)
      let start = lex_lower_index(members, min_bound)
      let end = lex_upper_index(members, max_bound)
      let total = end - start
      if offset < 0 {
        return Ok([])
      }
      let apply_offset = if offset > total { total } else { offset }
      let apply_count = if count <= 0 { total - apply_offset } else { count }
      let apply_end = if apply_offset + apply_count > total { total } else { apply_offset + apply_count }
      let result : Array[String] = []
      for i = apply_offset; i < apply_end; i = i + 1 {
        result.push(members[start + i])
      }
      Ok(result)
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok([])
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

pub fn Database::try_zrevrangebylex_with_limit(
  self : Database,
  key : String,
  max_bound : String,
  min_bound : String,
  offset : Int,
  count : Int,
) -> Result[Array[String], RedisError] {
  if !lex_bound_valid(min_bound) || !lex_bound_valid(max_bound) {
    return Ok([])
  }
  if self.check_expired(key) {
    return Ok([])
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(_)) => {
      let members = self.get_sorted_lex(key)
      let start = lex_lower_index(members, min_bound)
      let end = lex_upper_index(members, max_bound)
      let total = end - start
      if total <= 0 || offset < 0 {
        return Ok([])
      }
      let apply_offset = if offset > total { total } else { offset }
      let apply_count = if count <= 0 { total - apply_offset } else { count }
      let apply_end = if apply_offset + apply_count > total { total } else { apply_offset + apply_count }
      let result : Array[String] = []
      let mut index = end - 1 - apply_offset
      let stop = end - apply_end
      while index >= stop {
        result.push(members[index])
        index = index - 1
      }
      Ok(result)
    }
    Some(_) => Err(RedisError::WrongType)
    None => Ok([])
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
  let zsets : Array[Map[String, Float]] = []
  for key in keys {
    if self.check_expired(key) {
      return []
    }
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) => zsets.push(zset)
      _ => return []
    }
  }
  let mut min_idx = 0
  let mut min_len = zsets[0].length()
  for i = 1; i < zsets.length(); i = i + 1 {
    let len = zsets[i].length()
    if len < min_len {
      min_idx = i
      min_len = len
    }
  }
  let scores : Map[String, Float] = Map([], capacity=min_len)
  for member_val, first_score in zsets[min_idx] {
    let mut total = first_score
    let mut present = true
    for i = 0; i < zsets.length(); i = i + 1 {
      if i == min_idx {
        continue
      }
      match zsets[i].get(member_val) {
        Some(score) => total = total + score
        None => {
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
      let items = match self.zscan_snapshots.get(key) {
        Some(snapshot) => snapshot
        None => {
          let fresh = self.get_sorted_zset(key)
          self.zscan_snapshots.set(key, fresh)
          fresh
        }
      }
      let len = items.length()
      let limit = if count <= 0 { DEFAULT_SCAN_COUNT } else { count }
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

pub fn Database::try_zunionstore_with_options(
  self : Database,
  destination : String,
  keys : Array[String],
  weights : Array[Float]?,
  aggregate : String?,
) -> Result[Int, RedisError] {
  if !self.zsets_valid(keys) {
    return Err(RedisError::WrongType)
  }
  let zsets : Array[Map[String, Float]] = []
  for key in keys {
    if self.check_expired(key) {
      return Err(RedisError::WrongType)
    }
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) => zsets.push(zset)
      _ => return Err(RedisError::WrongType)
    }
  }
  let weights_arr = match weights {
    Some(w) => w
    None => {
      let default_weights : Array[Float] = []
      let mut i = 0
      while i < keys.length() {
        default_weights.push(1.0)
        i = i + 1
      }
      default_weights
    }
  }
  let aggregate_mode = match aggregate {
    Some("min") => 1
    Some("max") => 2
    _ => 0
  }
  let scores : Map[String, Float] = Map([], capacity=10)
  for i = 0; i < zsets.length(); i = i + 1 {
    let zset = zsets[i]
    let weight = weights_arr[i]
    for member_val, score in zset {
      let weighted_score = score * weight
      match scores.get(member_val) {
        Some(current) => {
          let new_score = match aggregate_mode {
            1 => if weighted_score < current { weighted_score } else { current }
            2 => if weighted_score > current { weighted_score } else { current }
            _ => current + weighted_score
          }
          scores.set(member_val, new_score)
        }
        None => scores.set(member_val, weighted_score)
      }
    }
  }
  self.put(destination, RedisValue::ZSet(scores))
  self.expires.remove(destination)
  Ok(scores.length())
}

pub fn Database::try_zunionstore(
  self : Database,
  destination : String,
  keys : Array[String],
) -> Result[Int, RedisError] {
  self.try_zunionstore_with_options(destination, keys, None, None)
}

pub fn Database::zunionstore(self : Database, destination : String, keys : Array[String]) -> Int {
  match self.try_zunionstore(destination, keys) {
    Ok(count) => count
    Err(_) => -1
  }
}

pub fn Database::try_zinterstore_with_options(
  self : Database,
  destination : String,
  keys : Array[String],
  weights : Array[Float]?,
  aggregate : String?,
) -> Result[Int, RedisError] {
  if !self.zsets_valid(keys) {
    return Err(RedisError::WrongType)
  }
  let zsets : Array[Map[String, Float]] = []
  for key in keys {
    if self.check_expired(key) {
      return Err(RedisError::WrongType)
    }
    match self.data.get(key) {
      Some(RedisValue::ZSet(zset)) => zsets.push(zset)
      _ => return Err(RedisError::WrongType)
    }
  }
  let weights_arr = match weights {
    Some(w) => w
    None => {
      let default_weights : Array[Float] = []
      let mut i = 0
      while i < keys.length() {
        default_weights.push(1.0)
        i = i + 1
      }
      default_weights
    }
  }
  let aggregate_mode = match aggregate {
    Some("min") => 1
    Some("max") => 2
    _ => 0
  }
  let mut min_idx = 0
  let mut min_len = zsets[0].length()
  for i = 1; i < zsets.length(); i = i + 1 {
    let len = zsets[i].length()
    if len < min_len {
      min_idx = i
      min_len = len
    }
  }
  let scores : Map[String, Float] = Map([], capacity=min_len)
  for member_val, first_score in zsets[min_idx] {
    let mut total = first_score * weights_arr[min_idx]
    let mut present = true
    for i = 0; i < zsets.length(); i = i + 1 {
      if i == min_idx {
        continue
      }
      match zsets[i].get(member_val) {
        Some(score) => {
          let weighted = score * weights_arr[i]
          total = match aggregate_mode {
            1 => if weighted < total { weighted } else { total }
            2 => if weighted > total { weighted } else { total }
            _ => total + weighted
          }
        }
        None => {
          present = false
          break
        }
      }
    }
    if present {
      scores.set(member_val, total)
    }
  }
  self.put(destination, RedisValue::ZSet(scores))
  self.expires.remove(destination)
  Ok(scores.length())
}

pub fn Database::try_zinterstore(
  self : Database,
  destination : String,
  keys : Array[String],
) -> Result[Int, RedisError] {
  self.try_zinterstore_with_options(destination, keys, None, None)
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
  if keys.length() == 0 {
    return Ok(0)
  }
  let mut base_key = keys[0]
  let mut base_len = -1
  for k in keys {
    self.check_expired(k) |> ignore
    let n = match self.data.get(k) {
      Some(RedisValue::ZSet(z)) => z.length()
      _ => 0
    }
    if base_len < 0 || n < base_len {
      base_key = k
      base_len = n
    }
  }
  let mut count = 0
  match self.data.get(base_key) {
    Some(RedisValue::ZSet(z)) => {
      for member_val, _ in z {
        let mut in_all = true
        for k in keys {
          if k == base_key {
            continue
          }
          let contained = match self.data.get(k) {
            Some(RedisValue::ZSet(other)) => other.contains(member_val)
            _ => false
          }
          if !contained {
            in_all = false
            break
          }
        }
        if in_all {
          count = count + 1
          if limit > 0 && count >= limit {
            return Ok(count)
          }
        }
      }
    }
    _ => ()
  }
  Ok(count)
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


pub fn Database::try_zmpop(
  self : Database,
  keys : Array[String],
  min : Bool,
  count : Int,
) -> Result[(String, Array[(String, Float)])?, RedisError] {
  if keys.length() == 0 || count <= 0 {
    return Err(RedisError::InvalidArgument)
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
        return Ok(Some((key, result)))
      }
      _ => ()
    }
  }
  Ok(None)
}

pub fn Database::zmpop(
  self : Database,
  keys : Array[String],
  min : Bool,
  count : Int,
) -> (String, Array[(String, Float)])? {
  match self.try_zmpop(keys, min, count) {
    Ok(result) => result
    Err(_) => None
  }
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
