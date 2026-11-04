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
