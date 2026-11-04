test "coverage_server_admin" {
  let db = Database::new()
  @test.assert_eq(db.auth("secret"), "OK")
  @test.assert_eq(db.client_list("LIST").has_prefix("id=1"), true)
  @test.assert_eq(db.client_list("ID"), "1")
  @test.assert_eq(db.config_get("maxmemory"), ["maxmemory", "0"])
  @test.assert_eq(db.config_get("databases"), ["databases", "16"])
  @test.assert_eq(db.slowlog("LEN"), ["0"])
  @test.assert_eq(db.slowlog("RESET"), ["OK"])
  @test.assert_eq(db.slowlog("GET").length(), 0)
  @test.assert_eq(db.select(2), "OK")
  @test.assert_eq(db.monitor(), "OK")
  @test.assert_eq(Database::ping(), "PONG")

  @test.assert_eq(db.object_refcount("missing"), None)
  db.set("s1", "v")
  @test.assert_eq(db.object_refcount("s1"), Some(1))
  db.hset("h1", "f", "v") |> ignore
  @test.assert_eq(db.object_refcount("h1"), Some(1))
  db.rpush("l1", "x") |> ignore
  @test.assert_eq(db.object_refcount("l1"), Some(1))
  db.sadd("se1", "m") |> ignore
  @test.assert_eq(db.object_refcount("se1"), Some(1))
  db.zadd("z1", 1.0, "m") |> ignore
  @test.assert_eq(db.object_refcount("z1"), Some(1))

  db.set("gone", "v")
  db.pexpire("gone", 1L) |> ignore
  db.advance_time(50)
  @test.assert_eq(db.object_refcount("gone"), None)
  @test.assert_eq(db.object_encoding("gone"), None)
  @test.assert_eq(db.object_idletime("gone"), Some(0))

  db.set("idle", "v")
  @test.assert_eq(db.object_idletime("idle"), Some(0))
  db.pexpire("idle", 5000L) |> ignore
  db.advance_time(2000)
  @test.assert_eq(db.object_idletime("idle"), Some(3))

  @test.assert_eq(db.object_freq("idle"), Some(0))
  @test.assert_eq(db.object_freq("missing"), None)

  @test.assert_eq(db.dump("idle"), Some("v"))
  @test.assert_eq(db.dump("z1"), None)
  @test.assert_eq(db.dump("missing"), None)
  @test.assert_eq(db.restore("r1", 1000L, "data"), true)
  @test.assert_eq(db.pttl("r1") > 0, true)
  @test.assert_eq(db.restore("r2", 0L, "data"), true)
  @test.assert_eq(db.pttl("r2"), -1)

  @test.assert_eq(db.move_key("idle", 0), true)
  @test.assert_eq(db.move_key("missing", 0), false)
  @test.assert_eq(db.move_key("idle", 5), false)

  db.set("bits", "abc")
  @test.assert_eq(db.bitcount("bits"), 10)
  @test.assert_eq(db.bitcount("missing"), 0)

  @test.assert_eq(db.command_info(["SET", "NOPE"]), ["SET", ""])
}

test "coverage_hash_gaps" {
  let db = Database::new()
  db.set("str", "x")

  match db.try_hincrbyfloat("str", "f", 1.0) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_hincrbyfloat("h1", "f", 1.5) {
    Ok(v) => @test.assert_eq(v, 1.5)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.hget("h1", "f"), Some("1.5"))
  db.hset("h1", "bad", "abc") |> ignore
  match db.try_hincrbyfloat("h1", "bad", 1.0) {
    Err(RedisError::NotInteger) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  let zero : Float = 0.0
  db.hset("h1", "nan", "1.0") |> ignore
  match db.try_hincrbyfloat("h1", "nan", zero / zero) {
    Err(RedisError::Overflow) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  match db.try_hmset("bad", ["a"], []) {
    Err(RedisError::WrongArgumentCount(_)) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_hmset("str", ["a"], ["b"]) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_hget("str", "f") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_hset("str", "f", "v") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.hmset("scan1", ["one", "two", "three"], ["1", "2", "3"]) |> ignore
  @test.assert_eq(db.hscan_with_match("scan1", 0, 10, Some("*e")).1.length(), 2)
  @test.assert_eq(db.hscan_with_match("scan1", 0, 10, Some("t*")).1.length(), 2)
  @test.assert_eq(db.hscan_with_match("scan1", 0, 10, Some("one")).1.length(), 1)
  @test.assert_eq(db.hscan_with_match("scan1", 0, 10, Some("*")).1.length(), 3)
  @test.assert_eq(db.hscan_with_match("scan1", 0, 10, None).1.length(), 3)
  @test.assert_eq(db.hscan_with_match("scan1", 0, 10, Some("xyz*")).1.length(), 0)
}

test "coverage_list_gaps" {
  let db = Database::new()
  db.set("str", "x")

  match db.try_lpush("str", "v") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_lpop("str") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_rpop("str") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.lpushx("missing", "v"), 0)
  @test.assert_eq(db.rpushx("missing", "v"), 0)
  @test.assert_eq(db.lpushx("str", "v"), -1)
  @test.assert_eq(db.rpushx("str", "v"), -1)

  db.rpush_all("l1", ["a", "b", "c"]) |> ignore
  @test.assert_eq(db.lindex("l1", -1), Some("c"))
  @test.assert_eq(db.lindex("l1", 99), None)
  @test.assert_eq(db.lset("l1", 99, "x"), false)
  @test.assert_eq(db.linsert("l1", true, "pivot_missing", "v"), -1)

  db.rpush_all("m1", ["a", "b", "c"]) |> ignore
  db.rpush_all("m2", ["x", "y"]) |> ignore
  @test.assert_eq(
    match db.lmpop(["missing", "m1"], true, 2) {
      Some((k, vals)) => k == "m1" && vals == ["a", "b"]
      None => false
    },
    true,
  )
  @test.assert_eq(
    match db.lmpop(["m2"], false, 5) {
      Some((k, vals)) => k == "m2" && vals == ["y", "x"]
      None => false
    },
    true,
  )
  @test.assert_eq(db.exists("m2"), false)
  @test.assert_eq(db.lmpop(["m1"], true, 0), None)
  @test.assert_eq(db.lmpop([], true, 1), None)
  @test.assert_eq(db.lmpop(["missing"], true, 1), None)
  @test.assert_eq(
    match db.lmpop(["str", "m1"], true, 1) {
      Some((k, vals)) => k == "m1" && vals == ["c"]
      None => false
    },
    true,
  )

  db.rpush_all("p1", ["1", "2", "3", "4"]) |> ignore
  @test.assert_eq(db.lpop_count("p1", 2), ["1", "2"])
  @test.assert_eq(db.rpop_count("p1", 5), ["4", "3"])
  @test.assert_eq(db.lpop_count("p1", 1), [])
  @test.assert_eq(db.exists("p1"), false)
  @test.assert_eq(db.lpop_count("missing", 1), [])
  @test.assert_eq(db.rpop_count("missing", 1), [])
  @test.assert_eq(db.lpop_count("str", 1), [])
  @test.assert_eq(db.rpop_count("str", 1), [])

  db.rpush_all("s1", ["a", "b"]) |> ignore
  db.rpush_all("d1", ["c"]) |> ignore
  @test.assert_eq(db.rpoplpush("s1", "d1"), Some("b"))
  @test.assert_eq(db.lrange("d1", 0, -1), ["b", "c"])
  @test.assert_eq(db.rpoplpush("s1", "d1"), Some("a"))
  @test.assert_eq(db.rpoplpush("s1", "d1"), None)
}

test "coverage_zset_mpop_random" {
  let db = Database::new()
  db.zadd("z1", 1.0, "a") |> ignore
  db.zadd("z1", 2.0, "b") |> ignore
  db.zadd("z1", 3.0, "c") |> ignore

  @test.assert_eq(
    match db.zmpop(["missing", "z1"], true, 2) {
      Some((k, vals)) => k == "z1" && vals.length() == 2 && vals[0].0 == "a"
      None => false
    },
    true,
  )
  @test.assert_eq(db.zcard("z1"), 1)

  db.zadd("z2", 1.0, "x") |> ignore
  db.zadd("z2", 2.0, "y") |> ignore
  @test.assert_eq(
    match db.zmpop(["z2"], false, 1) {
      Some((k, vals)) => k == "z2" && vals.length() == 1 && vals[0].0 == "y"
      None => false
    },
    true,
  )
  @test.assert_eq(db.exists("z2"), true)

  @test.assert_eq(db.zmpop([], true, 1), None)
  @test.assert_eq(db.zmpop(["z1"], true, 0), None)
  @test.assert_eq(db.zmpop(["missing"], true, 1), None)

  db.set("str", "x")
  @test.assert_eq(
    match db.zmpop(["str", "z1"], true, 1) {
      Some((k, vals)) => k == "z1" && vals.length() == 1
      None => false
    },
    true,
  )

  db.zadd("z3", 1.0, "a") |> ignore
  db.zadd("z3", 2.0, "b") |> ignore
  @test.assert_eq(db.zrandmember_with_scores("z3", 2).length(), 2)
  @test.assert_eq(db.zrandmember_with_scores("z3", -5).length(), 5)
  @test.assert_eq(db.zrandmember_with_scores("missing", 1).length(), 0)
  @test.assert_eq(db.zrandmember_count("z3", 2).length(), 2)
  @test.assert_eq(db.zrandmember_count("z3", -5).length(), 5)
  @test.assert_eq(db.zrandmember_count("missing", 1).length(), 0)

  db.zadd("z4", 1.0, "a") |> ignore
  db.zadd("z4", 2.0, "b") |> ignore
  db.zadd("z4", 3.0, "c") |> ignore
  @test.assert_eq(db.zpopmin_count("z4", 2).length(), 2)
  @test.assert_eq(db.zpopmin_count("z4", 0).length(), 0)
  @test.assert_eq(db.zpopmin_count("missing", 2).length(), 0)
}

test "coverage_zset_try_range" {
  let db = Database::new()
  db.set("str", "x")
  db.zadd("z1", 1.0, "a") |> ignore
  db.zadd("z1", 2.0, "b") |> ignore

  match db.try_zrange("z1", 0, -1, true) {
    Ok(items) => @test.assert_eq(items, ["a", "1", "b", "2"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrange("z1", 0, 0, false) {
    Ok(items) => @test.assert_eq(items, ["a"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrange("str", 0, -1, false) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zrange("missing", 0, -1, false) {
    Ok(items) => @test.assert_eq(items.length(), 0)
    Err(_) => @test.assert_eq(true, false)
  }

  match db.try_zscore("str", "f") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zscore("missing", "f") {
    Err(RedisError::NotFound) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zscore("z1", "nope") {
    Err(RedisError::NotFound) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  let zero : Float = 0.0
  match db.try_zadd("zz", zero / zero, "m") {
    Err(RedisError::InvalidArgument) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  db.zadd("zl", 0.0, "a") |> ignore
  db.zadd("zl", 0.0, "b") |> ignore
  db.zadd("zl", 0.0, "c") |> ignore
  db.zadd("zl", 0.0, "d") |> ignore
  match db.try_zrangebylex_with_limit("zl", "-", "+", 1, 2) {
    Ok(items) => @test.assert_eq(items, ["b", "c"])
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangebylex_with_limit("zl", "-", "+", 0, 0) {
    Ok(items) => @test.assert_eq(items.length(), 4)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangebylex_with_limit("zl", "-", "+", -1, 2) {
    Ok(items) => @test.assert_eq(items.length(), 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangebylex_with_limit("zl", "-", "+", 10, 2) {
    Ok(items) => @test.assert_eq(items.length(), 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangebylex_with_limit("str", "-", "+", 0, 1) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_zrangebylex_with_limit("zl", "bad", "+", 0, 1) {
    Ok(items) => @test.assert_eq(items.length(), 0)
    Err(_) => @test.assert_eq(true, false)
  }
}

test "coverage_merge_sort_big_zset" {
  let db = Database::new()
  for i = 0; i < 20; i = i + 1 {
    db.zadd("big", Float::from_int(i), "m\{i}") |> ignore
  }
  let u = db.zunion(["big"])
  @test.assert_eq(u.length(), 20)
  @test.assert_eq(u[0].0, "m0")
  @test.assert_eq(u[19].0, "m19")
}

test "coverage_set_gaps" {
  let db = Database::new()
  db.set("str", "x")

  match db.try_sadd("str", "v") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.sadd("str", "v"), false)
  @test.assert_eq(db.sadd_all("str", ["v"]), -1)
  @test.assert_eq(db.srem_all("str", ["v"]), -1)
  @test.assert_eq(db.srem("str", "v"), false)
  match db.try_smembers("str") {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  @test.assert_eq(db.sadd_all("new1", ["a", "b"]), 2)
  @test.assert_eq(db.smismember("new1", ["a", "zz"]), [true, false])
  @test.assert_eq(db.smismember("missing", ["a"]), [false])
  @test.assert_eq(db.smismember("str", ["a"]), [false])
  match db.try_smismember("str", ["a"]) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }

  @test.assert_eq(db.smove("missing", "d1", "v"), false)
  @test.assert_eq(db.smove("str", "d1", "v"), false)
  @test.assert_eq(db.spop("missing"), None)
  @test.assert_eq(db.srandmember("missing"), None)
  @test.assert_eq(db.srandmember_count("missing", 2).length(), 0)

  db.sadd_all("ss", ["one", "two", "three"]) |> ignore
  @test.assert_eq(db.sscan_with_match("ss", 0, 10, Some("t*")).1.length(), 2)
  @test.assert_eq(db.sscan_with_match("ss", 0, 10, None).1.length(), 3)

  db.sadd_all("a1", ["x", "y"]) |> ignore
  db.sadd_all("b1", ["y", "z"]) |> ignore
  @test.assert_eq(db.sinter(["a1", "b1"]), ["y"])
  @test.assert_eq(db.sunion(["a1", "missing"]).length(), 2)
  @test.assert_eq(db.sdiff(["a1", "b1"]), ["x"])
  @test.assert_eq(db.sinterstore("dst1", ["a1", "missing"]), 0)
  @test.assert_eq(db.sunionstore("dst2", ["a1", "b1"]), 3)
  @test.assert_eq(db.sdiffstore("dst3", ["a1", "b1"]), 1)
  @test.assert_eq(db.sintercard(["a1", "missing"], 0), 0)
  match db.try_sinterstore("str", ["a1"]) {
    Ok(count) => @test.assert_eq(count, 2)
    Err(_) => @test.assert_eq(true, false)
  }
  db.set("str2", "x")
  match db.try_sunion(["str2"]) {
    Err(RedisError::WrongType) => @test.assert_eq(true, true)
    _ => @test.assert_eq(true, false)
  }
  match db.try_sdiffstore("str2", ["a1"]) {
    Ok(count) => @test.assert_eq(count, 2)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_sintercard(["str2"], 0) {
    Ok(count) => @test.assert_eq(count, 2)
    Err(_) => @test.assert_eq(true, false)
  }
}

test "coverage_key_gaps" {
  let db = Database::new()
  db.set("k1", "v")
  @test.assert_eq(db.expire("k1", 0), true)
  @test.assert_eq(db.exists("k1"), false)
  db.set("k2", "v")
  @test.assert_eq(db.pexpire("k2", 0L), true)
  @test.assert_eq(db.exists("k2"), false)
  db.set("k3", "v")
  @test.assert_eq(db.expireat("k3", 0L), true)
  @test.assert_eq(db.exists("k3"), false)
  db.set("k4", "v")
  @test.assert_eq(db.pexpireat("k4", 0L), true)
  @test.assert_eq(db.exists("k4"), false)
  db.set("k5", "v")
  @test.assert_eq(db.expireat("k5", 9223372036854775807L), false)

  db.set("k6", "v")
  @test.assert_eq(db.expire_with_options("k6", 100, false, false, false, false), true)
  @test.assert_eq(db.expire_with_options("k6", 200, false, true, false, false), true)
  @test.assert_eq(db.expire_with_options("k6", 300, false, false, true, false), true)
  @test.assert_eq(db.expire_with_options("k6", 50, false, false, false, true), true)
  @test.assert_eq(db.expire_with_options("missing", 100, true, false, false, false), false)

  db.hset("kh", "f", "v") |> ignore
  db.rpush("kl", "x") |> ignore
  db.sadd("ks", "m") |> ignore
  db.zadd("kz", 1.0, "m") |> ignore
  @test.assert_eq(db.copy("kh", "c1"), true)
  @test.assert_eq(db.copy("kl", "c2"), true)
  @test.assert_eq(db.copy("ks", "c3"), true)
  @test.assert_eq(db.copy("kz", "c4"), true)
  @test.assert_eq(db.hget("c1", "f"), Some("v"))
  @test.assert_eq(db.lrange("c2", 0, -1), ["x"])
  @test.assert_eq(db.scard("c3"), 1)
  @test.assert_eq(db.zcard("c4"), 1)

  @test.assert_eq(db.scan(9999, 10), (0, []))
  @test.assert_eq(db.keys_pattern("*").length(), db.keys().length())
}

test "coverage_string_exat_pxat" {
  let db = Database::new()

  @test.assert_eq(
    match db.set_with_options("e1", "v", false, false, false, None, None, Some(1000), None, false) {
      Ok(_) => true
      Err(_) => false
    },
    true,
  )
  @test.assert_eq(db.ttl("e1"), 1)
  @test.assert_eq(
    match db.set_with_options("e2", "v", false, false, false, None, None, Some(0), None, false) {
      Ok(_) => true
      Err(_) => false
    },
    true,
  )
  @test.assert_eq(
    match db.set_with_options("p1", "v", false, false, false, None, None, None, Some(2000), false) {
      Ok(_) => true
      Err(_) => false
    },
    true,
  )
  @test.assert_eq(db.pttl("p1"), 2000)
  @test.assert_eq(
    match db.set_with_options("p2", "v", false, false, false, None, None, None, Some(0), false) {
      Ok(_) => true
      Err(_) => false
    },
    true,
  )
  @test.assert_eq(db.exists("p2"), false)
  @test.assert_eq(
    match db.set_with_options("x1", "v", false, false, false, Some(10), None, Some(1000), None, false) {
      Err(RedisError::SyntaxError) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.set_with_options("x2", "v", false, false, false, None, Some(10L), Some(1000), None, false) {
      Err(RedisError::SyntaxError) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.set_with_options("x3", "v", false, false, false, None, None, Some(1000), Some(2000), false) {
      Err(RedisError::SyntaxError) => true
      _ => false
    },
    true,
  )
}

test "coverage_util_gaps" {
  @test.assert_eq(int_to_string(-123), "-123")
  @test.assert_eq(int_to_string(-9), "-9")
  @test.assert_eq(int_to_string(-101), "-101")
  @test.assert_eq(int_to_string(0), "0")
  @test.assert_eq(int_to_string(456789), "456789")
  for i = 0; i < 10; i = i + 1 {
    @test.assert_eq(digit_to_uint16(i).to_int(), 48 + i)
  }

  @test.assert_eq(parse_float("-2.5"), Some(-2.5))
  @test.assert_eq(parse_float("+3.5"), Some(3.5))
  @test.assert_eq(parse_float(".5"), Some(0.5))
  @test.assert_eq(parse_float("5."), Some(5.0))
  @test.assert_eq(parse_float("1.2.3"), None)
  @test.assert_eq(parse_float("abc"), None)
  @test.assert_eq(parse_float(""), None)
  @test.assert_eq(parse_float("-"), None)
  @test.assert_eq(parse_float("+"), None)
  @test.assert_eq(parse_float("12a"), None)

  @test.assert_eq(parse_int64("+7"), Some(7L))
  @test.assert_eq(parse_int64("-"), None)
  @test.assert_eq(parse_int64("9223372036854775808"), None)
  @test.assert_eq(parse_int64("-9223372036854775808"), Some(-9223372036854775807L - 1))
  @test.assert_eq(parse_int64("99999999999999999999"), None)
  @test.assert_eq(parse_int("2147483648"), None)
  @test.assert_eq(parse_int("-2147483649"), None)
  @test.assert_eq(parse_int("42"), Some(42))

  @test.assert_eq(format_float(0.0), "0")
  @test.assert_eq(format_float(-2.5), "-2.5")
  @test.assert_eq(format_float(3.0), "3")
  @test.assert_eq(format_float(0.25), "0.25")
  @test.assert_eq(format_float(-0.25), "-0.25")
}

test "coverage_transaction_replay" {
  let db = Database::new()
  db.set("n", "10")
  db.set("maxv", "9223372036854775807")
  db.hset("hh", "f1", "v1") |> ignore
  ignore(db.multi())
  ignore(db.incr("n"))
  ignore(db.decr("n"))
  ignore(db.incr("fresh"))
  ignore(db.decr("fresh2"))
  ignore(db.incr("maxv"))
  ignore(db.hset("hh", "f2", "v2"))
  ignore(db.hdel("hh", "f1"))
  @test.assert_eq(db.exec(), "OK")
  @test.assert_eq(db.get("n"), Some("10"))
  @test.assert_eq(db.get("fresh"), Some("1"))
  @test.assert_eq(db.get("fresh2"), Some("-1"))
  @test.assert_eq(db.get("maxv"), Some("9223372036854775807"))
  @test.assert_eq(db.hget("hh", "f1"), None)
  @test.assert_eq(db.hget("hh", "f2"), Some("v2"))
}
