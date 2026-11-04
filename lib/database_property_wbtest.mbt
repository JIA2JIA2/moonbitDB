struct PropRng {
  mut state : Int
}

fn PropRng::new(seed : Int) -> PropRng {
  PropRng::{ state: if seed == 0 { 1 } else { seed } }
}

fn PropRng::next(self : PropRng) -> Int {
  let mut x = self.state & 0x7fffffff
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
  self.state = x
  x
}

fn PropRng::below(self : PropRng, n : Int) -> Int {
  if n <= 0 {
    0
  } else {
    self.next() % n
  }
}

fn prop_sorted_zset(model : Map[String, Float]) -> Array[(String, Float)] {
  let items : Array[(String, Float)] = []
  for member_val, score in model {
    items.push((member_val, score))
  }
  items.sort_by((a, b) => {
    if a.1 < b.1 {
      -1
    } else if a.1 > b.1 {
      1
    } else {
      a.0.lexical_compare(b.0)
    }
  })
  items
}

fn prop_sorted_hash(model : Map[String, String]) -> Array[(String, String)] {
  let items : Array[(String, String)] = []
  for field, value in model {
    items.push((field, value))
  }
  items.sort_by((a, b) => a.0.lexical_compare(b.0))
  items
}

fn prop_sorted_set(model : Map[String, Bool]) -> Array[String] {
  let items : Array[String] = []
  for member_val, _ in model {
    items.push(member_val)
  }
  items.sort_by((a, b) => a.lexical_compare(b))
  items
}

fn prop_sorted_strings(items : Array[String]) -> Array[String] {
  let copy : Array[String] = []
  for item in items {
    copy.push(item)
  }
  copy.sort_by((a, b) => a.lexical_compare(b))
  copy
}

fn prop_lrem_model(
  model : Array[String],
  count : Int,
  value : String,
) -> (Array[String], Int) {
  let len = model.length()
  let result : Array[String] = []
  let mut removed = 0
  if count > 0 {
    for i = 0; i < len; i = i + 1 {
      if removed < count && model[i] == value {
        removed = removed + 1
      } else {
        result.push(model[i])
      }
    }
  } else if count < 0 {
    let limit = -count
    let mut seen = 0
    let mut cut = -1
    for i = len - 1; i >= 0; i = i - 1 {
      if model[i] == value {
        seen = seen + 1
        if seen == limit {
          cut = i
          break
        }
      }
    }
    if cut == -1 && seen > 0 {
      cut = 0
    }
    for i = 0; i < len; i = i + 1 {
      if cut != -1 && i >= cut && model[i] == value {
        removed = removed + 1
      } else {
        result.push(model[i])
      }
    }
  } else {
    for i = 0; i < len; i = i + 1 {
      if model[i] == value {
        removed = removed + 1
      } else {
        result.push(model[i])
      }
    }
  }
  (result, removed)
}

test "property deque matches array model" {
  let deque = Deque::new()
  let model : Array[String] = []
  let rng = PropRng::new(7)
  for step = 0; step < 500; step = step + 1 {
    match rng.below(6) {
      0 => {
        let value = "v:\{step}"
        deque.push_front(value)
        model.insert(0, value)
      }
      1 => {
        let value = "v:\{step}"
        deque.push_back(value)
        model.push(value)
      }
      2 => {
        let actual = deque.pop_front()
        let expected = if model.length() > 0 {
          Some(model.remove(0))
        } else {
          None
        }
        @test.assert_eq(actual, expected)
      }
      3 => {
        let actual = deque.pop_back()
        let expected = if model.length() > 0 {
          Some(model.pop().unwrap())
        } else {
          None
        }
        @test.assert_eq(actual, expected)
      }
      4 => {
        let index = rng.below(model.length() + 1)
        let actual = deque.get_at(index)
        let expected = if index < model.length() {
          Some(model[index])
        } else {
          None
        }
        @test.assert_eq(actual, expected)
      }
      _ => {
        let index = rng.below(model.length() + 1)
        let value = "set:\{step}"
        let actual = deque.set_at(index, value)
        if index < model.length() {
          model[index] = value
          @test.assert_eq(actual, true)
        } else {
          @test.assert_eq(actual, false)
        }
      }
    }
    @test.assert_eq(deque.length(), model.length())
  }
  @test.assert_eq(deque.to_array(), model)
}

test "property zset ordering matches model" {
  let db = Database::new()
  let model = Map([], capacity=64)
  let rng = PropRng::new(11)
  for step = 0; step < 300; step = step + 1 {
    let member_val = "member:\{rng.below(40)}"
    let op = rng.below(10)
    if op < 6 {
      let score = Float::from_int(rng.below(100)) / 10.0
      db.zadd("z", score, member_val) |> ignore
      model.set(member_val, score)
    } else if op < 8 {
      db.zrem("z", member_val) |> ignore
      model.remove(member_val)
    } else {
      let increment = Float::from_int(rng.below(21) - 10)
      db.zincrby("z", increment, member_val) |> ignore
      let current = match model.get(member_val) {
        Some(score) => score
        None => 0.0
      }
      model.set(member_val, current + increment)
    }
    @test.assert_eq(db.zrange_with_scores("z", 0, -1), prop_sorted_zset(model))
    @test.assert_eq(db.zcard("z"), model.length())
  }
}

test "property scan covers all keys" {
  let db = Database::new()
  for i = 0; i < 137; i = i + 1 {
    db.set("prop:\{i}", "v") |> ignore
  }
  let seen = Map([], capacity=137)
  let mut cursor = 0
  while true {
    let (next, keys) = db.scan(cursor, 7)
    for key in keys {
      seen.set(key, true)
    }
    if next == 0 {
      break
    }
    cursor = next
  }
  @test.assert_eq(seen.length(), 137)
}

test "command list is unique and complete" {
  let commands = Database::command()
  let seen = Map([], capacity=commands.length())
  for command in commands {
    @test.assert_eq(seen.contains(command), false)
    seen.set(command, true)
  }
  @test.assert_eq(commands.length(), 146)
}

test "integer parser edge cases" {
  @test.assert_eq(parse_int(""), None)
  @test.assert_eq(parse_int("-"), None)
  @test.assert_eq(parse_int("+12"), Some(12))
  @test.assert_eq(parse_int("12x"), None)
  @test.assert_eq(parse_int("0"), Some(0))
  @test.assert_eq(parse_int64(""), None)
  @test.assert_eq(parse_int64("+42"), Some(42))
  @test.assert_eq(parse_int64("4x"), None)
  @test.assert_eq(parse_int64("9223372036854775807"), Some(9223372036854775807))
  @test.assert_eq(int_to_string(0), "0")
  @test.assert_eq(int_to_string(-42), "-42")
  @test.assert_eq(int_to_string(2147483647), "2147483647")
  @test.assert_eq(parse_int("-42"), Some(-42))
  @test.assert_eq(parse_int64("-42"), Some(-42))
  @test.assert_eq(digit_to_uint16(0), 48)
  @test.assert_eq(digit_to_uint16(9), 57)
  @test.assert_eq(digit_to_uint16(10), 48)
  @test.assert_eq(uint16_to_digit(48), Some(0))
  @test.assert_eq(uint16_to_digit(57), Some(9))
  @test.assert_eq(uint16_to_digit(65), None)
  @test.assert_eq(match_pattern("a*b", "acb"), true)
  @test.assert_eq(match_pattern("a*b", "ab"), true)
  @test.assert_eq(match_pattern("a*b", "ac"), false)
  @test.assert_eq(match_pattern_helper("a*", "abc", 0, 0), true)
}

test "deque index operations" {
  let deque = Deque::new()
  deque.push_back("a")
  deque.push_back("b")
  deque.push_back("c")
  @test.assert_eq(deque.find_index("b"), 1)
  @test.assert_eq(deque.find_index("missing"), -1)
  @test.assert_eq(deque.insert_at(-1, "x"), false)
  @test.assert_eq(deque.insert_at(4, "x"), false)
  @test.assert_eq(deque.insert_at(1, "x"), true)
  @test.assert_eq(deque.to_array(), ["a", "x", "b", "c"])
  @test.assert_eq(deque.remove_at(-1), false)
  @test.assert_eq(deque.remove_at(4), false)
  @test.assert_eq(deque.remove_at(1), true)
  @test.assert_eq(deque.to_array(), ["a", "b", "c"])
  @test.assert_eq(deque.remove_at(0), true)
  @test.assert_eq(deque.to_array(), ["b", "c"])
  @test.assert_eq(deque.remove_at(1), true)
  @test.assert_eq(deque.to_array(), ["b"])
  deque.pop_back() |> ignore
  @test.assert_eq(deque.length(), 0)
  @test.assert_eq(deque.to_array(), [])
}

test "property hash matches model" {
  let db = Database::new()
  let model : Map[String, String] = Map([], capacity=32)
  let rng = PropRng::new(21)
  for step = 0; step < 300; step = step + 1 {
    let field = "f:\{rng.below(20)}"
    if rng.below(10) < 7 {
      let value = "v:\{rng.below(100)}"
      db.hset("h", field, value) |> ignore
      model.set(field, value)
    } else {
      db.hdel("h", field) |> ignore
      model.remove(field)
    }
  }
  @test.assert_eq(prop_sorted_hash(db.hgetall("h")), prop_sorted_hash(model))
  @test.assert_eq(db.hlen("h"), model.length())
}

test "property set matches model" {
  let db = Database::new()
  let model : Map[String, Bool] = Map([], capacity=32)
  let rng = PropRng::new(23)
  for step = 0; step < 300; step = step + 1 {
    let member_val = "m:\{rng.below(25)}"
    if rng.below(10) < 7 {
      db.sadd("s", member_val) |> ignore
      model.set(member_val, true)
    } else {
      db.srem("s", member_val) |> ignore
      model.remove(member_val)
    }
  }
  @test.assert_eq(prop_sorted_strings(db.smembers("s")), prop_sorted_set(model))
  @test.assert_eq(db.scard("s"), model.length())
}

test "property lrem matches model" {
  let rng = PropRng::new(31)
  for _trial = 0; _trial < 40; _trial = _trial + 1 {
    let db = Database::new()
    let model : Array[String] = []
    let len = rng.below(15) + 1
    for i = 0; i < len; i = i + 1 {
      let value = if rng.below(3) == 0 { "a" } else { "b" }
      db.rpush("l", value) |> ignore
      model.push(value)
    }
    let count = rng.below(7) - 3
    let (expected, expected_removed) = prop_lrem_model(model, count, "a")
    @test.assert_eq(db.lrem("l", count, "a"), expected_removed)
    @test.assert_eq(db.lrange("l", 0, -1), expected)
  }
}