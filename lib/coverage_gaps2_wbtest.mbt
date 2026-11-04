test "coverage_deque_gaps" {
  let d = Deque::new()
  @test.assert_eq(d.pop_front(), None)
  @test.assert_eq(d.pop_back(), None)
  @test.assert_eq(d.get_at(-1), None)
  @test.assert_eq(d.get_at(0), None)
  @test.assert_eq(d.set_at(-1, "x"), false)
  @test.assert_eq(d.set_at(0, "x"), false)
  @test.assert_eq(d.remove_at(0), false)
  @test.assert_eq(d.find_index("zz"), -1)
  @test.assert_eq(d.insert_at(-1, "x"), false)
  @test.assert_eq(d.insert_at(1, "x"), false)

  d.push_back("a")
  d.push_back("b")
  @test.assert_eq(d.get_at(2), None)
  @test.assert_eq(d.set_at(2, "x"), false)
  @test.assert_eq(d.insert_at(3, "x"), false)
  @test.assert_eq(d.insert_at(1, "m"), true)
  @test.assert_eq(d.to_array(), ["a", "m", "b"])
  @test.assert_eq(d.remove_at(5), false)
  @test.assert_eq(d.remove_at(1), true)
  @test.assert_eq(d.to_array(), ["a", "b"])
  @test.assert_eq(d.get_at(-1), None)
  @test.assert_eq(d.find_index("b"), 1)
}

test "coverage_set_expired_gaps" {
  let db = Database::new()
  db.sadd_all("es1", ["a", "b"]) |> ignore
  db.pexpire("es1", 1L) |> ignore
  db.advance_time(10)

  @test.assert_eq(db.srem("es1", "a"), false)
  @test.assert_eq(db.scard("es1"), 0)
  @test.assert_eq(db.sismember("es1", "a"), false)
  @test.assert_eq(db.spop("es1"), None)
  @test.assert_eq(db.srandmember("es1"), None)
  @test.assert_eq(db.srandmember_count("es1", 1).length(), 0)
  @test.assert_eq(db.smembers("es1"), [])
  @test.assert_eq(db.sinter(["es1", "missing"]).length(), 0)
  @test.assert_eq(db.sunion(["es1"]).length(), 0)
  @test.assert_eq(db.sdiff(["es1"]).length(), 0)
  @test.assert_eq(db.sintercard(["es1"], 0), 0)
  @test.assert_eq(db.smove("es1", "dst", "a"), false)
  @test.assert_eq(db.sinterstore("dste", ["es1"]), 0)
  @test.assert_eq(db.sunionstore("dste", ["es1"]), 0)
  @test.assert_eq(db.sdiffstore("dste", ["es1"]), 0)
  @test.assert_eq(db.sadd_all("es2", []), -1)
  @test.assert_eq(db.srem_all("es2", []), -1)
  @test.assert_eq(db.sscan("es1", 0, 10), (0, []))
  @test.assert_eq(db.sscan_with_match("missing", 0, 10, Some("*")).1.length(), 0)

  db.sadd_all("live", ["x", "y"]) |> ignore
  @test.assert_eq(db.srandmember("live") != None, true)
  @test.assert_eq(db.sinter(["live", "nope1", "nope2"]).length(), 0)
  @test.assert_eq(db.sdiff(["live"]).length(), 2)
}

test "coverage_getex_variants" {
  let db = Database::new()
  db.set("g1", "v1")

  match db.try_getex_ex("g1", 100) {
    Ok(v) => @test.assert_eq(v, "v1")
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.pttl("g1"), 100000)
  match db.try_getex_ex("g1", 0) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_getex_ex("g1", -5) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_getex_ex("missing", 100) {
    Err(RedisError::NotFound) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  db.hset("gh", "f", "v") |> ignore
  match db.try_getex_ex("gh", 100) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.set("g2", "v2")
  match db.try_getex_px("g2", 500L) {
    Ok(v) => @test.assert_eq(v, "v2")
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.pttl("g2"), 500)
  match db.try_getex_px("g2", 0L) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_getex_px("missing", 100L) {
    Err(RedisError::NotFound) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.set("g3", "v3")
  match db.try_getex_exat("g3", 1000000L) {
    Ok(v) => @test.assert_eq(v, "v3")
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.ttl("g3") > 0, true)
  match db.try_getex_exat("g3", 0L) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_getex_exat("missing", 1000000L) {
    Err(RedisError::NotFound) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.set("g4", "v4")
  match db.try_getex_pxat("g4", 1000000L) {
    Ok(v) => @test.assert_eq(v, "v4")
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_getex_pxat("g4", 0L) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_getex_pxat("missing", 1000000L) {
    Err(RedisError::NotFound) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  @test.assert_eq(db.getex("g4", Some(3000L)), Some("v4"))
  @test.assert_eq(db.getex("g4", None), Some("v4"))
  @test.assert_eq(db.ttl("g4"), -1)
  @test.assert_eq(db.getex("missing", None), None)
  @test.assert_eq(db.getex("missing", Some(100L)), None)
}

test "d1_past_timestamp_deletes_key" {
  let db = Database::new()
  db.set("p1", "v")
  @test.assert_eq(db.expireat("p1", 0L), true)
  @test.assert_eq(db.exists("p1"), false)
  db.set("p2", "v")
  @test.assert_eq(db.pexpireat("p2", 0L), true)
  @test.assert_eq(db.exists("p2"), false)
  db.set("p3", "v")
  @test.assert_eq(db.pexpireat("p3", -1000L), true)
  @test.assert_eq(db.exists("p3"), false)

  db.set("p4", "v")
  @test.assert_eq(db.expireat("p4", 2000000000L), true)
  @test.assert_eq(db.ttl("p4") > 0, true)
  db.set("p5", "v")
  @test.assert_eq(db.pexpireat_with_options("p5", 2000000000000L, true, false, false, false), true)
  db.set("p6", "v")
  db.pexpire("p6", 1000L) |> ignore
  @test.assert_eq(db.pexpireat_with_options("p6", 2000000000000L, false, false, true, false), true)
  @test.assert_eq(db.pexpireat_with_options("p6", 1000000000000L, false, false, true, false), false)
  @test.assert_eq(db.expireat_with_options("missing", 2000000000L, true, false, false, false), false)
}

test "d1_try_lset_and_consistency" {
  let db = Database::new()
  db.set("h1", "x")
  db.hset("hh", "f", "v") |> ignore

  match db.try_lset("missing", 0, "v") {
    Err(RedisError::NotFound) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_lset("hh", 0, "v") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  db.rpush_all("l1", ["a", "b", "c"]) |> ignore
  match db.try_lset("l1", 99, "v") {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_lset("l1", -99, "v") {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_lset("l1", 1, "B") {
    Ok(v) => @test.assert_eq(v, true)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.lrange("l1", 0, -1), ["a", "B", "c"])
  @test.assert_eq(db.lset("l1", 99, "v"), false)

  @test.assert_eq(db.getset("hh", "x"), None)
  @test.assert_eq(db.hget("hh", "f"), Some("v"))
  db.set("gs", "old")
  @test.assert_eq(db.getset("gs", "new"), Some("old"))
  @test.assert_eq(db.get("gs"), Some("new"))

  @test.assert_eq(db.hmset("hm1", ["a", "b"], ["1", "2"]), true)
  @test.assert_eq(db.hmset("h1", ["a"], ["1"]), false)
  @test.assert_eq(db.hmset("hm1", ["a"], []), false)
}
