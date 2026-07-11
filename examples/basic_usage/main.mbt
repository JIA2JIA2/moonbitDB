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

fn show_opt_float(opt : Float?) -> String {
  match opt {
    Some(v) => "\{v}"
    None => "(nil)"
  }
}

fn main {
  let db = @moonbitDB.Database::new()

  println("=== String 操作 ===")
  db.set("name", "Alice")
  db.set("age", "25")
  println("GET name: \{show_opt(db.get("name"))}")
  println("GET age: \{show_opt(db.get("age"))}")
  println("STRLEN name: \{db.strlen("name")}")
  println("APPEND name ' Smith': \{db.append("name", " Smith")}")
  println("GET name after append: \{show_opt(db.get("name"))}")
  println("INCR age: \{show_opt_int(db.incr("age"))}")
  println("DECR age: \{show_opt_int(db.decr("age"))}")

  println("\n=== Hash 操作 ===")
  db.hset("user:1", "username", "alice") |> ignore
  db.hset("user:1", "email", "alice@example.com") |> ignore
  db.hset("user:1", "age", "25") |> ignore
  println("HGET user:1 username: \{show_opt(db.hget("user:1", "username"))}")
  println("HLEN user:1: \{db.hlen("user:1")}")
  let all = db.hgetall("user:1")
  let fields : Array[String] = []
  for f, _ in all {
    fields.push(f)
  }
  println("HGETALL fields: [\{fields.join(", ")}]")

  println("\n=== List 操作 ===")
  db.rpush("tasks", "Task 1") |> ignore
  db.rpush("tasks", "Task 2") |> ignore
  db.rpush("tasks", "Task 3") |> ignore
  println("LLEN tasks: \{db.llen("tasks")}")
  let tasks = db.lrange("tasks", 0, -1)
  println("LRANGE tasks 0 -1: [\{tasks.join(", ")}]")
  println("LPOP tasks: \{show_opt(db.lpop("tasks"))}")
  println("RPOP tasks: \{show_opt(db.rpop("tasks"))}")
  println("LINDEX tasks 0: \{show_opt(db.lindex("tasks", 0))}")

  println("\n=== Set 操作 ===")
  db.sadd("tags:post:1", "tech") |> ignore
  db.sadd("tags:post:1", "programming") |> ignore
  db.sadd("tags:post:1", "moonbit") |> ignore
  println("SCARD tags:post:1: \{db.scard("tags:post:1")}")
  println("SISMEMBER moonbit: \{db.sismember("tags:post:1", "moonbit")}")
  let tags = db.smembers("tags:post:1")
  println("SMEMBERS: [\{tags.join(", ")}]")

  println("\n=== Sorted Set 操作 ===")
  db.zadd("leaderboard", 100.0, "player1") |> ignore
  db.zadd("leaderboard", 200.0, "player2") |> ignore
  db.zadd("leaderboard", 150.0, "player3") |> ignore
  println("ZCARD leaderboard: \{db.zcard("leaderboard")}")
  println("ZSCORE player2: \{show_opt_float(db.zscore("leaderboard", "player2"))}")
  println("ZRANK player3: \{show_opt_int(db.zrank("leaderboard", "player3"))}")
  let top = db.zrange("leaderboard", 0, 2)
  println("ZRANGE 0 2: [\{top.join(", ")}]")

  println("\n=== Key 过期机制 ===")
  db.set("session:user1", "abc123") |> ignore
  db.expire("session:user1", 30) |> ignore
  println("TTL session:user1: \{db.ttl("session:user1")}s")
  db.advance_time(20000)
  println("20秒后 TTL: \{db.ttl("session:user1")}s")
  db.advance_time(15000)
  println("35秒后 EXISTS: \{db.exists("session:user1")}")

  println("\n=== 批量操作 ===")
  db.mset(["k1", "k2", "k3"], ["v1", "v2", "v3"]) |> ignore
  let vals = db.mget(["k1", "k2", "k3", "k4"])
  println("MGET k1,k2,k3,k4:")
  for i = 0; i < vals.length(); i = i + 1 {
    println("  [\{i}] = \{show_opt(vals[i])}")
  }
  let deleted = db.mdel(["k1", "k2", "k3"])
  println("MDEL 删除数量: \{deleted}")

  println("\n=== 服务器信息 ===")
  println("DBSIZE: \{db.dbsize()}")
  println("PING: \{@moonbitDB.Database::ping()}")
  println("ECHO: \{@moonbitDB.Database::echo(db, "Hello MoonBitDB!")}")

  println("\n=== TYPE 检查 ===")
  println("TYPE user:1: \{db.type_of("user:1")}")
  println("TYPE tasks: \{db.type_of("tasks")}")
  println("TYPE leaderboard: \{db.type_of("leaderboard")}")
  println("TYPE nonexistent: \{db.type_of("nonexistent")}")

  println("\n✅ 基本使用示例运行完成！")
}
