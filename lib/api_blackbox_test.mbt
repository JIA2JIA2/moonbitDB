test "blackbox core string api" {
  let db = @lib.Database::new()
  db.set("k", "v")
   @test.assert_eq(db.get("k"), Some("v"))
   @test.assert_eq(db.get("missing"), None)
   @test.assert_eq(db.exists("k"), true)
   @test.assert_eq(db.append("k", "!"), 2)
   @test.assert_eq(db.strlen("k"), 2)
   @test.assert_eq(db.getrange("k", 0, 1), "v!")
   @test.assert_eq(db.incr("counter"), Some(1))
   @test.assert_eq(db.incrby("counter", 10), Some(11))
   @test.assert_eq(db.del("k"), true)
   @test.assert_eq(db.exists("k"), false)
}

test "blackbox hash api" {
  let db = @lib.Database::new()
   @test.assert_eq(db.hset("h", "f", "v"), 1)
   @test.assert_eq(db.hget("h", "f"), Some("v"))
   @test.assert_eq(db.hexists("h", "f"), true)
   @test.assert_eq(db.hdel("h", "f"), true)
   @test.assert_eq(db.hget("h", "f"), None)
}

test "blackbox list api" {
  let db = @lib.Database::new()
   @test.assert_eq(db.lpush("l", "b"), 1)
   @test.assert_eq(db.lpush("l", "a"), 2)
   @test.assert_eq(db.rpush("l", "c"), 3)
   @test.assert_eq(db.lrange("l", 0, -1), ["a", "b", "c"])
   @test.assert_eq(db.lpop("l"), Some("a"))
   @test.assert_eq(db.rpop("l"), Some("c"))
   @test.assert_eq(db.llen("l"), 1)
}

test "blackbox set api" {
  let db = @lib.Database::new()
   @test.assert_eq(db.sadd("s", "a"), true)
   @test.assert_eq(db.sadd("s", "a"), false)
   @test.assert_eq(db.sismember("s", "a"), true)
   @test.assert_eq(db.scard("s"), 1)
   @test.assert_eq(db.srem("s", "a"), true)
   @test.assert_eq(db.sismember("s", "a"), false)
}

test "blackbox zset api" {
  let db = @lib.Database::new()
   @test.assert_eq(db.zadd("z", 1.0, "a"), true)
   @test.assert_eq(db.zadd("z", 2.0, "b"), true)
   @test.assert_eq(db.zscore("z", "a"), Some(1.0))
   @test.assert_eq(db.zcard("z"), 2)
   @test.assert_eq(db.zrank("z", "b"), Some(1))
   @test.assert_eq(db.zrange("z", 0, -1), ["a", "b"])
   @test.assert_eq(db.zrem("z", "a"), true)
   @test.assert_eq(db.zcard("z"), 1)
}

test "blackbox expire and keys" {
  let db = @lib.Database::new()
  db.set("k", "v")
   @test.assert_eq(db.expire("k", 100), true)
   @test.assert_eq(db.ttl("k") > 0, true)
   @test.assert_eq(db.persist("k"), true)
   @test.assert_eq(db.ttl("k"), -1)
  db.set("k2", "v")
   @test.assert_eq(db.keys().length(), 2)
   @test.assert_eq(db.type_of("k"), "string")
   @test.assert_eq(db.dbsize(), 2)
}

test "blackbox error channels" {
  let db = @lib.Database::new()
  ignore(db.hset("h", "f", "v"))
  match db.try_get("h") {
    Err(@lib.RedisError::WrongType) => ()
    _ => @test.assert_eq(true, false)
  }
  match db.try_incrby("h", 1) {
    Err(@lib.RedisError::WrongType) => ()
    _ => @test.assert_eq(true, false)
  }
  match db.try_lrem("h", 0, "x") {
    Err(@lib.RedisError::WrongType) => ()
    _ => @test.assert_eq(true, false)
  }
  match db.try_rename("missing", "r") {
    Err(@lib.RedisError::NotFound) => ()
    _ => @test.assert_eq(true, false)
  }
}

test "blackbox transaction and watch" {
  let db = @lib.Database::new()
  db.set("n", "10")
  ignore(db.multi())
  ignore(db.incr("n"))
  ignore(db.incr("n"))
   @test.assert_eq(db.exec(), "OK")
   @test.assert_eq(db.get("n"), Some("12"))

  db.watch("n")
  db.set("n", "changed")
  ignore(db.multi())
  ignore(db.incr("n"))
  match db.try_exec() {
    Err(@lib.RedisError::TransactionAborted) => ()
    _ => @test.assert_eq(true, false)
  }
   @test.assert_eq(db.get("n"), Some("changed"))
}

test "blackbox sort and modifiers" {
  let db = @lib.Database::new()
  ignore(db.rpush_all("l", ["3", "1", "2"]))
   @test.assert_eq(db.sort("l", false, false, 0, 0, None), ["1", "2", "3"])
  ignore(db.zadd("z", 1.0, "a"))
  ignore(db.zadd("z", 2.0, "b"))
   @test.assert_eq(db.zrange_modifiers("z", "0", "-1", None, true, false, 0, 0), ["b", "a"])
   ignore(db.set("gd", "take-me"))
  match db.try_getdel("gd") {
    Ok(Some(v)) => @test.assert_eq(v, "take-me")
    _ => @test.assert_eq(true, false)
  }
}


test "blackbox scan and stores" {
  let db = @lib.Database::new()
  ignore(db.sadd_all("s", ["one", "two", "three"]))
  let (c1, batch1) = db.sscan("s", 0, 10)
  @test.assert_eq(batch1.length(), 3)
  @test.assert_eq(c1, 0)
  ignore(db.zadd("z", 1.0, "a"))
  ignore(db.zadd("z", 2.0, "b"))
  let (_, zbatch) = db.zscan("z", 0, 10)
  @test.assert_eq(zbatch.length(), 2)
  @test.assert_eq(db.zrangestore("zdst", "z", 0, 0), 1)
  @test.assert_eq(db.zcard("zdst"), 1)
  @test.assert_eq(db.zunionstore("zu", ["z", "z"]), 2)
  @test.assert_eq(db.zdiffstore("zd", ["z", "z"]), 0)
}

test "blackbox config and client" {
  let db = @lib.Database::new()
  @test.assert_eq(db.config_set("maxmemory", "64mb"), "OK")
  @test.assert_eq(db.config_get("maxmemory"), ["maxmemory", "64mb"])
  @test.assert_eq(db.client_setname("cli-1"), "OK")
  @test.assert_eq(db.client_getname(), "cli-1")
  @test.assert_eq(db.slowlog("LEN"), ["0"])
  @test.assert_eq(db.lolwut().contains("0.1.4"), true)
}

test "blackbox bitfield api" {
  let db = @lib.Database::new()
  match db.try_bitfield_set("bf", 0, 8, false, 200) {
    Ok(old) => @test.assert_eq(old, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_get("bf", 0, 8, false) {
    Ok(v) => @test.assert_eq(v, 200)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_incrby_mode("bf", 0, 8, false, 100, Some("SAT")) {
    Ok(v) => @test.assert_eq(v, Some(255))
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_incrby_mode("bf", 0, 8, false, 1, Some("FAIL")) {
    Ok(v) => @test.assert_eq(v, None)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_bitfield_get("bf", -1, 8, false) {
    Err(@lib.RedisError::InvalidArgument) => ()
    _ => @test.assert_eq(true, false)
  }
}

test "blackbox getex variants" {
  let db = @lib.Database::new()
  db.set("g", "v")
  @test.assert_eq(db.getex_ex("g", 100), Some("v"))
  @test.assert_eq(db.pttl("g"), 100000)
  @test.assert_eq(db.getex_exat("g", 9999999999999), Some("v"))
  @test.assert_eq(db.getex_pxat("g", 9999999999999), Some("v"))
  @test.assert_eq(db.getex("g", None), Some("v"))
  @test.assert_eq(db.ttl("g"), -1)
}

test "blackbox hscan and lpos" {
  let db = @lib.Database::new()
  ignore(db.hmset("h", ["f1", "f2"], ["1", "2"]))
  let (c, batch) = db.hscan_with_match("h", 0, 10, Some("f*"))
  @test.assert_eq(batch.length(), 2)
  @test.assert_eq(c, 0)
  ignore(db.rpush_all("l", ["a", "b", "a"]))
  @test.assert_eq(db.lpos_options("l", "a", 1, -1, 0), [0, 2])
  @test.assert_eq(db.lpos_options("l", "zz", 1, 0, 0), [])
}

test "blackbox misc commands" {
  let db = @lib.Database::new()
  db.set("k", "v")
  @test.assert_eq(db.memory_usage("k") != None, true)
  @test.assert_eq(db.command_info(["GET"]).length(), 1)
  @test.assert_eq(db.command_count() > 150, true)
  @test.assert_eq(db.wait(1, 100), 0)
  @test.assert_eq(db.save(), "OK")
  @test.assert_eq(db.exists_many(["k", "missing"]), 1)
  @test.assert_eq(db.unlink_many(["k", "missing"]), 1)
}


test "blackbox zrange modifiers" {
  let db = @lib.Database::new()
  ignore(db.zadd("z", 1.0, "a"))
  ignore(db.zadd("z", 2.0, "b"))
  @test.assert_eq(db.zrange_modifiers("z", "0", "-1", None, false, true, 0, 0), ["a", "1", "b", "2"])
  @test.assert_eq(db.zrange_modifiers("z", "-inf", "+inf", Some("byscore"), true, false, 0, 0), ["b", "a"])
  @test.assert_eq(db.zrange_modifiers("z", "-", "+", Some("bylex"), false, false, 0, 0), ["a", "b"])
}

test "blackbox zrangestore variants" {
  let db = @lib.Database::new()
  ignore(db.zadd("src", 1.0, "a"))
  match db.try_zrangestore("d1", "src", 0, -1) {
    Ok(n) => @test.assert_eq(n, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangestore_byscore("d2", "src", "-inf", "+inf", 0, 0) {
    Ok(n) => @test.assert_eq(n, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangestore_bylex("d3", "src", "-", "+") {
    Ok(n) => @test.assert_eq(n, 1)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_zrangestore("d1", "missing", 0, -1) {
    Ok(n) => @test.assert_eq(n, 0)
    Err(_) => @test.assert_eq(true, false)
  }
  @test.assert_eq(db.exists("d1"), false)
}

test "blackbox restore busykey" {
  let db = @lib.Database::new()
  db.set("r", "old")
  match db.try_restore("r", 1000L, "v", false, false) {
    Err(@lib.RedisError::BusyKey) => ()
    _ => @test.assert_eq(true, false)
  }
  match db.try_restore("r", 1000L, "v", true, false) {
    Ok(v) => @test.assert_eq(v, true)
    Err(_) => @test.assert_eq(true, false)
  }
}

test "blackbox setex family" {
  let db = @lib.Database::new()
  match db.try_setex("a", 10, "v") {
    Ok(v) => @test.assert_eq(v, true)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_psetex("b", 1000, "v") {
    Ok(v) => @test.assert_eq(v, true)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_setnx("c", "v") {
    Ok(v) => @test.assert_eq(v, true)
    Err(_) => @test.assert_eq(true, false)
  }
  match db.try_setex("a", 0, "v") {
    Err(@lib.RedisError::InvalidArgument) => ()
    _ => @test.assert_eq(true, false)
  }
}

test "blackbox hmget and exists_many" {
  let db = @lib.Database::new()
  ignore(db.hmset("h", ["a", "b"], ["1", "2"]))
  @test.assert_eq(db.hmget("h", ["a", "b", "c"]), [Some("1"), Some("2"), None])
  @test.assert_eq(db.exists_many(["h", "c"]), 1)
  @test.assert_eq(db.unlink_many(["h", "c"]), 1)
  @test.assert_eq(db.exists("h"), false)
}

test "blackbox bitop and bitcount" {
  let db = @lib.Database::new()
  ignore(db.set("s1", "\u{f}"))
  @test.assert_eq(db.bitop("NOT", "d", ["s1"]), 1)
  @test.assert_eq(db.bitcount("d"), 12)
  @test.assert_eq(db.bitop("NOOP", "d", ["s1"]), -1)
  match db.try_bitop("AND", "d", ["missing"]) {
    Ok(len) => @test.assert_eq(len, 0)
    Err(_) => @test.assert_eq(true, false)
  }
}

test "blackbox glob patterns" {
  let db = @lib.Database::new()
  ignore(db.set("user:1", "a"))
  ignore(db.set("user:2", "b"))
  ignore(db.set("admin:1", "c"))
  @test.assert_eq(db.keys_pattern("user:*").length(), 2)
  @test.assert_eq(db.keys_pattern("user:?").length(), 2)
  @test.assert_eq(db.keys_pattern("[au]*").length(), 3)
  @test.assert_eq(db.keys_pattern("user:\\*").length(), 0)
}
