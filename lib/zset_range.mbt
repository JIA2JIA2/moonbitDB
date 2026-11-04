/// ZRANGE：按排名区间返回成员（升序）
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
/// ZRANGE：按排名区间返回成员（with_scores 时返回成员/分数交替数组）
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
/// ZRANGEBYSCORE LIMIT：分数区间分页（负 offset 返回空，对齐 Redis）
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
/// ZRANGEBYSCORE：按分数区间返回成员
pub fn Database::zrangebyscore(self : Database, key : String, min_score : Float, max_score : Float) -> Array[String] {
  self.zrangebyscore_with_limit(key, min_score, max_score, 0, 0)
}
/// ZRANGEBYSCORE WITHSCORES：分数区间返回 (成员, 分数)
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
/// ZCOUNT：分数区间成员计数
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
/// ZREVRANGE：按排名区间降序返回成员
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
/// ZREVRANGEBYSCORE：分数区间降序返回成员（max 在前）
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
/// ZREVRANGEBYSCORE LIMIT：分数区间降序分页
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
/// ZRANGEBYSCORE 边界版：支持 [a/(a/-inf/+inf 串与 LIMIT
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
/// ZREVRANGEBYSCORE 边界版（REV 方向）
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
/// ZCOUNT 边界版：分数区间计数
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
/// ZREMRANGEBYSCORE 边界版：按分数区间删除
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
/// ZREVRANGEBYSCORE WITHSCORES：分数区间降序返回 (成员, 分数)
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
/// ZRANK：成员升序排名（0 起，不存在为 None）
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
/// ZREVRANK：成员降序排名
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
/// ZINCRBY 精确错误版：增加成员分数
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
/// ZINCRBY：增加成员分数（支持负增量）
pub fn Database::zincrby(self : Database, key : String, increment : Float, member_val : String) -> Float {
  match self.try_zincrby(key, increment, member_val) {
    Ok(score) => score
    Err(_) => 0.0
  }
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
/// ZRANGE 6.2 统一语法：BYSCORE/BYLEX/REV/LIMIT/WITHSCORES 分发
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
/// ZRANGE 6.2 统一语法便捷版
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
/// ZREMRANGEBYSCORE 精确错误版：按排名区间删除
pub fn Database::try_zremrangebyrank(self : Database, key : String, start : Int, end : Int) -> Result[Int, RedisError] {
  if self.check_expired(key) {
    return Ok(0)
  }
  match self.data.get(key) {
    Some(RedisValue::ZSet(_)) => Ok(self.zremrangebyrank(key, start, end))
    Some(_) => Err(RedisError::WrongType)
    None => Ok(0)
  }
}
/// ZREMRANGEBYRANK：按排名区间删除，返回删除个数
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
/// ZREMRANGEBYSCORE：按分数区间删除，返回删除个数
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
/// ZMSCORE：批量取成员分数（缺失为 None）
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
/// ZRANGE WITHSCORES：按排名区间返回 (成员, 分数) 元组
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
/// ZREVRANGE WITHSCORES：按排名区间降序返回 (成员, 分数)
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
/// ZRANGEBYLEX：按字典序区间返回成员
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
/// ZRANGEBYLEX LIMIT：字典序分页（精确错误版）
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
/// ZREVRANGEBYLEX：字典序降序返回成员
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
/// ZREVRANGEBYLEX LIMIT：字典序降序分页
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
/// ZLEXCOUNT：字典序区间成员计数
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
/// ZREMRANGEBYLEX：按字典序区间删除
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
