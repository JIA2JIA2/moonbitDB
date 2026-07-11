fn main() -> Unit {
  let db = Database::new()
  
  println("=== String 操作 ===")
  db.set("name", "Alice")
  db.set("age", "25")
  println("GET name: \{db.get("name")}")
  println("GET age: \{db.get("age")}")
  println("STRLEN name: \{db.strlen("name")}")
  println("APPEND name ' Smith': \{db.append("name", " Smith")}")
  println("GET name after append: \{db.get("name")}")
  println("INCR age: \{db.incr("age")}")
  println("DECR age: \{db.decr("age")}")
  
  println("\n=== Hash 操作 ===")
  db.hset("user:1", "username", "alice") |> ignore
  db.hset("user:1", "email", "alice@example.com") |> ignore
  db.hset("user:1", "age", "25") |> ignore
  println("HGET user:1 username: \{db.hget("user:1", "username")}")
  println("HLEN user:1: \{db.hlen("user:1")}")
  println("HEXISTS user:1 email: \{db.hexists("user:1", "email")}")
  let user_fields = db.hkeys("user:1")
  println("HKEYS user:1: \{user_fields.join(", ")}")
  
  println("\n=== List 操作 ===")
  db.rpush("tasks", "Task 1") |> ignore
  db.rpush("tasks", "Task 2") |> ignore
  db.rpush("tasks", "Task 3") |> ignore
  println("LLEN tasks: \{db.llen("tasks")}")
  let tasks = db.lrange("tasks", 0, -1)
  println("LRANGE tasks 0 -1: \{tasks.join(", ")}")
  println("LPOP tasks: \{db.lpop("tasks")}")
  println("RPOP tasks: \{db.rpop("tasks")}")
  println("LINDEX tasks 0: \{db.lindex("tasks", 0)}")
  
  println("\n=== Set 操作 ===")
  db.sadd("tags:post:1", "tech") |> ignore
  db.sadd("tags:post:1", "programming") |> ignore
  db.sadd("tags:post:1", "moonbit") |> ignore
  println("SCARD tags:post:1: \{db.scard("tags:post:1")}")
  println("SISMEMBER moonbit: \{db.sismember("tags:post:1", "moonbit")}")
  let tags = db.smembers("tags:post:1")
  println("SMEMBERS: \{tags.join(", ")}")
  
  println("\n=== Sorted Set 操作 ===")
  db.zadd("leaderboard", 100.0, "player1") |> ignore
  db.zadd("leaderboard", 200.0, "player2") |> ignore
  db.zadd("leaderboard", 150.0, "player3") |> ignore
  println("ZCARD leaderboard: \{db.zcard("leaderboard")}")
  println("ZSCORE player2: \{db.zscore("leaderboard", "player2")}")
  println("ZRANK player3: \{db.zrank("leaderboard", "player3")}")
  let top = db.zrange("leaderboard", 0, 2)
  println("ZRANGE 0 2: \{top.join(", ")}")
  
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
    println("  [\{i}] = \{vals[i]}")
  }
  let deleted = db.mdel(["k1", "k2", "k3"])
  println("MDEL 删除数量: \{deleted}")
  
  println("\n=== 服务器信息 ===")
  println("DBSIZE: \{db.dbsize()}")
  println("PING: \{Database::ping()}")
  println("ECHO: \{Database::echo(db, "Hello MoonBitDB!")}")
  let info = db.info()
  println("INFO (前3行):")
  let info_lines = info.split("\n")
  for i = 0; i < 3 && i < info_lines.length(); i = i + 1 {
    println("  \{info_lines[i]}")
  }
  
  println("\n=== TYPE 检查 ===")
  println("TYPE user:1: \{db.type_of("user:1")}")
  println("TYPE tasks: \{db.type_of("tasks")}")
  println("TYPE leaderboard: \{db.type_of("leaderboard")}")
  println("TYPE nonexistent: \{db.type_of("nonexistent")}")
  
  println("\n✅ 基本使用示例运行完成！")
}
