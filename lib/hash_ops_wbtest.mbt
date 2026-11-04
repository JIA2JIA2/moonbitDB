test "hash_operations" {
  let db = Database::new()
  
  db.hset("user:1", "name", "Bob") |> ignore
  db.hset("user:1", "email", "bob@example.com") |> ignore
  
  @test.assert_eq(db.hget("user:1", "name"), Some("Bob"))
  @test.assert_eq(db.hget("user:1", "email"), Some("bob@example.com"))
  
  @test.assert_eq(db.hlen("user:1"), 2)
  
  db.hdel("user:1", "email") |> ignore
  @test.assert_eq(db.hget("user:1", "email"), None)
}

test "hash_boundary_cases" {
  let db = Database::new()
  
  @test.assert_eq(db.hget("nonexistent", "field"), None)
  @test.assert_eq(db.hlen("nonexistent"), 0)
  @test.assert_eq(db.hdel("nonexistent", "field"), false)
  
  let empty_hash = db.hgetall("nonexistent")
  @test.assert_eq(empty_hash.length(), 0)
  
  db.hset("hash1", "field1", "value1") |> ignore
  db.hset("hash1", "field2", "value2") |> ignore
  
  let hash_all = db.hgetall("hash1")
  @test.assert_eq(hash_all.length(), 2)
  
  db.hset("hash1", "field1", "updated") |> ignore
  @test.assert_eq(db.hget("hash1", "field1"), Some("updated"))
  @test.assert_eq(db.hlen("hash1"), 2)
  
  db.hdel("hash1", "field1") |> ignore
  @test.assert_eq(db.hlen("hash1"), 1)
  
  @test.assert_eq(db.hdel("hash1", "nonexistent_field"), false)
  
  db.hset("hash2", "", "empty_field") |> ignore
  @test.assert_eq(db.hget("hash2", ""), Some("empty_field"))
}

test "hmset_hmget_operations" {
  let db = Database::new()
  
  db.hmset("user:1", ["name", "email", "age"], ["Alice", "alice@example.com", "30"]) |> ignore
  
  @test.assert_eq(db.hget("user:1", "name"), Some("Alice"))
  @test.assert_eq(db.hget("user:1", "email"), Some("alice@example.com"))
  @test.assert_eq(db.hget("user:1", "age"), Some("30"))
  
  let fields = db.hmget("user:1", ["name", "email", "age"])
  @test.assert_eq(fields.length(), 3)
  @test.assert_eq(fields[0], Some("Alice"))
  @test.assert_eq(fields[1], Some("alice@example.com"))
  @test.assert_eq(fields[2], Some("30"))
  
  let mixed = db.hmget("user:1", ["name", "nonexistent", "age"])
  @test.assert_eq(mixed.length(), 3)
  @test.assert_eq(mixed[0], Some("Alice"))
  @test.assert_eq(mixed[1], None)
  @test.assert_eq(mixed[2], Some("30"))
  
  db.hmset("user:1", ["age", "city"], ["31", "Beijing"]) |> ignore
  @test.assert_eq(db.hget("user:1", "age"), Some("31"))
  @test.assert_eq(db.hget("user:1", "city"), Some("Beijing"))
}

test "hash_extended_edge_cases" {
  let db = Database::new()
  
  db.hset("user:1", "name", "Alice") |> ignore
  db.hset("user:1", "age", "30") |> ignore
  db.hset("user:1", "email", "alice@test.com") |> ignore
  
  @test.assert_eq(db.hlen("user:1"), 3)
  
  db.hset("user:1", "age", "31") |> ignore
  @test.assert_eq(db.hget("user:1", "age"), Some("31"))
  @test.assert_eq(db.hlen("user:1"), 3)
  
  @test.assert_eq(db.hdel("user:1", "email"), true)
  @test.assert_eq(db.hdel("user:1", "email"), false)
  @test.assert_eq(db.hlen("user:1"), 2)
  
  @test.assert_eq(db.hget("user:1", "name"), Some("Alice"))
  @test.assert_eq(db.hget("user:1", "nonexistent"), None)
  
  let all = db.hgetall("user:1")
  @test.assert_eq(all.length(), 2)
  
  @test.assert_eq(db.hlen("nonexistent"), 0)
  let empty_all = db.hgetall("nonexistent")
  @test.assert_eq(empty_all.length(), 0)
}

test "hmset_wrong_type_noop" {
  let db = Database::new()
  db.set("k", "v") |> ignore
  db.hmset("k", ["a"], ["1"]) |> ignore
  @test.assert_eq(db.type_of("k"), "string")
  @test.assert_eq(db.get("k"), Some("v"))
}

test "hgetall_returns_copy" {
  let db = Database::new()
  db.hset("h", "f", "v") |> ignore
  let snapshot = db.hgetall("h")
  snapshot.set("f", "changed")
  snapshot.set("new", "x")
  @test.assert_eq(db.hget("h", "f"), Some("v"))
  @test.assert_eq(db.hexists("h", "new"), false)
}

test "hexists_hkeys_hvals" {
  let db = Database::new()
  @test.assert_eq(db.hexists("nonexistent", "field"), false)
  @test.assert_eq(db.hkeys("nonexistent").length(), 0)
  @test.assert_eq(db.hvals("nonexistent").length(), 0)

  db.hset("h", "a", "1") |> ignore
  db.hset("h", "b", "2") |> ignore
  @test.assert_eq(db.hexists("h", "a"), true)
  @test.assert_eq(db.hexists("h", "c"), false)
  @test.assert_eq(db.hexists("nonexistent", "a"), false)

  let keys = db.hkeys("h")
  @test.assert_eq(keys.length(), 2)
  @test.assert_eq(keys.contains("a"), true)
  @test.assert_eq(keys.contains("b"), true)

  let vals = db.hvals("h")
  @test.assert_eq(vals.length(), 2)
  @test.assert_eq(vals.contains("1"), true)
  @test.assert_eq(vals.contains("2"), true)

  db.hdel("h", "a") |> ignore
  @test.assert_eq(db.hexists("h", "a"), false)

  db.pexpire("h", 100) |> ignore
  db.advance_time(200)
  @test.assert_eq(db.hexists("h", "b"), false)
  @test.assert_eq(db.hkeys("h").length(), 0)
}

test "hincrby_hstrlen" {
  let db = Database::new()
  @test.assert_eq(db.hincrby("h", "count", 5), Some(5))
  @test.assert_eq(db.hincrby("h", "count", 3), Some(8))
  @test.assert_eq(db.hincrby("h", "count", -10), Some(-2))
  @test.assert_eq(db.hget("h", "count"), Some("-2"))

  db.hset("h", "text", "abc") |> ignore
  @test.assert_eq(db.hincrby("h", "text", 1), None)
  @test.assert_eq(db.hget("h", "text"), Some("abc"))

  @test.assert_eq(db.hstrlen("h", "text"), 3)
  @test.assert_eq(db.hstrlen("h", "count"), 2)
  @test.assert_eq(db.hstrlen("h", "nonexistent"), 0)
  @test.assert_eq(db.hstrlen("nonexistent", "f"), 0)
}

test "hset_returns_new_field_count" {
  let db = Database::new()
  @test.assert_eq(db.hset("h", "a", "1"), 1)
  @test.assert_eq(db.hset("h", "a", "2"), 0)
  @test.assert_eq(db.hset("h", "b", "3"), 1)
  @test.assert_eq(db.hlen("h"), 2)
}

test "hsetnx_operations" {
  let db = Database::new()
  @test.assert_eq(db.hsetnx("h", "f", "v"), true)
  @test.assert_eq(db.hsetnx("h", "f", "x"), false)
  @test.assert_eq(db.hget("h", "f"), Some("v"))

  db.set("s", "v") |> ignore
  @test.assert_eq(db.hsetnx("s", "f", "v"), false)
  @test.assert_eq(db.type_of("s"), "string")
}

test "mset_hmset_length_validation" {
  let db = Database::new()
  @test.assert_eq(
    match db.try_mset(["a", "b"], ["1"]) {
      Err(RedisError::WrongArgumentCount(_)) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.exists("a"), false)
  @test.assert_eq(db.exists("b"), false)
  @test.assert_eq(
    match db.try_mset([], []) {
      Err(RedisError::WrongArgumentCount(_)) => true
      _ => false
    },
    true,
  )

  db.mset(["c"], ["3"]) |> ignore
  @test.assert_eq(db.get("c"), Some("3"))

  @test.assert_eq(
    match db.try_hmset("h", ["f1", "f2"], ["v1"]) {
      Err(RedisError::WrongArgumentCount(_)) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.exists("h"), false)

  db.hmset("h", ["f1"], ["v1"]) |> ignore |> ignore
  @test.assert_eq(db.hget("h", "f1"), Some("v1"))

  db.set("str", "v") |> ignore
  @test.assert_eq(
    match db.try_hmset("str", ["f"], ["v"]) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.type_of("str"), "string")
}

test "transaction_hash_operations" {
  let db = Database::new()
  
  ignore(db.multi())
  ignore(db.hset("hash_key", "field1", "value1"))
  ignore(db.hset("hash_key", "field2", "value2"))
  ignore(db.hdel("hash_key", "field1"))
  @test.assert_eq(db.exec(), "OK")
  
  @test.assert_eq(db.hget("hash_key", "field1"), None)
  @test.assert_eq(db.hget("hash_key", "field2"), Some("value2"))
}
