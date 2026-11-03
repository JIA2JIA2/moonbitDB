test "cross_type_operations" {
  let db = Database::new()
  
  db.set("string_key", "hello")
  db.hset("hash_key", "field", "value") |> ignore
  db.lpush("list_key", "item") |> ignore
  db.sadd("set_key", "member") |> ignore
  db.zadd("zset_key", 100.0, "zmember") |> ignore
  
  debug_inspect(db.type_of("string_key"), content="\"string\"")
  debug_inspect(db.type_of("hash_key"), content="\"hash\"")
  debug_inspect(db.type_of("list_key"), content="\"list\"")
  debug_inspect(db.type_of("set_key"), content="\"set\"")
  debug_inspect(db.type_of("zset_key"), content="\"zset\"")
  debug_inspect(db.type_of("nonexistent"), content="\"none\"")
  
  let all_keys = db.keys()
  debug_inspect(all_keys.length(), content="5")
  
  debug_inspect(db.exists("string_key"), content="true")
  db.del("string_key") |> ignore
  debug_inspect(db.exists("string_key"), content="false")
  
  let remaining = db.keys()
  debug_inspect(remaining.length(), content="4")
}

test "string_operations" {
  let db = Database::new()
  
  db.set("name", "Alice")
  debug_inspect(db.get("name"), content="Some(\"Alice\")")
  
  db.set("age", "30")
  debug_inspect(db.get("age"), content="Some(\"30\")")
  
  db.append("name", " Smith") |> ignore
  debug_inspect(db.get("name"), content="Some(\"Alice Smith\")")
  
  debug_inspect(db.strlen("name"), content="11")
}

test "string_boundary_cases" {
  let db = Database::new()
  
  debug_inspect(db.get("nonexistent"), content="None")
  debug_inspect(db.strlen("nonexistent"), content="0")
  
  db.set("empty", "")
  debug_inspect(db.get("empty"), content="Some(\"\")")
  debug_inspect(db.strlen("empty"), content="0")
  
  db.set("key1", "value1")
  db.set("key1", "value2")
  debug_inspect(db.get("key1"), content="Some(\"value2\")")
  
  db.append("non_string_key", "test") |> ignore
  debug_inspect(db.get("non_string_key"), content="Some(\"test\")")
  
  debug_inspect(db.exists("key1"), content="true")
  debug_inspect(db.exists("nonexistent"), content="false")
  
  db.del("key1") |> ignore
  debug_inspect(db.exists("key1"), content="false")
  debug_inspect(db.del("nonexistent"), content="false")
}

test "incr_decr_operations" {
  let db = Database::new()
  
  debug_inspect(db.incr("counter"), content="Some(1)")
  debug_inspect(db.incr("counter"), content="Some(2)")
  debug_inspect(db.decr("counter"), content="Some(1)")
  debug_inspect(db.decr("counter"), content="Some(0)")
  debug_inspect(db.decr("counter"), content="Some(-1)")
  
  db.set("invalid", "abc")
  debug_inspect(db.incr("invalid"), content="None")
  
  debug_inspect(db.get("counter"), content="Some(\"-1\")")
}

test "keys_pattern_operations" {
  let db = Database::new()
  
  db.set("user:1", "Alice")
  db.set("user:2", "Bob")
  db.set("user:3", "Charlie")
  db.set("product:100", "Laptop")
  db.set("config", "default")
  
  let users = db.keys_pattern("user:*")
  debug_inspect(users.length(), content="3")
  
  let user1 = db.keys_pattern("user:1")
  debug_inspect(user1.length(), content="1")
  
  let any = db.keys_pattern("*")
  debug_inspect(any.length(), content="5")
  
  let product = db.keys_pattern("product:*")
  debug_inspect(product.length(), content="1")
  
  let user_any = db.keys_pattern("user:?")
  debug_inspect(user_any.length(), content="3")
  
  let no_match = db.keys_pattern("nonexistent:*")
  debug_inspect(no_match.length(), content="0")
}

test "hash_operations" {
  let db = Database::new()
  
  db.hset("user:1", "name", "Bob") |> ignore
  db.hset("user:1", "email", "bob@example.com") |> ignore
  
  debug_inspect(db.hget("user:1", "name"), content="Some(\"Bob\")")
  debug_inspect(db.hget("user:1", "email"), content="Some(\"bob@example.com\")")
  
  debug_inspect(db.hlen("user:1"), content="2")
  
  db.hdel("user:1", "email") |> ignore
  debug_inspect(db.hget("user:1", "email"), content="None")
}

test "hash_boundary_cases" {
  let db = Database::new()
  
  debug_inspect(db.hget("nonexistent", "field"), content="None")
  debug_inspect(db.hlen("nonexistent"), content="0")
  debug_inspect(db.hdel("nonexistent", "field"), content="false")
  
  let empty_hash = db.hgetall("nonexistent")
  debug_inspect(empty_hash.length(), content="0")
  
  db.hset("hash1", "field1", "value1") |> ignore
  db.hset("hash1", "field2", "value2") |> ignore
  
  let hash_all = db.hgetall("hash1")
  debug_inspect(hash_all.length(), content="2")
  
  db.hset("hash1", "field1", "updated") |> ignore
  debug_inspect(db.hget("hash1", "field1"), content="Some(\"updated\")")
  debug_inspect(db.hlen("hash1"), content="2")
  
  db.hdel("hash1", "field1") |> ignore
  debug_inspect(db.hlen("hash1"), content="1")
  
  debug_inspect(db.hdel("hash1", "nonexistent_field"), content="false")
  
  db.hset("hash2", "", "empty_field") |> ignore
  debug_inspect(db.hget("hash2", ""), content="Some(\"empty_field\")")
}

test "list_operations" {
  let db = Database::new()
  
  db.lpush("mylist", "a") |> ignore
  db.lpush("mylist", "b") |> ignore
  db.rpush("mylist", "c") |> ignore
  
  debug_inspect(db.llen("mylist"), content="3")
  debug_inspect(db.lpop("mylist"), content="Some(\"b\")")
  debug_inspect(db.rpop("mylist"), content="Some(\"c\")")
  debug_inspect(db.llen("mylist"), content="1")
}

test "list_boundary_cases" {
  let db = Database::new()
  
  debug_inspect(db.llen("nonexistent"), content="0")
  debug_inspect(db.lpop("nonexistent"), content="None")
  debug_inspect(db.rpop("nonexistent"), content="None")
  
  db.lpush("empty_list", "first") |> ignore
  debug_inspect(db.llen("empty_list"), content="1")
  debug_inspect(db.lpop("empty_list"), content="Some(\"first\")")
  debug_inspect(db.llen("empty_list"), content="0")
  debug_inspect(db.lpop("empty_list"), content="None")
  
  db.rpush("single_list", "item") |> ignore
  debug_inspect(db.rpop("single_list"), content="Some(\"item\")")
  debug_inspect(db.llen("single_list"), content="0")
  
  db.lpush("multi", "x") |> ignore
  db.lpush("multi", "y") |> ignore
  db.lpush("multi", "z") |> ignore
  debug_inspect(db.llen("multi"), content="3")
  
  debug_inspect(db.lpop("multi"), content="Some(\"z\")")
  debug_inspect(db.lpop("multi"), content="Some(\"y\")")
  debug_inspect(db.lpop("multi"), content="Some(\"x\")")
  debug_inspect(db.lpop("multi"), content="None")
  
  db.rpush("reverse", "a") |> ignore
  db.rpush("reverse", "b") |> ignore
  db.rpush("reverse", "c") |> ignore
  debug_inspect(db.rpop("reverse"), content="Some(\"c\")")
  debug_inspect(db.rpop("reverse"), content="Some(\"b\")")
  debug_inspect(db.rpop("reverse"), content="Some(\"a\")")
}

test "set_operations" {
  let db = Database::new()
  
  db.sadd("myset", "apple") |> ignore
  db.sadd("myset", "banana") |> ignore
  db.sadd("myset", "apple") |> ignore
  
  debug_inspect(db.scard("myset"), content="2")
  debug_inspect(db.sismember("myset", "apple"), content="true")
  debug_inspect(db.sismember("myset", "orange"), content="false")
  
  db.srem("myset", "banana") |> ignore
  debug_inspect(db.scard("myset"), content="1")
}

test "set_boundary_cases" {
  let db = Database::new()
  
  debug_inspect(db.scard("nonexistent"), content="0")
  debug_inspect(db.sismember("nonexistent", "item"), content="false")
  debug_inspect(db.srem("nonexistent", "item"), content="false")
  
  let empty_members = db.smembers("nonexistent")
  debug_inspect(empty_members.length(), content="0")
  
  debug_inspect(db.sadd("set1", "a"), content="true")
  debug_inspect(db.sadd("set1", "b"), content="true")
  debug_inspect(db.sadd("set1", "a"), content="false")
  
  debug_inspect(db.scard("set1"), content="2")
  
  debug_inspect(db.srem("set1", "c"), content="false")
  debug_inspect(db.srem("set1", "a"), content="true")
  
  debug_inspect(db.scard("set1"), content="1")
  debug_inspect(db.sismember("set1", "a"), content="false")
  debug_inspect(db.sismember("set1", "b"), content="true")
  
  let members = db.smembers("set1")
  debug_inspect(members.length(), content="1")
  
  db.srem("set1", "b") |> ignore
  debug_inspect(db.scard("set1"), content="0")
  
  db.sadd("empty_item", "") |> ignore
  debug_inspect(db.sismember("empty_item", ""), content="true")
}

test "sorted_set_operations" {
  let db = Database::new()
  
  db.zadd("scores", 100.0, "Alice") |> ignore
  db.zadd("scores", 90.0, "Bob") |> ignore
  db.zadd("scores", 95.0, "Charlie") |> ignore
  
  debug_inspect(db.zcard("scores"), content="3")
  debug_inspect(db.zscore("scores", "Alice"), content="Some(100)")
  
  let top_two = db.zrange("scores", 1, 2)
  debug_inspect(top_two.length(), content="2")
  
  db.zrem("scores", "Bob") |> ignore
  debug_inspect(db.zcard("scores"), content="2")
}

test "sorted_set_boundary_cases" {
  let db = Database::new()
  
  debug_inspect(db.zcard("nonexistent"), content="0")
  debug_inspect(db.zscore("nonexistent", "member"), content="None")
  debug_inspect(db.zrem("nonexistent", "member"), content="false")
  
  let empty_range = db.zrange("nonexistent", 0, -1)
  debug_inspect(empty_range.length(), content="0")
  
  debug_inspect(db.zadd("zset1", 10.0, "a"), content="true")
  debug_inspect(db.zadd("zset1", 20.0, "b"), content="true")
  debug_inspect(db.zadd("zset1", 10.0, "a"), content="false")
  
  debug_inspect(db.zcard("zset1"), content="2")
  debug_inspect(db.zscore("zset1", "a"), content="Some(10)")
  
  debug_inspect(db.zadd("zset1", 15.0, "a"), content="false")
  debug_inspect(db.zscore("zset1", "a"), content="Some(15)")
  
  let all_range = db.zrange("zset1", 0, -1)
  debug_inspect(all_range.length(), content="2")
  
  let first = db.zrange("zset1", 0, 0)
  debug_inspect(first.length(), content="1")
  
  let last = db.zrange("zset1", -1, -1)
  debug_inspect(last.length(), content="1")
  
  debug_inspect(db.zrem("zset1", "c"), content="false")
  debug_inspect(db.zrem("zset1", "a"), content="true")
  
  debug_inspect(db.zcard("zset1"), content="1")
  debug_inspect(db.zscore("zset1", "a"), content="None")
  
  db.zadd("neg_scores", -5.0, "low") |> ignore
  db.zadd("neg_scores", -10.0, "lower") |> ignore
  db.zadd("neg_scores", 0.0, "zero") |> ignore
  
  let sorted_neg = db.zrange("neg_scores", 0, -1)
  debug_inspect(sorted_neg.length(), content="3")
}

test "expire_operations" {
  let db = Database::new()
  
  db.set("key1", "value1")
  debug_inspect(db.exists("key1"), content="true")
  debug_inspect(db.ttl("key1"), content="-1")
  debug_inspect(db.pttl("key1"), content="-1")
  
  debug_inspect(db.expire("key1", 60), content="true")
  debug_inspect(db.ttl("key1") >= 0, content="true")
  debug_inspect(db.pttl("key1") >= 0, content="true")
  
  debug_inspect(db.expire("nonexistent", 60), content="false")
  
  debug_inspect(db.persist("key1"), content="true")
  debug_inspect(db.ttl("key1"), content="-1")
  debug_inspect(db.pttl("key1"), content="-1")
  
  debug_inspect(db.persist("key1"), content="false")
}

test "expire_after_timeout" {
  let db = Database::new()
  
  db.set("temp_key", "temp_value")
  debug_inspect(db.get("temp_key"), content="Some(\"temp_value\")")
  
  debug_inspect(db.pexpire("temp_key", 10), content="true")
  
  db.advance_time(20)
  
  debug_inspect(db.get("temp_key"), content="None")
  debug_inspect(db.exists("temp_key"), content="false")
  debug_inspect(db.ttl("temp_key"), content="-2")
  debug_inspect(db.pttl("temp_key"), content="-2")
}

test "expire_type_check" {
  let db = Database::new()
  
  db.set("string_key", "value")
  db.hset("hash_key", "field", "value") |> ignore
  db.lpush("list_key", "item") |> ignore
  db.sadd("set_key", "member") |> ignore
  db.zadd("zset_key", 100.0, "member") |> ignore
  
  debug_inspect(db.expire("string_key", 60), content="true")
  debug_inspect(db.expire("hash_key", 60), content="true")
  debug_inspect(db.expire("list_key", 60), content="true")
  debug_inspect(db.expire("set_key", 60), content="true")
  debug_inspect(db.expire("zset_key", 60), content="true")
  
  debug_inspect(db.ttl("string_key") >= 0, content="true")
  debug_inspect(db.ttl("hash_key") >= 0, content="true")
  debug_inspect(db.ttl("list_key") >= 0, content="true")
  debug_inspect(db.ttl("set_key") >= 0, content="true")
  debug_inspect(db.ttl("zset_key") >= 0, content="true")
}

test "expire_refresh_on_write" {
  let db = Database::new()
  
  db.set("key", "value")
  db.expire("key", 1) |> ignore
  debug_inspect(db.ttl("key") >= 0, content="true")
  
  db.set("key", "new_value")
  debug_inspect(db.ttl("key"), content="-1")
  
  db.set("key2", "100")
  db.expire("key2", 1) |> ignore
  debug_inspect(db.ttl("key2") >= 0, content="true")
  
  db.incr("key2") |> ignore
  debug_inspect(db.ttl("key2") >= 0, content="true")
}

test "mset_mget_operations" {
  let db = Database::new()
  
  db.mset(["key1", "key2", "key3"], ["value1", "value2", "value3"])
  
  debug_inspect(db.get("key1"), content="Some(\"value1\")")
  debug_inspect(db.get("key2"), content="Some(\"value2\")")
  debug_inspect(db.get("key3"), content="Some(\"value3\")")
  
  let values = db.mget(["key1", "key2", "key3"])
  debug_inspect(values.length(), content="3")
  debug_inspect(values[0], content="Some(\"value1\")")
  debug_inspect(values[1], content="Some(\"value2\")")
  debug_inspect(values[2], content="Some(\"value3\")")
  
  let mixed = db.mget(["key1", "nonexistent", "key3"])
  debug_inspect(mixed.length(), content="3")
  debug_inspect(mixed[0], content="Some(\"value1\")")
  debug_inspect(mixed[1], content="None")
  debug_inspect(mixed[2], content="Some(\"value3\")")
  
  db.mset(["key2", "key4"], ["updated", "value4"])
  debug_inspect(db.get("key2"), content="Some(\"updated\")")
  debug_inspect(db.get("key4"), content="Some(\"value4\")")
}

test "mdel_operations" {
  let db = Database::new()
  
  db.mset(["a", "b", "c", "d"], ["1", "2", "3", "4"])
  
  debug_inspect(db.exists("a"), content="true")
  debug_inspect(db.exists("b"), content="true")
  debug_inspect(db.exists("c"), content="true")
  debug_inspect(db.exists("d"), content="true")
  
  let deleted = db.mdel(["a", "c", "nonexistent"])
  debug_inspect(deleted, content="2")
  
  debug_inspect(db.exists("a"), content="false")
  debug_inspect(db.exists("b"), content="true")
  debug_inspect(db.exists("c"), content="false")
  debug_inspect(db.exists("d"), content="true")
  
  let deleted_all = db.mdel(["b", "d"])
  debug_inspect(deleted_all, content="2")
  
  let remaining = db.keys()
  debug_inspect(remaining.length(), content="0")
}

test "hmset_hmget_operations" {
  let db = Database::new()
  
  db.hmset("user:1", ["name", "email", "age"], ["Alice", "alice@example.com", "30"])
  
  debug_inspect(db.hget("user:1", "name"), content="Some(\"Alice\")")
  debug_inspect(db.hget("user:1", "email"), content="Some(\"alice@example.com\")")
  debug_inspect(db.hget("user:1", "age"), content="Some(\"30\")")
  
  let fields = db.hmget("user:1", ["name", "email", "age"])
  debug_inspect(fields.length(), content="3")
  debug_inspect(fields[0], content="Some(\"Alice\")")
  debug_inspect(fields[1], content="Some(\"alice@example.com\")")
  debug_inspect(fields[2], content="Some(\"30\")")
  
  let mixed = db.hmget("user:1", ["name", "nonexistent", "age"])
  debug_inspect(mixed.length(), content="3")
  debug_inspect(mixed[0], content="Some(\"Alice\")")
  debug_inspect(mixed[1], content="None")
  debug_inspect(mixed[2], content="Some(\"30\")")
  
  db.hmset("user:1", ["age", "city"], ["31", "Beijing"])
  debug_inspect(db.hget("user:1", "age"), content="Some(\"31\")")
  debug_inspect(db.hget("user:1", "city"), content="Some(\"Beijing\")")
}

test "batch_with_expire" {
  let db = Database::new()
  
  db.mset(["temp1", "temp2"], ["value1", "value2"])
  db.expire("temp1", 1) |> ignore
  
  let before_expire = db.mget(["temp1", "temp2"])
  debug_inspect(before_expire.length(), content="2")
  debug_inspect(before_expire[0], content="Some(\"value1\")")
  debug_inspect(before_expire[1], content="Some(\"value2\")")
  
  db.advance_time(2000)
  
  let after_expire = db.mget(["temp1", "temp2"])
  debug_inspect(after_expire.length(), content="2")
  debug_inspect(after_expire[0], content="None")
  debug_inspect(after_expire[1], content="Some(\"value2\")")
  
  let deleted = db.mdel(["temp1", "temp2"])
  debug_inspect(deleted, content="1")
  
  db.hmset("temp_hash", ["a", "b"], ["1", "2"])
  db.expire("temp_hash", 1) |> ignore
  
  db.advance_time(2000)
  
  let hash_fields = db.hmget("temp_hash", ["a", "b"])
  debug_inspect(hash_fields.length(), content="2")
  debug_inspect(hash_fields[0], content="None")
  debug_inspect(hash_fields[1], content="None")
}

test "lrange_operations" {
  let db = Database::new()
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "b") |> ignore
  db.rpush("list1", "c") |> ignore
  db.rpush("list1", "d") |> ignore
  db.rpush("list1", "e") |> ignore
  
  let range1 = db.lrange("list1", 0, -1)
  debug_inspect(range1.length(), content="5")
  
  let range2 = db.lrange("list1", 0, 2)
  debug_inspect(range2.length(), content="3")
  
  let range3 = db.lrange("list1", -3, -1)
  debug_inspect(range3.length(), content="3")
  
  let range4 = db.lrange("list1", 10, 20)
  debug_inspect(range4.length(), content="0")
  
  let range5 = db.lrange("nonexistent", 0, -1)
  debug_inspect(range5.length(), content="0")
}

test "lindex_lset_operations" {
  let db = Database::new()
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "b") |> ignore
  db.rpush("list1", "c") |> ignore
  
  debug_inspect(db.lindex("list1", 0), content="Some(\"a\")")
  debug_inspect(db.lindex("list1", 1), content="Some(\"b\")")
  debug_inspect(db.lindex("list1", -1), content="Some(\"c\")")
  debug_inspect(db.lindex("list1", 10), content="None")
  debug_inspect(db.lindex("nonexistent", 0), content="None")
  
  debug_inspect(db.lset("list1", 1, "new_b"), content="true")
  debug_inspect(db.lindex("list1", 1), content="Some(\"new_b\")")
  
  debug_inspect(db.lset("list1", -1, "new_c"), content="true")
  debug_inspect(db.lindex("list1", -1), content="Some(\"new_c\")")
  
  debug_inspect(db.lset("list1", 10, "invalid"), content="false")
  debug_inspect(db.lset("nonexistent", 0, "value"), content="false")
}

test "lrem_operations" {
  let db = Database::new()
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "b") |> ignore
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "c") |> ignore
  db.rpush("list1", "a") |> ignore
  
  let removed1 = db.lrem("list1", 2, "a")
  debug_inspect(removed1, content="2")
  debug_inspect(db.llen("list1"), content="3")
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "a") |> ignore
  
  let removed2 = db.lrem("list1", -2, "a")
  debug_inspect(removed2, content="2")
  debug_inspect(db.llen("list1"), content="3")
  
  let removed3 = db.lrem("list1", 0, "a")
  debug_inspect(removed3, content="1")
  debug_inspect(db.llen("list1"), content="2")
  
  let removed4 = db.lrem("nonexistent", 1, "a")
  debug_inspect(removed4, content="0")
}

test "linsert_operations" {
  let db = Database::new()
  
  db.rpush("list1", "a") |> ignore
  db.rpush("list1", "b") |> ignore
  db.rpush("list1", "c") |> ignore
  
  let len1 = db.linsert("list1", true, "b", "new")
  debug_inspect(len1, content="4")
  debug_inspect(db.lindex("list1", 1), content="Some(\"new\")")
  
  let len2 = db.linsert("list1", false, "b", "after")
  debug_inspect(len2, content="5")
  debug_inspect(db.lindex("list1", 3), content="Some(\"after\")")
  
  let len3 = db.linsert("list1", true, "nonexistent", "value")
  debug_inspect(len3, content="-1")
  
  let len4 = db.linsert("nonexistent", true, "a", "value")
  debug_inspect(len4, content="-1")
}

test "lpushx_rpushx_operations" {
  let db = Database::new()
  
  let len1 = db.lpushx("nonexistent", "a")
  debug_inspect(len1, content="0")
  
  let len2 = db.rpushx("nonexistent", "a")
  debug_inspect(len2, content="0")
  
  db.rpush("list1", "a") |> ignore
  
  let len3 = db.lpushx("list1", "b")
  debug_inspect(len3, content="2")
  debug_inspect(db.lindex("list1", 0), content="Some(\"b\")")
  
  let len4 = db.rpushx("list1", "c")
  debug_inspect(len4, content="3")
  debug_inspect(db.lindex("list1", 2), content="Some(\"c\")")
}

test "rpoplpush_operations" {
  let db = Database::new()
  
  db.rpush("source", "a") |> ignore
  db.rpush("source", "b") |> ignore
  db.rpush("source", "c") |> ignore
  
  let val1 = db.rpoplpush("source", "dest")
  debug_inspect(val1, content="Some(\"c\")")
  debug_inspect(db.llen("source"), content="2")
  debug_inspect(db.llen("dest"), content="1")
  debug_inspect(db.lindex("dest", 0), content="Some(\"c\")")
  
  let val2 = db.rpoplpush("source", "dest")
  debug_inspect(val2, content="Some(\"b\")")
  debug_inspect(db.llen("source"), content="1")
  debug_inspect(db.llen("dest"), content="2")
  debug_inspect(db.lindex("dest", 0), content="Some(\"b\")")
  
  let val3 = db.rpoplpush("empty", "dest")
  debug_inspect(val3, content="None")
  
  db.rpush("rotate", "1") |> ignore
  db.rpush("rotate", "2") |> ignore
  db.rpush("rotate", "3") |> ignore
  db.rpush("rotate", "4") |> ignore
  
  let val4 = db.rpoplpush("rotate", "rotate")
  debug_inspect(val4, content="Some(\"4\")")
  debug_inspect(db.lindex("rotate", 0), content="Some(\"4\")")
  debug_inspect(db.lindex("rotate", 1), content="Some(\"1\")")
  debug_inspect(db.lindex("rotate", 2), content="Some(\"2\")")
  debug_inspect(db.lindex("rotate", 3), content="Some(\"3\")")
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
  debug_inspect(inter1.length(), content="2")
  debug_inspect(inter1.contains("b"), content="true")
  debug_inspect(inter1.contains("c"), content="true")
  
  let inter2 = db.sinter(["set1", "set2", "set3"])
  debug_inspect(inter2.length(), content="1")
  debug_inspect(inter2.contains("c"), content="true")
  
  let inter3 = db.sinter(["set1", "nonexistent"])
  debug_inspect(inter3.length(), content="0")
  
  let inter4 = db.sinter([])
  debug_inspect(inter4.length(), content="0")
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
  debug_inspect(count, content="2")
  debug_inspect(db.scard("dest"), content="2")
  debug_inspect(db.sismember("dest", "b"), content="true")
  debug_inspect(db.sismember("dest", "c"), content="true")
  debug_inspect(db.sismember("dest", "a"), content="false")
}

test "sunion_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  
  db.sadd("set2", "b") |> ignore
  db.sadd("set2", "c") |> ignore
  
  db.sadd("set3", "d") |> ignore
  
  let union1 = db.sunion(["set1", "set2"])
  debug_inspect(union1.length(), content="3")
  debug_inspect(union1.contains("a"), content="true")
  debug_inspect(union1.contains("b"), content="true")
  debug_inspect(union1.contains("c"), content="true")
  
  let union2 = db.sunion(["set1", "set2", "set3"])
  debug_inspect(union2.length(), content="4")
  
  let union3 = db.sunion(["set1", "nonexistent"])
  debug_inspect(union3.length(), content="2")
  
  let union4 = db.sunion([])
  debug_inspect(union4.length(), content="0")
}

test "sunionstore_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  
  db.sadd("set2", "c") |> ignore
  db.sadd("set2", "d") |> ignore
  
  let count = db.sunionstore("dest", ["set1", "set2"])
  debug_inspect(count, content="4")
  debug_inspect(db.scard("dest"), content="4")
  debug_inspect(db.sismember("dest", "a"), content="true")
  debug_inspect(db.sismember("dest", "d"), content="true")
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
  debug_inspect(diff1.length(), content="2")
  debug_inspect(diff1.contains("a"), content="true")
  debug_inspect(diff1.contains("d"), content="true")
  debug_inspect(diff1.contains("b"), content="false")
  debug_inspect(diff1.contains("c"), content="false")
  
  let diff2 = db.sdiff(["set1", "set2", "set3"])
  debug_inspect(diff2.length(), content="1")
  debug_inspect(diff2.contains("a"), content="true")
  
  let diff3 = db.sdiff(["set1", "nonexistent"])
  debug_inspect(diff3.length(), content="4")
  
  let diff4 = db.sdiff([])
  debug_inspect(diff4.length(), content="0")
}

test "sdiffstore_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  
  db.sadd("set2", "b") |> ignore
  
  let count = db.sdiffstore("dest", ["set1", "set2"])
  debug_inspect(count, content="2")
  debug_inspect(db.scard("dest"), content="2")
  debug_inspect(db.sismember("dest", "a"), content="true")
  debug_inspect(db.sismember("dest", "c"), content="true")
  debug_inspect(db.sismember("dest", "b"), content="false")
}

test "smove_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  
  db.sadd("set2", "x") |> ignore
  db.sadd("set2", "y") |> ignore
  
  debug_inspect(db.smove("set1", "set2", "b"), content="true")
  debug_inspect(db.sismember("set1", "b"), content="false")
  debug_inspect(db.sismember("set2", "b"), content="true")
  debug_inspect(db.scard("set1"), content="2")
  debug_inspect(db.scard("set2"), content="3")
  
  debug_inspect(db.smove("set1", "set2", "nonexistent"), content="false")
  
  debug_inspect(db.smove("nonexistent", "set2", "a"), content="false")
  
  debug_inspect(db.smove("set1", "newset", "a"), content="true")
  debug_inspect(db.sismember("newset", "a"), content="true")
}

test "spop_operations" {
  let db = Database::new()
  
  db.sadd("set1", "a") |> ignore
  db.sadd("set1", "b") |> ignore
  db.sadd("set1", "c") |> ignore
  
  let popped = db.spop("set1")
  debug_inspect(popped is None, content="false")
  debug_inspect(db.scard("set1"), content="2")
  
  db.spop("set1") |> ignore
  db.spop("set1") |> ignore
  
  debug_inspect(db.scard("set1"), content="0")
  
  let empty_pop = db.spop("set1")
  debug_inspect(empty_pop is None, content="true")
  
  let nonexistent_pop = db.spop("nonexistent")
  debug_inspect(nonexistent_pop is None, content="true")
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
  debug_inspect(inter.length(), content="0")
  
  let union = db.sunion(["set1", "set2"])
  debug_inspect(union.length(), content="3")
  
  let diff = db.sdiff(["set1", "set2"])
  debug_inspect(diff.length(), content="3")
}

test "zrangebyscore_zcount_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  db.zadd("zset1", 5.0, "e") |> ignore
  
  let range1 = db.zrangebyscore("zset1", 2.0, 4.0)
  debug_inspect(range1.length(), content="3")
  debug_inspect(range1[0], content="\"b\"")
  debug_inspect(range1[1], content="\"c\"")
  debug_inspect(range1[2], content="\"d\"")
  
  let range2 = db.zrangebyscore("zset1", 0.0, 100.0)
  debug_inspect(range2.length(), content="5")
  
  let range3 = db.zrangebyscore("zset1", 10.0, 20.0)
  debug_inspect(range3.length(), content="0")
  
  let count1 = db.zcount("zset1", 2.0, 4.0)
  debug_inspect(count1, content="3")
  
  let count2 = db.zcount("zset1", 0.0, 100.0)
  debug_inspect(count2, content="5")
  
  let count3 = db.zcount("nonexistent", 0.0, 100.0)
  debug_inspect(count3, content="0")
}

test "zrevrange_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  
  let rev1 = db.zrevrange("zset1", 0, -1)
  debug_inspect(rev1.length(), content="4")
  debug_inspect(rev1[0], content="\"d\"")
  debug_inspect(rev1[1], content="\"c\"")
  debug_inspect(rev1[2], content="\"b\"")
  debug_inspect(rev1[3], content="\"a\"")
  
  let rev2 = db.zrevrange("zset1", 0, 1)
  debug_inspect(rev2.length(), content="2")
  debug_inspect(rev2[0], content="\"d\"")
  debug_inspect(rev2[1], content="\"c\"")
  
  let rev3 = db.zrevrange("zset1", -2, -1)
  debug_inspect(rev3.length(), content="2")
  debug_inspect(rev3[0], content="\"b\"")
  debug_inspect(rev3[1], content="\"a\"")
}

test "zrevrangebyscore_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  
  let rev1 = db.zrevrangebyscore("zset1", 4.0, 2.0)
  debug_inspect(rev1.length(), content="3")
  debug_inspect(rev1[0], content="\"d\"")
  debug_inspect(rev1[1], content="\"c\"")
  debug_inspect(rev1[2], content="\"b\"")
  
  let rev2 = db.zrevrangebyscore("zset1", 3.0, 1.0)
  debug_inspect(rev2.length(), content="3")
  debug_inspect(rev2[0], content="\"c\"")
  
  let rev3 = db.zrevrangebyscore("zset1", 10.0, 5.0)
  debug_inspect(rev3.length(), content="0")
}

test "zrank_zrevrank_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 10.0, "a") |> ignore
  db.zadd("zset1", 20.0, "b") |> ignore
  db.zadd("zset1", 30.0, "c") |> ignore
  db.zadd("zset1", 40.0, "d") |> ignore
  
  debug_inspect(db.zrank("zset1", "a"), content="Some(0)")
  debug_inspect(db.zrank("zset1", "b"), content="Some(1)")
  debug_inspect(db.zrank("zset1", "d"), content="Some(3)")
  debug_inspect(db.zrank("zset1", "nonexistent"), content="None")
  debug_inspect(db.zrank("nonexistent", "a"), content="None")
  
  debug_inspect(db.zrevrank("zset1", "a"), content="Some(3)")
  debug_inspect(db.zrevrank("zset1", "b"), content="Some(2)")
  debug_inspect(db.zrevrank("zset1", "d"), content="Some(0)")
  debug_inspect(db.zrevrank("zset1", "nonexistent"), content="None")
}

test "zincrby_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 10.0, "a") |> ignore
  
  let score1 = db.zincrby("zset1", 5.0, "a")
  debug_inspect(score1, content="15")
  
  debug_inspect(db.zscore("zset1", "a"), content="Some(15)")
  
  let score2 = db.zincrby("zset1", -3.0, "a")
  debug_inspect(score2, content="12")
  
  let score3 = db.zincrby("zset1", 100.0, "new_member")
  debug_inspect(score3, content="100")
  debug_inspect(db.zscore("zset1", "new_member"), content="Some(100)")
  
  let score4 = db.zincrby("nonexistent", 5.0, "x")
  debug_inspect(score4, content="5")
}

test "zremrangebyrank_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  db.zadd("zset1", 5.0, "e") |> ignore
  
  let removed1 = db.zremrangebyrank("zset1", 0, 1)
  debug_inspect(removed1, content="2")
  debug_inspect(db.zcard("zset1"), content="3")
  debug_inspect(db.zrange("zset1", 0, -1)[0], content="\"c\"")
  
  let removed2 = db.zremrangebyrank("zset1", -2, -1)
  debug_inspect(removed2, content="2")
  debug_inspect(db.zcard("zset1"), content="1")
  
  let removed3 = db.zremrangebyrank("nonexistent", 0, 10)
  debug_inspect(removed3, content="0")
}

test "zremrangebyscore_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  db.zadd("zset1", 5.0, "e") |> ignore
  
  let removed1 = db.zremrangebyscore("zset1", 2.0, 4.0)
  debug_inspect(removed1, content="3")
  debug_inspect(db.zcard("zset1"), content="2")
  debug_inspect(db.zscore("zset1", "b"), content="None")
  debug_inspect(db.zscore("zset1", "a"), content="Some(1)")
  
  let removed2 = db.zremrangebyscore("zset1", 0.0, 100.0)
  debug_inspect(removed2, content="2")
  debug_inspect(db.zcard("zset1"), content="0")
  
  let removed3 = db.zremrangebyscore("nonexistent", 0.0, 100.0)
  debug_inspect(removed3, content="0")
}

test "zpopmin_zpopmax_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 10.0, "a") |> ignore
  db.zadd("zset1", 20.0, "b") |> ignore
  db.zadd("zset1", 30.0, "c") |> ignore
  db.zadd("zset1", 40.0, "d") |> ignore
  
  let min1 = db.zpopmin("zset1")
  debug_inspect(min1 is None, content="false")
  
  let min_val = match min1 {
    Some(v) => v
    None => ("", 0.0)
  }
  debug_inspect(min_val.0, content="\"a\"")
  debug_inspect(db.zcard("zset1"), content="3")
  
  let max1 = db.zpopmax("zset1")
  debug_inspect(max1 is None, content="false")
  
  let max_val = match max1 {
    Some(v) => v
    None => ("", 0.0)
  }
  debug_inspect(max_val.0, content="\"d\"")
  debug_inspect(db.zcard("zset1"), content="2")
  
  db.zpopmin("zset1") |> ignore
  db.zpopmin("zset1") |> ignore
  
  debug_inspect(db.zcard("zset1"), content="0")
  
  let empty_min = db.zpopmin("zset1")
  debug_inspect(empty_min is None, content="true")
  
  let empty_max = db.zpopmax("zset1")
  debug_inspect(empty_max is None, content="true")
  
  let nonexistent = db.zpopmin("nonexistent")
  debug_inspect(nonexistent is None, content="true")
}

test "dbsize_flushdb_operations" {
  let db = Database::new()
  
  debug_inspect(db.dbsize(), content="0")
  
  db.set("key1", "value1")
  db.set("key2", "value2")
  db.set("key3", "value3")
  
  debug_inspect(db.dbsize(), content="3")
  
  db.del("key1") |> ignore
  debug_inspect(db.dbsize(), content="2")
  
  db.flushdb()
  debug_inspect(db.dbsize(), content="0")
  debug_inspect(db.exists("key2"), content="false")
  debug_inspect(db.exists("key3"), content="false")
  
  db.set("a", "1") |> ignore
  db.flushall()
  debug_inspect(db.dbsize(), content="0")
}

test "randomkey_operations" {
  let db = Database::new()
  
  debug_inspect(db.randomkey() is None, content="true")
  
  db.set("key1", "value1")
  db.set("key2", "value2")
  db.set("key3", "value3")
  
  let key = db.randomkey()
  debug_inspect(key is None, content="false")
  
  let key_val = match key {
    Some(k) => k
    None => ""
  }
  debug_inspect(db.exists(key_val), content="true")
  
  db.del("key1") |> ignore
  db.del("key2") |> ignore
  db.del("key3") |> ignore
  
  debug_inspect(db.randomkey() is None, content="true")
}

test "rename_operations" {
  let db = Database::new()
  
  db.set("old_key", "value1")
  
  debug_inspect(db.rename("old_key", "new_key"), content="true")
  debug_inspect(db.get("new_key"), content="Some(\"value1\")")
  debug_inspect(db.exists("old_key"), content="false")
  
  debug_inspect(db.rename("nonexistent", "target"), content="false")
  
  db.set("key_a", "a") |> ignore
  db.set("key_b", "b") |> ignore
  debug_inspect(db.rename("key_a", "key_b"), content="true")
  debug_inspect(db.get("key_b"), content="Some(\"a\")")
  debug_inspect(db.exists("key_a"), content="false")
  
  db.set("src", "src_val") |> ignore
  db.expire("src", 100) |> ignore
  db.rename("src", "dst") |> ignore
  debug_inspect(db.ttl("dst") >= 0, content="true")
  debug_inspect(db.exists("src"), content="false")
}

test "renamenx_operations" {
  let db = Database::new()
  
  db.set("key1", "value1") |> ignore
  db.set("key2", "value2") |> ignore
  
  debug_inspect(db.renamenx("key1", "key3"), content="true")
  debug_inspect(db.get("key3"), content="Some(\"value1\")")
  debug_inspect(db.exists("key1"), content="false")
  
  db.set("key4", "value4") |> ignore
  debug_inspect(db.renamenx("key2", "key4"), content="false")
  debug_inspect(db.get("key4"), content="Some(\"value4\")")
  debug_inspect(db.get("key2"), content="Some(\"value2\")")
  
  debug_inspect(db.renamenx("nonexistent", "key5"), content="false")
}

test "ping_echo_operations" {
  let db = Database::new()
  
  debug_inspect(Database::ping(), content="\"PONG\"")
  
  debug_inspect(Database::echo(db, "Hello World"), content="\"Hello World\"")
  debug_inspect(Database::echo(db, ""), content="\"\"")
  debug_inspect(Database::echo(db, "123"), content="\"123\"")
}

test "info_operations" {
  let db = Database::new()
  
  db.set("key1", "value1") |> ignore
  db.set("key2", "value2") |> ignore
  db.expire("key1", 100) |> ignore
  
  let info = db.info()
  debug_inspect(info.contains("moonbit_db_version"), content="true")
  debug_inspect(info.contains("keys=2"), content="true")
  debug_inspect(info.contains("expires=1"), content="true")
  
  db.advance_time(200000)
  let info2 = db.info()
  debug_inspect(info2.contains("uptime_in_ms:200000"), content="true")
  debug_inspect(info2.contains("keys=1"), content="true")
}

test "time_operations" {
  let db = Database::new()
  
  let t1 = db.time()
  debug_inspect(t1.0, content="0")
  debug_inspect(t1.1, content="0")
  
  db.set_time(5000)
  let t2 = db.time()
  debug_inspect(t2.0, content="5")
  debug_inspect(t2.1, content="0")
  
  db.set_time(5678)
  let t3 = db.time()
  debug_inspect(t3.0, content="5")
  debug_inspect(t3.1, content="678000")
}

test "command_operations" {
  let commands = Database::command()
  debug_inspect(commands.length() > 50, content="true")
  
  debug_inspect(commands.contains("SET"), content="true")
  debug_inspect(commands.contains("GET"), content="true")
  debug_inspect(commands.contains("HSET"), content="true")
  debug_inspect(commands.contains("LPUSH"), content="true")
  debug_inspect(commands.contains("SADD"), content="true")
  debug_inspect(commands.contains("ZADD"), content="true")
  debug_inspect(commands.contains("DBSIZE"), content="true")
  debug_inspect(commands.contains("FLUSHDB"), content="true")
  debug_inspect(commands.contains("PING"), content="true")
}

test "object_encoding_operations" {
  let db = Database::new()
  
  db.set("str_key", "hello")
  debug_inspect(db.object_encoding("str_key"), content="Some(\"embstr\")")
  
  db.hset("hash_key", "field", "value") |> ignore
  debug_inspect(db.object_encoding("hash_key"), content="Some(\"listpack\")")
  
  db.rpush("list_key", "item") |> ignore
  debug_inspect(db.object_encoding("list_key"), content="Some(\"listpack\")")
  
  db.sadd("set_key", "member") |> ignore
  debug_inspect(db.object_encoding("set_key"), content="Some(\"listpack\")")
  
  db.zadd("zset_key", 1.0, "member") |> ignore
  debug_inspect(db.object_encoding("zset_key"), content="Some(\"listpack\")")
  
  debug_inspect(db.object_encoding("nonexistent"), content="None")
}

test "string_extended_edge_cases" {
  let db = Database::new()
  
  db.set("counter", "10")
  debug_inspect(db.incr("counter"), content="Some(11)")
  debug_inspect(db.incr("counter"), content="Some(12)")
  debug_inspect(db.decr("counter"), content="Some(11)")
  
  db.set("zero", "0")
  debug_inspect(db.decr("zero"), content="Some(-1)")
  debug_inspect(db.decr("zero"), content="Some(-2)")
  
  debug_inspect(db.incr("new_counter"), content="Some(1)")
  
  db.set("not_number", "abc")
  debug_inspect(db.incr("not_number"), content="None")
  debug_inspect(db.decr("not_number"), content="None")
  
  db.set("append_key", "Hello")
  db.append("append_key", " World") |> ignore
  debug_inspect(db.get("append_key"), content="Some(\"Hello World\")")
  debug_inspect(db.strlen("append_key"), content="11")
  
  db.append("new_append", "First") |> ignore
  debug_inspect(db.get("new_append"), content="Some(\"First\")")
  
  db.set("unicode", "你好")
  debug_inspect(db.strlen("unicode"), content="2")
}

test "hash_extended_edge_cases" {
  let db = Database::new()
  
  db.hset("user:1", "name", "Alice") |> ignore
  db.hset("user:1", "age", "30") |> ignore
  db.hset("user:1", "email", "alice@test.com") |> ignore
  
  debug_inspect(db.hlen("user:1"), content="3")
  
  db.hset("user:1", "age", "31") |> ignore
  debug_inspect(db.hget("user:1", "age"), content="Some(\"31\")")
  debug_inspect(db.hlen("user:1"), content="3")
  
  debug_inspect(db.hdel("user:1", "email"), content="true")
  debug_inspect(db.hdel("user:1", "email"), content="false")
  debug_inspect(db.hlen("user:1"), content="2")
  
  debug_inspect(db.hget("user:1", "name"), content="Some(\"Alice\")")
  debug_inspect(db.hget("user:1", "nonexistent"), content="None")
  
  let all = db.hgetall("user:1")
  debug_inspect(all.length(), content="2")
  
  debug_inspect(db.hlen("nonexistent"), content="0")
  let empty_all = db.hgetall("nonexistent")
  debug_inspect(empty_all.length(), content="0")
}

test "list_extended_edge_cases" {
  let db = Database::new()
  
  db.lpush("mix", "b") |> ignore
  db.rpush("mix", "c") |> ignore
  db.lpush("mix", "a") |> ignore
  db.rpush("mix", "d") |> ignore
  
  debug_inspect(db.lindex("mix", 0), content="Some(\"a\")")
  debug_inspect(db.lindex("mix", 1), content="Some(\"b\")")
  debug_inspect(db.lindex("mix", 2), content="Some(\"c\")")
  debug_inspect(db.lindex("mix", 3), content="Some(\"d\")")
  
  let rev_range = db.lrange("mix", 2, 0)
  debug_inspect(rev_range.length(), content="0")
  
  let full = db.lrange("mix", 0, -1)
  debug_inspect(full.length(), content="4")
  
  let single = db.lrange("mix", 1, 1)
  debug_inspect(single.length(), content="1")
  debug_inspect(single[0], content="\"b\"")
  
  debug_inspect(db.lindex("mix", -4), content="Some(\"a\")")
  debug_inspect(db.lindex("mix", -1), content="Some(\"d\")")
  debug_inspect(db.lindex("mix", 100), content="None")
  debug_inspect(db.lindex("mix", -100), content="None")
  
  db.lset("mix", 0, "A") |> ignore
  debug_inspect(db.lindex("mix", 0), content="Some(\"A\")")
  
  debug_inspect(db.lrem("mix", 0, "nonexistent"), content="0")
}

test "set_extended_edge_cases" {
  let db = Database::new()
  
  debug_inspect(db.sadd("s1", "a"), content="true")
  debug_inspect(db.sadd("s1", "a"), content="false")
  debug_inspect(db.sadd("s1", "b"), content="true")
  
  debug_inspect(db.srem("s1", "nonexistent"), content="false")
  debug_inspect(db.srem("s1", "a"), content="true")
  debug_inspect(db.srem("s1", "a"), content="false")
  
  debug_inspect(db.scard("nonexistent"), content="0")
  debug_inspect(db.sismember("nonexistent", "x"), content="false")
  
  db.sadd("empty_set", "temp") |> ignore
  db.srem("empty_set", "temp") |> ignore
  debug_inspect(db.scard("empty_set"), content="0")
  
  db.sadd("set_a", "1") |> ignore
  db.sadd("set_a", "2") |> ignore
  db.sadd("set_a", "3") |> ignore
  db.sadd("set_b", "3") |> ignore
  db.sadd("set_b", "4") |> ignore
  db.sadd("set_b", "5") |> ignore
  
  let inter = db.sinter(["set_a", "set_b"])
  debug_inspect(inter.length(), content="1")
  debug_inspect(inter.contains("3"), content="true")
  
  let self_inter = db.sinter(["set_a"])
  debug_inspect(self_inter.length(), content="3")
  
  let self_union = db.sunion(["set_a"])
  debug_inspect(self_union.length(), content="3")
  
  let self_diff = db.sdiff(["set_a"])
  debug_inspect(self_diff.length(), content="3")
  
  let no_overlap = db.sinter(["set_a", "set_b", "nonexistent"])
  debug_inspect(no_overlap.length(), content="0")
}

test "zset_extended_edge_cases" {
  let db = Database::new()
  
  db.zadd("zset", 1.0, "a") |> ignore
  debug_inspect(db.zadd("zset", 5.0, "a"), content="false")
  debug_inspect(db.zscore("zset", "a"), content="Some(5)")
  
  debug_inspect(db.zscore("zset", "nonexistent"), content="None")
  debug_inspect(db.zscore("nonexistent", "a"), content="None")
  
  debug_inspect(db.zcard("nonexistent"), content="0")
  
  let single = db.zrange("zset", 0, -1)
  debug_inspect(single.length(), content="1")
  
  db.zadd("zset", 3.0, "b") |> ignore
  db.zadd("zset", 3.0, "c") |> ignore
  
  let score_range = db.zrangebyscore("zset", 3.0, 3.0)
  debug_inspect(score_range.length(), content="2")
  
  let count_exact = db.zcount("zset", 3.0, 3.0)
  debug_inspect(count_exact, content="2")
  
  debug_inspect(db.zrank("zset", "a"), content="Some(2)")
  debug_inspect(db.zrank("zset", "b"), content="Some(0)")
  
  db.zincrby("zset", -10.0, "a") |> ignore
  debug_inspect(db.zscore("zset", "a"), content="Some(-5)")
  
  let rev = db.zrange("zset", 0, -1)
  debug_inspect(rev[0], content="\"a\"")
  
  debug_inspect(db.zrem("zset", "nonexistent"), content="false")
  debug_inspect(db.zrem("nonexistent", "a"), content="false")
  
  debug_inspect(db.zremrangebyrank("zset", 100, 200), content="0")
  debug_inspect(db.zremrangebyscore("zset", 100.0, 200.0), content="0")
}

test "expire_extended_edge_cases" {
  let db = Database::new()
  
  debug_inspect(db.expire("nonexistent", 100), content="false")
  
  debug_inspect(db.pexpire("nonexistent", 100000), content="false")
  
  db.set("key1", "value1") |> ignore
  debug_inspect(db.persist("key1"), content="false")
  
  db.expire("key1", 100) |> ignore
  debug_inspect(db.persist("key1"), content="true")
  debug_inspect(db.ttl("key1"), content="-1")
  debug_inspect(db.persist("key1"), content="false")
  
  db.set("key2", "value2") |> ignore
  db.pexpire("key2", 500) |> ignore
  debug_inspect(db.pttl("key2") >= 0, content="true")
  
  db.advance_time(300)
  debug_inspect(db.exists("key2"), content="true")
  
  db.advance_time(300)
  debug_inspect(db.exists("key2"), content="false")
  
  db.set("key3", "value3") |> ignore
  db.expire("key3", 100) |> ignore
  db.set("key3", "new_value") |> ignore
  debug_inspect(db.ttl("key3"), content="-1")
  
  db.set("key4", "value4") |> ignore
  db.expire("key4", 100) |> ignore
  db.del("key4") |> ignore
  debug_inspect(db.exists("key4"), content="false")
  debug_inspect(db.ttl("key4"), content="-2")
}

test "cross_type_safety" {
  let db = Database::new()
  
  db.set("str_key", "hello") |> ignore
  debug_inspect(db.lpush("str_key", "item"), content="-1")
  debug_inspect(db.type_of("str_key"), content="\"string\"")
  debug_inspect(db.get("str_key"), content="Some(\"hello\")")
  
  db.lpush("list_key", "item1") |> ignore
  debug_inspect(db.hset("list_key", "field", "value"), content="-1")
  debug_inspect(db.type_of("list_key"), content="\"list\"")
  
  db.sadd("set_key", "member") |> ignore
  debug_inspect(db.zadd("set_key", 1.0, "zmember"), content="false")
  debug_inspect(db.type_of("set_key"), content="\"set\"")
  
  db.hset("hash_key", "f", "v") |> ignore
  debug_inspect(db.incr("hash_key"), content="None")
  debug_inspect(db.type_of("hash_key"), content="\"hash\"")
  debug_inspect(db.append("hash_key", "x"), content="-1")
  debug_inspect(db.type_of("hash_key"), content="\"hash\"")
  
  db.hset("hash_key", "f", "v") |> ignore
  db.set("hash_key", "string_value") |> ignore
  debug_inspect(db.type_of("hash_key"), content="\"string\"")
  
  debug_inspect(db.type_of("nonexistent"), content="\"none\"")
}

test "hmset_wrong_type_noop" {
  let db = Database::new()
  db.set("k", "v") |> ignore
  db.hmset("k", ["a"], ["1"])
  debug_inspect(db.type_of("k"), content="\"string\"")
  debug_inspect(db.get("k"), content="Some(\"v\")")
}

test "getset_wrong_type_noop" {
  let db = Database::new()
  db.hset("h", "f", "v") |> ignore
  debug_inspect(db.getset("h", "new"), content="None")
  debug_inspect(db.type_of("h"), content="\"hash\"")
  debug_inspect(db.hget("h", "f"), content="Some(\"v\")")
}

test "large_ttl_no_overflow" {
  let db = Database::new()
  db.set("k", "v") |> ignore
  debug_inspect(db.expire("k", 2592000), content="true")
  debug_inspect(db.ttl("k"), content="2592000")
  debug_inspect(db.pttl("k"), content="2592000000")
  db.advance_time(2592000000)
  debug_inspect(db.exists("k"), content="false")

  db.set_time(0)
  db.set("k2", "v") |> ignore
  debug_inspect(db.expireat("k2", 1700000000), content="true")
  debug_inspect(db.pttl("k2"), content="1700000000000")
}

test "incr_supports_int64" {
  let db = Database::new()
  db.set("n", "2147483648") |> ignore
  match db.incr("n") {
    Some(v) => debug_inspect("\{v}", content="\"2147483649\"")
    None => debug_inspect("none", content="2147483649")
  }
}

test "hgetall_returns_copy" {
  let db = Database::new()
  db.hset("h", "f", "v") |> ignore
  let snapshot = db.hgetall("h")
  snapshot.set("f", "changed")
  snapshot.set("new", "x")
  debug_inspect(db.hget("h", "f"), content="Some(\"v\")")
  debug_inspect(db.hexists("h", "new"), content="false")
}

test "zset_cache_invalidated_on_delete_and_overwrite" {
  let db = Database::new()
  db.zadd("z", 1.0, "a") |> ignore
  db.zrange("z", 0, -1) |> ignore
  db.del("z") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  let r = db.zrange("z", 0, -1)
  debug_inspect(r.length(), content="1")
  debug_inspect(r[0], content="\"b\"")

  db.zadd("z2", 1.0, "a") |> ignore
  db.zrange("z2", 0, -1) |> ignore
  db.set("z2", "str") |> ignore
  db.del("z2") |> ignore
  db.zadd("z2", 5.0, "c") |> ignore
  debug_inspect(db.zrange("z2", 0, -1)[0], content="\"c\"")
}

test "rpoplpush_same_key_preserves_ttl" {
  let db = Database::new()
  db.rpush("q", "only") |> ignore
  db.expire("q", 100) |> ignore
  debug_inspect(db.rpoplpush("q", "q"), content="Some(\"only\")")
  debug_inspect(db.ttl("q") >= 0, content="true")
}

test "zset_score_tie_lexicographic" {
  let db = Database::new()
  db.zadd("z", 1.0, "b") |> ignore
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 1.0, "c") |> ignore
  db.zadd("z", 0.0, "z") |> ignore
  let asc = db.zrange("z", 0, -1)
  debug_inspect(asc.length(), content="4")
  debug_inspect(asc[0], content="\"z\"")
  debug_inspect(asc[1], content="\"a\"")
  debug_inspect(asc[2], content="\"b\"")
  debug_inspect(asc[3], content="\"c\"")
  let desc = db.zrevrange("z", 0, 2)
  debug_inspect(desc[0], content="\"c\"")
  debug_inspect(desc[1], content="\"b\"")
  debug_inspect(desc[2], content="\"a\"")
}

test "zset_range_by_score_bounds" {
  let db = Database::new()
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 2.0, "c") |> ignore
  db.zadd("z", 3.0, "d") |> ignore
  let mid = db.zrangebyscore("z", 2.0, 2.0)
  debug_inspect(mid.length(), content="2")
  debug_inspect(mid[0], content="\"b\"")
  debug_inspect(mid[1], content="\"c\"")
  debug_inspect(db.zcount("z", 2.0, 2.0), content="2")
  debug_inspect(db.zcount("z", 1.5, 2.5), content="2")
  let rev = db.zrevrangebyscore("z", 2.0, 2.0)
  debug_inspect(rev[0], content="\"c\"")
  debug_inspect(rev[1], content="\"b\"")
  debug_inspect(db.zrangebyscore("z", 4.0, 5.0).length(), content="0")
  debug_inspect(db.zcount("z", 5.0, 1.0), content="0")
}

test "lrem_counts_exceeding_occurrences" {
  let db = Database::new()
  for item in ["a", "a", "b", "a"] {
    db.rpush("l", item) |> ignore
  }
  debug_inspect(db.lrem("l", 5, "a"), content="3")
  debug_inspect(db.llen("l"), content="1")
  debug_inspect(db.lindex("l", 0), content="Some(\"b\")")

  for item in ["a", "b", "a"] {
    db.rpush("l2", item) |> ignore
  }
  debug_inspect(db.lrem("l2", -5, "a"), content="2")
  debug_inspect(db.lindex("l2", 0), content="Some(\"b\")")
}

test "pattern_matching_no_exponential_backtracking" {
  debug_inspect(match_pattern("a*a*a*a*a*b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), content="false")
  debug_inspect(match_pattern("*a*b*", "xxaxxb"), content="true")
  debug_inspect(match_pattern("a?c", "abc"), content="true")
  debug_inspect(match_pattern("a?c", "ac"), content="false")
  debug_inspect(match_pattern("*", ""), content="true")
}

test "batch_extended_edge_cases" {
  let db = Database::new()
  
  db.mset(["k1", "k2"], ["v1", "v2"]) |> ignore
  debug_inspect(db.get("k1"), content="Some(\"v1\")")
  debug_inspect(db.get("k2"), content="Some(\"v2\")")
  
  db.mset(["k1", "k2"], ["new_v1", "new_v2"]) |> ignore
  debug_inspect(db.get("k1"), content="Some(\"new_v1\")")
  
  db.set("k3", "existing") |> ignore
  let result = db.mget(["k1", "k2", "k3", "nonexistent"])
  debug_inspect(result.length(), content="4")
  debug_inspect(result[0], content="Some(\"new_v1\")")
  debug_inspect(result[1], content="Some(\"new_v2\")")
  debug_inspect(result[2], content="Some(\"existing\")")
  debug_inspect(result[3], content="None")
  
  db.mset(["k4", "k5", "k6"], ["v4"]) |> ignore
  debug_inspect(db.exists("k4"), content="true")
  debug_inspect(db.exists("k5"), content="false")
  debug_inspect(db.exists("k6"), content="false")
  
  let deleted = db.mdel(["k1", "k2", "k3", "nonexistent"])
  debug_inspect(deleted, content="3")
  
  db.hmset("hash_batch", ["f1", "f2"], ["v1", "v2"]) |> ignore
  db.hmset("hash_batch", ["f3", "f4"], ["v3", "v4"]) |> ignore
  debug_inspect(db.hlen("hash_batch"), content="4")
  
  db.hmset("hash_batch", ["f1"], ["updated"]) |> ignore
  debug_inspect(db.hget("hash_batch", "f1"), content="Some(\"updated\")")
  debug_inspect(db.hlen("hash_batch"), content="4")
}

test "server_extended_edge_cases" {
  let db = Database::new()
  
  db.set("key1", "value1") |> ignore
  db.set("key2", "value2") |> ignore
  db.expire("key1", 1) |> ignore
  
  db.advance_time(2000)
  
  debug_inspect(db.dbsize(), content="1")
  
  db.set("key3", "value3") |> ignore
  debug_inspect(db.rename("key3", "key3"), content="true")
  debug_inspect(db.get("key3"), content="Some(\"value3\")")
  
  db.set("key4", "value4") |> ignore
  db.expire("key4", 100) |> ignore
  db.rename("key4", "key5") |> ignore
  debug_inspect(db.ttl("key5") >= 0, content="true")
  
  db.set("key6", "value6") |> ignore
  db.set("key7", "value7") |> ignore
  debug_inspect(db.renamenx("key6", "key6"), content="false")
  
  let all_keys = db.keys()
  let _ = db.randomkey()
  debug_inspect(all_keys.length() > 0, content="true")
  
  db.flushdb()
  debug_inspect(db.dbsize(), content="0")
  debug_inspect(db.randomkey() is None, content="true")
}

test "deque_internal_operations" {
  let db = Database::new()
  
  db.lpush("dq", "c") |> ignore
  db.lpush("dq", "b") |> ignore
  db.lpush("dq", "a") |> ignore
  db.rpush("dq", "d") |> ignore
  db.rpush("dq", "e") |> ignore
  
  debug_inspect(db.lindex("dq", 0), content="Some(\"a\")")
  debug_inspect(db.lindex("dq", 4), content="Some(\"e\")")
  debug_inspect(db.lindex("dq", 2), content="Some(\"c\")")
  
  db.lset("dq", 2, "C") |> ignore
  debug_inspect(db.lindex("dq", 2), content="Some(\"C\")")
  
  db.lrem("dq", 1, "C") |> ignore
  debug_inspect(db.llen("dq"), content="4")
  
  db.linsert("dq", true, "b", "B") |> ignore
  debug_inspect(db.lindex("dq", 1), content="Some(\"B\")")
  
  db.linsert("dq", false, "d", "D") |> ignore
  debug_inspect(db.lindex("dq", 4), content="Some(\"D\")")
  debug_inspect(db.llen("dq"), content="6")
  
  let val = db.rpoplpush("dq", "dq2")
  debug_inspect(val, content="Some(\"e\")")
  debug_inspect(db.llen("dq"), content="5")
  debug_inspect(db.llen("dq2"), content="1")
}

test "zset_score_edge_cases" {
  let db = Database::new()
  
  db.zadd("zset", 0.0, "zero") |> ignore
  db.zadd("zset", -1.0, "negative") |> ignore
  db.zadd("zset", 3.14, "pi") |> ignore
  db.zadd("zset", -0.5, "neg_half") |> ignore
  
  let sorted = db.zrange("zset", 0, -1)
  debug_inspect(sorted[0], content="\"negative\"")
  debug_inspect(sorted[1], content="\"neg_half\"")
  debug_inspect(sorted[2], content="\"zero\"")
  debug_inspect(sorted[3], content="\"pi\"")
  
  let neg_range = db.zrangebyscore("zset", -1.0, 0.0)
  debug_inspect(neg_range.length(), content="3")
  
  let pos_count = db.zcount("zset", 0.0, 100.0)
  debug_inspect(pos_count, content="2")
  
  db.zincrby("zset", 1.5, "zero") |> ignore
  debug_inspect(db.zscore("zset", "zero"), content="Some(1.5)")
  
  let min_pop = db.zpopmin("zset")
  let min_val = match min_pop {
    Some(v) => v
    None => ("", 0.0)
  }
  debug_inspect(min_val.0, content="\"negative\"")
  
  let max_pop = db.zpopmax("zset")
  let max_val = match max_pop {
    Some(v) => v
    None => ("", 0.0)
  }
  debug_inspect(max_val.0, content="\"pi\"")
}

test "keys_pattern_expired_key" {
  let db = Database::new()
  db.set("user:1", "Alice")
  db.set("user:2", "Bob")
  db.set("admin:1", "Root")
  db.pexpire("user:1", 100) |> ignore
  db.advance_time(200)

  let users = db.keys_pattern("user:*")
  debug_inspect(users.length(), content="1")

  let all = db.keys_pattern("*")
  debug_inspect(all.length(), content="2")
}

test "rpoplpush_expired_destination" {
  let db = Database::new()
  db.rpush("src", "a") |> ignore
  db.rpush("src", "b") |> ignore
  db.rpush("dest", "old1") |> ignore
  db.pexpire("dest", 100) |> ignore
  db.advance_time(200)

  let val = db.rpoplpush("src", "dest")
  debug_inspect(val, content="Some(\"b\")")
  debug_inspect(db.llen("dest"), content="1")
  debug_inspect(db.lindex("dest", 0), content="Some(\"b\")")
}

test "renamenx_expired_newkey" {
  let db = Database::new()
  db.set("old_key", "value")
  db.set("new_key", "old_value")
  db.pexpire("new_key", 100) |> ignore
  db.advance_time(200)

  debug_inspect(db.renamenx("old_key", "new_key"), content="true")
  debug_inspect(db.get("new_key"), content="Some(\"value\")")
}

test "incr_decr_ttl_preserved" {
  let db = Database::new()
  db.set("counter", "10")
  db.expire("counter", 60) |> ignore
  let ttl_before = db.ttl("counter")

  db.incr("counter") |> ignore
  let ttl_after_incr = db.ttl("counter")
  debug_inspect(ttl_after_incr >= 0, content="true")
  debug_inspect(ttl_after_incr <= ttl_before + 1, content="true")

  db.decr("counter") |> ignore
  let ttl_after_decr = db.ttl("counter")
  debug_inspect(ttl_after_decr >= 0, content="true")

  db.set("counter2", "5")
  db.incr("counter2") |> ignore
  debug_inspect(db.ttl("counter2"), content="-1")
}

test "int_to_string_min_value" {
  let db = Database::new()
  db.set("counter", "-9223372036854775807")
  db.decr("counter") |> ignore
  let val = db.get("counter")
  debug_inspect(val is None, content="false")
}

test "parse_int_sign_only" {
  let db = Database::new()
  db.set("sign_minus", "-")
  db.set("sign_plus", "+")
  debug_inspect(db.incr("sign_minus"), content="None")
  debug_inspect(db.incr("sign_plus"), content="None")
}

test "append_cross_type_safety" {
  let db = Database::new()
  db.hset("myhash", "field", "value") |> ignore

  debug_inspect(db.append("myhash", "test"), content="-1")
  debug_inspect(db.type_of("myhash"), content="\"hash\"")
  debug_inspect(db.hget("myhash", "field"), content="Some(\"value\")")

  db.set("mystr", "hello")
  debug_inspect(db.append("mystr", " world"), content="11")
  debug_inspect(db.get("mystr"), content="Some(\"hello world\")")
}

test "ttl_ceil_rounding" {
  let db = Database::new()
  db.set("key1", "value1")
  db.pexpire("key1", 1500) |> ignore

  debug_inspect(db.ttl("key1"), content="2")

  debug_inspect(db.pttl("key1"), content="1500")

  db.set("key2", "value2")
  db.pexpire("key2", 1000) |> ignore
  debug_inspect(db.ttl("key2"), content="1")

  db.set("key3", "value3")
  db.pexpire("key3", 1) |> ignore
  debug_inspect(db.ttl("key3"), content="1")
}

test "pttl_precision" {
  let db = Database::new()
  db.set("key1", "value1")
  db.pexpire("key1", 1500) |> ignore

  debug_inspect(db.pttl("key1"), content="1500")
}

test "hexists_hkeys_hvals" {
  let db = Database::new()
  debug_inspect(db.hexists("nonexistent", "field"), content="false")
  debug_inspect(db.hkeys("nonexistent").length(), content="0")
  debug_inspect(db.hvals("nonexistent").length(), content="0")

  db.hset("h", "a", "1") |> ignore
  db.hset("h", "b", "2") |> ignore
  debug_inspect(db.hexists("h", "a"), content="true")
  debug_inspect(db.hexists("h", "c"), content="false")
  debug_inspect(db.hexists("nonexistent", "a"), content="false")

  let keys = db.hkeys("h")
  debug_inspect(keys.length(), content="2")
  debug_inspect(keys.contains("a"), content="true")
  debug_inspect(keys.contains("b"), content="true")

  let vals = db.hvals("h")
  debug_inspect(vals.length(), content="2")
  debug_inspect(vals.contains("1"), content="true")
  debug_inspect(vals.contains("2"), content="true")

  db.hdel("h", "a") |> ignore
  debug_inspect(db.hexists("h", "a"), content="false")

  db.pexpire("h", 100) |> ignore
  db.advance_time(200)
  debug_inspect(db.hexists("h", "b"), content="false")
  debug_inspect(db.hkeys("h").length(), content="0")
}

test "hincrby_hstrlen" {
  let db = Database::new()
  debug_inspect(db.hincrby("h", "count", 5), content="Some(5)")
  debug_inspect(db.hincrby("h", "count", 3), content="Some(8)")
  debug_inspect(db.hincrby("h", "count", -10), content="Some(-2)")
  debug_inspect(db.hget("h", "count"), content="Some(\"-2\")")

  db.hset("h", "text", "abc") |> ignore
  debug_inspect(db.hincrby("h", "text", 1), content="None")
  debug_inspect(db.hget("h", "text"), content="Some(\"abc\")")

  debug_inspect(db.hstrlen("h", "text"), content="3")
  debug_inspect(db.hstrlen("h", "count"), content="2")
  debug_inspect(db.hstrlen("h", "nonexistent"), content="0")
  debug_inspect(db.hstrlen("nonexistent", "f"), content="0")
}

test "hset_returns_new_field_count" {
  let db = Database::new()
  debug_inspect(db.hset("h", "a", "1"), content="1")
  debug_inspect(db.hset("h", "a", "2"), content="0")
  debug_inspect(db.hset("h", "b", "3"), content="1")
  debug_inspect(db.hlen("h"), content="2")
}

test "string_setnx_setex_getdel_getset" {
  let db = Database::new()
  debug_inspect(db.setnx("k", "v1"), content="true")
  debug_inspect(db.setnx("k", "v2"), content="false")
  debug_inspect(db.get("k"), content="Some(\"v1\")")

  debug_inspect(db.setex("t", 100, "tv"), content="true")
  debug_inspect(db.ttl("t") >= 0, content="true")
  debug_inspect(db.get("t"), content="Some(\"tv\")")
  debug_inspect(db.setex("t", 0, "x"), content="false")

  db.set("gd", "val") |> ignore
  debug_inspect(db.getdel("gd"), content="Some(\"val\")")
  debug_inspect(db.exists("gd"), content="false")
  debug_inspect(db.getdel("gd"), content="None")

  db.set("gs", "old") |> ignore
  debug_inspect(db.getset("gs", "new"), content="Some(\"old\")")
  debug_inspect(db.get("gs"), content="Some(\"new\")")
  debug_inspect(db.getset("fresh", "n"), content="None")

  db.set("gs_ttl", "v") |> ignore
  db.expire("gs_ttl", 100) |> ignore
  db.getset("gs_ttl", "v2") |> ignore
  debug_inspect(db.ttl("gs_ttl"), content="-1")

  db.set("nx_ttl", "v") |> ignore
  db.pexpire("nx_ttl", 100) |> ignore
  db.advance_time(200)
  debug_inspect(db.setnx("nx_ttl", "fresh"), content="true")
  debug_inspect(db.get("nx_ttl"), content="Some(\"fresh\")")
}

test "expireat_commands" {
  let db = Database::new()
  db.set("k", "v") |> ignore
  debug_inspect(db.expireat("k", 1000), content="true")
  db.advance_time(999000)
  debug_inspect(db.exists("k"), content="true")
  db.advance_time(1000)
  debug_inspect(db.exists("k"), content="false")

  db.set("k2", "v") |> ignore
  debug_inspect(db.pexpireat("k2", 2000000), content="true")
  db.advance_time(2000000)
  debug_inspect(db.exists("k2"), content="false")

  debug_inspect(db.expireat("nonexistent", 1000), content="false")
  debug_inspect(db.pexpireat("nonexistent", 1000), content="false")
}

test "srandmember_randomkey" {
  let db = Database::new()
  debug_inspect(db.srandmember("nonexistent") is None, content="true")

  db.sadd("s", "a") |> ignore
  db.sadd("s", "b") |> ignore
  db.sadd("s", "c") |> ignore
  let m = db.srandmember("s")
  debug_inspect(m is None, content="false")
  match m {
    Some(v) => debug_inspect(db.sismember("s", v), content="true")
    None => ()
  }
  debug_inspect(db.scard("s"), content="3")

  db.spop("s") |> ignore
  db.spop("s") |> ignore
  db.spop("s") |> ignore
  debug_inspect(db.srandmember("s") is None, content="true")

  let db2 = Database::new()
  db2.set("k1", "v") |> ignore
  let rk = db2.randomkey()
  match rk {
    Some(k) => debug_inspect(db2.exists(k), content="true")
    None => ()
  }
}

test "ltrim_operations" {
  let db = Database::new()
  db.rpush("l", "a") |> ignore
  db.rpush("l", "b") |> ignore
  db.rpush("l", "c") |> ignore
  db.rpush("l", "d") |> ignore
  debug_inspect(db.ltrim("l", 1, 2), content="true")
  debug_inspect(db.llen("l"), content="2")
  debug_inspect(db.lindex("l", 0), content="Some(\"b\")")
  debug_inspect(db.lindex("l", 1), content="Some(\"c\")")
  debug_inspect(db.ltrim("l", -2, -1), content="true")
  debug_inspect(db.lindex("l", 0), content="Some(\"b\")")
  debug_inspect(db.ltrim("nonexistent", 0, 1), content="false")
  debug_inspect(db.ltrim("l", 5, 10), content="true")
  debug_inspect(db.exists("l"), content="false")
}

test "zmscore_operations" {
  let db = Database::new()
  db.zadd("z", 1.5, "a") |> ignore
  db.zadd("z", 2.5, "b") |> ignore
  let scores = db.zmscore("z", ["a", "x", "b"])
  debug_inspect(scores.length(), content="3")
  debug_inspect(scores[0], content="Some(1.5)")
  debug_inspect(scores[1], content="None")
  debug_inspect(scores[2], content="Some(2.5)")
  let missing = db.zmscore("nonexistent", ["a"])
  debug_inspect(missing[0] is None, content="true")
}

test "negative_range_regression" {
  let db = Database::new()
  db.rpush("l", "a") |> ignore
  db.rpush("l", "b") |> ignore
  db.rpush("l", "c") |> ignore
  db.rpush("l", "d") |> ignore
  let lr = db.lrange("l", -100, 1)
  debug_inspect(lr.length(), content="2")
  debug_inspect(lr[0], content="\"a\"")
  debug_inspect(lr[1], content="\"b\"")

  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 3.0, "c") |> ignore
  db.zadd("z", 4.0, "d") |> ignore
  let zr = db.zrange("z", -100, 1)
  debug_inspect(zr.length(), content="2")
  debug_inspect(zr[0], content="\"a\"")

  let zrev = db.zrevrange("z", -100, -3)
  debug_inspect(zrev.length(), content="2")
  debug_inspect(zrev[0], content="\"d\"")

  let removed = db.zremrangebyrank("z", -100, 1)
  debug_inspect(removed, content="2")
  debug_inspect(db.zcard("z"), content="2")
}

test "empty_container_removal" {
  let db = Database::new()
  db.hset("h", "f", "v") |> ignore
  db.hdel("h", "f") |> ignore
  debug_inspect(db.exists("h"), content="false")

  db.rpush("l", "x") |> ignore
  db.lpop("l") |> ignore
  debug_inspect(db.exists("l"), content="false")

  db.rpush("l2", "x") |> ignore
  db.rpop("l2") |> ignore
  debug_inspect(db.exists("l2"), content="false")

  db.sadd("s", "x") |> ignore
  db.srem("s", "x") |> ignore
  debug_inspect(db.exists("s"), content="false")

  db.sadd("s2", "x") |> ignore
  db.spop("s2") |> ignore
  debug_inspect(db.exists("s2"), content="false")

  db.zadd("z", 1.0, "m") |> ignore
  db.zrem("z", "m") |> ignore
  debug_inspect(db.exists("z"), content="false")

  db.sadd("s3", "x") |> ignore
  db.sadd("s4", "y") |> ignore
  db.smove("s3", "s4", "x") |> ignore
  debug_inspect(db.exists("s3"), content="false")

  db.rpush("l3", "x") |> ignore
  db.ltrim("l3", 5, 10) |> ignore
  debug_inspect(db.exists("l3"), content="false")

  db.rpush("l4", "x") |> ignore
  db.rpoplpush("l4", "l5") |> ignore
  debug_inspect(db.exists("l4"), content="false")
  debug_inspect(db.llen("l5"), content="1")

  db.hset("h2", "f", "v") |> ignore
  db.expire("h2", 100) |> ignore
  db.hdel("h2", "f") |> ignore
  debug_inspect(db.exists("h2"), content="false")
  debug_inspect(db.ttl("h2"), content="-2")
}

test "expire_on_expired_key_semantics" {
  let db = Database::new()
  db.set("k", "v") |> ignore
  db.pexpire("k", 100) |> ignore
  db.advance_time(200)
  debug_inspect(db.expire("k", 100), content="false")
  debug_inspect(db.pexpire("k", 100), content="false")
  debug_inspect(db.persist("k"), content="false")

  db.set("k2", "v") |> ignore
  db.expire("k2", 100) |> ignore
  debug_inspect(db.persist("k2"), content="true")
  debug_inspect(db.ttl("k2"), content="-1")
}

test "zset_cache_consistency" {
  let db = Database::new()
  db.zadd("rank", 10.0, "alice") |> ignore
  db.zadd("rank", 20.0, "bob") |> ignore
  let r1 = db.zrange("rank", 0, -1)
  debug_inspect(r1[0], content="\"alice\"")

  db.zadd("rank", 1.0, "alice") |> ignore
  let r2 = db.zrange("rank", 0, -1)
  debug_inspect(r2[0], content="\"alice\"")
  debug_inspect(r2[1], content="\"bob\"")
  debug_inspect(db.zrank("rank", "alice"), content="Some(0)")
  debug_inspect(db.zrank("rank", "bob"), content="Some(1)")

  db.zincrby("rank", 100.0, "alice") |> ignore
  debug_inspect(db.zrank("rank", "bob"), content="Some(0)")
  debug_inspect(db.zrank("rank", "alice"), content="Some(1)")

  db.zpopmax("rank") |> ignore
  debug_inspect(db.zrange("rank", 0, -1).length(), content="1")

  db.zadd("rank", 5.0, "carol") |> ignore
  db.zrem("rank", "bob") |> ignore
  let r3 = db.zrange("rank", 0, -1)
  debug_inspect(r3.length(), content="1")
  debug_inspect(r3[0], content="\"carol\"")

  db.set("other", "v") |> ignore
  db.rename("rank", "rank2") |> ignore
  debug_inspect(db.zrange("rank2", 0, -1)[0], content="\"carol\"")
}

test "smove_same_key" {
  let db = Database::new()
  db.sadd("s", "x") |> ignore
  debug_inspect(db.smove("s", "s", "x"), content="true")
  debug_inspect(db.scard("s"), content="1")
  debug_inspect(db.sismember("s", "x"), content="true")
  debug_inspect(db.smove("s", "s", "y"), content="false")

  db.expire("s", 100) |> ignore
  db.smove("s", "s", "x") |> ignore
  debug_inspect(db.ttl("s") >= 0, content="true")
}

test "rpoplpush_single_element_roundtrip" {
  let db = Database::new()
  db.rpush("q", "only") |> ignore
  let v = db.rpoplpush("q", "q")
  debug_inspect(v, content="Some(\"only\")")
  debug_inspect(db.llen("q"), content="1")
  debug_inspect(db.lindex("q", 0), content="Some(\"only\")")
}

test "set_store_clears_destination_ttl" {
  let db = Database::new()
  db.sadd("dest", "old") |> ignore
  db.expire("dest", 100) |> ignore
  db.sadd("s1", "a") |> ignore
  db.sinterstore("dest", ["s1"]) |> ignore
  debug_inspect(db.ttl("dest"), content="-1")
  debug_inspect(db.sismember("dest", "a"), content="true")
}

test "incrby_decrby_operations" {
  let db = Database::new()
  db.set("n", "10") |> ignore
  debug_inspect(db.incrby("n", 5), content="Some(15)")
  debug_inspect(db.decrby("n", 3), content="Some(12)")
  debug_inspect(db.decrby("n", -2), content="Some(14)")
  debug_inspect(db.get("n"), content="Some(\"14\")")

  db.set("bad", "x") |> ignore
  debug_inspect(db.incrby("bad", 1), content="None")
  db.hset("h", "f", "v") |> ignore
  debug_inspect(db.incrby("h", 1), content="None")
  debug_inspect(db.type_of("h"), content="\"hash\"")
}

test "getrange_setrange_operations" {
  let db = Database::new()
  db.set("s", "Hello World") |> ignore
  debug_inspect(db.getrange("s", 0, 4), content="\"Hello\"")
  debug_inspect(db.getrange("s", -5, -1), content="\"World\"")
  debug_inspect(db.getrange("s", 6, 100), content="\"World\"")
  debug_inspect(db.getrange("nonexistent", 0, -1), content="\"\"")
  debug_inspect(db.setrange("s", 6, "Moon!"), content="11")
  debug_inspect(db.get("s"), content="Some(\"Hello Moon!\")")

  db.set("grow", "Hi") |> ignore
  debug_inspect(db.setrange("grow", 5, "!"), content="6")
  debug_inspect(db.getrange("grow", 0, 1), content="\"Hi\"")
  debug_inspect(db.getrange("grow", 5, 5), content="\"!\"")

  db.set("empty_val", "abc") |> ignore
  debug_inspect(db.setrange("empty_val", 1, ""), content="3")
  debug_inspect(db.setrange("missing", 2, "ab"), content="4")
  debug_inspect(db.getrange("missing", 2, 3), content="\"ab\"")
  debug_inspect(db.setrange("s", -1, "x"), content="-1")

  db.hset("hash_key", "f", "v") |> ignore
  debug_inspect(db.setrange("hash_key", 0, "x"), content="-1")
}

test "hsetnx_operations" {
  let db = Database::new()
  debug_inspect(db.hsetnx("h", "f", "v"), content="true")
  debug_inspect(db.hsetnx("h", "f", "x"), content="false")
  debug_inspect(db.hget("h", "f"), content="Some(\"v\")")

  db.set("s", "v") |> ignore
  debug_inspect(db.hsetnx("s", "f", "v"), content="false")
  debug_inspect(db.type_of("s"), content="\"string\"")
}

test "scan_operations" {
  let db = Database::new()
  for i = 0; i < 25; i = i + 1 {
    db.set("scan:\{i}", "v") |> ignore
  }

  let (c1, b1) = db.scan(0, 10)
  debug_inspect(b1.length(), content="10")
  debug_inspect(c1, content="10")
  let (c2, b2) = db.scan(c1, 10)
  debug_inspect(b2.length(), content="10")
  debug_inspect(c2, content="20")
  let (c3, b3) = db.scan(c2, 10)
  debug_inspect(b3.length(), content="5")
  debug_inspect(c3, content="0")

  let seen = Map([], capacity=25)
  for batch in [b1, b2, b3] {
    for key in batch {
      seen.set(key, true)
    }
  }
  debug_inspect(seen.length(), content="25")

  let (c4, b4) = db.scan(0, 100)
  debug_inspect(b4.length(), content="25")
  debug_inspect(c4, content="0")

  let (c5, b5) = db.scan(999, 10)
  debug_inspect(b5.length(), content="0")
  debug_inspect(c5, content="0")

  let (_, b6) = db.scan(0, 0)
  debug_inspect(b6.length(), content="10")
}

test "zset_with_scores_operations" {
  let db = Database::new()
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 3.0, "c") |> ignore

  let asc = db.zrange_with_scores("z", 0, -1)
  debug_inspect(asc.length(), content="3")
  debug_inspect(asc[0].0, content="\"a\"")
  debug_inspect(asc[0].1, content="1")
  debug_inspect(asc[2].0, content="\"c\"")
  debug_inspect(asc[2].1, content="3")

  let desc = db.zrevrange_with_scores("z", 0, 1)
  debug_inspect(desc.length(), content="2")
  debug_inspect(desc[0].0, content="\"c\"")
  debug_inspect(desc[0].1, content="3")
  debug_inspect(desc[1].0, content="\"b\"")
}

test "zrangebylex_operations" {
  let db = Database::new()
  for item in ["a", "b", "c", "d"] {
    db.zadd("z", 0.0, item) |> ignore
  }

  let all = db.zrangebylex("z", "-", "+")
  debug_inspect(all.length(), content="4")
  debug_inspect(all[0], content="\"a\"")
  debug_inspect(all[3], content="\"d\"")

  let incl = db.zrangebylex("z", "[b", "[c")
  debug_inspect(incl.length(), content="2")
  debug_inspect(incl[0], content="\"b\"")
  debug_inspect(incl[1], content="\"c\"")

  let excl = db.zrangebylex("z", "(b", "(d")
  debug_inspect(excl.length(), content="1")
  debug_inspect(excl[0], content="\"c\"")

  let upper = db.zrangebylex("z", "-", "(c")
  debug_inspect(upper.length(), content="2")
  debug_inspect(db.zrangebylex("z", "+", "+").length(), content="0")
  debug_inspect(db.zrangebylex("z", "b", "+").length(), content="0")
}

test "copy_operations" {
  let db = Database::new()
  db.hset("src", "f", "v") |> ignore
  debug_inspect(db.copy("src", "dst"), content="true")
  debug_inspect(db.hget("dst", "f"), content="Some(\"v\")")

  db.hset("src", "f", "changed") |> ignore
  debug_inspect(db.hget("dst", "f"), content="Some(\"v\")")
  debug_inspect(db.copy("src", "dst"), content="false")
  debug_inspect(db.copy("missing", "fresh"), content="false")
  debug_inspect(db.copy("src", "src"), content="false")

  db.rpush("list_src", "a") |> ignore
  debug_inspect(db.copy("list_src", "list_dst"), content="true")
  db.rpush("list_src", "b") |> ignore
  debug_inspect(db.llen("list_dst"), content="1")

  db.set("ttl_src", "v") |> ignore
  db.expire("ttl_src", 100) |> ignore
  debug_inspect(db.copy("ttl_src", "ttl_dst"), content="true")
  debug_inspect(db.ttl("ttl_dst"), content="-1")
}

test "touch_unlink_operations" {
  let db = Database::new()
  db.set("a", "1") |> ignore
  db.set("b", "2") |> ignore
  debug_inspect(db.touch(["a", "b", "missing"]), content="2")

  db.set("expired", "v") |> ignore
  db.pexpire("expired", 10) |> ignore
  db.advance_time(20)
  debug_inspect(db.touch(["expired"]), content="0")

  debug_inspect(db.unlink("a"), content="true")
  debug_inspect(db.exists("a"), content="false")
  debug_inspect(db.unlink("a"), content="false")
}

test "expire_heap_stress" {
  let db = Database::new()
  for i = 0; i < 100; i = i + 1 {
    db.set("ttl:\{i}", "v") |> ignore
    db.pexpire("ttl:\{i}", (i + 1).to_int64()) |> ignore
  }
  db.advance_time(50)
  debug_inspect(db.dbsize(), content="50")
  debug_inspect(db.exists("ttl:49"), content="false")
  debug_inspect(db.exists("ttl:50"), content="true")

  db.set("update", "v") |> ignore
  db.pexpire("update", 100) |> ignore
  db.advance_time(80)
  debug_inspect(db.exists("update"), content="true")
  db.pexpire("update", 10) |> ignore
  db.advance_time(20)
  debug_inspect(db.exists("update"), content="false")

  db.set("keep", "v") |> ignore
  db.pexpire("keep", 100) |> ignore
  debug_inspect(db.persist("keep"), content="true")
  db.advance_time(200)
  debug_inspect(db.exists("keep"), content="true")

  db.set("renamed", "v") |> ignore
  db.expire("renamed", 100) |> ignore
  db.rename("renamed", "renamed2") |> ignore
  debug_inspect(db.ttl("renamed2") > 0, content="true")
  db.advance_time(101000)
  debug_inspect(db.exists("renamed2"), content="false")

  db.flushdb()
  debug_inspect(db.expire_heap.length(), content="0")
}

test "scan_snapshot_consistency" {
  let db = Database::new()
  for i = 0; i < 30; i = i + 1 {
    db.set("scan:\{i}", "v") |> ignore
  }
  let (c1, b1) = db.scan(0, 10)
  debug_inspect(b1.length(), content="10")
  let (c2, b2) = db.scan(c1, 10)
  debug_inspect(b2.length(), content="10")

  db.set("scan:new", "v") |> ignore
  let (c3, b3) = db.scan(c2, 10)
  debug_inspect(b3.length(), content="10")
  debug_inspect(c3, content="30")

  let (c4, b4) = db.scan(0, 100)
  debug_inspect(b4.length(), content="31")
  debug_inspect(c4, content="0")
  debug_inspect(b4.contains("scan:new"), content="true")
}

test "zset_rank_and_lex_cache" {
  let db = Database::new()
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 3.0, "c") |> ignore
  debug_inspect(db.zrank("z", "c"), content="Some(2)")
  debug_inspect(db.zrevrank("z", "a"), content="Some(2)")

  db.zadd("z", 0.0, "c") |> ignore
  debug_inspect(db.zrank("z", "c"), content="Some(0)")
  debug_inspect(db.zrank("z", "b"), content="Some(2)")

  let lex1 = db.zrangebylex("z", "-", "+")
  let lex2 = db.zrangebylex("z", "-", "+")
  debug_inspect(lex1.length(), content="3")
  debug_inspect(lex2[0], content="\"a\"")

  db.zrem("z", "b") |> ignore
  let lex3 = db.zrangebylex("z", "-", "+")
  debug_inspect(lex3.length(), content="2")
  debug_inspect(db.zrank("z", "c"), content="Some(0)")

  db.rename("z", "z2") |> ignore
  debug_inspect(db.zrank("z2", "c"), content="Some(0)")
  debug_inspect(db.zrange("z2", 0, -1)[0], content="\"c\"")
}