test "cross_type_operations" {
  let db = Database::new()
  
  db.set("string_key", "hello")
  db.hset("hash_key", "field", "value") |> ignore
  db.lpush("list_key", "item") |> ignore
  db.sadd("set_key", "member") |> ignore
  db.zadd("zset_key", 100.0, "zmember") |> ignore
  
  @test.assert_eq(db.type_of("string_key"), "string")
  @test.assert_eq(db.type_of("hash_key"), "hash")
  @test.assert_eq(db.type_of("list_key"), "list")
  @test.assert_eq(db.type_of("set_key"), "set")
  @test.assert_eq(db.type_of("zset_key"), "zset")
  @test.assert_eq(db.type_of("nonexistent"), "none")
  
  let all_keys = db.keys()
  @test.assert_eq(all_keys.length(), 5)
  
  @test.assert_eq(db.exists("string_key"), true)
  db.del("string_key") |> ignore
  @test.assert_eq(db.exists("string_key"), false)
  
  let remaining = db.keys()
  @test.assert_eq(remaining.length(), 4)
}

test "string_operations" {
  let db = Database::new()
  
  db.set("name", "Alice")
  @test.assert_eq(db.get("name"), Some("Alice"))
  
  db.set("age", "30")
  @test.assert_eq(db.get("age"), Some("30"))
  
  db.append("name", " Smith") |> ignore
  @test.assert_eq(db.get("name"), Some("Alice Smith"))
  
  @test.assert_eq(db.strlen("name"), 11)
}

test "string_boundary_cases" {
  let db = Database::new()
  
  @test.assert_eq(db.get("nonexistent"), None)
  @test.assert_eq(db.strlen("nonexistent"), 0)
  
  db.set("empty", "")
  @test.assert_eq(db.get("empty"), Some(""))
  @test.assert_eq(db.strlen("empty"), 0)
  
  db.set("key1", "value1")
  db.set("key1", "value2")
  @test.assert_eq(db.get("key1"), Some("value2"))
  
  db.append("non_string_key", "test") |> ignore
  @test.assert_eq(db.get("non_string_key"), Some("test"))
  
  @test.assert_eq(db.exists("key1"), true)
  @test.assert_eq(db.exists("nonexistent"), false)
  
  db.del("key1") |> ignore
  @test.assert_eq(db.exists("key1"), false)
  @test.assert_eq(db.del("nonexistent"), false)
}

test "incr_decr_operations" {
  let db = Database::new()
  
  @test.assert_eq(db.incr("counter"), Some(1))
  @test.assert_eq(db.incr("counter"), Some(2))
  @test.assert_eq(db.decr("counter"), Some(1))
  @test.assert_eq(db.decr("counter"), Some(0))
  @test.assert_eq(db.decr("counter"), Some(-1))
  
  db.set("invalid", "abc")
  @test.assert_eq(db.incr("invalid"), None)
  
  @test.assert_eq(db.get("counter"), Some("-1"))
}

test "keys_pattern_operations" {
  let db = Database::new()
  
  db.set("user:1", "Alice")
  db.set("user:2", "Bob")
  db.set("user:3", "Charlie")
  db.set("product:100", "Laptop")
  db.set("config", "default")
  
  let users = db.keys_pattern("user:*")
  @test.assert_eq(users.length(), 3)
  
  let user1 = db.keys_pattern("user:1")
  @test.assert_eq(user1.length(), 1)
  
  let any = db.keys_pattern("*")
  @test.assert_eq(any.length(), 5)
  
  let product = db.keys_pattern("product:*")
  @test.assert_eq(product.length(), 1)
  
  let user_any = db.keys_pattern("user:?")
  @test.assert_eq(user_any.length(), 3)
  
  let no_match = db.keys_pattern("nonexistent:*")
  @test.assert_eq(no_match.length(), 0)
}

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

test "sorted_set_operations" {
  let db = Database::new()
  
  db.zadd("scores", 100.0, "Alice") |> ignore
  db.zadd("scores", 90.0, "Bob") |> ignore
  db.zadd("scores", 95.0, "Charlie") |> ignore
  
  @test.assert_eq(db.zcard("scores"), 3)
  @test.assert_eq(db.zscore("scores", "Alice"), Some(100))
  
  let top_two = db.zrange("scores", 1, 2)
  @test.assert_eq(top_two.length(), 2)
  
  db.zrem("scores", "Bob") |> ignore
  @test.assert_eq(db.zcard("scores"), 2)
}

test "sorted_set_boundary_cases" {
  let db = Database::new()
  
  @test.assert_eq(db.zcard("nonexistent"), 0)
  @test.assert_eq(db.zscore("nonexistent", "member"), None)
  @test.assert_eq(db.zrem("nonexistent", "member"), false)
  
  let empty_range = db.zrange("nonexistent", 0, -1)
  @test.assert_eq(empty_range.length(), 0)
  
  @test.assert_eq(db.zadd("zset1", 10.0, "a"), true)
  @test.assert_eq(db.zadd("zset1", 20.0, "b"), true)
  @test.assert_eq(db.zadd("zset1", 10.0, "a"), false)
  
  @test.assert_eq(db.zcard("zset1"), 2)
  @test.assert_eq(db.zscore("zset1", "a"), Some(10))
  
  @test.assert_eq(db.zadd("zset1", 15.0, "a"), false)
  @test.assert_eq(db.zscore("zset1", "a"), Some(15))
  
  let all_range = db.zrange("zset1", 0, -1)
  @test.assert_eq(all_range.length(), 2)
  
  let first = db.zrange("zset1", 0, 0)
  @test.assert_eq(first.length(), 1)
  
  let last = db.zrange("zset1", -1, -1)
  @test.assert_eq(last.length(), 1)
  
  @test.assert_eq(db.zrem("zset1", "c"), false)
  @test.assert_eq(db.zrem("zset1", "a"), true)
  
  @test.assert_eq(db.zcard("zset1"), 1)
  @test.assert_eq(db.zscore("zset1", "a"), None)
  
  db.zadd("neg_scores", -5.0, "low") |> ignore
  db.zadd("neg_scores", -10.0, "lower") |> ignore
  db.zadd("neg_scores", 0.0, "zero") |> ignore
  
  let sorted_neg = db.zrange("neg_scores", 0, -1)
  @test.assert_eq(sorted_neg.length(), 3)
}

test "expire_operations" {
  let db = Database::new()
  
  db.set("key1", "value1")
  @test.assert_eq(db.exists("key1"), true)
  @test.assert_eq(db.ttl("key1"), -1)
  @test.assert_eq(db.pttl("key1"), -1)
  
  @test.assert_eq(db.expire("key1", 60), true)
  @test.assert_eq(db.ttl("key1") >= 0, true)
  @test.assert_eq(db.pttl("key1") >= 0, true)
  
  @test.assert_eq(db.expire("nonexistent", 60), false)
  
  @test.assert_eq(db.persist("key1"), true)
  @test.assert_eq(db.ttl("key1"), -1)
  @test.assert_eq(db.pttl("key1"), -1)
  
  @test.assert_eq(db.persist("key1"), false)
}

test "expire_after_timeout" {
  let db = Database::new()
  
  db.set("temp_key", "temp_value")
  @test.assert_eq(db.get("temp_key"), Some("temp_value"))
  
  @test.assert_eq(db.pexpire("temp_key", 10), true)
  
  db.advance_time(20)
  
  @test.assert_eq(db.get("temp_key"), None)
  @test.assert_eq(db.exists("temp_key"), false)
  @test.assert_eq(db.ttl("temp_key"), -2)
  @test.assert_eq(db.pttl("temp_key"), -2)
}

test "expire_type_check" {
  let db = Database::new()
  
  db.set("string_key", "value")
  db.hset("hash_key", "field", "value") |> ignore
  db.lpush("list_key", "item") |> ignore
  db.sadd("set_key", "member") |> ignore
  db.zadd("zset_key", 100.0, "member") |> ignore
  
  @test.assert_eq(db.expire("string_key", 60), true)
  @test.assert_eq(db.expire("hash_key", 60), true)
  @test.assert_eq(db.expire("list_key", 60), true)
  @test.assert_eq(db.expire("set_key", 60), true)
  @test.assert_eq(db.expire("zset_key", 60), true)
  
  @test.assert_eq(db.ttl("string_key") >= 0, true)
  @test.assert_eq(db.ttl("hash_key") >= 0, true)
  @test.assert_eq(db.ttl("list_key") >= 0, true)
  @test.assert_eq(db.ttl("set_key") >= 0, true)
  @test.assert_eq(db.ttl("zset_key") >= 0, true)
}

test "expire_refresh_on_write" {
  let db = Database::new()
  
  db.set("key", "value")
  db.expire("key", 1) |> ignore
  @test.assert_eq(db.ttl("key") >= 0, true)
  
  db.set("key", "new_value")
  @test.assert_eq(db.ttl("key"), -1)
  
  db.set("key2", "100")
  db.expire("key2", 1) |> ignore
  @test.assert_eq(db.ttl("key2") >= 0, true)
  
  db.incr("key2") |> ignore
  @test.assert_eq(db.ttl("key2") >= 0, true)
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

test "mdel_operations" {
  let db = Database::new()
  
  db.mset(["a", "b", "c", "d"], ["1", "2", "3", "4"])
  
  @test.assert_eq(db.exists("a"), true)
  @test.assert_eq(db.exists("b"), true)
  @test.assert_eq(db.exists("c"), true)
  @test.assert_eq(db.exists("d"), true)
  
  let deleted = db.mdel(["a", "c", "nonexistent"])
  @test.assert_eq(deleted, 2)
  
  @test.assert_eq(db.exists("a"), false)
  @test.assert_eq(db.exists("b"), true)
  @test.assert_eq(db.exists("c"), false)
  @test.assert_eq(db.exists("d"), true)
  
  let deleted_all = db.mdel(["b", "d"])
  @test.assert_eq(deleted_all, 2)
  
  let remaining = db.keys()
  @test.assert_eq(remaining.length(), 0)
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

test "batch_with_expire" {
  let db = Database::new()
  
  db.mset(["temp1", "temp2"], ["value1", "value2"])
  db.expire("temp1", 1) |> ignore
  
  let before_expire = db.mget(["temp1", "temp2"])
  @test.assert_eq(before_expire.length(), 2)
  @test.assert_eq(before_expire[0], Some("value1"))
  @test.assert_eq(before_expire[1], Some("value2"))
  
  db.advance_time(2000)
  
  let after_expire = db.mget(["temp1", "temp2"])
  @test.assert_eq(after_expire.length(), 2)
  @test.assert_eq(after_expire[0], None)
  @test.assert_eq(after_expire[1], Some("value2"))
  
  let deleted = db.mdel(["temp1", "temp2"])
  @test.assert_eq(deleted, 1)
  
  db.hmset("temp_hash", ["a", "b"], ["1", "2"]) |> ignore
  db.expire("temp_hash", 1) |> ignore
  
  db.advance_time(2000)
  
  let hash_fields = db.hmget("temp_hash", ["a", "b"])
  @test.assert_eq(hash_fields.length(), 2)
  @test.assert_eq(hash_fields[0], None)
  @test.assert_eq(hash_fields[1], None)
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

test "zrangebyscore_zcount_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  db.zadd("zset1", 5.0, "e") |> ignore
  
  let range1 = db.zrangebyscore("zset1", 2.0, 4.0)
  @test.assert_eq(range1.length(), 3)
  @test.assert_eq(range1[0], "b")
  @test.assert_eq(range1[1], "c")
  @test.assert_eq(range1[2], "d")
  
  let range2 = db.zrangebyscore("zset1", 0.0, 100.0)
  @test.assert_eq(range2.length(), 5)
  
  let range3 = db.zrangebyscore("zset1", 10.0, 20.0)
  @test.assert_eq(range3.length(), 0)
  
  let count1 = db.zcount("zset1", 2.0, 4.0)
  @test.assert_eq(count1, 3)
  
  let count2 = db.zcount("zset1", 0.0, 100.0)
  @test.assert_eq(count2, 5)
  
  let count3 = db.zcount("nonexistent", 0.0, 100.0)
  @test.assert_eq(count3, 0)
}

test "zrevrange_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  
  let rev1 = db.zrevrange("zset1", 0, -1)
  @test.assert_eq(rev1.length(), 4)
  @test.assert_eq(rev1[0], "d")
  @test.assert_eq(rev1[1], "c")
  @test.assert_eq(rev1[2], "b")
  @test.assert_eq(rev1[3], "a")
  
  let rev2 = db.zrevrange("zset1", 0, 1)
  @test.assert_eq(rev2.length(), 2)
  @test.assert_eq(rev2[0], "d")
  @test.assert_eq(rev2[1], "c")
  
  let rev3 = db.zrevrange("zset1", -2, -1)
  @test.assert_eq(rev3.length(), 2)
  @test.assert_eq(rev3[0], "b")
  @test.assert_eq(rev3[1], "a")
}

test "zrevrangebyscore_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  
  let rev1 = db.zrevrangebyscore("zset1", 4.0, 2.0)
  @test.assert_eq(rev1.length(), 3)
  @test.assert_eq(rev1[0], "d")
  @test.assert_eq(rev1[1], "c")
  @test.assert_eq(rev1[2], "b")
  
  let rev2 = db.zrevrangebyscore("zset1", 3.0, 1.0)
  @test.assert_eq(rev2.length(), 3)
  @test.assert_eq(rev2[0], "c")
  
  let rev3 = db.zrevrangebyscore("zset1", 10.0, 5.0)
  @test.assert_eq(rev3.length(), 0)
}

test "zrank_zrevrank_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 10.0, "a") |> ignore
  db.zadd("zset1", 20.0, "b") |> ignore
  db.zadd("zset1", 30.0, "c") |> ignore
  db.zadd("zset1", 40.0, "d") |> ignore
  
  @test.assert_eq(db.zrank("zset1", "a"), Some(0))
  @test.assert_eq(db.zrank("zset1", "b"), Some(1))
  @test.assert_eq(db.zrank("zset1", "d"), Some(3))
  @test.assert_eq(db.zrank("zset1", "nonexistent"), None)
  @test.assert_eq(db.zrank("nonexistent", "a"), None)
  
  @test.assert_eq(db.zrevrank("zset1", "a"), Some(3))
  @test.assert_eq(db.zrevrank("zset1", "b"), Some(2))
  @test.assert_eq(db.zrevrank("zset1", "d"), Some(0))
  @test.assert_eq(db.zrevrank("zset1", "nonexistent"), None)
}

test "zincrby_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 10.0, "a") |> ignore
  
  let score1 = db.zincrby("zset1", 5.0, "a")
  @test.assert_eq(score1, 15)
  
  @test.assert_eq(db.zscore("zset1", "a"), Some(15))
  
  let score2 = db.zincrby("zset1", -3.0, "a")
  @test.assert_eq(score2, 12)
  
  let score3 = db.zincrby("zset1", 100.0, "new_member")
  @test.assert_eq(score3, 100)
  @test.assert_eq(db.zscore("zset1", "new_member"), Some(100))
  
  let score4 = db.zincrby("nonexistent", 5.0, "x")
  @test.assert_eq(score4, 5)
}

test "zremrangebyrank_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  db.zadd("zset1", 5.0, "e") |> ignore
  
  let removed1 = db.zremrangebyrank("zset1", 0, 1)
  @test.assert_eq(removed1, 2)
  @test.assert_eq(db.zcard("zset1"), 3)
  @test.assert_eq(db.zrange("zset1", 0, -1)[0], "c")
  
  let removed2 = db.zremrangebyrank("zset1", -2, -1)
  @test.assert_eq(removed2, 2)
  @test.assert_eq(db.zcard("zset1"), 1)
  
  let removed3 = db.zremrangebyrank("nonexistent", 0, 10)
  @test.assert_eq(removed3, 0)
}

test "zremrangebyscore_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  db.zadd("zset1", 5.0, "e") |> ignore
  
  let removed1 = db.zremrangebyscore("zset1", 2.0, 4.0)
  @test.assert_eq(removed1, 3)
  @test.assert_eq(db.zcard("zset1"), 2)
  @test.assert_eq(db.zscore("zset1", "b"), None)
  @test.assert_eq(db.zscore("zset1", "a"), Some(1))
  
  let removed2 = db.zremrangebyscore("zset1", 0.0, 100.0)
  @test.assert_eq(removed2, 2)
  @test.assert_eq(db.zcard("zset1"), 0)
  
  let removed3 = db.zremrangebyscore("nonexistent", 0.0, 100.0)
  @test.assert_eq(removed3, 0)
}

test "zpopmin_zpopmax_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 10.0, "a") |> ignore
  db.zadd("zset1", 20.0, "b") |> ignore
  db.zadd("zset1", 30.0, "c") |> ignore
  db.zadd("zset1", 40.0, "d") |> ignore
  
  let min1 = db.zpopmin("zset1")
  @test.assert_eq(min1 is None, false)
  
  let min_val = match min1 {
    Some(v) => v
    None => ("", 0.0)
  }
  @test.assert_eq(min_val.0, "a")
  @test.assert_eq(db.zcard("zset1"), 3)
  
  let max1 = db.zpopmax("zset1")
  @test.assert_eq(max1 is None, false)
  
  let max_val = match max1 {
    Some(v) => v
    None => ("", 0.0)
  }
  @test.assert_eq(max_val.0, "d")
  @test.assert_eq(db.zcard("zset1"), 2)
  
  db.zpopmin("zset1") |> ignore
  db.zpopmin("zset1") |> ignore
  
  @test.assert_eq(db.zcard("zset1"), 0)
  
  let empty_min = db.zpopmin("zset1")
  @test.assert_eq(empty_min is None, true)
  
  let empty_max = db.zpopmax("zset1")
  @test.assert_eq(empty_max is None, true)
  
  let nonexistent = db.zpopmin("nonexistent")
  @test.assert_eq(nonexistent is None, true)
}

test "dbsize_flushdb_operations" {
  let db = Database::new()
  
  @test.assert_eq(db.dbsize(), 0)
  
  db.set("key1", "value1")
  db.set("key2", "value2")
  db.set("key3", "value3")
  
  @test.assert_eq(db.dbsize(), 3)
  
  db.del("key1") |> ignore
  @test.assert_eq(db.dbsize(), 2)
  
  db.flushdb() |> ignore
  @test.assert_eq(db.dbsize(), 0)
  @test.assert_eq(db.exists("key2"), false)
  @test.assert_eq(db.exists("key3"), false)
  
  db.set("a", "1") |> ignore
  db.flushall() |> ignore
  @test.assert_eq(db.dbsize(), 0)
}

test "randomkey_operations" {
  let db = Database::new()
  
  @test.assert_eq(db.randomkey() is None, true)
  
  db.set("key1", "value1")
  db.set("key2", "value2")
  db.set("key3", "value3")
  
  let key = db.randomkey()
  @test.assert_eq(key is None, false)
  
  let key_val = match key {
    Some(k) => k
    None => ""
  }
  @test.assert_eq(db.exists(key_val), true)
  
  db.del("key1") |> ignore
  db.del("key2") |> ignore
  db.del("key3") |> ignore
  
  @test.assert_eq(db.randomkey() is None, true)
}

test "rename_operations" {
  let db = Database::new()
  
  db.set("old_key", "value1")
  
  @test.assert_eq(db.rename("old_key", "new_key"), true)
  @test.assert_eq(db.get("new_key"), Some("value1"))
  @test.assert_eq(db.exists("old_key"), false)
  
  @test.assert_eq(db.rename("nonexistent", "target"), false)
  
  db.set("key_a", "a") |> ignore
  db.set("key_b", "b") |> ignore
  @test.assert_eq(db.rename("key_a", "key_b"), true)
  @test.assert_eq(db.get("key_b"), Some("a"))
  @test.assert_eq(db.exists("key_a"), false)
  
  db.set("src", "src_val") |> ignore
  db.expire("src", 100) |> ignore
  db.rename("src", "dst") |> ignore
  @test.assert_eq(db.ttl("dst") >= 0, true)
  @test.assert_eq(db.exists("src"), false)
}

test "renamenx_operations" {
  let db = Database::new()
  
  db.set("key1", "value1") |> ignore
  db.set("key2", "value2") |> ignore
  
  @test.assert_eq(db.renamenx("key1", "key3"), true)
  @test.assert_eq(db.get("key3"), Some("value1"))
  @test.assert_eq(db.exists("key1"), false)
  
  db.set("key4", "value4") |> ignore
  @test.assert_eq(db.renamenx("key2", "key4"), false)
  @test.assert_eq(db.get("key4"), Some("value4"))
  @test.assert_eq(db.get("key2"), Some("value2"))
  
  @test.assert_eq(db.renamenx("nonexistent", "key5"), false)
}

test "ping_echo_operations" {
  let db = Database::new()
  
  @test.assert_eq(Database::ping(), "PONG")
  
  @test.assert_eq(Database::echo(db, "Hello World"), "Hello World")
  @test.assert_eq(Database::echo(db, ""), "")
  @test.assert_eq(Database::echo(db, "123"), "123")
}

test "info_operations" {
  let db = Database::new()
  
  db.set("key1", "value1") |> ignore
  db.set("key2", "value2") |> ignore
  db.expire("key1", 100) |> ignore
  
  let info = db.info()
  @test.assert_eq(info.contains("moonbit_db_version"), true)
  @test.assert_eq(info.contains("keys=2"), true)
  @test.assert_eq(info.contains("expires=1"), true)
  
  db.advance_time(200000)
  let info2 = db.info()
  @test.assert_eq(info2.contains("uptime_in_ms:200000"), true)
  @test.assert_eq(info2.contains("keys=1"), true)
}

test "time_operations" {
  let db = Database::new()
  
  let t1 = db.time()
  @test.assert_eq(t1.0, 0)
  @test.assert_eq(t1.1, 0)
  
  db.set_time(5000)
  let t2 = db.time()
  @test.assert_eq(t2.0, 5)
  @test.assert_eq(t2.1, 0)
  
  db.set_time(5678)
  let t3 = db.time()
  @test.assert_eq(t3.0, 5)
  @test.assert_eq(t3.1, 678000)
}

test "command_operations" {
  let commands = Database::command()
  @test.assert_eq(commands.length() > 50, true)
  
  @test.assert_eq(commands.contains("SET"), true)
  @test.assert_eq(commands.contains("GET"), true)
  @test.assert_eq(commands.contains("HSET"), true)
  @test.assert_eq(commands.contains("LPUSH"), true)
  @test.assert_eq(commands.contains("SADD"), true)
  @test.assert_eq(commands.contains("ZADD"), true)
  @test.assert_eq(commands.contains("DBSIZE"), true)
  @test.assert_eq(commands.contains("FLUSHDB"), true)
  @test.assert_eq(commands.contains("PING"), true)
}

test "object_encoding_operations" {
  let db = Database::new()
  
  db.set("str_key", "hello")
  @test.assert_eq(db.object_encoding("str_key"), Some("embstr"))
  
  db.hset("hash_key", "field", "value") |> ignore
  @test.assert_eq(db.object_encoding("hash_key"), Some("listpack"))
  
  db.rpush("list_key", "item") |> ignore
  @test.assert_eq(db.object_encoding("list_key"), Some("listpack"))
  
  db.sadd("set_key", "member") |> ignore
  @test.assert_eq(db.object_encoding("set_key"), Some("listpack"))
  
  db.zadd("zset_key", 1.0, "member") |> ignore
  @test.assert_eq(db.object_encoding("zset_key"), Some("listpack"))
  
  @test.assert_eq(db.object_encoding("nonexistent"), None)
}

test "string_extended_edge_cases" {
  let db = Database::new()
  
  db.set("counter", "10")
  @test.assert_eq(db.incr("counter"), Some(11))
  @test.assert_eq(db.incr("counter"), Some(12))
  @test.assert_eq(db.decr("counter"), Some(11))
  
  db.set("zero", "0")
  @test.assert_eq(db.decr("zero"), Some(-1))
  @test.assert_eq(db.decr("zero"), Some(-2))
  
  @test.assert_eq(db.incr("new_counter"), Some(1))
  
  db.set("not_number", "abc")
  @test.assert_eq(db.incr("not_number"), None)
  @test.assert_eq(db.decr("not_number"), None)
  
  db.set("append_key", "Hello")
  db.append("append_key", " World") |> ignore
  @test.assert_eq(db.get("append_key"), Some("Hello World"))
  @test.assert_eq(db.strlen("append_key"), 11)
  
  db.append("new_append", "First") |> ignore
  @test.assert_eq(db.get("new_append"), Some("First"))
  
  db.set("unicode", "你好")
  @test.assert_eq(db.strlen("unicode"), 2)
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

test "zset_extended_edge_cases" {
  let db = Database::new()
  
  db.zadd("zset", 1.0, "a") |> ignore
  @test.assert_eq(db.zadd("zset", 5.0, "a"), false)
  @test.assert_eq(db.zscore("zset", "a"), Some(5))
  
  @test.assert_eq(db.zscore("zset", "nonexistent"), None)
  @test.assert_eq(db.zscore("nonexistent", "a"), None)
  
  @test.assert_eq(db.zcard("nonexistent"), 0)
  
  let single = db.zrange("zset", 0, -1)
  @test.assert_eq(single.length(), 1)
  
  db.zadd("zset", 3.0, "b") |> ignore
  db.zadd("zset", 3.0, "c") |> ignore
  
  let score_range = db.zrangebyscore("zset", 3.0, 3.0)
  @test.assert_eq(score_range.length(), 2)
  
  let count_exact = db.zcount("zset", 3.0, 3.0)
  @test.assert_eq(count_exact, 2)
  
  @test.assert_eq(db.zrank("zset", "a"), Some(2))
  @test.assert_eq(db.zrank("zset", "b"), Some(0))
  
  db.zincrby("zset", -10.0, "a") |> ignore
  @test.assert_eq(db.zscore("zset", "a"), Some(-5))
  
  let rev = db.zrange("zset", 0, -1)
  @test.assert_eq(rev[0], "a")
  
  @test.assert_eq(db.zrem("zset", "nonexistent"), false)
  @test.assert_eq(db.zrem("nonexistent", "a"), false)
  
  @test.assert_eq(db.zremrangebyrank("zset", 100, 200), 0)
  @test.assert_eq(db.zremrangebyscore("zset", 100.0, 200.0), 0)
}

test "expire_extended_edge_cases" {
  let db = Database::new()
  
  @test.assert_eq(db.expire("nonexistent", 100), false)
  
  @test.assert_eq(db.pexpire("nonexistent", 100000), false)
  
  db.set("key1", "value1") |> ignore
  @test.assert_eq(db.persist("key1"), false)
  
  db.expire("key1", 100) |> ignore
  @test.assert_eq(db.persist("key1"), true)
  @test.assert_eq(db.ttl("key1"), -1)
  @test.assert_eq(db.persist("key1"), false)
  
  db.set("key2", "value2") |> ignore
  db.pexpire("key2", 500) |> ignore
  @test.assert_eq(db.pttl("key2") >= 0, true)
  
  db.advance_time(300)
  @test.assert_eq(db.exists("key2"), true)
  
  db.advance_time(300)
  @test.assert_eq(db.exists("key2"), false)
  
  db.set("key3", "value3") |> ignore
  db.expire("key3", 100) |> ignore
  db.set("key3", "new_value") |> ignore
  @test.assert_eq(db.ttl("key3"), -1)
  
  db.set("key4", "value4") |> ignore
  db.expire("key4", 100) |> ignore
  db.del("key4") |> ignore
  @test.assert_eq(db.exists("key4"), false)
  @test.assert_eq(db.ttl("key4"), -2)
}

test "cross_type_safety" {
  let db = Database::new()
  
  db.set("str_key", "hello") |> ignore
  @test.assert_eq(db.lpush("str_key", "item"), -1)
  @test.assert_eq(db.type_of("str_key"), "string")
  @test.assert_eq(db.get("str_key"), Some("hello"))
  
  db.lpush("list_key", "item1") |> ignore
  @test.assert_eq(db.hset("list_key", "field", "value"), -1)
  @test.assert_eq(db.type_of("list_key"), "list")
  
  db.sadd("set_key", "member") |> ignore
  @test.assert_eq(db.zadd("set_key", 1.0, "zmember"), false)
  @test.assert_eq(db.type_of("set_key"), "set")
  
  db.hset("hash_key", "f", "v") |> ignore
  @test.assert_eq(db.incr("hash_key"), None)
  @test.assert_eq(db.type_of("hash_key"), "hash")
  @test.assert_eq(db.append("hash_key", "x"), -1)
  @test.assert_eq(db.type_of("hash_key"), "hash")
  
  db.hset("hash_key", "f", "v") |> ignore
  db.set("hash_key", "string_value") |> ignore
  @test.assert_eq(db.type_of("hash_key"), "string")
  
  @test.assert_eq(db.type_of("nonexistent"), "none")
}

test "hmset_wrong_type_noop" {
  let db = Database::new()
  db.set("k", "v") |> ignore
  db.hmset("k", ["a"], ["1"]) |> ignore
  @test.assert_eq(db.type_of("k"), "string")
  @test.assert_eq(db.get("k"), Some("v"))
}

test "getset_wrong_type_noop" {
  let db = Database::new()
  db.hset("h", "f", "v") |> ignore
  @test.assert_eq(db.getset("h", "new"), None)
  @test.assert_eq(db.type_of("h"), "hash")
  @test.assert_eq(db.hget("h", "f"), Some("v"))
}

test "large_ttl_no_overflow" {
  let db = Database::new()
  db.set("k", "v") |> ignore
  @test.assert_eq(db.expire("k", 2592000), true)
  @test.assert_eq(db.ttl("k"), 2592000)
  @test.assert_eq(db.pttl("k"), 2592000000)
  db.advance_time(2592000000)
  @test.assert_eq(db.exists("k"), false)

  db.set_time(0)
  db.set("k2", "v") |> ignore
  @test.assert_eq(db.expireat("k2", 1700000000), true)
  @test.assert_eq(db.pttl("k2"), 1700000000000)
}

test "incr_supports_int64" {
  let db = Database::new()
  db.set("n", "2147483648") |> ignore
  match db.incr("n") {
    Some(v) => @test.assert_eq("\{v}", "2147483649")
    None => @test.assert_eq(false, true)
  }
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

test "zset_cache_invalidated_on_delete_and_overwrite" {
  let db = Database::new()
  db.zadd("z", 1.0, "a") |> ignore
  db.zrange("z", 0, -1) |> ignore
  db.del("z") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  let r = db.zrange("z", 0, -1)
  @test.assert_eq(r.length(), 1)
  @test.assert_eq(r[0], "b")

  db.zadd("z2", 1.0, "a") |> ignore
  db.zrange("z2", 0, -1) |> ignore
  db.set("z2", "str") |> ignore
  db.del("z2") |> ignore
  db.zadd("z2", 5.0, "c") |> ignore
  @test.assert_eq(db.zrange("z2", 0, -1)[0], "c")
}

test "rpoplpush_same_key_preserves_ttl" {
  let db = Database::new()
  db.rpush("q", "only") |> ignore
  db.expire("q", 100) |> ignore
  @test.assert_eq(db.rpoplpush("q", "q"), Some("only"))
  @test.assert_eq(db.ttl("q") >= 0, true)
}

test "zset_score_tie_lexicographic" {
  let db = Database::new()
  db.zadd("z", 1.0, "b") |> ignore
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 1.0, "c") |> ignore
  db.zadd("z", 0.0, "z") |> ignore
  let asc = db.zrange("z", 0, -1)
  @test.assert_eq(asc.length(), 4)
  @test.assert_eq(asc[0], "z")
  @test.assert_eq(asc[1], "a")
  @test.assert_eq(asc[2], "b")
  @test.assert_eq(asc[3], "c")
  let desc = db.zrevrange("z", 0, 2)
  @test.assert_eq(desc[0], "c")
  @test.assert_eq(desc[1], "b")
  @test.assert_eq(desc[2], "a")
}

test "zset_range_by_score_bounds" {
  let db = Database::new()
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 2.0, "c") |> ignore
  db.zadd("z", 3.0, "d") |> ignore
  let mid = db.zrangebyscore("z", 2.0, 2.0)
  @test.assert_eq(mid.length(), 2)
  @test.assert_eq(mid[0], "b")
  @test.assert_eq(mid[1], "c")
  @test.assert_eq(db.zcount("z", 2.0, 2.0), 2)
  @test.assert_eq(db.zcount("z", 1.5, 2.5), 2)
  let rev = db.zrevrangebyscore("z", 2.0, 2.0)
  @test.assert_eq(rev[0], "c")
  @test.assert_eq(rev[1], "b")
  @test.assert_eq(db.zrangebyscore("z", 4.0, 5.0).length(), 0)
  @test.assert_eq(db.zcount("z", 5.0, 1.0), 0)
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

test "pattern_matching_no_exponential_backtracking" {
  @test.assert_eq(match_pattern("a*a*a*a*a*b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), false)
  @test.assert_eq(match_pattern("*a*b*", "xxaxxb"), true)
  @test.assert_eq(match_pattern("a?c", "abc"), true)
  @test.assert_eq(match_pattern("a?c", "ac"), false)
  @test.assert_eq(match_pattern("*", ""), true)
}

test "batch_extended_edge_cases" {
  let db = Database::new()
  
  db.mset(["k1", "k2"], ["v1", "v2"]) |> ignore
  @test.assert_eq(db.get("k1"), Some("v1"))
  @test.assert_eq(db.get("k2"), Some("v2"))
  
  db.mset(["k1", "k2"], ["new_v1", "new_v2"]) |> ignore
  @test.assert_eq(db.get("k1"), Some("new_v1"))
  
  db.set("k3", "existing") |> ignore
  let result = db.mget(["k1", "k2", "k3", "nonexistent"])
  @test.assert_eq(result.length(), 4)
  @test.assert_eq(result[0], Some("new_v1"))
  @test.assert_eq(result[1], Some("new_v2"))
  @test.assert_eq(result[2], Some("existing"))
  @test.assert_eq(result[3], None)
  
  db.mset(["k4", "k5", "k6"], ["v4"]) |> ignore
  @test.assert_eq(db.exists("k4"), false)
  @test.assert_eq(db.exists("k5"), false)
  @test.assert_eq(db.exists("k6"), false)
  
  let deleted = db.mdel(["k1", "k2", "k3", "nonexistent"])
  @test.assert_eq(deleted, 3)
  
  db.hmset("hash_batch", ["f1", "f2"], ["v1", "v2"]) |> ignore |> ignore
  db.hmset("hash_batch", ["f3", "f4"], ["v3", "v4"]) |> ignore |> ignore
  @test.assert_eq(db.hlen("hash_batch"), 4)
  
  db.hmset("hash_batch", ["f1"], ["updated"]) |> ignore |> ignore
  @test.assert_eq(db.hget("hash_batch", "f1"), Some("updated"))
  @test.assert_eq(db.hlen("hash_batch"), 4)
}

test "server_extended_edge_cases" {
  let db = Database::new()
  
  db.set("key1", "value1") |> ignore
  db.set("key2", "value2") |> ignore
  db.expire("key1", 1) |> ignore
  
  db.advance_time(2000)
  
  @test.assert_eq(db.dbsize(), 1)
  
  db.set("key3", "value3") |> ignore
  @test.assert_eq(db.rename("key3", "key3"), true)
  @test.assert_eq(db.get("key3"), Some("value3"))
  
  db.set("key4", "value4") |> ignore
  db.expire("key4", 100) |> ignore
  db.rename("key4", "key5") |> ignore
  @test.assert_eq(db.ttl("key5") >= 0, true)
  
  db.set("key6", "value6") |> ignore
  db.set("key7", "value7") |> ignore
  @test.assert_eq(db.renamenx("key6", "key6"), false)
  
  let all_keys = db.keys()
  let _ = db.randomkey()
  @test.assert_eq(all_keys.length() > 0, true)
  
  db.flushdb() |> ignore
  @test.assert_eq(db.dbsize(), 0)
  @test.assert_eq(db.randomkey() is None, true)
}

test "deque_internal_operations" {
  let db = Database::new()
  
  db.lpush("dq", "c") |> ignore
  db.lpush("dq", "b") |> ignore
  db.lpush("dq", "a") |> ignore
  db.rpush("dq", "d") |> ignore
  db.rpush("dq", "e") |> ignore
  
  @test.assert_eq(db.lindex("dq", 0), Some("a"))
  @test.assert_eq(db.lindex("dq", 4), Some("e"))
  @test.assert_eq(db.lindex("dq", 2), Some("c"))
  
  db.lset("dq", 2, "C") |> ignore
  @test.assert_eq(db.lindex("dq", 2), Some("C"))
  
  db.lrem("dq", 1, "C") |> ignore
  @test.assert_eq(db.llen("dq"), 4)
  
  db.linsert("dq", true, "b", "B") |> ignore
  @test.assert_eq(db.lindex("dq", 1), Some("B"))
  
  db.linsert("dq", false, "d", "D") |> ignore
  @test.assert_eq(db.lindex("dq", 4), Some("D"))
  @test.assert_eq(db.llen("dq"), 6)
  
  let val = db.rpoplpush("dq", "dq2")
  @test.assert_eq(val, Some("e"))
  @test.assert_eq(db.llen("dq"), 5)
  @test.assert_eq(db.llen("dq2"), 1)
}

test "zset_score_edge_cases" {
  let db = Database::new()
  
  db.zadd("zset", 0.0, "zero") |> ignore
  db.zadd("zset", -1.0, "negative") |> ignore
  db.zadd("zset", 3.14, "pi") |> ignore
  db.zadd("zset", -0.5, "neg_half") |> ignore
  
  let sorted = db.zrange("zset", 0, -1)
  @test.assert_eq(sorted[0], "negative")
  @test.assert_eq(sorted[1], "neg_half")
  @test.assert_eq(sorted[2], "zero")
  @test.assert_eq(sorted[3], "pi")
  
  let neg_range = db.zrangebyscore("zset", -1.0, 0.0)
  @test.assert_eq(neg_range.length(), 3)
  
  let pos_count = db.zcount("zset", 0.0, 100.0)
  @test.assert_eq(pos_count, 2)
  
  db.zincrby("zset", 1.5, "zero") |> ignore
  @test.assert_eq(db.zscore("zset", "zero"), Some(1.5))
  
  let min_pop = db.zpopmin("zset")
  let min_val = match min_pop {
    Some(v) => v
    None => ("", 0.0)
  }
  @test.assert_eq(min_val.0, "negative")
  
  let max_pop = db.zpopmax("zset")
  let max_val = match max_pop {
    Some(v) => v
    None => ("", 0.0)
  }
  @test.assert_eq(max_val.0, "pi")
}

test "keys_pattern_expired_key" {
  let db = Database::new()
  db.set("user:1", "Alice")
  db.set("user:2", "Bob")
  db.set("admin:1", "Root")
  db.pexpire("user:1", 100) |> ignore
  db.advance_time(200)

  let users = db.keys_pattern("user:*")
  @test.assert_eq(users.length(), 1)

  let all = db.keys_pattern("*")
  @test.assert_eq(all.length(), 2)
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

test "renamenx_expired_newkey" {
  let db = Database::new()
  db.set("old_key", "value")
  db.set("new_key", "old_value")
  db.pexpire("new_key", 100) |> ignore
  db.advance_time(200)

  @test.assert_eq(db.renamenx("old_key", "new_key"), true)
  @test.assert_eq(db.get("new_key"), Some("value"))
}

test "incr_decr_ttl_preserved" {
  let db = Database::new()
  db.set("counter", "10")
  db.expire("counter", 60) |> ignore
  let ttl_before = db.ttl("counter")

  db.incr("counter") |> ignore
  let ttl_after_incr = db.ttl("counter")
  @test.assert_eq(ttl_after_incr >= 0, true)
  @test.assert_eq(ttl_after_incr <= ttl_before + 1, true)

  db.decr("counter") |> ignore
  let ttl_after_decr = db.ttl("counter")
  @test.assert_eq(ttl_after_decr >= 0, true)

  db.set("counter2", "5")
  db.incr("counter2") |> ignore
  @test.assert_eq(db.ttl("counter2"), -1)
}

test "int_to_string_min_value" {
  let db = Database::new()
  db.set("counter", "-9223372036854775807")
  db.decr("counter") |> ignore
  let val = db.get("counter")
  @test.assert_eq(val is None, false)
}

test "parse_int_sign_only" {
  let db = Database::new()
  db.set("sign_minus", "-")
  db.set("sign_plus", "+")
  @test.assert_eq(db.incr("sign_minus"), None)
  @test.assert_eq(db.incr("sign_plus"), None)
}

test "append_cross_type_safety" {
  let db = Database::new()
  db.hset("myhash", "field", "value") |> ignore

  @test.assert_eq(db.append("myhash", "test"), -1)
  @test.assert_eq(db.type_of("myhash"), "hash")
  @test.assert_eq(db.hget("myhash", "field"), Some("value"))

  db.set("mystr", "hello")
  @test.assert_eq(db.append("mystr", " world"), 11)
  @test.assert_eq(db.get("mystr"), Some("hello world"))
}

test "ttl_rounding" {
  let db = Database::new()
  db.set("key1", "value1")
  db.pexpire("key1", 1500) |> ignore

  @test.assert_eq(db.ttl("key1"), 2)

  @test.assert_eq(db.pttl("key1"), 1500)

  db.set("key2", "value2")
  db.pexpire("key2", 1000) |> ignore
  @test.assert_eq(db.ttl("key2"), 1)

  db.set("key3", "value3")
  db.pexpire("key3", 1) |> ignore
  @test.assert_eq(db.ttl("key3"), 1)

  db.set("key4", "value4")
  db.pexpire("key4", 499) |> ignore
  @test.assert_eq(db.ttl("key4"), 1)

  db.set("key5", "value5")
  db.pexpire("key5", 500) |> ignore
  @test.assert_eq(db.ttl("key5"), 1)
}

test "pttl_precision" {
  let db = Database::new()
  db.set("key1", "value1")
  db.pexpire("key1", 1500) |> ignore

  @test.assert_eq(db.pttl("key1"), 1500)
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

test "string_setnx_setex_getdel_getset" {
  let db = Database::new()
  @test.assert_eq(db.setnx("k", "v1"), true)
  @test.assert_eq(db.setnx("k", "v2"), false)
  @test.assert_eq(db.get("k"), Some("v1"))

  @test.assert_eq(db.setex("t", 100, "tv"), true)
  @test.assert_eq(db.ttl("t") >= 0, true)
  @test.assert_eq(db.get("t"), Some("tv"))
  @test.assert_eq(db.setex("t", 0, "x"), false)

  db.set("gd", "val") |> ignore
  @test.assert_eq(db.getdel("gd"), Some("val"))
  @test.assert_eq(db.exists("gd"), false)
  @test.assert_eq(db.getdel("gd"), None)

  db.set("gs", "old") |> ignore
  @test.assert_eq(db.getset("gs", "new"), Some("old"))
  @test.assert_eq(db.get("gs"), Some("new"))
  @test.assert_eq(db.getset("fresh", "n"), None)

  db.set("gs_ttl", "v") |> ignore
  db.expire("gs_ttl", 100) |> ignore
  db.getset("gs_ttl", "v2") |> ignore
  @test.assert_eq(db.ttl("gs_ttl"), -1)

  db.set("nx_ttl", "v") |> ignore
  db.pexpire("nx_ttl", 100) |> ignore
  db.advance_time(200)
  @test.assert_eq(db.setnx("nx_ttl", "fresh"), true)
  @test.assert_eq(db.get("nx_ttl"), Some("fresh"))
}

test "expireat_commands" {
  let db = Database::new()
  db.set("k", "v") |> ignore
  @test.assert_eq(db.expireat("k", 1000), true)
  db.advance_time(999000)
  @test.assert_eq(db.exists("k"), true)
  db.advance_time(1000)
  @test.assert_eq(db.exists("k"), false)

  db.set("k2", "v") |> ignore
  @test.assert_eq(db.pexpireat("k2", 2000000), true)
  db.advance_time(2000000)
  @test.assert_eq(db.exists("k2"), false)

  @test.assert_eq(db.expireat("nonexistent", 1000), false)
  @test.assert_eq(db.pexpireat("nonexistent", 1000), false)
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

test "zmscore_operations" {
  let db = Database::new()
  db.zadd("z", 1.5, "a") |> ignore
  db.zadd("z", 2.5, "b") |> ignore
  let scores = db.zmscore("z", ["a", "x", "b"])
  @test.assert_eq(scores.length(), 3)
  @test.assert_eq(scores[0], Some(1.5))
  @test.assert_eq(scores[1], None)
  @test.assert_eq(scores[2], Some(2.5))
  let missing = db.zmscore("nonexistent", ["a"])
  @test.assert_eq(missing[0] is None, true)
}

test "negative_range_regression" {
  let db = Database::new()
  db.rpush("l", "a") |> ignore
  db.rpush("l", "b") |> ignore
  db.rpush("l", "c") |> ignore
  db.rpush("l", "d") |> ignore
  let lr = db.lrange("l", -100, 1)
  @test.assert_eq(lr.length(), 2)
  @test.assert_eq(lr[0], "a")
  @test.assert_eq(lr[1], "b")

  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 3.0, "c") |> ignore
  db.zadd("z", 4.0, "d") |> ignore
  let zr = db.zrange("z", -100, 1)
  @test.assert_eq(zr.length(), 2)
  @test.assert_eq(zr[0], "a")

  let zrev = db.zrevrange("z", -100, -3)
  @test.assert_eq(zrev.length(), 2)
  @test.assert_eq(zrev[0], "d")

  let removed = db.zremrangebyrank("z", -100, 1)
  @test.assert_eq(removed, 2)
  @test.assert_eq(db.zcard("z"), 2)
}

test "empty_container_removal" {
  let db = Database::new()
  db.hset("h", "f", "v") |> ignore
  db.hdel("h", "f") |> ignore
  @test.assert_eq(db.exists("h"), false)

  db.rpush("l", "x") |> ignore
  db.lpop("l") |> ignore
  @test.assert_eq(db.exists("l"), false)

  db.rpush("l2", "x") |> ignore
  db.rpop("l2") |> ignore
  @test.assert_eq(db.exists("l2"), false)

  db.sadd("s", "x") |> ignore
  db.srem("s", "x") |> ignore
  @test.assert_eq(db.exists("s"), false)

  db.sadd("s2", "x") |> ignore
  db.spop("s2") |> ignore
  @test.assert_eq(db.exists("s2"), false)

  db.zadd("z", 1.0, "m") |> ignore
  db.zrem("z", "m") |> ignore
  @test.assert_eq(db.exists("z"), false)

  db.sadd("s3", "x") |> ignore
  db.sadd("s4", "y") |> ignore
  db.smove("s3", "s4", "x") |> ignore
  @test.assert_eq(db.exists("s3"), false)

  db.rpush("l3", "x") |> ignore
  db.ltrim("l3", 5, 10) |> ignore
  @test.assert_eq(db.exists("l3"), false)

  db.rpush("l4", "x") |> ignore
  db.rpoplpush("l4", "l5") |> ignore
  @test.assert_eq(db.exists("l4"), false)
  @test.assert_eq(db.llen("l5"), 1)

  db.hset("h2", "f", "v") |> ignore
  db.expire("h2", 100) |> ignore
  db.hdel("h2", "f") |> ignore
  @test.assert_eq(db.exists("h2"), false)
  @test.assert_eq(db.ttl("h2"), -2)
}

test "expire_on_expired_key_semantics" {
  let db = Database::new()
  db.set("k", "v") |> ignore
  db.pexpire("k", 100) |> ignore
  db.advance_time(200)
  @test.assert_eq(db.expire("k", 100), false)
  @test.assert_eq(db.pexpire("k", 100), false)
  @test.assert_eq(db.persist("k"), false)

  db.set("k2", "v") |> ignore
  db.expire("k2", 100) |> ignore
  @test.assert_eq(db.persist("k2"), true)
  @test.assert_eq(db.ttl("k2"), -1)
}

test "zset_cache_consistency" {
  let db = Database::new()
  db.zadd("rank", 10.0, "alice") |> ignore
  db.zadd("rank", 20.0, "bob") |> ignore
  let r1 = db.zrange("rank", 0, -1)
  @test.assert_eq(r1[0], "alice")

  db.zadd("rank", 1.0, "alice") |> ignore
  let r2 = db.zrange("rank", 0, -1)
  @test.assert_eq(r2[0], "alice")
  @test.assert_eq(r2[1], "bob")
  @test.assert_eq(db.zrank("rank", "alice"), Some(0))
  @test.assert_eq(db.zrank("rank", "bob"), Some(1))

  db.zincrby("rank", 100.0, "alice") |> ignore
  @test.assert_eq(db.zrank("rank", "bob"), Some(0))
  @test.assert_eq(db.zrank("rank", "alice"), Some(1))

  db.zpopmax("rank") |> ignore
  @test.assert_eq(db.zrange("rank", 0, -1).length(), 1)

  db.zadd("rank", 5.0, "carol") |> ignore
  db.zrem("rank", "bob") |> ignore
  let r3 = db.zrange("rank", 0, -1)
  @test.assert_eq(r3.length(), 1)
  @test.assert_eq(r3[0], "carol")

  db.set("other", "v") |> ignore
  db.rename("rank", "rank2") |> ignore
  @test.assert_eq(db.zrange("rank2", 0, -1)[0], "carol")
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

test "rpoplpush_single_element_roundtrip" {
  let db = Database::new()
  db.rpush("q", "only") |> ignore
  let v = db.rpoplpush("q", "q")
  @test.assert_eq(v, Some("only"))
  @test.assert_eq(db.llen("q"), 1)
  @test.assert_eq(db.lindex("q", 0), Some("only"))
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

test "incrby_decrby_operations" {
  let db = Database::new()
  db.set("n", "10") |> ignore
  @test.assert_eq(db.incrby("n", 5), Some(15))
  @test.assert_eq(db.decrby("n", 3), Some(12))
  @test.assert_eq(db.decrby("n", -2), Some(14))
  @test.assert_eq(db.get("n"), Some("14"))

  db.set("bad", "x") |> ignore
  @test.assert_eq(db.incrby("bad", 1), None)
  db.hset("h", "f", "v") |> ignore
  @test.assert_eq(db.incrby("h", 1), None)
  @test.assert_eq(db.type_of("h"), "hash")
}

test "getrange_setrange_operations" {
  let db = Database::new()
  db.set("s", "Hello World") |> ignore
  @test.assert_eq(db.getrange("s", 0, 4), "Hello")
  @test.assert_eq(db.getrange("s", -5, -1), "World")
  @test.assert_eq(db.getrange("s", 6, 100), "World")
  @test.assert_eq(db.getrange("nonexistent", 0, -1), "")
  @test.assert_eq(db.setrange("s", 6, "Moon!"), 11)
  @test.assert_eq(db.get("s"), Some("Hello Moon!"))

  db.set("grow", "Hi") |> ignore
  @test.assert_eq(db.setrange("grow", 5, "!"), 6)
  @test.assert_eq(db.getrange("grow", 0, 1), "Hi")
  @test.assert_eq(db.getrange("grow", 5, 5), "!")

  db.set("empty_val", "abc") |> ignore
  @test.assert_eq(db.setrange("empty_val", 1, ""), 3)
  @test.assert_eq(db.setrange("missing", 2, "ab"), 4)
  @test.assert_eq(db.getrange("missing", 2, 3), "ab")
  @test.assert_eq(db.setrange("s", -1, "x"), -1)

  db.hset("hash_key", "f", "v") |> ignore
  @test.assert_eq(db.setrange("hash_key", 0, "x"), -1)
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

test "scan_operations" {
  let db = Database::new()
  for i = 0; i < 25; i = i + 1 {
    db.set("scan:\{i}", "v") |> ignore
  }

  let (c1, b1) = db.scan(0, 10)
  @test.assert_eq(b1.length(), 10)
  @test.assert_eq(c1, 10)
  let (c2, b2) = db.scan(c1, 10)
  @test.assert_eq(b2.length(), 10)
  @test.assert_eq(c2, 20)
  let (c3, b3) = db.scan(c2, 10)
  @test.assert_eq(b3.length(), 5)
  @test.assert_eq(c3, 0)

  let seen = Map([], capacity=25)
  for batch in [b1, b2, b3] {
    for key in batch {
      seen.set(key, true)
    }
  }
  @test.assert_eq(seen.length(), 25)

  let (c4, b4) = db.scan(0, 100)
  @test.assert_eq(b4.length(), 25)
  @test.assert_eq(c4, 0)

  let (c5, b5) = db.scan(999, 10)
  @test.assert_eq(b5.length(), 0)
  @test.assert_eq(c5, 0)

  let (_, b6) = db.scan(0, 0)
  @test.assert_eq(b6.length(), 10)
}

test "zset_with_scores_operations" {
  let db = Database::new()
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 3.0, "c") |> ignore

  let asc = db.zrange_with_scores("z", 0, -1)
  @test.assert_eq(asc.length(), 3)
  @test.assert_eq(asc[0].0, "a")
  @test.assert_eq(asc[0].1, 1)
  @test.assert_eq(asc[2].0, "c")
  @test.assert_eq(asc[2].1, 3)

  let desc = db.zrevrange_with_scores("z", 0, 1)
  @test.assert_eq(desc.length(), 2)
  @test.assert_eq(desc[0].0, "c")
  @test.assert_eq(desc[0].1, 3)
  @test.assert_eq(desc[1].0, "b")
}

test "zrangebylex_operations" {
  let db = Database::new()
  for item in ["a", "b", "c", "d"] {
    db.zadd("z", 0.0, item) |> ignore
  }

  let all = db.zrangebylex("z", "-", "+")
  @test.assert_eq(all.length(), 4)
  @test.assert_eq(all[0], "a")
  @test.assert_eq(all[3], "d")

  let incl = db.zrangebylex("z", "[b", "[c")
  @test.assert_eq(incl.length(), 2)
  @test.assert_eq(incl[0], "b")
  @test.assert_eq(incl[1], "c")

  let excl = db.zrangebylex("z", "(b", "(d")
  @test.assert_eq(excl.length(), 1)
  @test.assert_eq(excl[0], "c")

  let upper = db.zrangebylex("z", "-", "(c")
  @test.assert_eq(upper.length(), 2)
  @test.assert_eq(db.zrangebylex("z", "+", "+").length(), 0)
  @test.assert_eq(db.zrangebylex("z", "b", "+").length(), 0)
}

test "copy_operations" {
  let db = Database::new()
  db.hset("src", "f", "v") |> ignore
  @test.assert_eq(db.copy("src", "dst"), true)
  @test.assert_eq(db.hget("dst", "f"), Some("v"))

  db.hset("src", "f", "changed") |> ignore
  @test.assert_eq(db.hget("dst", "f"), Some("v"))
  @test.assert_eq(db.copy("src", "dst"), false)
  @test.assert_eq(db.copy("missing", "fresh"), false)
  @test.assert_eq(db.copy("src", "src"), false)

  db.rpush("list_src", "a") |> ignore
  @test.assert_eq(db.copy("list_src", "list_dst"), true)
  db.rpush("list_src", "b") |> ignore
  @test.assert_eq(db.llen("list_dst"), 1)

  db.set("ttl_src", "v") |> ignore
  db.expire("ttl_src", 100) |> ignore
  @test.assert_eq(db.copy("ttl_src", "ttl_dst"), true)
  @test.assert_eq(db.ttl("ttl_dst"), -1)
}

test "touch_unlink_operations" {
  let db = Database::new()
  db.set("a", "1") |> ignore
  db.set("b", "2") |> ignore
  @test.assert_eq(db.touch(["a", "b", "missing"]), 2)

  db.set("expired", "v") |> ignore
  db.pexpire("expired", 10) |> ignore
  db.advance_time(20)
  @test.assert_eq(db.touch(["expired"]), 0)

  @test.assert_eq(db.unlink("a"), true)
  @test.assert_eq(db.exists("a"), false)
  @test.assert_eq(db.unlink("a"), false)
}

test "expire_heap_stress" {
  let db = Database::new()
  for i = 0; i < 100; i = i + 1 {
    db.set("ttl:\{i}", "v") |> ignore
    db.pexpire("ttl:\{i}", (i + 1).to_int64()) |> ignore
  }
  db.advance_time(50)
  @test.assert_eq(db.dbsize(), 50)
  @test.assert_eq(db.exists("ttl:49"), false)
  @test.assert_eq(db.exists("ttl:50"), true)

  db.set("update", "v") |> ignore
  db.pexpire("update", 100) |> ignore
  db.advance_time(80)
  @test.assert_eq(db.exists("update"), true)
  db.pexpire("update", 10) |> ignore
  db.advance_time(20)
  @test.assert_eq(db.exists("update"), false)

  db.set("keep", "v") |> ignore
  db.pexpire("keep", 100) |> ignore
  @test.assert_eq(db.persist("keep"), true)
  db.advance_time(200)
  @test.assert_eq(db.exists("keep"), true)

  db.set("renamed", "v") |> ignore
  db.expire("renamed", 100) |> ignore
  db.rename("renamed", "renamed2") |> ignore
  @test.assert_eq(db.ttl("renamed2") > 0, true)
  db.advance_time(101000)
  @test.assert_eq(db.exists("renamed2"), false)

  db.flushdb() |> ignore
  @test.assert_eq(db.expire_heap.length(), 0)
}

test "scan_snapshot_consistency" {
  let db = Database::new()
  for i = 0; i < 30; i = i + 1 {
    db.set("scan:\{i}", "v") |> ignore
  }
  let (c1, b1) = db.scan(0, 10)
  @test.assert_eq(b1.length(), 10)
  let (c2, b2) = db.scan(c1, 10)
  @test.assert_eq(b2.length(), 10)

  db.set("scan:new", "v") |> ignore
  let (c3, b3) = db.scan(c2, 10)
  @test.assert_eq(b3.length(), 10)
  @test.assert_eq(c3, 30)

  let (c4, b4) = db.scan(0, 100)
  @test.assert_eq(b4.length(), 31)
  @test.assert_eq(c4, 0)
  @test.assert_eq(b4.contains("scan:new"), true)
}

test "zset_rank_and_lex_cache" {
  let db = Database::new()
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 3.0, "c") |> ignore
  @test.assert_eq(db.zrank("z", "c"), Some(2))
  @test.assert_eq(db.zrevrank("z", "a"), Some(2))

  db.zadd("z", 0.0, "c") |> ignore
  @test.assert_eq(db.zrank("z", "c"), Some(0))
  @test.assert_eq(db.zrank("z", "b"), Some(2))

  let lex1 = db.zrangebylex("z", "-", "+")
  let lex2 = db.zrangebylex("z", "-", "+")
  @test.assert_eq(lex1.length(), 3)
  @test.assert_eq(lex2[0], "a")

  db.zrem("z", "b") |> ignore
  let lex3 = db.zrangebylex("z", "-", "+")
  @test.assert_eq(lex3.length(), 2)
  @test.assert_eq(db.zrank("z", "c"), Some(0))

  db.rename("z", "z2") |> ignore
  @test.assert_eq(db.zrank("z2", "c"), Some(0))
  @test.assert_eq(db.zrange("z2", 0, -1)[0], "c")
}

test "try_api_error_model" {
  let db = Database::new()
  @test.assert_eq(
    match db.try_get("missing") {
      Err(RedisError::NotFound) => true
      _ => false
    },
    true,
  )
  db.hset("h", "f", "v") |> ignore
  @test.assert_eq(
    match db.try_get("h") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )

  db.set("s", "10") |> ignore
  @test.assert_eq(
    match db.try_get("s") {
      Ok(value) => value
      _ => ""
    },
    "10",
  )
  @test.assert_eq(
    match db.try_incrby("s", 5) {
      Ok(value) => "\{value}"
      Err(_) => "err"
    },
    "15",
  )
  db.set("bad", "x") |> ignore
  @test.assert_eq(
    match db.try_incrby("bad", 1) {
      Err(RedisError::NotInteger) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_incrby("h", 1) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )

  @test.assert_eq(
    match db.try_hincrby("h", "f", 1) {
      Err(RedisError::NotInteger) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_hset("s", "f", "v") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_lpush("s", "x") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_sadd("s", "x") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_zadd("s", 1.0, "x") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_zincrby("s", 1.0, "x") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_setrange("s", -1, "x") {
      Err(RedisError::InvalidArgument) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_sinterstore("dest", ["s"]) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(RedisError::WrongType.message().length() > 0, true)
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

test "scan_match_and_copy_replace" {
  let db = Database::new()
  for i = 0; i < 20; i = i + 1 {
    db.set("user:\{i}", "v") |> ignore
  }
  db.set("order:1", "v") |> ignore

  let matched = Map([], capacity=20)
  let mut cursor = 0
  while true {
    let (next, keys) = db.scan_match(cursor, 5, "user:*")
    for key in keys {
      matched.set(key, true)
    }
    if next == 0 {
      break
    }
    cursor = next
  }
  @test.assert_eq(matched.length(), 20)
  @test.assert_eq(matched.contains("order:1"), false)

  db.zadd("src", 1.0, "a") |> ignore
  db.set("dst", "old") |> ignore
  db.expire("dst", 100) |> ignore
  @test.assert_eq(db.copy_with_replace("src", "dst", false), false)
  @test.assert_eq(db.copy_with_replace("src", "dst", true), true)
  @test.assert_eq(db.type_of("dst"), "zset")
  @test.assert_eq(db.ttl("dst"), -1)
  @test.assert_eq(db.copy_with_replace("src", "fresh", false), true)
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

test "integer_overflow_protection" {
  let db = Database::new()
  let max_value : Int64 = 9223372036854775807L
  let min_value : Int64 = -9223372036854775807L - 1L

  db.set("max", "9223372036854775807") |> ignore
  @test.assert_eq(
    match db.try_incrby("max", 1) {
      Err(RedisError::Overflow) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.get("max"), Some("9223372036854775807"))

  db.set("min", "-9223372036854775808") |> ignore
  @test.assert_eq(
    match db.try_incrby("min", -1) {
      Err(RedisError::Overflow) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_decrby("min", min_value) {
      Err(RedisError::Overflow) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.get("min"), Some("-9223372036854775808"))
  @test.assert_eq(
    match db.try_incrby("min", 1) {
      Ok(value) => "\{value}"
      Err(_) => "err"
    },
    "-9223372036854775807",
  )

  db.hset("h", "f", "9223372036854775807") |> ignore
  @test.assert_eq(
    match db.try_hincrby("h", "f", 1) {
      Err(RedisError::Overflow) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.hget("h", "f"), Some("9223372036854775807"))

  @test.assert_eq(parse_int64("9223372036854775808"), None)
  @test.assert_eq(parse_int64("-9223372036854775809"), None)
  @test.assert_eq(parse_int64("-9223372036854775808"), Some(min_value))
  @test.assert_eq(parse_int64("9223372036854775807"), Some(max_value))
  @test.assert_eq(parse_int("2147483648"), None)
  @test.assert_eq(parse_int("2147483647"), Some(2147483647))
  @test.assert_eq(parse_int("-2147483648"), Some(-2147483648))
}

test "setrange_size_limit" {
  let db = Database::new()
  db.set("k", "v") |> ignore
  let too_large = 536870913
  @test.assert_eq(
    match db.try_setrange("k", too_large, "x") {
      Err(RedisError::InvalidArgument) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.get("k"), Some("v"))
  @test.assert_eq(db.setrange("missing", too_large, "x"), -1)
  @test.assert_eq(db.exists("missing"), false)
  @test.assert_eq(
    match db.try_setrange("k", too_large, "") {
      Ok(length) => length
      Err(_) => -1
    },
    1,
  )
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

test "append_limit_and_error_channel" {
  let db = Database::new()
  db.set("s", "abc") |> ignore
  @test.assert_eq(
    match db.try_append("s", "d") {
      Ok(length) => length
      Err(_) => -1
    },
    4,
  )
  @test.assert_eq(db.get("s"), Some("abcd"))

  db.hset("h", "f", "v") |> ignore
  @test.assert_eq(
    match db.try_append("h", "x") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.append("h", "x"), -1)
  @test.assert_eq(db.type_of("h"), "hash")
}

test "ttl_arithmetic_overflow_protection" {
  let db = Database::new()
  let max_value : Int64 = 9223372036854775807L

  db.set_time(1)
  db.set("k", "v") |> ignore
  @test.assert_eq(db.pexpire("k", max_value), false)
  @test.assert_eq(db.exists("k"), true)
  @test.assert_eq(db.ttl("k"), -1)

  db.set_time(0)
  @test.assert_eq(db.expireat("k", 9223372036854776L), false)
  @test.assert_eq(db.expireat("k", 9223372036854775L), true)

  db.set_time(max_value - 500L)
  @test.assert_eq(db.setex("k2", 1, "v"), false)
  @test.assert_eq(db.exists("k2"), false)

  db.set("timeout", "v") |> ignore
  db.pexpire("timeout", 5) |> ignore
  db.advance_time(1000)
  @test.assert_eq(db.exists("timeout"), true)
  db.advance_time(5)
  @test.assert_eq(db.exists("timeout"), false)
  db.set_time(0)
}

test "try_read_error_channels" {
  let db = Database::new()
  db.set("str", "v") |> ignore

  @test.assert_eq(
    match db.try_hget("str", "f") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  db.hset("h", "f", "v") |> ignore
  @test.assert_eq(
    match db.try_hget("h", "f") {
      Ok(value) => value
      Err(_) => ""
    },
    "v",
  )
  @test.assert_eq(
    match db.try_hget("h", "missing") {
      Err(RedisError::NotFound) => true
      _ => false
    },
    true,
  )

  @test.assert_eq(
    match db.try_lpop("str") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_lpop("empty") {
      Err(RedisError::NotFound) => true
      _ => false
    },
    true,
  )
  db.rpush("list", "a") |> ignore
  @test.assert_eq(
    match db.try_lpop("list") {
      Ok(value) => value
      Err(_) => ""
    },
    "a",
  )
  db.rpush_all("list2", ["x", "y"]) |> ignore
  @test.assert_eq(
    match db.try_rpop("list2") {
      Ok(value) => value
      Err(_) => ""
    },
    "y",
  )

  @test.assert_eq(
    match db.try_zscore("str", "m") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  db.zadd("z", 1.5, "m") |> ignore
  @test.assert_eq(
    match db.try_zscore("z", "m") {
      Ok(score) => score
      Err(_) => 0.0
    },
    1.5,
  )
  @test.assert_eq(
    match db.try_zscore("z", "none") {
      Err(RedisError::NotFound) => true
      _ => false
    },
    true,
  )

  @test.assert_eq(
    match db.try_smembers("str") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_smembers("missing") {
      Ok(values) => values.length()
      Err(_) => -1
    },
    0,
  )
  db.sadd("set", "a") |> ignore
  @test.assert_eq(
    match db.try_smembers("set") {
      Ok(values) => values.length()
      Err(_) => -1
    },
    1,
  )
}

test "expire_heap_compacts_after_delete_and_persist" {
  let db = Database::new()
  for i = 0; i < 200; i = i + 1 {
    db.set("ttl:\{i}", "v") |> ignore
    db.pexpire("ttl:\{i}", 1000) |> ignore
  }
  for i = 0; i < 190; i = i + 1 {
    db.del("ttl:\{i}") |> ignore
  }
  @test.assert_eq(db.expire_heap.length() <= 36, true)

  let db2 = Database::new()
  for i = 0; i < 200; i = i + 1 {
    db2.set("ttl:\{i}", "v") |> ignore
    db2.pexpire("ttl:\{i}", 1000) |> ignore
  }
  for i = 0; i < 190; i = i + 1 {
    db2.persist("ttl:\{i}") |> ignore
  }
  @test.assert_eq(db2.expire_heap.length() <= 36, true)
}

test "zset_write_through_removals" {
  let db = Database::new()
  for member_val in ["b", "aa", "c", "a"] {
    db.zadd("z", 0.0, member_val) |> ignore
  }
  @test.assert_eq(db.zrangebylex("z", "-", "+"), ["a", "aa", "b", "c"])

  db.zadd("z", 0.0, "ab") |> ignore
  @test.assert_eq(db.zrangebylex("z", "-", "+"), ["a", "aa", "ab", "b", "c"])
  @test.assert_eq(db.zrange("z", 0, -1), ["a", "aa", "ab", "b", "c"])

  db.zrem("z", "aa") |> ignore
  @test.assert_eq(db.zrange("z", 0, -1), ["a", "ab", "b", "c"])
  @test.assert_eq(db.zrangebylex("z", "-", "+"), ["a", "ab", "b", "c"])

  db.zrange("z", 0, -1) |> ignore
  db.zremrangebyrank("z", 0, 0) |> ignore
  @test.assert_eq(db.zrange("z", 0, -1), ["ab", "b", "c"])

  db.zrange("z", 0, -1) |> ignore
  db.zremrangebyscore("z", 0.0, 0.0) |> ignore
  @test.assert_eq(db.zcard("z"), 0)
  @test.assert_eq(db.exists("z"), false)
}

test "zset_pop_write_through_cache" {
  let db = Database::new()
  for i = 0; i < 6; i = i + 1 {
    db.zadd("z", Float::from_int(i), "m:\{i}") |> ignore
  }
  @test.assert_eq(db.zrange("z", 0, -1).length(), 6)
  @test.assert_eq(
    match db.zpopmin("z") {
      Some((member_val, _)) => member_val
      None => ""
    },
    "m:0",
  )
  @test.assert_eq(db.zrange("z", 0, -1).length(), 5)
  @test.assert_eq(
    match db.zpopmax("z") {
      Some((member_val, _)) => member_val
      None => ""
    },
    "m:5",
  )
  @test.assert_eq(db.zrange("z", 0, -1), ["m:1", "m:2", "m:3", "m:4"])
  @test.assert_eq(db.zrangebylex("z", "-", "+").length(), 4)
}

test "keys_pattern_prefix_pruning" {
  let db = Database::new()
  for key in ["user:1", "user:10", "user:2", "user:20", "product:1", "u", "user:"] {
    db.set(key, "v") |> ignore
  }
  @test.assert_eq(db.keys_pattern("user:*").length(), 5)
  @test.assert_eq(db.keys_pattern("user:1*").length(), 2)
  @test.assert_eq(db.keys_pattern("user:?").length(), 2)
  @test.assert_eq(db.keys_pattern("product:*").length(), 1)
  @test.assert_eq(db.keys_pattern("*").length(), 7)
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

test "zset_extended_commands" {
  let db = Database::new()
  db.zadd("z1", 1.0, "a") |> ignore
  db.zadd("z1", 2.0, "b") |> ignore
  db.zadd("z1", 3.0, "c") |> ignore
  db.zadd("z2", 10.0, "b") |> ignore
  db.zadd("z2", 20.0, "c") |> ignore
  db.zadd("z2", 30.0, "d") |> ignore

  @test.assert_eq(
    db.zunion(["z1", "z2"]),
    [("a", 1.0), ("b", 12.0), ("c", 23.0), ("d", 30.0)],
  )
  @test.assert_eq(db.zinter(["z1", "z2"]), [("b", 12.0), ("c", 23.0)])
  @test.assert_eq(db.zdiff_with_scores(["z1", "z2"]), [("a", 1.0)])
  @test.assert_eq(db.zdiff_with_scores(["z1", "missing"]), [("a", 1.0), ("b", 2.0), ("c", 3.0)])
  @test.assert_eq(
    match db.zrandmember("z1") {
      Some(_) => true
      None => false
    },
    true,
  )

  for member_val in ["a", "aa", "b", "c"] {
    db.zadd("lex", 0.0, member_val) |> ignore
  }
  @test.assert_eq(db.zrevrangebylex("lex", "+", "-"), ["c", "b", "aa", "a"])
  @test.assert_eq(db.zlexcount("lex", "[a", "(c"), 3)
  @test.assert_eq(db.zremrangebylex("lex", "[a", "(c"), 3)
  @test.assert_eq(db.zrangebylex("lex", "-", "+"), ["c"])

  for i = 0; i < 6; i = i + 1 {
    db.zadd("zs", Float::from_int(i), "m:\{i}") |> ignore
  }
  let (next, batch) = db.zscan("zs", 0, 2)
  @test.assert_eq(batch.length(), 2)
  @test.assert_eq(next, 2)
  let (next2, batch2) = db.zscan("zs", next, 100)
  @test.assert_eq(batch2.length(), 4)
  @test.assert_eq(next2, 0)
}

test "scan_variants" {
  let db = Database::new()
  db.set("str", "v") |> ignore
  db.hmset("h", ["f1", "f2", "f3"], ["v1", "v2", "v3"]) |> ignore
  db.rpush_all("list", ["a", "b"]) |> ignore
  db.sadd_all("set", ["a", "b", "c"]) |> ignore
  db.zadd("zset", 1.0, "a") |> ignore

  let (_, strings) = db.scan_type(0, 100, "string")
  @test.assert_eq(strings, ["str"])
  let (_, hashes) = db.scan_type(0, 100, "hash")
  @test.assert_eq(hashes, ["h"])
  let (_, sets) = db.scan_type(0, 100, "set")
  @test.assert_eq(sets, ["set"])

  let seen : Map[String, String] = Map([], capacity=3)
  let mut cursor = 0
  while true {
    let (next, pairs) = db.hscan("h", cursor, 2)
    for item in pairs {
      let (field, value) = item
      seen.set(field, value)
    }
    if next == 0 {
      break
    }
    cursor = next
  }
  @test.assert_eq(seen.length(), 3)
  @test.assert_eq(seen.get("f2"), Some("v2"))

  let members : Map[String, Bool] = Map([], capacity=3)
  let mut cursor2 = 0
  while true {
    let (next, batch) = db.sscan("set", cursor2, 2)
    for member_val in batch {
      members.set(member_val, true)
    }
    if next == 0 {
      break
    }
    cursor2 = next
  }
  @test.assert_eq(members.length(), 3)
  @test.assert_eq(members.contains("c"), true)
}

test "zrangebylex_binary_bounds" {
  let db = Database::new()
  for member_val in ["a", "aa", "ab", "b", "ba", "c"] {
    db.zadd("z", 0.0, member_val) |> ignore
  }
  @test.assert_eq(db.zrangebylex("z", "[aa", "(b"), ["aa", "ab"])
  @test.assert_eq(db.zrangebylex("z", "-", "(b"), ["a", "aa", "ab"])
  @test.assert_eq(db.zrangebylex("z", "(b", "+"), ["ba", "c"])
  @test.assert_eq(db.zrevrangebylex("z", "+", "[aa"), ["c", "ba", "b", "ab", "aa"])
  @test.assert_eq(db.zlexcount("z", "[aa", "(b"), 2)
  @test.assert_eq(db.zremrangebylex("z", "[aa", "(b"), 2)
  @test.assert_eq(db.zrangebylex("z", "-", "+"), ["a", "b", "ba", "c"])
}

test "scan_snapshot_cache_invalidation" {
  let db = Database::new()
  db.hmset("h", ["f1", "f2", "f3", "f4"], ["v1", "v2", "v3", "v4"]) |> ignore
  let (h_cursor, h_batch) = db.hscan("h", 0, 2)
  @test.assert_eq(h_cursor, 2)
  @test.assert_eq(h_batch.length(), 2)
  @test.assert_eq(db.hscan_snapshots.contains("h"), true)

  db.hset("h", "f5", "v5") |> ignore
  @test.assert_eq(db.hscan_snapshots.contains("h"), false)
  let (h_cursor2, h_batch2) = db.hscan("h", h_cursor, 100)
  @test.assert_eq(h_batch2.length(), 3)
  @test.assert_eq(h_cursor2, 0)
  @test.assert_eq(db.hscan_snapshots.contains("h"), false)

  db.sadd_all("s", ["a", "b", "c", "d"]) |> ignore
  let (s_cursor, s_batch) = db.sscan("s", 0, 2)
  @test.assert_eq(s_cursor, 2)
  @test.assert_eq(s_batch.length(), 2)
  @test.assert_eq(db.sscan_snapshots.contains("s"), true)

  db.sadd("s", "e") |> ignore
  @test.assert_eq(db.sscan_snapshots.contains("s"), false)
  let (s_cursor2, s_batch2) = db.sscan("s", s_cursor, 100)
  @test.assert_eq(s_batch2.length(), 3)
  @test.assert_eq(s_cursor2, 0)
  @test.assert_eq(db.sscan_snapshots.contains("s"), false)

  db.scan(0, 1) |> ignore
  @test.assert_eq(
    match db.scan_snapshot {
      Some(_) => true
      None => false
    },
    true,
  )
  db.clear_scan_caches()
  @test.assert_eq(
    match db.scan_snapshot {
      Some(_) => false
      None => true
    },
    true,
  )
  @test.assert_eq(db.hscan_snapshots.length(), 0)
  @test.assert_eq(db.sscan_snapshots.length(), 0)
}

test "try_api_r3_commands" {
  let db = Database::new()
  db.set("str", "v") |> ignore

  @test.assert_eq(
    match db.try_hscan("str", 0, 10) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  db.hmset("h", ["f"], ["v"]) |> ignore
  @test.assert_eq(
    match db.try_hscan("h", 0, 10) {
      Ok((_, pairs)) => pairs.length()
      Err(_) => -1
    },
    1,
  )

  @test.assert_eq(
    match db.try_lmove("missing", "dest", true, true) {
      Err(RedisError::NotFound) => true
      _ => false
    },
    true,
  )
  db.rpush_all("l", ["a", "b"]) |> ignore
  db.set("dest", "x") |> ignore
  @test.assert_eq(
    match db.try_lmove("l", "dest", true, true) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.llen("l"), 2)

  @test.assert_eq(
    match db.try_sintercard(["str"], 0) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_smismember("str", ["a"]) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_zscan("str", 0, 10) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_zrandmember("str") {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_zunion(["str"]) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_msetnx(["a"], []) {
      Err(RedisError::WrongArgumentCount(_)) => true
      _ => false
    },
    true,
  )
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

test "zset_store_and_card_commands" {
  let db = Database::new()
  db.zadd("z1", 1.0, "a") |> ignore
  db.zadd("z1", 2.0, "b") |> ignore
  db.zadd("z1", 3.0, "c") |> ignore
  db.zadd("z2", 10.0, "b") |> ignore
  db.zadd("z2", 20.0, "c") |> ignore
  db.zadd("z2", 30.0, "d") |> ignore
  db.set("str", "v") |> ignore

  @test.assert_eq(db.zunionstore("u", ["z1", "z2"]), 4)
  @test.assert_eq(
    db.zrange_with_scores("u", 0, -1),
    [("a", 1.0), ("b", 12.0), ("c", 23.0), ("d", 30.0)],
  )
  @test.assert_eq(db.zinterstore("i", ["z1", "z2"]), 2)
  @test.assert_eq(db.zrange_with_scores("i", 0, -1), [("b", 12.0), ("c", 23.0)])
  @test.assert_eq(db.zdiffstore("d", ["z1", "z2"]), 1)
  @test.assert_eq(db.zrange_with_scores("d", 0, -1), [("a", 1.0)])
  @test.assert_eq(db.zrangestore("r", "z1", 0, 1), 2)
  @test.assert_eq(db.zrange_with_scores("r", 0, -1), [("a", 1.0), ("b", 2.0)])
  @test.assert_eq(db.zintercard(["z1", "z2"], 0), 2)
  @test.assert_eq(db.zintercard(["z1", "z2"], 1), 1)
  @test.assert_eq(db.zintercard(["z1", "str"], 0), -1)
}

test "zset_store_weights_aggregate" {
  let db = Database::new()
  db.zadd("z1", 1.0, "a") |> ignore
  db.zadd("z1", 2.0, "b") |> ignore
  db.zadd("z2", 10.0, "a") |> ignore
  db.zadd("z2", 20.0, "b") |> ignore

  @test.assert_eq(
    match db.try_zunionstore_with_options("u", ["z1", "z2"], Some([2.0, 3.0]), None) {
      Ok(count) => {
        @test.assert_eq(count, 2)
        @test.assert_eq(
          db.zrange_with_scores("u", 0, -1),
          [("a", 32.0), ("b", 64.0)],
        )
        true
      }
      Err(_) => false
    },
    true,
  )

  @test.assert_eq(
    match db.try_zunionstore_with_options("u2", ["z1", "z2"], None, Some("min")) {
      Ok(count) => {
        @test.assert_eq(count, 2)
        @test.assert_eq(
          db.zrange_with_scores("u2", 0, -1),
          [("a", 1.0), ("b", 2.0)],
        )
        true
      }
      Err(_) => false
    },
    true,
  )

  @test.assert_eq(
    match db.try_zunionstore_with_options("u3", ["z1", "z2"], None, Some("max")) {
      Ok(count) => {
        @test.assert_eq(count, 2)
        @test.assert_eq(
          db.zrange_with_scores("u3", 0, -1),
          [("a", 10.0), ("b", 20.0)],
        )
        true
      }
      Err(_) => false
    },
    true,
  )

  @test.assert_eq(
    match db.try_zinterstore_with_options("i", ["z1", "z2"], Some([2.0, 3.0]), None) {
      Ok(count) => {
        @test.assert_eq(count, 2)
        @test.assert_eq(
          db.zrange_with_scores("i", 0, -1),
          [("a", 32.0), ("b", 64.0)],
        )
        true
      }
      Err(_) => false
    },
    true,
  )

  @test.assert_eq(
    match db.try_zinterstore_with_options("i2", ["z1", "z2"], None, Some("min")) {
      Ok(count) => {
        @test.assert_eq(count, 2)
        @test.assert_eq(
          db.zrange_with_scores("i2", 0, -1),
          [("a", 1.0), ("b", 2.0)],
        )
        true
      }
      Err(_) => false
    },
    true,
  )

  @test.assert_eq(
    match db.try_zinterstore_with_options("i3", ["z1", "z2"], None, Some("max")) {
      Ok(count) => {
        @test.assert_eq(count, 2)
        @test.assert_eq(
          db.zrange_with_scores("i3", 0, -1),
          [("a", 10.0), ("b", 20.0)],
        )
        true
      }
      Err(_) => false
    },
    true,
  )
}

test "count_and_random_extensions" {
  let db = Database::new()
  db.sadd_all("s", ["a", "b", "c", "d"]) |> ignore
  @test.assert_eq(db.srandmember_count("s", 2).length(), 2)
  @test.assert_eq(db.srandmember_count("s", 10).length(), 4)
  @test.assert_eq(db.srandmember_count("s", -5).length(), 5)

  db.hmset("h", ["f1", "f2", "f3"], ["v1", "v2", "v3"]) |> ignore
  @test.assert_eq(db.hrandfield("h", 2).length(), 2)
  @test.assert_eq(db.hrandfield("h", 10).length(), 3)

  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 3.0, "c") |> ignore
  @test.assert_eq(db.zrandmember_count("z", 2).length(), 2)
  @test.assert_eq(db.zrandmember_count("z", -4).length(), 4)

  db.rpush_all("list", ["a", "b", "a"]) |> ignore
  @test.assert_eq(db.lpos("list", "a"), Some(0))
  @test.assert_eq(db.lpos("list", "b"), Some(1))
  @test.assert_eq(db.lpos("list", "missing"), None)
  @test.assert_eq(db.lpop_count("list", 2), ["a", "b"])
  @test.assert_eq(db.lrange("list", 0, -1), ["a"])
  @test.assert_eq(db.rpop_count("list", 1), ["a"])
  @test.assert_eq(db.exists("list"), false)

  db.zadd("zs", 1.0, "a") |> ignore
  db.zadd("zs", 2.0, "b") |> ignore
  db.zadd("zs", 3.0, "c") |> ignore
  db.zadd("zs", 4.0, "d") |> ignore
  @test.assert_eq(db.zpopmin_count("zs", 2), [("a", 1.0), ("b", 2.0)])
  @test.assert_eq(db.zpopmax_count("zs", 2), [("d", 4.0), ("c", 3.0)])
  @test.assert_eq(db.exists("zs"), false)
}

test "error_message_variants" {
  @test.assert_eq(RedisError::WrongType.message().length() > 0, true)
  @test.assert_eq(RedisError::NotInteger.message().length() > 0, true)
  @test.assert_eq(RedisError::NotFound.message().length() > 0, true)
  @test.assert_eq(RedisError::InvalidArgument.message().length() > 0, true)
  @test.assert_eq(RedisError::Overflow.message().length() > 0, true)
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

test "zset_error_paths_more" {
  let db = Database::new()
  db.set("str", "v") |> ignore
  db.zadd("z", 1.0, "a") |> ignore

  @test.assert_eq(
    match db.try_zscan("missing", 0, 10) {
      Ok((cursor, items)) => cursor == 0 && items.length() == 0
      Err(_) => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_zunion(["missing"]) {
      Ok(items) => items.length()
      Err(_) => -1
    },
    0,
  )
  @test.assert_eq(
    match db.try_zinter(["missing"]) {
      Ok(items) => items.length()
      Err(_) => -1
    },
    0,
  )
  @test.assert_eq(
    match db.try_zdiff(["missing"]) {
      Ok(items) => items.length()
      Err(_) => -1
    },
    0,
  )
  @test.assert_eq(
    match db.try_zrandmember("missing") {
      Err(RedisError::NotFound) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.zrandmember_count("missing", 2).length(), 0)
  @test.assert_eq(db.zrandmember_count("z", 0).length(), 0)
  @test.assert_eq(db.zpopmin_count("missing", 2).length(), 0)
  @test.assert_eq(db.zpopmax_count("z", 0).length(), 0)
  @test.assert_eq(db.zpopmin_count("str", 2).length(), 0)
  @test.assert_eq(db.zintercard([], 0), 0)
  @test.assert_eq(db.zintercard(["missing"], 0), 0)
  @test.assert_eq(
    match db.try_zunionstore("d", ["str"]) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.zunionstore("d", ["str"]), -1)
  @test.assert_eq(
    match db.try_zrangestore("d", "missing", 0, -1) {
      Ok(count) => count
      Err(_) => -1
    },
    0,
  )
  @test.assert_eq(db.zrangestore("d", "str", 0, -1), -1)
}

test "expire_and_scan_error_paths_more" {
  let db = Database::new()
  db.set("k", "v") |> ignore
  @test.assert_eq(db.pexpire_with_options("k", 1500L, false, false, false, false), true)
  @test.assert_eq(db.pttl("k"), 1500)
  @test.assert_eq(db.expire_with_options("missing", 10, false, false, false, false), false)

  @test.assert_eq(db.hrandfield("missing", 2).length(), 0)
  db.hset("h", "f", "v") |> ignore
  @test.assert_eq(db.hrandfield("h", 0).length(), 0)
  @test.assert_eq(
    match db.try_hscan("missing", 0, 10) {
      Ok((cursor, pairs)) => cursor == 0 && pairs.length() == 0
      Err(_) => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_sscan("missing", 0, 10) {
      Ok((cursor, values)) => cursor == 0 && values.length() == 0
      Err(_) => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_set_get("new", "v") {
      Ok(None) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_msetnx(["a", "b"], ["1"]) {
      Err(RedisError::WrongArgumentCount(_)) => true
      _ => false
    },
    true,
  )
}

test "info_avg_ttl" {
  let db = Database::new()
  db.set("a", "v") |> ignore
  db.set("b", "v") |> ignore
  db.expire("a", 100) |> ignore
  db.expire("b", 300) |> ignore
  let info = db.info()
  @test.assert_eq(info.contains("avg_ttl=0"), false)
  @test.assert_eq(info.contains("avg_ttl=200000"), true)
  db.advance_time(150000)
  let info2 = db.info()
  @test.assert_eq(info2.contains("expires=1"), true)
  @test.assert_eq(info2.contains("avg_ttl=150000"), true)
}

test "coverage_edge_cases" {
  let db = Database::new()
  db.zadd("z1", 1.0, "a") |> ignore
  db.zadd("z1", 2.0, "b") |> ignore
  @test.assert_eq(db.zrangebylex("z1", "bad", "+").length(), 0)

  db.hmset("h", ["a", "b"], ["1", "2"]) |> ignore
  @test.assert_eq(db.hrandfield("h", -3).length(), 3)
  db.sadd_all("s", ["a"]) |> ignore
  @test.assert_eq(db.srandmember_count("s", -3).length(), 3)

  db.rpush_all("l", ["a", "b", "c"]) |> ignore
  @test.assert_eq(db.lpop_count("l", 10).length(), 3)
  @test.assert_eq(db.exists("l"), false)

  db.zadd("zs", 1.0, "a") |> ignore
  @test.assert_eq(db.zpopmin_count("zs", 10).length(), 1)

  @test.assert_eq(
    match db.try_zunionstore("u2", ["z1"]) {
      Ok(count) => count
      Err(_) => -1
    },
    2,
  )
  @test.assert_eq(
    match db.try_zinterstore("i2", ["z1"]) {
      Ok(count) => count
      Err(_) => -1
    },
    2,
  )
  @test.assert_eq(
    match db.try_zdiffstore("d2", ["z1"]) {
      Ok(count) => count
      Err(_) => -1
    },
    2,
  )
  @test.assert_eq(
    match db.try_zrangestore("r2", "z1", 0, 0) {
      Ok(count) => count
      Err(_) => -1
    },
    1,
  )

  @test.assert_eq(
    match db.try_sadd_all("s2", ["a", "b"]) {
      Ok(count) => count
      Err(_) => -1
    },
    2,
  )
  @test.assert_eq(
    match db.try_srem_all("s2", ["a"]) {
      Ok(count) => count
      Err(_) => -1
    },
    1,
  )
  @test.assert_eq(
    match db.try_rpush_all("l2", ["a", "b"]) {
      Ok(length) => length
      Err(_) => -1
    },
    2,
  )
  @test.assert_eq(
    match db.try_lpush_all("l2", ["x"]) {
      Ok(length) => length
      Err(_) => -1
    },
    3,
  )
  @test.assert_eq(
    match db.try_hmset("h2", ["f"], ["v"]) {
      Ok(_) => 1
      Err(_) => -1
    },
    1,
  )

  db.set("k", "v") |> ignore
  @test.assert_eq(db.pexpire_with_options("k", 1000L, true, false, false, false), true)
  @test.assert_eq(db.pexpire_with_options("k", 2000L, false, false, false, true), false)
  @test.assert_eq(
    match db.set_with_options("ex0", "v", false, false, false, Some(0), None, None, None, false) {
      Err(RedisError::InvalidArgument) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.set_with_options("px0", "v", false, false, false, None, Some(0L), None, None, false) {
      Err(RedisError::InvalidArgument) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(
    match db.set_with_options("keep2", "v", false, false, false, Some(10), None, None, None, true) {
      Err(RedisError::SyntaxError) => true
      _ => false
    },
    true,
  )
}

test "transaction_multi_exec" {
  let db = Database::new()
  
  ignore(db.multi())
  db.set("key1", "value1")
  db.set("key2", "value2")
  ignore(db.incr("counter"))
  @test.assert_eq(db.exec(), "OK")
  
  @test.assert_eq(db.get("key1"), Some("value1"))
  @test.assert_eq(db.get("key2"), Some("value2"))
  @test.assert_eq(db.get("counter"), Some("1"))
}

test "transaction_discard" {
  let db = Database::new()
  
  ignore(db.multi())
  db.set("key1", "value1")
  db.set("key2", "value2")
  @test.assert_eq(db.discard(), "OK")
  
  @test.assert_eq(db.get("key1"), None)
  @test.assert_eq(db.get("key2"), None)
}

test "transaction_nested_multi" {
  let db = Database::new()
  
  ignore(db.multi())
  db.set("key1", "value1")
  @test.assert_eq(db.multi(), "ERR MULTI calls can not be nested")
  @test.assert_eq(db.exec(), "OK")
  
  @test.assert_eq(db.get("key1"), Some("value1"))
}

test "transaction_exec_without_multi" {
  let db = Database::new()
  
  @test.assert_eq(db.exec(), "ERR EXEC without MULTI")
}

test "transaction_discard_without_multi" {
  let db = Database::new()
  
  @test.assert_eq(db.discard(), "ERR DISCARD without MULTI")
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

test "transaction_list_operations" {
  let db = Database::new()
  
  ignore(db.multi())
  ignore(db.lpush("list_key", "value1"))
  ignore(db.rpush("list_key", "value2"))
  @test.assert_eq(db.exec(), "OK")
  
  @test.assert_eq(db.lrange("list_key", 0, -1), ["value1", "value2"])
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

test "transaction_zset_operations" {
  let db = Database::new()
  
  ignore(db.multi())
  ignore(db.zadd("zset_key", 10.0, "member1"))
  ignore(db.zadd("zset_key", 20.0, "member2"))
  ignore(db.zrem("zset_key", "member1"))
  @test.assert_eq(db.exec(), "OK")
  
  @test.assert_eq(db.zrange("zset_key", 0, -1), ["member2"])
}

test "transaction_del" {
  let db = Database::new()
  
  db.set("key1", "value1")
  db.set("key2", "value2")
  
  ignore(db.multi())
  ignore(db.del("key1"))
  @test.assert_eq(db.exec(), "OK")
  
  @test.assert_eq(db.get("key1"), None)
  @test.assert_eq(db.get("key2"), Some("value2"))
}