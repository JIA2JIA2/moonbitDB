fn show_opt(opt : String?) -> String {
  match opt {
    Some(v) => "\"\{v}\""
    None => "(nil)"
  }
}

fn show_opt_int(opt : Int?) -> String {
  match opt {
    Some(v) => "\{v}"
    None => "(nil)"
  }
}

fn show_opt_int64(opt : Int64?) -> String {
  match opt {
    Some(v) => "\{v}"
    None => "(nil)"
  }
}

fn show_opt_float(opt : Float?) -> String {
  match opt {
    Some(v) => "\{v}"
    None => "(nil)"
  }
}

fn main {
  let db = @lib.Database::new()

  println("=== String 操作 ===")
  db.set("name", "Alice")
  db.set("age", "25")
  println("GET name: \{show_opt(db.get("name"))}")
  println("GET age: \{show_opt(db.get("age"))}")
  println("STRLEN name: \{db.strlen("name")}")
  println("APPEND name ' Smith': \{db.append("name", " Smith")}")
  println("GET name after append: \{show_opt(db.get("name"))}")
  println("INCR age: \{show_opt_int64(db.incr("age"))}")
  println("DECR age: \{show_opt_int64(db.decr("age"))}")
  println("INCRBY age 10: \{show_opt_int64(db.incrby("age", 10))}")
  println("GETRANGE name 0 4: \"\{db.getrange("name", 0, 4)}\"")
  println("SETRANGE name 0 A: \{db.setrange("name", 0, "A")}")
  println("GET name after setrange: \{show_opt(db.get("name"))}")
  let append_result = db.try_append("name", "!")
  let append_msg = match append_result {
    Ok(length) => "\{length}"
    Err(error) => @lib.RedisError::message(error)
  }
  println("TRY_APPEND name '!': \{append_msg}")

  println("\n=== Hash 操作 ===")
  db.hset("user:1", "username", "alice") |> ignore
  db.hset("user:1", "email", "alice@example.com") |> ignore
  db.hset("user:1", "age", "25") |> ignore
  println("HSETNX username bob: \{db.hsetnx("user:1", "username", "bob")}")
  db.hmset("user:1", ["city", "nickname"], ["Beijing", "Ally"]) |> ignore
  println("HGET user:1 username: \{show_opt(db.hget("user:1", "username"))}")
  println("HGET user:1 city: \{show_opt(db.hget("user:1", "city"))}")
  println("HLEN user:1: \{db.hlen("user:1")}")
  let all = db.hgetall("user:1")
  let fields : Array[String] = []
  for f, _ in all {
    fields.push(f)
  }
  println("HGETALL fields: [\{fields.join(", ")}]")

  println("\n=== List 操作 ===")
  db.rpush_all("tasks", ["Task 1", "Task 2", "Task 3"]) |> ignore
  db.lpush_all("tasks", ["Urgent Task"]) |> ignore
  println("LLEN tasks: \{db.llen("tasks")}")
  let tasks = db.lrange("tasks", 0, -1)
  println("LRANGE tasks 0 -1: [\{tasks.join(", ")}]")
  println("LPOP tasks: \{show_opt(db.lpop("tasks"))}")
  println("RPOP tasks: \{show_opt(db.rpop("tasks"))}")
  println("LINDEX tasks 0: \{show_opt(db.lindex("tasks", 0))}")

  println("\n=== Set 操作 ===")
  let added = db.sadd_all("tags:post:1", ["tech", "programming", "moonbit", "tech"])
  println("SADD_ALL added: \{added}")
  println("SCARD tags:post:1: \{db.scard("tags:post:1")}")
  println("SISMEMBER moonbit: \{db.sismember("tags:post:1", "moonbit")}")
  let tags = db.smembers("tags:post:1")
  println("SMEMBERS: [\{tags.join(", ")}]")
  db.set("not_a_set", "plain string") |> ignore
  let inter = db.try_sinter(["tags:post:1", "not_a_set"])
  let inter_msg = match inter {
    Ok(values) => "[\{values.join(", ")}]"
    Err(error) => @lib.RedisError::message(error)
  }
  println("TRY_SINTER with wrong type: \{inter_msg}")

  println("\n=== Sorted Set 操作 ===")
  db.zadd("leaderboard", 100.0, "player1") |> ignore
  db.zadd("leaderboard", 200.0, "player2") |> ignore
  db.zadd("leaderboard", 150.0, "player3") |> ignore
  println("ZCARD leaderboard: \{db.zcard("leaderboard")}")
  println("ZSCORE player2: \{show_opt_float(db.zscore("leaderboard", "player2"))}")
  println("ZRANK player3: \{show_opt_int(db.zrank("leaderboard", "player3"))}")
  let top = db.zrange_with_scores("leaderboard", 0, 2)
  println("ZRANGE WITHSCORES 0 2:")
  for i = 0; i < top.length(); i = i + 1 {
    println("  \{top[i].0} = \{top[i].1}")
  }
  for name in ["alice", "bob", "carol", "dave"] {
    db.zadd("lex:names", 0.0, name) |> ignore
  }
  let lex = db.zrangebylex("lex:names", "[b", "(d")
  println("ZRANGEBYLEX [b (d: [\{lex.join(", ")}]")

  println("\n=== Key 过期机制 ===")
  db.set("session:user1", "abc123") |> ignore
  db.expire("session:user1", 30) |> ignore
  println("TTL session:user1: \{db.ttl("session:user1")}s")
  db.advance_time(20000)
  println("20秒后 TTL: \{db.ttl("session:user1")}s")
  db.advance_time(15000)
  println("35秒后 EXISTS: \{db.exists("session:user1")}")

  db.set("session:backup", "user1") |> ignore
  println("COPY session:backup -> session:copy: \{db.copy("session:backup", "session:copy")}")
  println("TOUCH 3 keys: \{db.touch(["session:backup", "session:copy", "missing"])}")
  println("UNLINK session:backup: \{db.unlink("session:backup")}")
  let (cursor, batch) = db.scan(0, 3)
  println("SCAN 0 3 -> cursor \{cursor}, keys [\{batch.join(", ")}]")

  println("\n=== 批量操作与错误模型 ===")
  db.mset(["k1", "k2", "k3"], ["v1", "v2", "v3"]) |> ignore
  let vals = db.mget(["k1", "k2", "k3", "k4"])
  println("MGET k1,k2,k3,k4:")
  for i = 0; i < vals.length(); i = i + 1 {
    println("  [\{i}] = \{show_opt(vals[i])}")
  }
  let deleted = db.mdel(["k1", "k2", "k3"])
  println("MDEL 删除数量: \{deleted}")
  let bad_mset = db.try_mset(["only_key"], [])
  let bad_mset_msg = match bad_mset {
    Ok(_) => "OK"
    Err(error) => @lib.RedisError::message(error)
  }
  println("TRY_MSET 长度不一致: \{bad_mset_msg}")
  db.set("bad_number", "abc") |> ignore
  let bad_incr = db.try_incrby("bad_number", 1)
  let bad_incr_msg = match bad_incr {
    Ok(value) => "\{value}"
    Err(error) => @lib.RedisError::message(error)
  }
  println("TRY_INCRBY 非整数: \{bad_incr_msg}")

  println("\n=== 服务器信息 ===")
  println("DBSIZE: \{db.dbsize()}")
  println("PING: \{@lib.Database::ping()}")
  println("ECHO: \{@lib.Database::echo(db, "Hello MoonBitDB!")}")

  println("\n=== TYPE 检查 ===")
  println("TYPE user:1: \{db.type_of("user:1")}")
  println("TYPE tasks: \{db.type_of("tasks")}")
  println("TYPE leaderboard: \{db.type_of("leaderboard")}")
  println("TYPE nonexistent: \{db.type_of("nonexistent")}")

  println("\n=== 扩展命令 ===")
  let (hscan_cursor, hscan_batch) = db.hscan("user:1", 0, 2)
  println("HSCAN user:1 0 2: cursor=\{hscan_cursor}, fields=\{hscan_batch.length()}")
  println("LMOVE tasks tasks LEFT RIGHT: \{show_opt(db.lmove("tasks", "tasks", true, false))}")
  db.sadd_all("tags:other", ["tech", "database"]) |> ignore
  println("SINTERCARD tags: \{db.sintercard(["tags:post:1", "tags:other"], 0)}")
  let flags = db.smismember("tags:post:1", ["tech", "database"])
  println("SMISMEMBER: tech=\{flags[0]}, database=\{flags[1]}")
  println("MSETNX with existing: \{db.msetnx(["name", "nx"], ["x", "y"])}")
  println("ZUNION leaderboard: \{db.zunion(["leaderboard"]).length()}")
  println("ZUNIONSTORE leaderboard:copy: \{db.zunionstore("leaderboard:copy", ["leaderboard"])}")
  println("LPOS tasks Urgent Task: \{show_opt_int(db.lpos("tasks", "Urgent Task"))}")
  println("HRANDFIELD user:1 2: \{db.hrandfield("user:1", 2).length()}")
  println("SRANDMEMBER tags 2: \{db.srandmember_count("tags:post:1", 2).length()}")
  let (zscan_cursor, zscan_batch) = db.zscan("leaderboard", 0, 2)
  println("ZSCAN leaderboard 0 2: cursor=\{zscan_cursor}, members=\{zscan_batch.length()}")

  println("\n✅ 基本使用示例运行完成！")
}
