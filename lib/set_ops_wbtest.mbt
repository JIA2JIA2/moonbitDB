test "set_operations" {
  let db = Database::new()
  
  db.sadd("myset", "apple") |> ignore
  db.sadd("myset", "banana") |> ignore
  db.sadd("myset", "apple") |> ignore
  
  @test.assert_eq(db.scard("myset"), 2)
  @test.assert_eq(db.sismember("myset", "apple"), true)
  @test.assert_eq(db.sismember("myset", "orange"), false)
  
  db.srem("myset", "banana") |> ignore
  @test.assert_eq(db.scard("myset"), 1)
}

test "set_boundary_cases" {
  let db = Database::new()
  
  @test.assert_eq(db.scard("nonexistent"), 0)
  @test.assert_eq(db.sismember("nonexistent", "item"), false)
  @test.assert_eq(db.srem("nonexistent", "item"), false)
  
  let empty_members = db.smembers("nonexistent")
  @test.assert_eq(empty_members.length(), 0)
  
  @test.assert_eq(db.sadd("set1", "a"), true)
  @test.assert_eq(db.sadd("set1", "b"), true)
  @test.assert_eq(db.sadd("set1", "a"), false)
  
  @test.assert_eq(db.scard("set1"), 2)
  
  @test.assert_eq(db.srem("set1", "c"), false)
  @test.assert_eq(db.srem("set1", "a"), true)
  
  @test.assert_eq(db.scard("set1"), 1)
  @test.assert_eq(db.sismember("set1", "a"), false)
  @test.assert_eq(db.sismember("set1", "b"), true)
  
  let members = db.smembers("set1")
  @test.assert_eq(members.length(), 1)
  
  db.srem("set1", "b") |> ignore
  @test.assert_eq(db.scard("set1"), 0)
  
  db.sadd("empty_item", "") |> ignore
  @test.assert_eq(db.sismember("empty_item", ""), true)
}

test "mset_mget_operations" {
  let db = Database::new()
  
  db.mset(["key1", "key2", "key3"], ["value1", "value2", "value3"])
  
  @test.assert_eq(db.get("key1"), Some("value1"))
  @test.assert_eq(db.get("key2"), Some("value2"))
  @test.assert_eq(db.get("key3"), Some("value3"))
  
  let values = db.mget(["key1", "key2", "key3"])
  @test.assert_eq(values.length(), 3)
  @test.assert_eq(values[0], Some("value1"))
  @test.assert_eq(values[1], Some("value2"))
  @test.assert_eq(values[2], Some("value3"))
  
  let mixed = db.mget(["key1", "nonexistent", "key3"])
  @test.assert_eq(mixed.length(), 3)
  @test.assert_eq(mixed[0], Some("value1"))
  @test.assert_eq(mixed[1], None)
  @test.assert_eq(mixed[2], Some("value3"))
  
  db.mset(["key2", "key4"], ["updated", "value4"])
  @test.assert_eq(db.get("key2"), Some("updated"))
  @test.assert_eq(db.get("key4"), Some("value4"))
}

test "sinter_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  db.sadd("set1", "d") |> ignore
  
  db.sadd("set2", "b") |> ignore
  db.sadd("set2", "c") |> ignore
  db.sadd("set2", "e") |> ignore
  db.sadd("set2", "f") |> ignore
  
  db.sadd("set3", "c") |> ignore
  db.sadd("set3", "d") |> ignore
  db.sadd("set3", "g") |> ignore
  
  let inter1 = db.sinter(["set1", "set2"])
  @test.assert_eq(inter1.length(), 2)
  @test.assert_eq(inter1.contains("b"), true)
  @test.assert_eq(inter1.contains("c"), true)
  
  let inter2 = db.sinter(["set1", "set2", "set3"])
  @test.assert_eq(inter2.length(), 1)
  @test.assert_eq(inter2.contains("c"), true)
  
  let inter3 = db.sinter(["set1", "nonexistent"])
  @test.assert_eq(inter3.length(), 0)
  
  let inter4 = db.sinter([])
  @test.assert_eq(inter4.length(), 0)
}

test "sinterstore_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  
  db.sadd("set2", "b") |> ignore
  db.sadd("set2", "c") |> ignore
  db.sadd("set2", "d") |> ignore
  
  let count = db.sinterstore("dest", ["set1", "set2"])
  @test.assert_eq(count, 2)
  @test.assert_eq(db.scard("dest"), 2)
  @test.assert_eq(db.sismember("dest", "b"), true)
  @test.assert_eq(db.sismember("dest", "c"), true)
  @test.assert_eq(db.sismember("dest", "a"), false)
}

test "sunion_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  
  db.sadd("set2", "b") |> ignore
  db.sadd("set2", "c") |> ignore
  
  db.sadd("set3", "d") |> ignore
  
  let union1 = db.sunion(["set1", "set2"])
  @test.assert_eq(union1.length(), 3)
  @test.assert_eq(union1.contains("a"), true)
  @test.assert_eq(union1.contains("b"), true)
  @test.assert_eq(union1.contains("c"), true)
  
  let union2 = db.sunion(["set1", "set2", "set3"])
  @test.assert_eq(union2.length(), 4)
  
  let union3 = db.sunion(["set1", "nonexistent"])
  @test.assert_eq(union3.length(), 2)
  
  let union4 = db.sunion([])
  @test.assert_eq(union4.length(), 0)
}

test "sunionstore_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  
  db.sadd("set2", "c") |> ignore
  db.sadd("set2", "d") |> ignore
  
  let count = db.sunionstore("dest", ["set1", "set2"])
  @test.assert_eq(count, 4)
  @test.assert_eq(db.scard("dest"), 4)
  @test.assert_eq(db.sismember("dest", "a"), true)
  @test.assert_eq(db.sismember("dest", "d"), true)
}

test "sdiff_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  db.sadd("set1", "d") |> ignore
  
  db.sadd("set2", "b") |> ignore
  db.sadd("set2", "c") |> ignore
  
  db.sadd("set3", "d") |> ignore
  
  let diff1 = db.sdiff(["set1", "set2"])
  @test.assert_eq(diff1.length(), 2)
  @test.assert_eq(diff1.contains("a"), true)
  @test.assert_eq(diff1.contains("d"), true)
  @test.assert_eq(diff1.contains("b"), false)
  @test.assert_eq(diff1.contains("c"), false)
  
  let diff2 = db.sdiff(["set1", "set2", "set3"])
  @test.assert_eq(diff2.length(), 1)
  @test.assert_eq(diff2.contains("a"), true)
  
  let diff3 = db.sdiff(["set1", "nonexistent"])
  @test.assert_eq(diff3.length(), 4)
  
  let diff4 = db.sdiff([])
  @test.assert_eq(diff4.length(), 0)
}

test "sdiffstore_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  
  db.sadd("set2", "b") |> ignore
  
  let count = db.sdiffstore("dest", ["set1", "set2"])
  @test.assert_eq(count, 2)
  @test.assert_eq(db.scard("dest"), 2)
  @test.assert_eq(db.sismember("dest", "a"), true)
  @test.assert_eq(db.sismember("dest", "c"), true)
  @test.assert_eq(db.sismember("dest", "b"), false)
}

test "smove_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  
  db.sadd("set2", "x") |> ignore
  db.sadd("set2", "y") |> ignore
  
  @test.assert_eq(db.smove("set1", "set2", "b"), true)
  @test.assert_eq(db.sismember("set1", "b"), false)
  @test.assert_eq(db.sismember("set2", "b"), true)
  @test.assert_eq(db.scard("set1"), 2)
  @test.assert_eq(db.scard("set2"), 3)
  
  @test.assert_eq(db.smove("set1", "set2", "nonexistent"), false)
  
  @test.assert_eq(db.smove("nonexistent", "set2", "a"), false)
  
  @test.assert_eq(db.smove("set1", "newset", "a"), true)
  @test.assert_eq(db.sismember("newset", "a"), true)
}

test "spop_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  
  let popped = db.spop("set1")
  @test.assert_eq(popped is None, false)
  @test.assert_eq(db.scard("set1"), 2)
  
  db.spop("set1") |> ignore
  db.spop("set1") |> ignore
  
  @test.assert_eq(db.scard("set1"), 0)
  
  let empty_pop = db.spop("set1")
  @test.assert_eq(empty_pop is None, true)
  
  let nonexistent_pop = db.spop("nonexistent")
  @test.assert_eq(nonexistent_pop is None, true)
}

test "set_operations_with_expire" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  
  db.sadd("set2", "b") |> ignore
  db.sadd("set2", "c") |> ignore
  db.sadd("set2", "d") |> ignore
  
  db.expire("set2", 1) |> ignore
  
  db.advance_time(2000)
  
  let inter = db.sinter(["set1", "set2"])
  @test.assert_eq(inter.length(), 0)
  
  let union = db.sunion(["set1", "set2"])
  @test.assert_eq(union.length(), 3)
  
  let diff = db.sdiff(["set1", "set2"])
  @test.assert_eq(diff.length(), 3)
}

test "set_extended_edge_cases" {
  let db = Database::new()
  
  @test.assert_eq(db.sadd("s1", "a"), true)
  @test.assert_eq(db.sadd("s1", "a"), false)
  @test.assert_eq(db.sadd("s1", "b"), true)
  
  @test.assert_eq(db.srem("s1", "nonexistent"), false)
  @test.assert_eq(db.srem("s1", "a"), true)
  @test.assert_eq(db.srem("s1", "a"), false)
  
  @test.assert_eq(db.scard("nonexistent"), 0)
  @test.assert_eq(db.sismember("nonexistent", "x"), false)
  
  db.sadd("empty_set", "temp") |> ignore
  db.srem("empty_set", "temp") |> ignore
  @test.assert_eq(db.scard("empty_set"), 0)
  
  db.sadd("set_a", "1") |> ignore
  db.sadd("set_a", "2") |> ignore
  db.sadd("set_a", "3") |> ignore
  db.sadd("set_b", "3") |> ignore
  db.sadd("set_b", "4") |> ignore
  db.sadd("set_b", "5") |> ignore
  
  let inter = db.sinter(["set_a", "set_b"])
  @test.assert_eq(inter.length(), 1)
  @test.assert_eq(inter.contains("3"), true)
  
  let self_inter = db.sinter(["set_a"])
  @test.assert_eq(self_inter.length(), 3)
  
  let self_union = db.sunion(["set_a"])
  @test.assert_eq(self_union.length(), 3)
  
  let self_diff = db.sdiff(["set_a"])
  @test.assert_eq(self_diff.length(), 3)
  
  let no_overlap = db.sinter(["set_a", "set_b", "nonexistent"])
  @test.assert_eq(no_overlap.length(), 0)
}

test "getset_wrong_type_noop" {
  let db = Database::new()
  db.hset("h", "f", "v") |> ignore
  @test.assert_eq(db.getset("h", "new"), None)
  @test.assert_eq(db.type_of("h"), "hash")
  @test.assert_eq(db.hget("h", "f"), Some("v"))
}

test "srandmember_randomkey" {
  let db = Database::new()
  @test.assert_eq(db.srandmember("nonexistent") is None, true)

  db.sadd("s", "a") |> ignore
  db.sadd("s", "b") |> ignore
  db.sadd("s", "c") |> ignore
  let m = db.srandmember("s")
  @test.assert_eq(m is None, false)
  match m {
    Some(v) => @test.assert_eq(db.sismember("s", v), true)
    None => ()
  }
  @test.assert_eq(db.scard("s"), 3)

  db.spop("s") |> ignore
  db.spop("s") |> ignore
  db.spop("s") |> ignore
  @test.assert_eq(db.srandmember("s") is None, true)

  let db2 = Database::new()
  db2.set("k1", "v") |> ignore
  let rk = db2.randomkey()
  match rk {
    Some(k) => @test.assert_eq(db2.exists(k), true)
    None => ()
  }
}

test "smove_same_key" {
  let db = Database::new()
  db.sadd("s", "x") |> ignore
  @test.assert_eq(db.smove("s", "s", "x"), true)
  @test.assert_eq(db.scard("s"), 1)
  @test.assert_eq(db.sismember("s", "x"), true)
  @test.assert_eq(db.smove("s", "s", "y"), false)

  db.expire("s", 100) |> ignore
  db.smove("s", "s", "x") |> ignore
  @test.assert_eq(db.ttl("s") >= 0, true)
}

test "set_store_clears_destination_ttl" {
  let db = Database::new()
  db.sadd("dest", "old") |> ignore
  db.expire("dest", 100) |> ignore
  db.sadd("s1", "a") |> ignore
  db.sinterstore("dest", ["s1"]) |> ignore
  @test.assert_eq(db.ttl("dest"), -1)
  @test.assert_eq(db.sismember("dest", "a"), true)
}

test "batch_set_commands" {
  let db = Database::new()
  @test.assert_eq(db.rpush_all("q", ["a", "b", "c"]), 3)
  @test.assert_eq(db.lindex("q", 0), Some("a"))
  @test.assert_eq(db.lpush_all("q", ["x", "y"]), 5)
  @test.assert_eq(db.lindex("q", 0), Some("y"))
  @test.assert_eq(db.lindex("q", 1), Some("x"))
  @test.assert_eq(db.lpush_all("q", []), -1)
  db.set("str", "v") |> ignore
  @test.assert_eq(db.lpush_all("str", ["a"]), -1)

  @test.assert_eq(db.sadd_all("set", ["a", "b", "a"]), 2)
  @test.assert_eq(db.srem_all("set", ["a", "c"]), 1)
  @test.assert_eq(db.scard("set"), 1)
  @test.assert_eq(db.sadd_all("str", ["a"]), -1)
}

test "read_set_type_errors" {
  let db = Database::new()
  db.sadd("s1", "a") |> ignore
  db.set("str", "v") |> ignore

  @test.assert_eq(
    match db.try_sinter(["s1", "str"]) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.sinter(["s1", "str"]).length(), 0)
  @test.assert_eq(
    match db.try_sunion(["s1", "str"]) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_sdiff(["s1", "str"]) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_sinter(["s1"]) {
      Ok(values) => values.length()
      Err(_) => -1
    },
    1,
  )
}

test "set_options_and_getex" {
  let db = Database::new()

  @test.assert_eq(
    match db.set_with_options("k", "v", true, false, false, None, None, None, None, false) {
      Ok(_) => true
      Err(_) => false
    },
    true,
  )
  @test.assert_eq(
    match db.set_with_options("k", "v2", true, false, false, None, None, None, None, false) {
      Ok(Some(_)) => true
      Ok(None) => false
      Err(_) => false
    },
    false,
  )
  @test.assert_eq(db.get("k"), Some("v"))
  @test.assert_eq(
    match db.set_with_options("missing", "v", false, true, false, None, None, None, None, false) {
      Ok(Some(_)) => true
      Ok(None) => false
      Err(_) => false
    },
    false,
  )
  @test.assert_eq(db.setxx("k", "v3"), true)
  @test.assert_eq(db.get("k"), Some("v3"))
  @test.assert_eq(db.setxx("missing", "v"), false)

  @test.assert_eq(
    match db.set_with_options("ex", "v", false, false, false, Some(10), None, None, None, false) {
      Ok(_) => true
      Err(_) => false
    },
    true,
  )
  @test.assert_eq(db.ttl("ex"), 10)
  @test.assert_eq(
    match db.set_with_options("px", "v", false, false, false, None, Some(1500L), None, None, false) {
      Ok(_) => true
      Err(_) => false
    },
    true,
  )
  @test.assert_eq(db.pttl("px"), 1500)

  db.set("keep", "v") |> ignore
  db.expire("keep", 100) |> ignore
  @test.assert_eq(
    match db.set_with_options("keep", "v2", false, false, false, None, None, None, None, true) {
      Ok(_) => true
      Err(_) => false
    },
    true,
  )
  @test.assert_eq(db.ttl("keep"), 100)

  @test.assert_eq(
    match db.set_with_options("x", "v", true, true, false, None, None, None, None, false) {
      Err(RedisError::SyntaxError) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.set_with_options("x", "v", false, false, false, Some(10), Some(10L), None, None, false) {
      Err(RedisError::SyntaxError) => true
      _ => false
    },
    true,
  )

  db.set("g", "v") |> ignore
  @test.assert_eq(
    match db.try_getex_px("g", 20L) {
      Ok(value) => value
      Err(_) => ""
    },
    "v",
  )
  @test.assert_eq(db.pttl("g"), 20)
  @test.assert_eq(
    match db.try_getex_persist("g") {
      Ok(value) => value
      Err(_) => ""
    },
    "v",
  )
  @test.assert_eq(db.ttl("g"), -1)
  @test.assert_eq(
    match db.try_getex_persist("missing") {
      Err(RedisError::NotFound) => true
      _ => false
    },
    true,
  )

  @test.assert_eq(db.msetnx(["a", "b"], ["1", "2"]), true)
  @test.assert_eq(db.msetnx(["b", "c"], ["x", "3"]), false)
  @test.assert_eq(db.exists("c"), false)
  @test.assert_eq(db.get("b"), Some("2"))
}

test "set_get_and_expire_options" {
  let db = Database::new()
  db.set("s", "old") |> ignore
  @test.assert_eq(db.set_get("s", "new"), Some("old"))
  @test.assert_eq(db.get("s"), Some("new"))
  @test.assert_eq(db.set_get("missing", "v"), None)
  @test.assert_eq(db.get("missing"), Some("v"))
  db.hset("h", "f", "v") |> ignore
  @test.assert_eq(db.set_get("h", "x"), None)
  @test.assert_eq(db.type_of("h"), "hash")
  @test.assert_eq(
    match db.try_set_get("h", "x") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )

  db.set("k", "v") |> ignore
  @test.assert_eq(db.expire_with_options("k", 100, true, false, false, false), true)
  @test.assert_eq(db.ttl("k"), 100)
  @test.assert_eq(db.expire_with_options("k", 200, true, false, false, false), false)
  @test.assert_eq(db.expire_with_options("k", 200, false, true, false, false), true)
  @test.assert_eq(db.ttl("k"), 200)
  @test.assert_eq(db.expire_with_options("k", 100, false, false, false, true), true)
  @test.assert_eq(db.ttl("k"), 100)
  @test.assert_eq(db.expire_with_options("k", 300, false, false, true, false), true)
  @test.assert_eq(db.ttl("k"), 300)
  @test.assert_eq(db.expire_with_options("k", 100, false, false, true, false), false)
  @test.assert_eq(db.expire_with_options("k", 100, true, true, false, false), false)

  db.set("no_ttl", "v") |> ignore
  @test.assert_eq(db.expire_with_options("no_ttl", 100, false, true, false, false), false)
  @test.assert_eq(db.expire_with_options("no_ttl", 100, false, false, false, true), true)
  @test.assert_eq(db.ttl("no_ttl"), 100)
}

test "transaction_set_operations" {
  let db = Database::new()
  
  ignore(db.multi())
  ignore(db.sadd("set_key", "member1"))
  ignore(db.sadd("set_key", "member2"))
  ignore(db.srem("set_key", "member1"))
  @test.assert_eq(db.exec(), "OK")
  
  @test.assert_eq(db.smembers("set_key"), ["member2"])
}
