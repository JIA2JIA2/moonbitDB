/// ZADD 精确错误版：添加/更新成员（NaN 拒绝）
pub fn Database::try_zadd(self : Database, key : String, score : Double, member_val : String) -> Result[Bool, RedisError] {
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
/// ZADD 便捷版：添加/更新成员（事务中排队）
pub fn Database::zadd(self : Database, key : String, score : Double, member_val : String) -> Bool {
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
/// ZADD NX/XX/CH/GT/LT 完整版（返回新增或变更数）
pub fn Database::try_zadd_with_options(
  self : Database,
  key : String,
  score : Double,
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
/// ZADD INCR 模式（GT/LT 过滤，None 表示未更新）
pub fn Database::try_zadd_incr(
  self : Database,
  key : String,
  increment : Double,
  member_val : String,
  nx : Bool,
  xx : Bool,
  gt : Bool,
  lt : Bool,
) -> Result[Double?, RedisError] {
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
fn insertion_sort(arr : Array[(String, Double)]) -> Array[(String, Double)] {
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
pub fn sort_by_score(items : Array[(String, Double)]) -> Array[(String, Double)] {
  if items.length() <= 16 {
    insertion_sort(items)
  } else {
    merge_sort(items)
  }
}
pub fn merge_sort(arr : Array[(String, Double)]) -> Array[(String, Double)] {
  let len = arr.length()
  if len <= 1 {
    return arr
  }
  let tmp : Array[(String, Double)] = []
  for i = 0; i < len; i = i + 1 {
    tmp.push(arr[i])
  }
  merge_sort_range(arr, tmp, 0, len)
  arr
}
fn merge_sort_range(arr : Array[(String, Double)], tmp : Array[(String, Double)], lo : Int, hi : Int) -> Unit {
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
pub fn merge(left : Array[(String, Double)], right : Array[(String, Double)]) -> Array[(String, Double)] {
  let result : Array[(String, Double)] = []
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
/// ZCARD：成员基数（过期返回 0）
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
/// ZSCORE 精确错误版：取成员分数
pub fn Database::try_zscore(self : Database, key : String, member_val : String) -> Result[Double, RedisError] {
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
/// ZSCORE 便捷版：取成员分数
pub fn Database::zscore(self : Database, key : String, member_val : String) -> Double? {
  match self.try_zscore(key, member_val) {
    Ok(score) => Some(score)
    Err(_) => None
  }
}
/// ZREM 便捷版：移除成员（事务中排队）
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
fn Database::get_sorted_zset(self : Database, key : String) -> Array[(String, Double)] {
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
          let items : Array[(String, Double)] = []
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
  items : Array[(String, Double)],
  member_val : String,
  new_score : Double?,
) -> Array[(String, Double)] {
  let result : Array[(String, Double)] = []
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
  items : Array[(String, Double)],
  removed : Map[String, Bool],
) -> Array[(String, Double)] {
  let result : Array[(String, Double)] = []
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
fn Database::remove_zset_collected(
  self : Database,
  key : String,
  zset : Map[String, Double],
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
fn Database::zpop_extreme(self : Database, key : String, from_min : Bool) -> (String, Double)? {
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
/// ZPOPMIN：弹出最低分成员
pub fn Database::zpopmin(self : Database, key : String) -> (String, Double)? {
  self.zpop_extreme(key, true)
}
/// ZPOPMAX：弹出最高分成员
pub fn Database::zpopmax(self : Database, key : String) -> (String, Double)? {
  self.zpop_extreme(key, false)
}
/// ZPOPMIN count：弹出多个最低分成员
pub fn Database::zpopmin_count(
  self : Database,
  key : String,
  count : Int,
) -> Array[(String, Double)] {
  if self.check_expired(key) || count <= 0 {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(zset)) => {
      let sorted = self.get_sorted_zset(key)
      let take = if count > sorted.length() { sorted.length() } else { count }
      let result : Array[(String, Double)] = []
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
/// ZPOPMAX count：弹出多个最高分成员
pub fn Database::zpopmax_count(
  self : Database,
  key : String,
  count : Int,
) -> Array[(String, Double)] {
  if self.check_expired(key) || count <= 0 {
    return []
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(zset)) => {
      let sorted = self.get_sorted_zset(key)
      let len = sorted.length()
      let take = if count > len { len } else { count }
      let result : Array[(String, Double)] = []
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
