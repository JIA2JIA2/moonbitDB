test "list_operations" {
  let db = Database::new()
  
  db.lpush("mylist", "a") |> ignore
  db.lpush("mylist", "b") |> ignore
  db.rpush("mylist", "c") |> ignore
  
  @test.assert_eq(db.llen("mylist"), 3)
  @test.assert_eq(db.lpop("mylist"), Some("b"))
  @test.assert_eq(db.rpop("mylist"), Some("c"))
  @test.assert_eq(db.llen("mylist"), 1)
}

test "list_boundary_cases" {
  let db = Database::new()
  
  @test.assert_eq(db.llen("nonexistent"), 0)
  @test.assert_eq(db.lpop("nonexistent"), None)
  @test.assert_eq(db.rpop("nonexistent"), None)
  
  db.lpush("empty_list", "first") |> ignore
  @test.assert_eq(db.llen("empty_list"), 1)
  @test.assert_eq(db.lpop("empty_list"), Some("first"))
  @test.assert_eq(db.llen("empty_list"), 0)
  @test.assert_eq(db.lpop("empty_list"), None)
  
  db.rpush("single_list", "item") |> ignore
  @test.assert_eq(db.rpop("single_list"), Some("item"))
  @test.assert_eq(db.llen("single_list"), 0)
  
  db.lpush("multi", "x") |> ignore
  db.lpush("multi", "y") |> ignore
  db.lpush("multi", "z") |> ignore
  @test.assert_eq(db.llen("multi"), 3)
  
  @test.assert_eq(db.lpop("multi"), Some("z"))
  @test.assert_eq(db.lpop("multi"), Some("y"))
  @test.assert_eq(db.lpop("multi"), Some("x"))
  @test.assert_eq(db.lpop("multi"), None)
  
  db.rpush("reverse", "a") |> ignore
  db.rpush("reverse", "b") |> ignore
  db.rpush("reverse", "c") |> ignore
  @test.assert_eq(db.rpop("reverse"), Some("c"))
  @test.assert_eq(db.rpop("reverse"), Some("b"))
  @test.assert_eq(db.rpop("reverse"), Some("a"))
}

test "lrange_operations" {
  let db = Database::new()
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "b") |> ignore
  db.rpush("list1", "c") |> ignore
  db.rpush("list1", "d") |> ignore
  db.rpush("list1", "e") |> ignore
  
  let range1 = db.lrange("list1", 0, -1)
  @test.assert_eq(range1.length(), 5)
  
  let range2 = db.lrange("list1", 0, 2)
  @test.assert_eq(range2.length(), 3)
  
  let range3 = db.lrange("list1", -3, -1)
  @test.assert_eq(range3.length(), 3)
  
  let range4 = db.lrange("list1", 10, 20)
  @test.assert_eq(range4.length(), 0)
  
  let range5 = db.lrange("nonexistent", 0, -1)
  @test.assert_eq(range5.length(), 0)
}

test "lindex_lset_operations" {
  let db = Database::new()
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "b") |> ignore
  db.rpush("list1", "c") |> ignore
  
  @test.assert_eq(db.lindex("list1", 0), Some("a"))
  @test.assert_eq(db.lindex("list1", 1), Some("b"))
  @test.assert_eq(db.lindex("list1", -1), Some("c"))
  @test.assert_eq(db.lindex("list1", 10), None)
  @test.assert_eq(db.lindex("nonexistent", 0), None)
  
  @test.assert_eq(db.lset("list1", 1, "new_b"), true)
  @test.assert_eq(db.lindex("list1", 1), Some("new_b"))
  
  @test.assert_eq(db.lset("list1", -1, "new_c"), true)
  @test.assert_eq(db.lindex("list1", -1), Some("new_c"))
  
  @test.assert_eq(db.lset("list1", 10, "invalid"), false)
  @test.assert_eq(db.lset("nonexistent", 0, "value"), false)
}

test "lrem_operations" {
  let db = Database::new()
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "b") |> ignore
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "c") |> ignore
  db.rpush("list1", "a") |> ignore
  
  let removed1 = db.lrem("list1", 2, "a")
  @test.assert_eq(removed1, 2)
  @test.assert_eq(db.llen("list1"), 3)
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "a") |> ignore
  
  let removed2 = db.lrem("list1", -2, "a")
  @test.assert_eq(removed2, 2)
  @test.assert_eq(db.llen("list1"), 3)
  
  let removed3 = db.lrem("list1", 0, "a")
  @test.assert_eq(removed3, 1)
  @test.assert_eq(db.llen("list1"), 2)
  
  let removed4 = db.lrem("nonexistent", 1, "a")
  @test.assert_eq(removed4, 0)
}

test "linsert_operations" {
  let db = Database::new()
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "b") |> ignore
  db.rpush("list1", "c") |> ignore
  
  let len1 = db.linsert("list1", true, "b", "new")
  @test.assert_eq(len1, 4)
  @test.assert_eq(db.lindex("list1", 1), Some("new"))
  
  let len2 = db.linsert("list1", false, "b", "after")
  @test.assert_eq(len2, 5)
  @test.assert_eq(db.lindex("list1", 3), Some("after"))
  
  let len3 = db.linsert("list1", true, "nonexistent", "value")
  @test.assert_eq(len3, -1)
  
  let len4 = db.linsert("nonexistent", true, "a", "value")
  @test.assert_eq(len4, -1)
}

test "lpushx_rpushx_operations" {
  let db = Database::new()
  
  let len1 = db.lpushx("nonexistent", "a")
  @test.assert_eq(len1, 0)
  
  let len2 = db.rpushx("nonexistent", "a")
  @test.assert_eq(len2, 0)
  
  db.rpush("list1", "a") |> ignore
  
  let len3 = db.lpushx("list1", "b")
  @test.assert_eq(len3, 2)
  @test.assert_eq(db.lindex("list1", 0), Some("b"))
  
  let len4 = db.rpushx("list1", "c")
  @test.assert_eq(len4, 3)
  @test.assert_eq(db.lindex("list1", 2), Some("c"))
}

test "rpoplpush_operations" {
  let db = Database::new()
  
  db.rpush("source", "a") |> ignore
  db.rpush("source", "b") |> ignore
  db.rpush("source", "c") |> ignore
  
  let val1 = db.rpoplpush("source", "dest")
  @test.assert_eq(val1, Some("c"))
  @test.assert_eq(db.llen("source"), 2)
  @test.assert_eq(db.llen("dest"), 1)
  @test.assert_eq(db.lindex("dest", 0), Some("c"))
  
  let val2 = db.rpoplpush("source", "dest")
  @test.assert_eq(val2, Some("b"))
  @test.assert_eq(db.llen("source"), 1)
  @test.assert_eq(db.llen("dest"), 2)
  @test.assert_eq(db.lindex("dest", 0), Some("b"))
  
  let val3 = db.rpoplpush("empty", "dest")
  @test.assert_eq(val3, None)
  
  db.rpush("rotate", "1") |> ignore
  db.rpush("rotate", "2") |> ignore
  db.rpush("rotate", "3") |> ignore
  db.rpush("rotate", "4") |> ignore
  
  let val4 = db.rpoplpush("rotate", "rotate")
  @test.assert_eq(val4, Some("4"))
  @test.assert_eq(db.lindex("rotate", 0), Some("4"))
  @test.assert_eq(db.lindex("rotate", 1), Some("1"))
  @test.assert_eq(db.lindex("rotate", 2), Some("2"))
  @test.assert_eq(db.lindex("rotate", 3), Some("3"))
}

test "list_extended_edge_cases" {
  let db = Database::new()
  
  db.lpush("mix", "b") |> ignore
  db.rpush("mix", "c") |> ignore
  db.lpush("mix", "a") |> ignore
  db.rpush("mix", "d") |> ignore
  
  @test.assert_eq(db.lindex("mix", 0), Some("a"))
  @test.assert_eq(db.lindex("mix", 1), Some("b"))
  @test.assert_eq(db.lindex("mix", 2), Some("c"))
  @test.assert_eq(db.lindex("mix", 3), Some("d"))
  
  let rev_range = db.lrange("mix", 2, 0)
  @test.assert_eq(rev_range.length(), 0)
  
  let full = db.lrange("mix", 0, -1)
  @test.assert_eq(full.length(), 4)
  
  let single = db.lrange("mix", 1, 1)
  @test.assert_eq(single.length(), 1)
  @test.assert_eq(single[0], "b")
  
  @test.assert_eq(db.lindex("mix", -4), Some("a"))
  @test.assert_eq(db.lindex("mix", -1), Some("d"))
  @test.assert_eq(db.lindex("mix", 100), None)
  @test.assert_eq(db.lindex("mix", -100), None)
  
  db.lset("mix", 0, "A") |> ignore
  @test.assert_eq(db.lindex("mix", 0), Some("A"))
  
  @test.assert_eq(db.lrem("mix", 0, "nonexistent"), 0)
}

test "rpoplpush_same_key_preserves_ttl" {
  let db = Database::new()
  db.rpush("q", "only") |> ignore
  db.expire("q", 100) |> ignore
  @test.assert_eq(db.rpoplpush("q", "q"), Some("only"))
  @test.assert_eq(db.ttl("q") >= 0, true)
}

test "lrem_counts_exceeding_occurrences" {
  let db = Database::new()
  for item in ["a", "a", "b", "a"] {
    db.rpush("l", item) |> ignore
  }
  @test.assert_eq(db.lrem("l", 5, "a"), 3)
  @test.assert_eq(db.llen("l"), 1)
  @test.assert_eq(db.lindex("l", 0), Some("b"))

  for item in ["a", "b", "a"] {
    db.rpush("l2", item) |> ignore
  }
  @test.assert_eq(db.lrem("l2", -5, "a"), 2)
  @test.assert_eq(db.lindex("l2", 0), Some("b"))
}

test "rpoplpush_expired_destination" {
  let db = Database::new()
  db.rpush("src", "a") |> ignore
  db.rpush("src", "b") |> ignore
  db.rpush("dest", "old1") |> ignore
  db.pexpire("dest", 100) |> ignore
  db.advance_time(200)

  let val = db.rpoplpush("src", "dest")
  @test.assert_eq(val, Some("b"))
  @test.assert_eq(db.llen("dest"), 1)
  @test.assert_eq(db.lindex("dest", 0), Some("b"))
}

test "ltrim_operations" {
  let db = Database::new()
  db.rpush("l", "a") |> ignore
  db.rpush("l", "b") |> ignore
  db.rpush("l", "c") |> ignore
  db.rpush("l", "d") |> ignore
  @test.assert_eq(db.ltrim("l", 1, 2), true)
  @test.assert_eq(db.llen("l"), 2)
  @test.assert_eq(db.lindex("l", 0), Some("b"))
  @test.assert_eq(db.lindex("l", 1), Some("c"))
  @test.assert_eq(db.ltrim("l", -2, -1), true)
  @test.assert_eq(db.lindex("l", 0), Some("b"))
  @test.assert_eq(db.ltrim("nonexistent", 0, 1), false)
  @test.assert_eq(db.ltrim("l", 5, 10), true)
  @test.assert_eq(db.exists("l"), false)
}

test "rpoplpush_single_element_roundtrip" {
  let db = Database::new()
  db.rpush("q", "only") |> ignore
  let v = db.rpoplpush("q", "q")
  @test.assert_eq(v, Some("only"))
  @test.assert_eq(db.llen("q"), 1)
  @test.assert_eq(db.lindex("q", 0), Some("only"))
}

test "set_and_list_extended_commands" {
  let db = Database::new()
  db.sadd_all("s1", ["a", "b", "c"]) |> ignore
  db.sadd_all("s2", ["b", "c", "d"]) |> ignore

  @test.assert_eq(db.sintercard(["s1", "s2"], 0), 2)
  @test.assert_eq(db.sintercard(["s1", "s2"], 1), 1)
  @test.assert_eq(db.smismember("s1", ["a", "z"]), [true, false])
  @test.assert_eq(db.smismember("missing", ["a"]), [false])

  db.rpush_all("q", ["a", "b", "c"]) |> ignore
  @test.assert_eq(
    match db.lmove("q", "q", true, false) {
      Some(value) => value
      None => ""
    },
    "a",
  )
  @test.assert_eq(db.lrange("q", 0, -1), ["b", "c", "a"])
  @test.assert_eq(
    match db.lmove("q", "q", false, true) {
      Some(value) => value
      None => ""
    },
    "a",
  )
  @test.assert_eq(db.lrange("q", 0, -1), ["a", "b", "c"])
  @test.assert_eq(
    match db.lmove("q", "dest", true, true) {
      Some(value) => value
      None => ""
    },
    "a",
  )
  @test.assert_eq(db.lrange("q", 0, -1), ["b", "c"])
  @test.assert_eq(db.lrange("dest", 0, -1), ["a"])

  db.set("str", "v") |> ignore
  @test.assert_eq(
    match db.lmove("q", "str", true, true) {
      Some(_) => true
      None => false
    },
    false,
  )
  @test.assert_eq(db.lrange("q", 0, -1), ["b", "c"])
}

test "set_list_error_paths_more" {
  let db = Database::new()
  db.set("str", "v") |> ignore

  @test.assert_eq(db.sintercard([], 0), 0)
  @test.assert_eq(db.sintercard(["missing"], 0), 0)
  @test.assert_eq(db.smismember("missing", ["a", "b"]), [false, false])
  @test.assert_eq(
    match db.try_smismember("missing", ["a"]) {
      Ok(values) => values.length()
      Err(_) => -1
    },
    1,
  )
  @test.assert_eq(db.srandmember_count("missing", 2).length(), 0)
  @test.assert_eq(db.srandmember_count("missing", -2).length(), 0)
  db.sadd("s", "a") |> ignore
  @test.assert_eq(db.srandmember_count("s", 0).length(), 0)
  @test.assert_eq(
    match db.try_sadd_all("str", ["a"]) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_srem_all("str", ["a"]) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_srem_all("s", []) {
      Err(RedisError::InvalidArgument) => true
      _ => false
    },
    true,
  )

  @test.assert_eq(db.lpop_count("missing", 2).length(), 0)
  @test.assert_eq(db.rpop_count("missing", 2).length(), 0)
  @test.assert_eq(db.lpop_count("str", 2).length(), 0)
  @test.assert_eq(db.lpop_count("s", 0).length(), 0)
  @test.assert_eq(
    match db.try_lpos("missing", "a") {
      Err(RedisError::NotFound) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_lpos("str", "a") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  db.rpush_all("l1", ["a", "b"]) |> ignore
  @test.assert_eq(
    match db.try_lmove("l1", "l1", true, true) {
      Ok(value) => value
      Err(_) => ""
    },
    "a",
  )
  @test.assert_eq(db.lrange("l1", 0, -1), ["a", "b"])
}

test "transaction_list_operations" {
  let db = Database::new()
  
  ignore(db.multi())
  ignore(db.lpush("list_key", "value1"))
  ignore(db.rpush("list_key", "value2"))
  @test.assert_eq(db.exec(), "OK")
  
  @test.assert_eq(db.lrange("list_key", 0, -1), ["value1", "value2"])
}
