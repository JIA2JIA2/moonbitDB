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
  db.sadd_all("es1", ["a", "b"]) |> ignore |> ignore
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

  db.sadd_all("live", ["x", "y"]) |> ignore |> ignore
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
  match db.try_getex_ex("missing", 100) {
    Err(RedisError::NotFound) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  db.hset("gh", "f", "v") |> ignore |> ignore
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

test "f6_bitfield" {
  let db = Database::new()
  db.set("bf", "hello")

  match db.try_bitfield_get("bf", 0, 8, false) {
    Ok(v) => @test.assert_eq(v, 104)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_get("bf", 0, 8, true) {
    Ok(v) => @test.assert_eq(v, 104)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_get("bf", 0, 16, false) {
    Ok(v) => @test.assert_eq(v, 104)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_get("bf", 16, 8, false) {
    Ok(v) => @test.assert_eq(v, 101)
    Err(_) => @test.assert_eq(true, false)
  }

  match db.try_bitfield_set("bf", 0, 8, false, 65) {
    Ok(old) => @test.assert_eq(old, 104)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_get("bf", 0, 8, false) {
    Ok(v) => @test.assert_eq(v, 65)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.get("bf"), Some("Aello"))

  match db.try_bitfield_incrby("bf", 0, 8, false, 1) {
    Ok(v) => @test.assert_eq(v, 66)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_incrby("bf", 0, 8, false, 190) {
    Ok(v) => @test.assert_eq(v, 0)
    Err(_) => @test.assert_eq(true, false)
  }

  match db.try_bitfield_get("missing", 0, 8, false) {
    Ok(v) => @test.assert_eq(v, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_set("missing", 0, 8, false, 255) {
    Ok(old) => @test.assert_eq(old, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_get("missing", 0, 8, false) {
    Ok(v) => @test.assert_eq(v, 255)
    Err(_) => @test.assert_eq(true, false)
  }

  db.hset("bfh", "f", "v") |> ignore
  match db.try_bitfield_get("bfh", 0, 8, false) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_bitfield_get("bf", -1, 8, false) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_bitfield_get("bf", 0, 33, false) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.set("sign", "")
  match db.try_bitfield_set("sign", 0, 8, true, -1) {
    Ok(old) => @test.assert_eq(old, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_get("sign", 0, 8, true) {
    Ok(v) => @test.assert_eq(v, -1)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_get("sign", 0, 8, false) {
    Ok(v) => @test.assert_eq(v, 255)
    Err(_) => @test.assert_eq(true, false)
  }
}

test "f3_getdel_lmpop_copy_client_config" {
  let db = Database::new()
  db.set("gd", "v")
  db.hset("gdh", "f", "v") |> ignore

  match db.try_getdel("gd") {
    Ok(v) => @test.assert_eq(v, Some("v"))
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.exists("gd"), false)
  match db.try_getdel("gdh") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.hget("gdh", "f"), Some("v"))
  match db.try_getdel("missing") {
    Ok(v) => @test.assert_eq(v, None)
    Err(_) => @test.assert_eq(true, false)
  }

  db.rpush_all("ml", ["a", "b", "c"]) |> ignore
  match db.try_lmpop(["missing", "ml"], true, 2) {
    Ok(r) => @test.assert_eq(
      match r {
        Some((k, vals)) => k == "ml" && vals == ["a", "b"]
        None => false
      },
      true,
    )
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_lmpop(["ml"], false, 0) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_lmpop(["missing"], true, 1) {
    Ok(r) => @test.assert_eq(r, None)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_lmpop([], true, 1) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.zadd("mz", 1.0, "x") |> ignore
  db.zadd("mz", 2.0, "y") |> ignore
  match db.try_zmpop(["mz"], true, 1) {
    Ok(r) => @test.assert_eq(
      match r {
        Some((k, vals)) => k == "mz" && vals.length() == 1 && vals[0].0 == "x"
        None => false
      },
      true,
    )
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zmpop(["mz"], true, 0) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.set("csrc", "data")
  db.set("cdest", "old")
  @test.assert_eq(db.copy_with_replace("csrc", "cdest", false), false)
  @test.assert_eq(db.copy_with_replace("csrc", "cdest", true), true)
  @test.assert_eq(db.get("cdest"), Some("data"))

  @test.assert_eq(db.client_setname("myclient"), "OK")
  @test.assert_eq(db.client_getname(), "myclient")
  @test.assert_eq(db.client_kill("1"), "OK")

  @test.assert_eq(db.config_set("maxmemory", "100mb"), "OK")
  @test.assert_eq(db.config_get("maxmemory"), ["maxmemory", "100mb"])
  @test.assert_eq(db.config_get("databases"), ["databases", "16"])
}

test "f5_deque_rebalance_gaps" {
  let d = Deque::new()
  d.push_back("b1")
  d.push_front("f1")
  d.push_back("b2")
  d.push_front("f2")

  @test.assert_eq(d.pop_front(), Some("f2"))
  @test.assert_eq(d.pop_front(), Some("f1"))
  @test.assert_eq(d.pop_front(), Some("b1"))
  @test.assert_eq(d.pop_back(), Some("b2"))
  @test.assert_eq(d.pop_back(), None)
  @test.assert_eq(d.pop_front(), None)

  d.push_back("x")
  d.push_back("y")
  @test.assert_eq(d.insert_at(2, "m"), true)
  @test.assert_eq(d.get_at(0) != None, true)
  @test.assert_eq(d.get_at(1) != None, true)
  @test.assert_eq(d.get_at(2) != None, true)
  @test.assert_eq(d.length(), 3)
  @test.assert_eq(d.remove_at(2), true)
  @test.assert_eq(d.length(), 2)
  @test.assert_eq(d.find_index("m"), -1)
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
  db.hset("hh", "f", "v") |> ignore |> ignore

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
    Err(RedisError::IndexOutOfRange) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_lset("l1", -99, "v") {
    Err(RedisError::IndexOutOfRange) => @test.assert_eq(true, true)
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

test "d2_reverse_range_limits" {
  let db = Database::new()
  db.set("str", "x")
  db.zadd("z", 0.0, "a") |> ignore
  db.zadd("z", 0.0, "b") |> ignore
  db.zadd("z", 0.0, "c") |> ignore
  db.zadd("z", 0.0, "d") |> ignore

  match db.try_zrevrangebylex_with_limit("z", "+", "-", 1, 2) {
    Ok(items) => @test.assert_eq(items, ["c", "b"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrevrangebylex_with_limit("z", "+", "-", 0, 0) {
    Ok(items) => @test.assert_eq(items, ["d", "c", "b", "a"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrevrangebylex_with_limit("z", "+", "-", -1, 2) {
    Ok(items) => @test.assert_eq(items.length(), 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrevrangebylex_with_limit("z", "+", "-", 10, 2) {
    Ok(items) => @test.assert_eq(items.length(), 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrevrangebylex_with_limit("str", "+", "-", 0, 1) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zrevrangebylex_with_limit("z", "bad", "-", 0, 1) {
    Ok(items) => @test.assert_eq(items.length(), 0)
    Err(_) => @test.assert_eq(true, false)
  }

  db.zadd("zs", 1.0, "a") |> ignore
  db.zadd("zs", 2.0, "b") |> ignore
  db.zadd("zs", 3.0, "c") |> ignore
  db.zadd("zs", 4.0, "d") |> ignore
  @test.assert_eq(db.zrevrangebyscore_with_limit("zs", 4.0, 1.0, 1, 2), ["c", "b"])
  @test.assert_eq(db.zrevrangebyscore_with_limit("zs", 4.0, 1.0, 0, 0), ["d", "c", "b", "a"])
  @test.assert_eq(db.zrevrangebyscore_with_limit("zs", 4.0, 1.0, -1, 2), [])
  @test.assert_eq(db.zrevrangebyscore_with_limit("zs", 4.0, 1.0, 10, 2), [])
  @test.assert_eq(db.zrangebyscore_with_limit("zs", 1.0, 4.0, -1, 2), [])
  @test.assert_eq(db.zrangebyscore_with_limit("zs", 1.0, 4.0, 1, 2), ["b", "c"])
}

test "d2_getex_convenience_spop_hsetall" {
  let db = Database::new()
  db.set("g1", "v")
  @test.assert_eq(db.getex_ex("g1", 100), Some("v"))
  @test.assert_eq(db.pttl("g1"), 100000)
  @test.assert_eq(db.getex_ex("g1", 0), None)
  @test.assert_eq(db.getex_exat("g1", 1000000L), Some("v"))
  @test.assert_eq(db.ttl("g1") > 0, true)
  @test.assert_eq(db.getex_pxat("g1", 2000000L), Some("v"))
  @test.assert_eq(db.pttl("g1") > 0, true)
  @test.assert_eq(db.getex_pxat("g1", 0L), None)

  db.sadd_all("s1", ["a", "b", "c", "d"]) |> ignore |> ignore
  @test.assert_eq(db.spop_count("s1", 2).length(), 2)
  @test.assert_eq(db.scard("s1"), 2)
  @test.assert_eq(db.spop_count("s1", 10).length(), 2)
  @test.assert_eq(db.exists("s1"), false)
  @test.assert_eq(db.spop_count("s1", 1), [])
  @test.assert_eq(db.spop_count("missing", 1), [])
  @test.assert_eq(db.spop_count("s1", 0), [])
  @test.assert_eq(db.spop_count("s1", -1), [])

  db.set("str", "x")
  @test.assert_eq(db.hset_all("missing", ["a", "b"], ["1", "2"]), 2)
  @test.assert_eq(db.hset_all("missing", ["a", "b"], ["9", "9"]), 0)
  @test.assert_eq(db.hget("missing", "a"), Some("9"))
  @test.assert_eq(db.hset_all("missing", ["c"], ["3"]), 1)
  @test.assert_eq(db.hset_all("str", ["a"], ["1"]), -1)
  @test.assert_eq(db.hset_all("missing", ["a"], []), -1)
  @test.assert_eq(db.hset_all("missing", [], ["1"]), -1)
  match db.try_hset_all("str", ["a"], ["1"]) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
}

test "d3_score_bounds" {
  let db = Database::new()
  db.set("str", "x")
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 3.0, "c") |> ignore
  db.zadd("z", 4.0, "d") |> ignore

  match db.try_zrangebyscore_by_bound("z", "(1", "(4", 0, 0) {
    Ok(items) => @test.assert_eq(items, ["b", "c"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangebyscore_by_bound("z", "1", "4", 0, 0) {
    Ok(items) => @test.assert_eq(items, ["a", "b", "c", "d"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangebyscore_by_bound("z", "(1", "3", 0, 0) {
    Ok(items) => @test.assert_eq(items, ["b", "c"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangebyscore_by_bound("z", "2", "(4", 0, 0) {
    Ok(items) => @test.assert_eq(items, ["b", "c"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangebyscore_by_bound("z", "-inf", "+inf", 0, 0) {
    Ok(items) => @test.assert_eq(items, ["a", "b", "c", "d"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangebyscore_by_bound("z", "-inf", "+inf", 1, 2) {
    Ok(items) => @test.assert_eq(items, ["b", "c"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangebyscore_by_bound("z", "(1", "bad", 0, 0) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zrangebyscore_by_bound("str", "-inf", "+inf", 0, 0) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  match db.try_zrevrangebyscore_by_bound("z", "+inf", "(2", 0, 0) {
    Ok(items) => @test.assert_eq(items, ["d", "c"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrevrangebyscore_by_bound("z", "(4", "1", 0, 0) {
    Ok(items) => @test.assert_eq(items, ["c", "b", "a"])
    Err(_) => @test.assert_eq(true, false)
  }

  match db.try_zcount_by_bound("z", "(1", "(4") {
    Ok(count) => @test.assert_eq(count, 2)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zcount_by_bound("z", "-inf", "+inf") {
    Ok(count) => @test.assert_eq(count, 4)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zcount_by_bound("z", "x", "+inf") {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.zadd("z2", 1.0, "x") |> ignore
  db.zadd("z2", 2.0, "y") |> ignore
  db.zadd("z2", 3.0, "z") |> ignore
  match db.try_zremrangebyscore_by_bound("z2", "(1", "(3") {
    Ok(count) => @test.assert_eq(count, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.zcard("z2"), 2)
  @test.assert_eq(db.zscore("z2", "y"), None)
  match db.try_zremrangebyscore_by_bound("z2", "-inf", "+inf") {
    Ok(count) => @test.assert_eq(count, 2)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.exists("z2"), false)
  match db.try_zremrangebyscore_by_bound("str", "-inf", "+inf") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
}

test "d3_zrangestore_byscore_bylex" {
  let db = Database::new()
  db.set("str", "x")
  db.zadd("src", 1.0, "a") |> ignore
  db.zadd("src", 2.0, "b") |> ignore
  db.zadd("src", 3.0, "c") |> ignore
  db.zadd("src", 4.0, "d") |> ignore

  match db.try_zrangestore_byscore("dst1", "src", "(1", "(4", 0, 0) {
    Ok(count) => {
      @test.assert_eq(count, 2)
      @test.assert_eq(db.zrange_with_scores("dst1", 0, -1), [("b", 2.0), ("c", 3.0)])
    }
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangestore_byscore("dst2", "src", "-inf", "+inf", 1, 2) {
    Ok(count) => {
      @test.assert_eq(count, 2)
      @test.assert_eq(db.zrange_with_scores("dst2", 0, -1), [("b", 2.0), ("c", 3.0)])
    }
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangestore_byscore("dst3", "src", "bad", "+inf", 0, 0) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zrangestore_byscore("dst3", "str", "-inf", "+inf", 0, 0) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.zadd("lexsrc", 0.0, "a") |> ignore
  db.zadd("lexsrc", 0.0, "b") |> ignore
  db.zadd("lexsrc", 0.0, "c") |> ignore
  db.zadd("lexsrc", 0.0, "d") |> ignore
  match db.try_zrangestore_bylex("ldst1", "lexsrc", "[b", "(d") {
    Ok(count) => {
      @test.assert_eq(count, 2)
      @test.assert_eq(db.zrange("ldst1", 0, -1), ["b", "c"])
    }
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangestore_bylex("ldst2", "lexsrc", "-", "+") {
    Ok(count) => @test.assert_eq(count, 4)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangestore_bylex("ldst3", "lexsrc", "bad", "+") {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zrangestore_bylex("ldst3", "str", "-", "+") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zrangestore_bylex("ldst4", "missing", "-", "+") {
    Ok(count) => @test.assert_eq(count, 0)
    Err(_) => @test.assert_eq(true, false)
  }
}

test "d4_transaction_extended_ops" {
  let db = Database::new()
  db.set("d1", "v")
  db.set("d2", "old")
  db.rpush_all("dl", ["a", "b", "c"]) |> ignore
  db.set("exp1", "v")
  db.set("per1", "v")
  db.pexpire("per1", 50000L) |> ignore

  ignore(db.multi())
  ignore(db.del("d1"))
  ignore(db.getset("d2", "new"))
  ignore(db.lpop("dl"))
  ignore(db.rpop("dl"))
  ignore(db.expire("exp1", 100))
  ignore(db.persist("per1"))
  @test.assert_eq(db.exec(), "OK")

  @test.assert_eq(db.exists("d1"), false)
  @test.assert_eq(db.get("d2"), Some("new"))
  @test.assert_eq(db.lrange("dl", 0, -1), ["b"])
  @test.assert_eq(db.ttl("exp1") > 0, true)
  @test.assert_eq(db.ttl("per1"), -1)

  ignore(db.multi())
  ignore(db.lpop("dl"))
  ignore(db.lpop("dl"))
  @test.assert_eq(db.exec(), "OK")
  @test.assert_eq(db.exists("dl"), false)
}

test "d4_hgetall_entries" {
  let db = Database::new()
  @test.assert_eq(db.hgetall_entries("missing").length(), 0)
  db.hset("h1", "a", "1") |> ignore
  db.hset("h1", "b", "2") |> ignore
  let entries = db.hgetall_entries("h1")
  @test.assert_eq(entries.length(), 2)
  db.set("str", "x")
  @test.assert_eq(db.hgetall_entries("str").length(), 0)
  db.pexpire("h1", 1L) |> ignore
  db.advance_time(10)
  @test.assert_eq(db.hgetall_entries("h1").length(), 0)
}

test "d4_sort_and_sample_direct" {
  let arr : Array[(String, Float)] = [("m", 5.0), ("a", 3.0), ("z", 9.0), ("b", 3.0), ("c", 1.0)]
  let sorted = merge_sort(arr)
  @test.assert_eq(sorted.length(), 5)
  @test.assert_eq(sorted[0].0, "c")
  @test.assert_eq(sorted[4].0, "z")

  let arr2 : Array[(String, Float)] = [("e", 5.0), ("d", 4.0), ("c", 3.0), ("b", 2.0), ("a", 1.0)]
  let sorted2 = merge_sort(arr2)
  @test.assert_eq(sorted2[0].0, "a")
  @test.assert_eq(sorted2[4].0, "e")

  let m1 : Array[(String, Float)] = [("a", 1.0), ("c", 3.0)]
  let m2 : Array[(String, Float)] = [("b", 2.0)]
  let merged = merge(m1, m2)
  @test.assert_eq(merged.length(), 3)
  @test.assert_eq(merged[0].0, "a")
  @test.assert_eq(merged[1].0, "b")
  @test.assert_eq(merged[2].0, "c")

  let m3 : Array[(String, Float)] = [("x", 2.0)]
  let m4 : Array[(String, Float)] = [("a", 1.0), ("c", 3.0)]
  let merged2 = merge(m3, m4)
  @test.assert_eq(merged2[0].0, "a")
  @test.assert_eq(merged2[1].0, "x")
  @test.assert_eq(merged2[2].0, "c")

  let small : Array[(String, Float)] = [("n", 2.0), ("m", 1.0), ("k", 2.0)]
  let sorted3 = sort_by_score(small)
  @test.assert_eq(sorted3[0].0, "m")
  @test.assert_eq(sorted3[1].0, "k")
  @test.assert_eq(sorted3[2].0, "n")

  let db = Database::new()
  @test.assert_eq(db.random_sample([], 3).length(), 0)
  @test.assert_eq(db.random_sample(["a"], 0).length(), 0)
  @test.assert_eq(db.random_sample(["a", "b"], 5).length(), 2)
  let src : Array[String] = ["p", "q", "r"]
  @test.assert_eq(db.random_sample(src, 2).length(), 2)
  @test.assert_eq(db.random_sample(src, -4).length(), 4)
}

test "d5_zadd_options_and_expiretime" {
  let db = Database::new()
  db.set("str", "x")
  db.zadd("z", 1.0, "a") |> ignore

  match db.try_zadd_with_options("z", 2.0, "a", true, false, false, false, false) {
    Ok(count) => @test.assert_eq(count, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_with_options("z", 2.0, "a", false, true, false, false, false) {
    Ok(count) => @test.assert_eq(count, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_with_options("z", 2.0, "a", false, true, true, false, false) {
    Ok(count) => @test.assert_eq(count, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_with_options("z", 3.0, "a", false, true, true, false, false) {
    Ok(count) => @test.assert_eq(count, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_with_options("z", 2.0, "a", false, true, true, true, false) {
    Ok(count) => @test.assert_eq(count, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_with_options("z", 4.0, "a", false, true, true, true, false) {
    Ok(count) => @test.assert_eq(count, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_with_options("z", 5.0, "a", false, true, true, false, true) {
    Ok(count) => @test.assert_eq(count, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_with_options("z", 3.0, "a", false, true, true, false, true) {
    Ok(count) => @test.assert_eq(count, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_with_options("z", 6.0, "a", false, true, true, false, true) {
    Ok(count) => @test.assert_eq(count, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_with_options("z", 1.0, "b", false, false, true, false, false) {
    Ok(count) => @test.assert_eq(count, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_with_options("z", 1.0, "b", true, true, false, false, false) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zadd_with_options("z", 1.0, "b", false, true, false, true, true) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zadd_with_options("str", 1.0, "b", false, false, false, false, false) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zadd_with_options("z", 0.0 / 0.0, "c", false, false, false, false, false) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  match db.try_zadd_incr("z", 10.0, "i", false, false, false, false) {
    Ok(v) => @test.assert_eq(v, Some(10.0))
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_incr("z", 5.0, "i", false, false, false, false) {
    Ok(v) => @test.assert_eq(v, Some(15.0))
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_incr("z", 5.0, "i", true, false, false, false) {
    Ok(v) => @test.assert_eq(v, None)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_incr("z", -100.0, "i", false, false, true, false) {
    Ok(v) => @test.assert_eq(v, None)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_incr("z", 1.0, "i", false, false, true, false) {
    Ok(v) => @test.assert_eq(v, Some(16.0))
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zadd_incr("z", 1.0, "i", false, false, false, true) {
    Ok(v) => @test.assert_eq(v, None)
    Err(_) => @test.assert_eq(true, false)
  }

  db.set("et1", "v")
  @test.assert_eq(db.expiretime("et1"), -1)
  @test.assert_eq(db.pexpiretime("et1"), -1)
  db.expire("et1", 100) |> ignore
  @test.assert_eq(db.expiretime("et1") > 0, true)
  @test.assert_eq(db.pexpiretime("et1") > 0, true)
  @test.assert_eq(db.pexpiretime("et1") > db.expiretime("et1") * 999, true)
  db.pexpire("et1", 1L) |> ignore
  db.advance_time(10)
  @test.assert_eq(db.expiretime("et1"), -2)
  @test.assert_eq(db.expiretime("missing"), -2)
}

test "d5_bitmap_commands" {
  let db = Database::new()
  db.set("h1", "x")
  db.hset("hh1", "f", "v") |> ignore

  match db.try_setbit("b1", 0, 1) {
    Ok(old) => @test.assert_eq(old, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.getbit("b1", 0), 1)
  @test.assert_eq(db.getbit("b1", 1), 0)
  @test.assert_eq(db.getbit("b1", 100), 0)

  match db.try_setbit("b1", 20, 1) {
    Ok(old) => @test.assert_eq(old, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.getbit("b1", 20), 1)
  @test.assert_eq(db.getbit("b1", 4), 0)

  match db.try_setbit("b1", 0, 0) {
    Ok(old) => @test.assert_eq(old, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.getbit("b1", 0), 0)

  match db.try_setbit("b1", -1, 1) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_setbit("b1", 0, 2) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_setbit("hh1", 0, 1) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_getbit("hh1", 0) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.setbit("hh1", 0, 1), -1)

  db.setbit("bp", 0, 1) |> ignore
  db.setbit("bp", 50, 1) |> ignore
  @test.assert_eq(db.bitpos("bp", 1), 0)
  match db.try_bitpos("bp", 1, 3, -1) {
    Ok(pos) => @test.assert_eq(pos, 50)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitpos("bp", 1, 1, 2) {
    Ok(pos) => @test.assert_eq(pos, -1)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.bitpos("bp", 0), 1)
  match db.try_bitpos("hh1", 1, 0, -1) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.bitpos("missing", 1), -1)
}

test "d6_watch_optimistic_lock" {
  let db = Database::new()
  db.set("w1", "v1")
  db.set("w2", "v2")

  db.watch("w1")
  db.set("w1", "outside") |> ignore
  ignore(db.multi())
  ignore(db.set("w1", "changed"))
  match db.try_exec() {
    Err(RedisError::TransactionAborted) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.get("w1"), Some("outside"))
  @test.assert_eq(RedisError::message(RedisError::TransactionAborted), "TransactionAborted Transaction aborted because the watched keys were modified")

  db.watch("w1")
  ignore(db.multi())
  ignore(db.set("w1", "ok"))
  @test.assert_eq(db.exec(), "OK")
  @test.assert_eq(db.get("w1"), Some("ok"))

  db.watch("w1")
  db.unwatch()
  db.set("w1", "free") |> ignore
  ignore(db.multi())
  ignore(db.set("w1", "again"))
  match db.try_exec() {
    Ok(_) => @test.assert_eq(true, true)
    Err(_) => @test.assert_eq(true, false)
  }

  db.watch("fresh")
  db.set("fresh", "outside") |> ignore
  ignore(db.multi())
  ignore(db.set("fresh", "created"))
  match db.try_exec() {
    Err(RedisError::TransactionAborted) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.get("fresh"), Some("outside"))

  db.set("exp1", "v")
  db.watch("exp1")
  db.pexpire("exp1", 1L) |> ignore
  db.advance_time(10)
  ignore(db.exists("exp1"))
  ignore(db.multi())
  ignore(db.set("exp1", "x"))
  match db.try_exec() {
    Err(RedisError::TransactionAborted) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.watch("w1")
  ignore(db.multi())
  ignore(db.set("w1", "discarded"))
  ignore(db.discard())
  db.set("w1", "after_discard") |> ignore
  ignore(db.multi())
  ignore(db.set("w1", "final"))
  match db.try_exec() {
    Err(RedisError::TransactionAborted) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.get("w1"), Some("after_discard"))

  db.unwatch()
  db.set("w1", "no_watch") |> ignore
  ignore(db.multi())
  ignore(db.set("w1", "x"))
  match db.try_exec() {
    Ok(_) => @test.assert_eq(true, true)
    Err(_) => @test.assert_eq(true, false)
  }
}

test "e1_info_sections_and_placeholders" {
  let db = Database::new()
  let server = db.info_section("server")
  @test.assert_eq(server.contains("# Server"), true)
  @test.assert_eq(server.contains("uptime_in_ms:0"), true)
  @test.assert_eq(db.info_section("keyspace").contains("db0:keys=0"), true)
  @test.assert_eq(db.info_section("memory").contains("# Memory"), true)
  @test.assert_eq(db.info_section("stats").contains("# Stats"), true)
  @test.assert_eq(db.info_section("unknown"), "")

  let full = db.info()
  @test.assert_eq(full.contains("# Server"), true)
  @test.assert_eq(full.contains("# Memory"), true)
  @test.assert_eq(full.contains("# Keyspace"), true)
  @test.assert_eq(full.contains("# Stats"), true)

  db.advance_time(5000)
  @test.assert_eq(db.info_section("server").contains("uptime_in_ms:5000"), true)
  db.set_time(1000000L)
  @test.assert_eq(db.info_section("server").contains("uptime_in_ms:0"), true)

  db.set("m1", "hello")
  @test.assert_eq(db.info_section("memory").contains("keys:1"), true)
  db.pexpire("m1", 1000L) |> ignore
  @test.assert_eq(db.info_section("keyspace").contains("expires=1"), true)

  db.hset("mh", "f", "v") |> ignore
  db.sadd("ms", "member") |> ignore
  db.zadd("mz", 1.0, "zm") |> ignore
  db.rpush("ml", "item") |> ignore
  let mem = db.info_section("memory")
  @test.assert_eq(mem.contains("keys:5"), true)
  ignore(db.zrange("mz", 0, -1))
  @test.assert_eq(db.info_section("stats").contains("zset_sorted_cache_entries:1"), true)
  @test.assert_eq(db.client_list("GETNAME"), "")
  @test.assert_eq(db.client_list("INFO").has_prefix("id=1"), true)
  @test.assert_eq(db.config_get("appendonly"), ["appendonly", "no"])
  @test.assert_eq(db.config_get("save"), ["save", ""])
  @test.assert_eq(db.config_get("timeout"), ["timeout", "0"])
  @test.assert_eq(db.config_get("weird_param"), ["weird_param", ""])
}

test "e2_bitop_and_bitcount_range" {
  let db = Database::new()
  db.setbit("s1", 0, 1) |> ignore
  db.setbit("s1", 1, 1) |> ignore
  db.setbit("s1", 2, 1) |> ignore
  db.setbit("s1", 3, 1) |> ignore
  db.setbit("s2", 2, 1) |> ignore
  db.setbit("s2", 3, 1) |> ignore
  db.setbit("s2", 4, 1) |> ignore
  db.setbit("s2", 5, 1) |> ignore

  match db.try_bitop("OR", "d1", ["s1", "s2"]) {
    Ok(len) => @test.assert_eq(len, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.bitcount("d1"), 6)
  match db.try_bitop("AND", "d2", ["s1", "s2"]) {
    Ok(len) => @test.assert_eq(len, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.bitcount("d2"), 2)
  match db.try_bitop("XOR", "d3", ["s1", "s2"]) {
    Ok(len) => @test.assert_eq(len, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.bitcount("d3"), 4)
  match db.try_bitop("NOT", "d4", ["s1"]) {
    Ok(len) => @test.assert_eq(len, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.bitcount("d4"), 12)
  @test.assert_eq(db.bitop("NOOP", "dx", ["s1"]), -1)
  @test.assert_eq(db.bitop("NOT", "dx", ["s1", "s2"]), -1)
  @test.assert_eq(db.bitop("AND", "dx", []), -1)
  db.hset("hh_missing_type", "f", "v") |> ignore
  match db.try_bitop("OR", "dx", ["hh_missing_type"]) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_bitop("OR", "dx", ["missing_key"]) {
    Ok(len) => @test.assert_eq(len, 0)
    Err(_) => @test.assert_eq(true, false)
  }

  db.set("bc", "abc")
  @test.assert_eq(db.bitcount("bc"), 10)
  @test.assert_eq(db.bitcount_range("bc", 0, 0), 3)
  @test.assert_eq(db.bitcount_range("bc", 1, 2), 7)
  @test.assert_eq(db.bitcount_range("bc", -1, -1), 4)
  @test.assert_eq(db.bitcount_range("bc", 0, 99), 10)
  @test.assert_eq(db.bitcount_range("bc", 5, 99), 0)
  match db.try_bitcount_range("missing", 0, -1) {
    Ok(count) => @test.assert_eq(count, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  db.hset("bc_hash", "f", "v") |> ignore
  match db.try_bitcount_range("bc_hash", 0, -1) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
}

test "e2_lpos_options_and_misc" {
  let db = Database::new()
  db.rpush_all("l", ["a", "b", "a", "c", "a"]) |> ignore
  match db.try_lpos_options("l", "a", 1, 0, 0) {
    Ok(indices) => @test.assert_eq(indices, [0])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_lpos_options("l", "a", 2, 0, 0) {
    Ok(indices) => @test.assert_eq(indices, [2])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_lpos_options("l", "a", 1, 1, 0) {
    Ok(indices) => @test.assert_eq(indices, [0])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_lpos_options("l", "a", -1, 0, 0) {
    Ok(indices) => @test.assert_eq(indices, [4])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_lpos_options("l", "a", -2, 0, 0) {
    Ok(indices) => @test.assert_eq(indices, [2])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_lpos_options("l", "a", 1, -1, 0) {
    Ok(indices) => @test.assert_eq(indices, [0, 2, 4])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_lpos_options("l", "a", 2, -1, 0) {
    Ok(indices) => @test.assert_eq(indices, [2, 4])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_lpos_options("l", "a", -2, -1, 0) {
    Ok(indices) => @test.assert_eq(indices, [2, 0])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_lpos_options("l", "a", 1, 0, 3) {
    Ok(indices) => @test.assert_eq(indices, [0])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_lpos_options("l", "a", 1, -1, 3) {
    Ok(indices) => @test.assert_eq(indices, [0, 2])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_lpos_options("l", "zz", 1, 0, 0) {
    Err(RedisError::NotFound) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_lpos_options("l", "a", 0, 0, 0) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.lpos_options("missing", "a", 1, 0, 0), [])

  @test.assert_eq(db.save(), "OK")
  @test.assert_eq(db.bgsave(), "Background saving started")
  @test.assert_eq(db.lastsave() >= 0, true)
  @test.assert_eq(db.command_count() > 100, true)
  db.set("r1", "v")
  db.watch("r1")
  @test.assert_eq(db.reset(), "RESET")
  db.set("r1", "changed") |> ignore
  ignore(db.multi())
  ignore(db.set("r1", "x"))
  match db.try_exec() {
    Ok(_) => @test.assert_eq(true, true)
    Err(_) => @test.assert_eq(true, false)
  }
}

test "e3_parse_float_scientific" {
  @test.assert_eq(parse_float("1e5"), Some(100000.0))
  @test.assert_eq(parse_float("1E5"), Some(100000.0))
  @test.assert_eq(parse_float("1.5e2"), Some(150.0))
  match parse_float("2e-3") {
    Some(v) => @test.assert_eq(v > 0.0019 && v < 0.0021, true)
    None => @test.assert_eq(true, false)
  }
  @test.assert_eq(parse_float("1e+2"), Some(100.0))
  @test.assert_eq(parse_float("-1e2"), Some(-100.0))
  @test.assert_eq(parse_float("1e"), None)
  @test.assert_eq(parse_float("e5"), None)
  @test.assert_eq(parse_float("1e5.5"), None)
  @test.assert_eq(parse_float("1e2e3"), None)
  @test.assert_eq(parse_float("1e999"), None)
  @test.assert_eq(parse_float("2.5"), Some(2.5))
  @test.assert_eq(parse_float("abc"), None)

  let db = Database::new()
  db.set("f1", "1e5")
  match db.try_incrbyfloat("f1", 1.0) {
    Ok(v) => @test.assert_eq(v, 100001.0)
    Err(_) => @test.assert_eq(true, false)
  }
}

test "e3_zscan_snapshot_stability" {
  let db = Database::new()
  db.zadd("zs", 1.0, "a") |> ignore
  db.zadd("zs", 2.0, "b") |> ignore
  db.zadd("zs", 3.0, "c") |> ignore
  db.zadd("zs", 4.0, "d") |> ignore

  let (cursor1, page1) = db.zscan("zs", 0, 2)
  @test.assert_eq(page1.length(), 2)
  db.zadd("zs", 5.0, "e") |> ignore
  db.zrem("zs", "a") |> ignore
  let (cursor2, page2) = db.zscan("zs", cursor1, 2)
  @test.assert_eq(page2.length(), 2)
  @test.assert_eq(cursor2, 0)
  let seen = page1
  seen.append(page2)
  @test.assert_eq(seen.length(), 4)
  let mut unique = true
  for i = 0; i < seen.length(); i = i + 1 {
    for j = i + 1; j < seen.length(); j = j + 1 {
      if seen[i].0 == seen[j].0 {
        unique = false
      }
    }
  }
  @test.assert_eq(unique, true)
}

test "e3_error_variants_and_flush" {
  let db = Database::new()
  db.rpush_all("l1", ["a", "b"]) |> ignore
  match db.try_lset("l1", 99, "x") {
    Err(RedisError::IndexOutOfRange) => @test.assert_eq(RedisError::message(RedisError::IndexOutOfRange), "ERR index out of range")
    _ => @test.assert_eq(true, false)
  }
  db.rpush_all("l2", ["a", "b", "a"]) |> ignore
  match db.try_lpos_options("l2", "a", 1, -2, 0) {
    Err(RedisError::OutOfRange) => @test.assert_eq(RedisError::message(RedisError::OutOfRange), "ERR value is out of range, must be positive")
    _ => @test.assert_eq(true, false)
  }
  @test.assert_eq(
    match db.set_with_options("sx", "v", true, true, false, None, None, None, None, false) {
      Err(RedisError::SyntaxError) => RedisError::message(RedisError::SyntaxError) == "ERR syntax error"
      _ => false
    },
    true,
  )
  db.set("fd", "v")
  @test.assert_eq(db.flushdb(), "OK")
  @test.assert_eq(db.exists("fd"), false)
  @test.assert_eq(db.flushall(), "OK")
}

test "e5_zset_boundary_gaps" {
  let db = Database::new()
  db.zadd("zb", 1.0, "a") |> ignore
  db.zadd("zb", 2.0, "b") |> ignore
  db.zadd("zb", 3.0, "c") |> ignore
  db.pexpire("zb", 1L) |> ignore
  db.advance_time(10)

  @test.assert_eq(db.zpopmin("zb"), None)
  @test.assert_eq(db.zpopmax("zb"), None)
  @test.assert_eq(db.zremrangebyrank("zb", 0, -1), 0)
  @test.assert_eq(db.zremrangebyscore("zb", 1.0, 3.0), 0)
  @test.assert_eq(db.zdiff(["zb"]).length(), 0)
  @test.assert_eq(db.zrangebyscore("zb", 1.0, 3.0).length(), 0)
  @test.assert_eq(db.zrevrangebyscore("zb", 3.0, 1.0).length(), 0)
  @test.assert_eq(db.zrank("zb", "a"), None)
  @test.assert_eq(db.zrevrank("zb", "a"), None)
  @test.assert_eq(db.zincrby("zb", 1.0, "a"), 1.0)

  db.set("str2", "x")
  @test.assert_eq(db.zpopmin("str2"), None)
  @test.assert_eq(db.zpopmax("str2"), None)
  @test.assert_eq(db.zremrangebyrank("str2", 0, -1), 0)
  @test.assert_eq(db.zremrangebyscore("str2", 1.0, 2.0), 0)

  db.zadd("live", 5.0, "m1") |> ignore
  db.zadd("live", 6.0, "m2") |> ignore
  @test.assert_eq(
    match db.zpopmax("live") {
      Some(item) => item.0 == "m2"
      None => false
    },
    true,
  )
  @test.assert_eq(db.zpopmin("live"), Some(("m1", 5.0)))
  @test.assert_eq(db.exists("live"), false)
  @test.assert_eq(db.zdiff(["nope1", "nope2"]).length(), 0)
}

test "e6_sort_command" {
  let db = Database::new()
  db.rpush_all("nums", ["3", "1", "2"]) |> ignore

  match db.try_sort("nums", false, false, 0, 0, None) {
    Ok(items) => @test.assert_eq(items, ["1", "2", "3"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort("nums", false, true, 0, 0, None) {
    Ok(items) => @test.assert_eq(items, ["3", "2", "1"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort("nums", false, false, 1, 2, None) {
    Ok(items) => @test.assert_eq(items, ["2", "3"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort("nums", false, false, 0, 1, None) {
    Ok(items) => @test.assert_eq(items, ["1"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort("nums", false, false, 0, -1, None) {
    Ok(items) => @test.assert_eq(items, ["1", "2", "3"])
    Err(_) => @test.assert_eq(true, false)
  }

  db.rpush_all("strs", ["10", "9", "2"]) |> ignore
  match db.try_sort("strs", false, false, 0, 0, None) {
    Ok(items) => @test.assert_eq(items, ["2", "9", "10"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort("strs", true, false, 0, 0, None) {
    Ok(items) => @test.assert_eq(items, ["10", "2", "9"])
    Err(_) => @test.assert_eq(true, false)
  }

  db.sadd_all("st", ["b", "a", "c"]) |> ignore
  match db.try_sort("st", true, false, 0, 0, None) {
    Ok(items) => @test.assert_eq(items, ["a", "b", "c"])
    Err(_) => @test.assert_eq(true, false)
  }
  db.zadd("zs", 1.0, "z2") |> ignore
  db.zadd("zs", 2.0, "z1") |> ignore
  match db.try_sort("zs", true, false, 0, 0, None) {
    Ok(items) => @test.assert_eq(items, ["z1", "z2"])
    Err(_) => @test.assert_eq(true, false)
  }

  match db.try_sort("nums", false, false, 0, 0, Some("dest1")) {
    Ok(items) => @test.assert_eq(items, ["1", "2", "3"])
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.lrange("dest1", 0, -1), ["1", "2", "3"])
  @test.assert_eq(db.type_of("dest1"), "list")

  match db.try_sort("nums", false, false, 0, 0, Some("dest1")) {
    Ok(items) => @test.assert_eq(items, ["1", "2", "3"])
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.llen("dest1"), 3)

  db.rpush_all("bad", ["x", "1"]) |> ignore
  match db.try_sort("bad", false, false, 0, 0, None) {
    Err(RedisError::NotInteger) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  db.hset("hh", "f", "v") |> ignore
  match db.try_sort("hh", false, false, 0, 0, None) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_sort("missing", false, false, 0, 0, None) {
    Ok(items) => @test.assert_eq(items.length(), 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort("missing", false, false, 0, 0, Some("dest2")) {
    Ok(items) => @test.assert_eq(items.length(), 0)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.exists("dest2"), false)
  match db.try_sort("nums", false, false, -1, 1, None) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.sort("nums", false, true, 0, 0, None), ["3", "2", "1"])
}

test "f1_watch_dirty_paths" {
  let db = Database::new()

  db.zadd("wz", 1.0, "a") |> ignore
  db.watch("wz")
  db.zadd("wz", 2.0, "b") |> ignore
  ignore(db.multi())
  ignore(db.set("wz", "x"))
  match db.try_exec() {
    Err(RedisError::TransactionAborted) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.watch("wz")
  db.zincrby("wz", 1.0, "a") |> ignore
  ignore(db.multi())
  ignore(db.set("wz", "x"))
  match db.try_exec() {
    Err(RedisError::TransactionAborted) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.set("wr", "old")
  db.watch("wr")
  db.rename("wr", "wr2") |> ignore
  ignore(db.multi())
  ignore(db.set("wr2", "x"))
  match db.try_exec() {
    Err(RedisError::TransactionAborted) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.set("wf", "v")
  db.watch("wf")
  ignore(db.flushdb())
  ignore(db.multi())
  ignore(db.set("wf", "x"))
  match db.try_exec() {
    Err(RedisError::TransactionAborted) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.set("wt", "v")
  db.watch("wt")
  db.expire("wt", 100) |> ignore
  ignore(db.multi())
  ignore(db.set("wt", "x"))
  match db.try_exec() {
    Err(RedisError::TransactionAborted) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.set("wp", "v")
  db.pexpire("wp", 5000L) |> ignore
  db.watch("wp")
  ignore(db.getex("wp", None))
  ignore(db.multi())
  ignore(db.set("wp", "x"))
  match db.try_exec() {
    Err(RedisError::TransactionAborted) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.set("ok1", "v")
  db.watch("ok1")
  db.set("other", "v") |> ignore
  ignore(db.multi())
  ignore(db.set("ok1", "x"))
  match db.try_exec() {
    Ok(_) => @test.assert_eq(true, true)
    Err(_) => @test.assert_eq(true, false)
  }
}


test "g1_zrange_modifiers" {
  let db = Database::new()
  db.set("str", "x")
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 3.0, "c") |> ignore

  match db.try_zrange_modifiers("z", "0", "-1", None, false, false, 0, 0) {
    Ok(items) => @test.assert_eq(items, ["a", "b", "c"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrange_modifiers("z", "0", "-1", None, true, false, 0, 0) {
    Ok(items) => @test.assert_eq(items, ["c", "b", "a"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrange_modifiers("z", "0", "-1", None, false, true, 0, 0) {
    Ok(items) => @test.assert_eq(items, ["a", "1", "b", "2", "c", "3"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrange_modifiers("z", "(1", "(3", Some("byscore"), false, false, 0, 0) {
    Ok(items) => @test.assert_eq(items, ["b"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrange_modifiers("z", "(1", "+inf", Some("byscore"), true, false, 0, 0) {
    Ok(items) => @test.assert_eq(items, ["c", "b"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrange_modifiers("z", "-inf", "+inf", Some("byscore"), false, true, 1, 1) {
    Ok(items) => @test.assert_eq(items, ["b", "2"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrange_modifiers("z", "-", "+", Some("bylex"), false, false, 1, 1) {
    Ok(items) => @test.assert_eq(items, ["b"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrange_modifiers("z", "-", "+", Some("bylex"), true, true, 0, 0) {
    Ok(items) => @test.assert_eq(items, ["c", "3", "b", "2", "a", "1"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrange_modifiers("z", "0", "-1", Some("bad"), false, false, 0, 0) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zrange_modifiers("str", "0", "-1", None, false, false, 0, 0) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zrange_modifiers("missing", "0", "-1", None, false, false, 0, 0) {
    Ok(items) => @test.assert_eq(items.length(), 0)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.zrange_modifiers("z", "0", "-1", None, false, false, 0, 0), ["a", "b", "c"])
}

test "g1_multi_key_and_memory_usage" {
  let db = Database::new()
  db.set("k1", "v")
  db.rpush_all("k2", ["a"]) |> ignore
  @test.assert_eq(db.exists_many(["k1", "k2", "k3"]), 2)
  @test.assert_eq(db.exists_many([]), 0)
  @test.assert_eq(db.unlink_many(["k1", "k2", "k3"]), 2)
  @test.assert_eq(db.exists_many(["k1", "k2"]), 0)

  db.set("mu", "hello")
  match db.memory_usage("mu") {
    Some(bytes) => @test.assert_eq(bytes, 2 * 2 + 5 * 2)
    None => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.memory_usage("missing"), None)
  db.hset("muh", "f", "vv") |> ignore
  match db.memory_usage("muh") {
    Some(bytes) => @test.assert_eq(bytes, 3 * 2 + (1 + 2) * 2)
    None => @test.assert_eq(true, false)
  }
}

test "g1_pop_count_negative" {
  let db = Database::new()
  db.rpush_all("l", ["a", "b"]) |> ignore
  match db.try_lpop_count("l", -1) {
    Err(RedisError::OutOfRange) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_rpop_count("l", -1) {
    Err(RedisError::OutOfRange) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_lpop_count("l", 1) {
    Ok(vals) => @test.assert_eq(vals, ["a"])
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.lpop_count("l", -1), [])
}


test "g2_bitfield_overflow_modes" {
  let db = Database::new()
  db.set("bo", "")
  ignore(db.try_bitfield_set("bo", 0, 8, false, 250))

  match db.try_bitfield_incrby_mode("bo", 0, 8, false, 10, Some("WRAP")) {
    Ok(v) => @test.assert_eq(v, Some(4))
    Err(_) => @test.assert_eq(true, false)
  }
  ignore(db.try_bitfield_set("bo", 0, 8, false, 250))
  match db.try_bitfield_incrby_mode("bo", 0, 8, false, 10, Some("SAT")) {
    Ok(v) => @test.assert_eq(v, Some(255))
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_incrby_mode("bo", 0, 8, false, 1, Some("FAIL")) {
    Ok(v) => @test.assert_eq(v, None)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_get("bo", 0, 8, false) {
    Ok(v) => @test.assert_eq(v, 255)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_incrby_mode("bo", 0, 8, false, -300, Some("SAT")) {
    Ok(v) => @test.assert_eq(v, Some(0))
    Err(_) => @test.assert_eq(true, false)
  }
  ignore(db.try_bitfield_set("bo", 8, 8, true, 120))
  match db.try_bitfield_incrby_mode("bo", 8, 8, true, 10, Some("SAT")) {
    Ok(v) => @test.assert_eq(v, Some(127))
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_incrby_mode("bo", 8, 8, true, 1, Some("FAIL")) {
    Ok(v) => @test.assert_eq(v, None)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_incrby_mode("bo", 8, 8, true, -200, Some("SAT")) {
    Ok(v) => @test.assert_eq(v, Some(-73))
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_incrby_mode("bo", 0, 8, false, 1, Some("BAD")) {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_bitfield_incrby_mode("bo", 0, 8, false, 1, None) {
    Ok(v) => @test.assert_eq(v, Some(1))
    Err(_) => @test.assert_eq(true, false)
  }
}


test "h1_restore_options" {
  let db = Database::new()
  db.set("r1", "old")
  match db.try_restore("r1", 1000L, "data", false, false) {
    Err(RedisError::BusyKey) => @test.assert_eq(RedisError::message(RedisError::BusyKey), "BUSYKEY Target key name already exists.")
    _ => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.get("r1"), Some("old"))
  match db.try_restore("r1", 1000L, "data", true, false) {
    Ok(v) => @test.assert_eq(v, true)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.get("r1"), Some("data"))
  @test.assert_eq(db.pttl("r1") > 0, true)
  match db.try_restore("r2", 0L, "no-ttl", true, false) {
    Ok(_) => @test.assert_eq(true, true)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.ttl("r2"), -1)
  match db.try_restore("r3", 5000L, "abs", false, true) {
    Ok(_) => @test.assert_eq(true, true)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.pttl("r3") > 0, true)
  match db.try_restore("r4", 0L, "past-abs", false, true) {
    Ok(_) => @test.assert_eq(true, true)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.exists("r4"), true)
  @test.assert_eq(db.ttl("r4"), -1)
  @test.assert_eq(db.restore("brand-new", 1000L, "v"), true)
  @test.assert_eq(db.get("brand-new"), Some("v"))
}


test "h2_placeholder_commands" {
  let db = Database::new()
  @test.assert_eq(db.wait(1, 100), 0)
  @test.assert_eq(db.shutdown(), "OK")
  ignore(db.config_set("maxmemory", "100mb"))
  @test.assert_eq(db.config_get("maxmemory"), ["maxmemory", "100mb"])
  @test.assert_eq(db.config_resetstat(), "OK")
  @test.assert_eq(db.config_get("maxmemory"), ["maxmemory", "0"])
  let lwut = db.lolwut()
  @test.assert_eq(lwut.contains("moonbitDB ver. 0.1.4"), true)
}


test "h3_combination_fixes" {
  let db = Database::new()
  db.hset("hh", "f", "v") |> ignore
  ignore(db.multi())
  ignore(db.getset("hh", "overwrite"))
  ignore(db.exec())
  @test.assert_eq(db.hget("hh", "f"), Some("v"))

  db.hset("hh2", "f", "v") |> ignore
  ignore(db.multi())
  ignore(db.incr("hh2"))
  ignore(db.exec())
  @test.assert_eq(db.hget("hh2", "f"), Some("v"))

  db.zadd("zs1", 1.0, "a") |> ignore
  match db.try_zrangestore("dst1", "zs1", 5, 10) {
    Ok(count) => @test.assert_eq(count, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.exists("dst1"), false)
  match db.try_zrangestore("dst1", "missing", 0, -1) {
    Ok(count) => @test.assert_eq(count, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.exists("dst1"), false)
  db.set("dst1", "string-value")
  match db.try_zrangestore_byscore("dst1", "zs1", "-inf", "+inf", 0, 0) {
    Ok(count) => @test.assert_eq(count, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.type_of("dst1"), "zset")

  match db.try_incrbyfloat("fresh", 1.5) {
    Ok(v) => @test.assert_eq(v, 1.5)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.get("fresh"), Some("1.5"))
}


test "h4_sort_by_get" {
  let db = Database::new()
  db.hset("w_a", "wt", "3") |> ignore
  db.hset("w_a", "name", "Alice") |> ignore
  db.hset("w_b", "wt", "1") |> ignore
  db.hset("w_b", "name", "Bob") |> ignore
  db.hset("w_c", "name", "Cara") |> ignore
  db.rpush_all("players", ["a", "b", "c"]) |> ignore

  match db.try_sort_advanced("players", Some("w_*->wt"), [], false, false, 0, 0, None) {
    Ok(items) => @test.assert_eq(items, ["c", "b", "a"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort_advanced("players", Some("w_*->wt"), [], false, true, 0, 0, None) {
    Ok(items) => @test.assert_eq(items, ["a", "b", "c"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort_advanced("players", Some("w_*->wt"), ["w_*->name", "#"], false, false, 0, 0, None) {
    Ok(items) => @test.assert_eq(items, ["Cara", "c", "Bob", "b", "Alice", "a"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort_advanced("players", None, ["#"], true, false, 0, 0, None) {
    Ok(items) => @test.assert_eq(items, ["a", "b", "c"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort_advanced("players", Some("nosort"), ["#"], false, false, 0, 0, None) {
    Ok(items) => @test.assert_eq(items, ["a", "b", "c"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort_advanced("players", Some("w_*->wt"), ["w_*->name"], false, false, 1, 1, None) {
    Ok(items) => @test.assert_eq(items, ["Bob"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort_advanced("players", Some("w_*->wt"), ["w_*->name"], false, false, 0, 0, Some("sorted-out")) {
    Ok(items) => @test.assert_eq(items, ["Cara", "Bob", "Alice"])
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.lrange("sorted-out", 0, -1), ["Cara", "Bob", "Alice"])
  match db.try_sort_advanced("players", Some("w_*->wt"), [], false, false, 0, 0, None) {
    Ok(items) => @test.assert_eq(items, ["c", "b", "a"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort_advanced("players", Some("bad_pattern"), [], false, false, 0, 0, None) {
    Ok(items) => @test.assert_eq(items, ["a", "b", "c"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sort_advanced("missing", None, [], false, false, 0, 0, None) {
    Ok(items) => @test.assert_eq(items.length(), 0)
    Err(_) => @test.assert_eq(true, false)
  }
}


test "k1_glob_classes_and_escape" {
  @test.assert_eq(match_pattern("a*c", "abc"), true)
  @test.assert_eq(match_pattern("a?c", "abc"), true)
  @test.assert_eq(match_pattern("a?c", "ab"), false)
  @test.assert_eq(match_pattern("[abc]", "b"), true)
  @test.assert_eq(match_pattern("[abc]", "d"), false)
  @test.assert_eq(match_pattern("[a-z]*", "hello"), true)
  @test.assert_eq(match_pattern("[a-z]*", "Hello"), false)
  @test.assert_eq(match_pattern("[^a-z]*", "Hello"), true)
  @test.assert_eq(match_pattern("[^a-z]*", "hello"), false)
  @test.assert_eq(match_pattern("\\*", "*"), true)
  @test.assert_eq(match_pattern("\\*", "a"), false)
  @test.assert_eq(match_pattern("\\*b", "*b"), true)
  @test.assert_eq(match_pattern("h?llo", "hello"), true)
  @test.assert_eq(match_pattern("h\\[a]llo", "h[a]llo"), true)
  @test.assert_eq(match_pattern("h[a]llo", "h[a]llo"), false)
  @test.assert_eq(match_pattern("h[a]llo", "hallo"), true)
  @test.assert_eq(match_pattern("*", "anything"), true)
  @test.assert_eq(match_pattern("[", "["), true)
  @test.assert_eq(match_pattern("", ""), true)
  @test.assert_eq(match_pattern("*", ""), true)

  let db = Database::new()
  ignore(db.sadd_all("g1", ["one", "two", "three"]) )
  @test.assert_eq(db.sscan_with_match("g1", 0, 10, Some("t?o")).1.length(), 1)
  @test.assert_eq(db.sscan_with_match("g1", 0, 10, Some("t*")).1.length(), 2)
  @test.assert_eq(db.sscan_with_match("g1", 0, 10, Some("[ot]*")).1.length(), 3)
  db.hmset("g2", ["one", "two"], ["1", "2"]) |> ignore
  @test.assert_eq(db.hscan_with_match("g2", 0, 10, Some("t?o")).1.length(), 1)
  db.zadd("g3", 1.0, "one") |> ignore
  db.zadd("g3", 2.0, "two") |> ignore
  @test.assert_eq(db.zscan_with_match("g3", 0, 10, Some("t*o")).1.length(), 1)
}


test "k4_try_variants" {
  let db = Database::new()
  db.hset("hh", "f", "v") |> ignore

  match db.try_setnx("hh", "x") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_setnx("sx", "v") {
    Ok(set) => @test.assert_eq(set, true)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_setex("hh", 10, "x") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_setex("se", 0, "x") {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_psetex("pe", -1L, "x") {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.rpush_all("l1", ["a", "b", "a"]) |> ignore
  match db.try_lrem("hh", 0, "x") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_lrem("l1", 1, "a") {
    Ok(n) => @test.assert_eq(n, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_ltrim("hh", 0, -1) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_ltrim("l1", 0, 0) {
    Ok(_) => @test.assert_eq(true, true)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_linsert("l1", true, "pivot_missing", "v") {
    Ok(n) => @test.assert_eq(n, -1)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_linsert("hh", true, "f", "v") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.sadd_all("s1", ["a"]) |> ignore
  match db.try_smove("hh", "s1", "x") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_smove("s1", "s2", "a") {
    Ok(moved) => @test.assert_eq(moved, true)
    Err(_) => @test.assert_eq(true, false)
  }

  db.zadd("z1", 1.0, "a") |> ignore
  match db.try_zremrangebyrank("hh", 0, -1) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zremrangebyrank("z1", 0, -1) {
    Ok(n) => @test.assert_eq(n, 1)
    Err(_) => @test.assert_eq(true, false)
  }

  match db.try_rename("missing", "r2") {
    Err(RedisError::NotFound) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  db.set("rn1", "v")
  match db.try_rename("rn1", "rn2") {
    Ok(v) => @test.assert_eq(v, true)
    Err(_) => @test.assert_eq(true, false)
  }
}
