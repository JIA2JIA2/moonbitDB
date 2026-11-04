/// ZRANGESTORE BYSCORE：分数区间结果存入 destination（空结果删除 destination）
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
    self.delete_key(destination)
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
      if scores.length() == 0 {
        self.delete_key(destination)
      } else {
        self.put(destination, RedisValue::ZSet(scores))
        self.expires.remove(destination)
      }
      Ok(scores.length())
    }
    Some(_) => Err(RedisError::WrongType)
    None => {
      self.delete_key(destination)
      Ok(0)
    }
  }
}
/// ZRANGESTORE BYLEX：字典序区间结果存入 destination
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
    self.delete_key(destination)
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
      if scores.length() == 0 {
        self.delete_key(destination)
      } else {
        self.put(destination, RedisValue::ZSet(scores))
        self.expires.remove(destination)
      }
      Ok(scores.length())
    }
    Some(_) => Err(RedisError::WrongType)
    None => {
      self.delete_key(destination)
      Ok(0)
    }
  }
}
/// ZRANDMEMBER：随机返回一个成员
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
/// ZUNION：多集合并集（分数相加）
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
/// ZINTER：多集合交集（分数相加）
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
/// ZDIFF WITHSCORES：差集返回 (成员, 分数)
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
/// ZDIFF：首集合相对差集
pub fn Database::zdiff(self : Database, keys : Array[String]) -> Array[String] {
  let items = self.zdiff_with_scores(keys)
  let result : Array[String] = []
  for item in items {
    result.push(item.0)
  }
  result
}
/// ZSCAN MATCH：游标遍历（独立快照，迭代期间写入不重复/不漏读）
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
/// ZSCAN：游标遍历成员与分数
pub fn Database::zscan(
  self : Database,
  key : String,
  cursor : Int,
  count : Int,
) -> (Int, Array[(String, Float)]) {
  self.zscan_with_match(key, cursor, count, None)
}
/// ZSCAN 精确错误版
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
/// ZRANDMEMBER 精确错误版
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
/// ZUNION 精确错误版
pub fn Database::try_zunion(self : Database, keys : Array[String]) -> Result[Array[(String, Float)], RedisError] {
  if !self.zsets_valid(keys) {
    Err(RedisError::WrongType)
  } else {
    Ok(self.zunion(keys))
  }
}
/// ZINTER 精确错误版
pub fn Database::try_zinter(self : Database, keys : Array[String]) -> Result[Array[(String, Float)], RedisError] {
  if !self.zsets_valid(keys) {
    Err(RedisError::WrongType)
  } else {
    Ok(self.zinter(keys))
  }
}
/// ZDIFF 精确错误版
pub fn Database::try_zdiff(self : Database, keys : Array[String]) -> Result[Array[(String, Float)], RedisError] {
  if !self.zsets_valid(keys) {
    Err(RedisError::WrongType)
  } else {
    Ok(self.zdiff_with_scores(keys))
  }
}
/// ZUNIONSTORE WEIGHTS/AGGREGATE 完整版
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
/// ZUNIONSTORE 精确错误版
pub fn Database::try_zunionstore(
  self : Database,
  destination : String,
  keys : Array[String],
) -> Result[Int, RedisError] {
  self.try_zunionstore_with_options(destination, keys, None, None)
}
/// ZUNIONSTORE：并集存入 destination，返回基数
pub fn Database::zunionstore(self : Database, destination : String, keys : Array[String]) -> Int {
  match self.try_zunionstore(destination, keys) {
    Ok(count) => count
    Err(_) => -1
  }
}
/// ZINTERSTORE WEIGHTS/AGGREGATE 完整版
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
/// ZINTERSTORE 精确错误版
pub fn Database::try_zinterstore(
  self : Database,
  destination : String,
  keys : Array[String],
) -> Result[Int, RedisError] {
  self.try_zinterstore_with_options(destination, keys, None, None)
}
/// ZINTERSTORE：交集存入 destination，返回基数
pub fn Database::zinterstore(self : Database, destination : String, keys : Array[String]) -> Int {
  match self.try_zinterstore(destination, keys) {
    Ok(count) => count
    Err(_) => -1
  }
}
/// ZDIFFSTORE 精确错误版
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
/// ZDIFFSTORE：差集存入 destination，返回基数
pub fn Database::zdiffstore(self : Database, destination : String, keys : Array[String]) -> Int {
  match self.try_zdiffstore(destination, keys) {
    Ok(count) => count
    Err(_) => -1
  }
}
/// ZINTERCARD：交集计数（limit 短路）
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
/// ZINTERCARD 便捷版
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
    self.delete_key(destination)
    return Ok(0)
  }
  match self.data.get(source) {
    Some(RedisValue::ZSet(_)) => {
      let result = self.zrange_with_scores(source, start, end)
      let scores : Map[String, Float] = Map([], capacity=result.length())
      for pair in result {
        scores.set(pair.0, pair.1)
      }
      if scores.length() == 0 {
        self.delete_key(destination)
      } else {
        self.put(destination, RedisValue::ZSet(scores))
        self.expires.remove(destination)
      }
      Ok(result.length())
    }
    Some(_) => Err(RedisError::WrongType)
    None => {
      self.delete_key(destination)
      Ok(0)
    }
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
/// ZRANDMEMBER count：随机返回多个成员（负数可重复）
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





/// ZMPOP 精确错误版：多 key 弹出最值（count<=0 报 InvalidArgument）
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

/// ZMPOP 便捷版：多 key 弹出最值
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

/// ZRANDMEMBER count WITHSCORES：随机返回多个 (成员, 分数)
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
