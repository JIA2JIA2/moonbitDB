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

fn main() -> Unit {
  println("========================================")
  println("  MoonBitDB - 快速开始演示")
  println("========================================")
  
  let db = @lib.Database::new()
  
  println("\n📌 1. String 基础操作")
  println("----------------------------------------")
  db.set("greeting", "Hello, MoonBitDB!")
  println("  SET greeting 'Hello, MoonBitDB!'")
  println("  GET greeting = \{show_opt(db.get("greeting"))}")
  println("  STRLEN greeting = \{db.strlen("greeting")}")
  
  db.set("counter", "10")
  println("  SET counter 10")
  println("  INCR counter = \{show_opt_int(db.incr("counter"))}")
  println("  INCR counter = \{show_opt_int(db.incr("counter"))}")
  println("  DECR counter = \{show_opt_int(db.decr("counter"))}")
  
  println("\n📌 2. Hash 用户信息存储")
  println("----------------------------------------")
  db.hset("user:1001", "username", "alice") |> ignore
  db.hset("user:1001", "email", "alice@example.com") |> ignore
  db.hset("user:1001", "level", "42") |> ignore
  println("  HSET user:1001 {username, email, level}")
  println("  HGET user:1001 username = \{show_opt(db.hget("user:1001", "username"))}")
  println("  HLEN user:1001 = \{db.hlen("user:1001")}")
  let user_email = db.hget("user:1001", "email")
  let has_email = match user_email {
    Some(_) => true
    None => false
  }
  println("  email字段存在 = \{has_email}")
  let all_fields = db.hgetall("user:1001")
  let field_list : Array[String] = []
  for f, _ in all_fields {
    field_list.push(f)
  }
  println("  字段列表 = [\{field_list.join(", ")}]")
  
  println("\n📌 3. List 任务队列")
  println("----------------------------------------")
  db.rpush("tasks", "完成报告") |> ignore
  db.rpush("tasks", "代码审查") |> ignore
  db.rpush("tasks", "团队会议") |> ignore
  println("  RPUSH tasks [完成报告, 代码审查, 团队会议]")
  println("  LLEN tasks = \{db.llen("tasks")}")
  let task_list = db.lrange("tasks", 0, -1)
  println("  LRANGE tasks 0 -1 = [\{task_list.join(", ")}]")
  println("  LPOP tasks = \{show_opt(db.lpop("tasks"))}")
  println("  RPOP tasks = \{show_opt(db.rpop("tasks"))}")
  println("  剩余 LLEN = \{db.llen("tasks")}")
  
  println("\n📌 4. Set 标签系统")
  println("----------------------------------------")
  db.sadd("tags:post:1", "技术") |> ignore
  db.sadd("tags:post:1", "编程") |> ignore
  db.sadd("tags:post:1", "MoonBit") |> ignore
  db.sadd("tags:post:1", "技术") |> ignore
  println("  SADD tags:post:1 {技术, 编程, MoonBit, 技术}")
  println("  SCARD = \{db.scard("tags:post:1")} (自动去重)")
  println("  SISMEMBER MoonBit = \{db.sismember("tags:post:1", "MoonBit")}")
  let tags = db.smembers("tags:post:1")
  println("  SMEMBERS = [\{tags.join(", ")}]")
  
  println("\n📌 5. Sorted Set 排行榜")
  println("----------------------------------------")
  db.zadd("leaderboard", 1000.0, "玩家A") |> ignore
  db.zadd("leaderboard", 2500.0, "玩家B") |> ignore
  db.zadd("leaderboard", 1800.0, "玩家C") |> ignore
  db.zadd("leaderboard", 3200.0, "玩家D") |> ignore
  println("  ZADD leaderboard {玩家A:1000, 玩家B:2500, 玩家C:1800, 玩家D:3200}")
  println("  ZCARD = \{db.zcard("leaderboard")}")
  println("  ZSCORE 玩家B = \{show_opt_float(db.zscore("leaderboard", "玩家B"))}")
  println("  ZRANK 玩家B = \{show_opt_int(db.zrank("leaderboard", "玩家B"))} (从小到大)")
  let top3 = db.zrevrange("leaderboard", 0, 2)
  println("  TOP 3 (ZREVRANGE 0 2):")
  for i = 0; i < top3.length(); i = i + 1 {
    let score = db.zscore("leaderboard", top3[i])
    let score_str = show_opt_float(score)
    println("    #\{i + 1} \{top3[i]} - \{score_str}分")
  }
  
  println("\n📌 6. Key 过期机制")
  println("----------------------------------------")
  db.set("session:token_abc", "user:1001")
  db.expire("session:token_abc", 60) |> ignore
  println("  SET session:token_abc + EXPIRE 60s")
  println("  TTL = \{db.ttl("session:token_abc")} 秒")
  db.advance_time(30000)
  println("  30秒后 TTL = \{db.ttl("session:token_abc")} 秒")
  db.advance_time(35000)
  println("  65秒后 EXISTS = \{db.exists("session:token_abc")}")
  println("  TTL = \{db.ttl("session:token_abc")} (-2 表示不存在)")
  
  println("\n📌 7. 批量操作")
  println("----------------------------------------")
  db.mset(["config:theme", "config:lang", "config:tz"], ["dark", "zh-CN", "Asia/Shanghai"]) |> ignore
  println("  MSET 3个配置项")
  let config_vals = db.mget(["config:theme", "config:lang", "config:tz", "config:nonexist"])
  println("  MGET 结果:")
  let keys = ["config:theme", "config:lang", "config:tz", "config:nonexist"]
  for i = 0; i < config_vals.length(); i = i + 1 {
    println("    \{keys[i]} = \{show_opt(config_vals[i])}")
  }
  
  println("\n📌 8. 服务器信息")
  println("----------------------------------------")
  println("  DBSIZE = \{db.dbsize()}")
  println("  PING = \{@lib.Database::ping()}")
  let rk = db.randomkey()
  println("  RANDOMKEY = \{show_opt(rk)}")
  let all_keys = db.keys()
  println("  KEYS * (\{all_keys.length()} 个): [\{all_keys.join(", ")}]")
  println("  支持命令数 = \{@lib.Database::command().length()}")
  
  println("\n========================================")
  println("  ✅ 演示完成！")
  println("========================================")
}
