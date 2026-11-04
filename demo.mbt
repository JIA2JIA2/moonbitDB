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

fn show_opt_float(opt : Double?) -> String {
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
  println("  INCR counter = \{show_opt_int64(db.incr("counter"))}")
  println("  INCR counter = \{show_opt_int64(db.incr("counter"))}")
  println("  DECR counter = \{show_opt_int64(db.decr("counter"))}")
  println("  INCRBY counter 5 = \{show_opt_int64(db.incrby("counter", 5))}")
  println("  GETRANGE greeting 0 4 = \"\{db.getrange("greeting", 0, 4)}\"")
  println("  SETRANGE greeting 7 'MoonBit' = \{db.setrange("greeting", 7, "MoonBit")} (新长度)")
  let append_result = db.try_append("greeting", "!")
  let append_msg = match append_result {
    Ok(length) => "\{length}"
    Err(error) => @lib.RedisError::message(error)
  }
  println("  TRY_APPEND greeting '!' = \{append_msg}")

  println("\n📌 2. Hash 用户信息存储")
  println("----------------------------------------")
  db.hset("user:1001", "username", "alice") |> ignore
  db.hset("user:1001", "email", "alice@example.com") |> ignore
  db.hset("user:1001", "level", "42") |> ignore
  println("  HSET user:1001 {username, email, level}")
  println("  HSETNX username bob = \{db.hsetnx("user:1001", "username", "bob")} (已存在，不覆盖)")
  db.hmset("user:1001", ["nickname", "city"], ["Ally", "Beijing"]) |> ignore
  println("  HMSET user:1001 {nickname, city}")
  println("  HGET user:1001 username = \{show_opt(db.hget("user:1001", "username"))}")
  println("  HGET user:1001 city = \{show_opt(db.hget("user:1001", "city"))}")
  println("  HLEN user:1001 = \{db.hlen("user:1001")}")
  let (hscan_cursor, hscan_batch) = db.hscan("user:1001", 0, 2)
  println("  HSCAN user:1001 0 2 = cursor \{hscan_cursor}, \{hscan_batch.length()} 个字段")
  println("  HRANDFIELD user:1001 2 = \{db.hrandfield("user:1001", 2).length()} 个字段")
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
  db.rpush_all("tasks", ["完成报告", "代码审查", "团队会议"]) |> ignore
  println("  RPUSH_ALL tasks [完成报告, 代码审查, 团队会议]")
  db.lpush_all("tasks", ["紧急任务"]) |> ignore
  println("  LPUSH_ALL tasks [紧急任务]")
  println("  LLEN tasks = \{db.llen("tasks")}")
  let task_list = db.lrange("tasks", 0, -1)
  println("  LRANGE tasks 0 -1 = [\{task_list.join(", ")}]")
  println("  LMOVE tasks tasks LEFT RIGHT = \{show_opt(db.lmove("tasks", "tasks", true, false))}")
  println("  LPOS tasks 代码审查 = \{show_opt_int(db.lpos("tasks", "代码审查"))}")
  println("  LPOP tasks = \{show_opt(db.lpop("tasks"))}")
  println("  RPOP tasks = \{show_opt(db.rpop("tasks"))}")
  println("  剩余 LLEN = \{db.llen("tasks")}")
  
  println("\n📌 4. Set 标签系统")
  println("----------------------------------------")
  let added = db.sadd_all("tags:post:1", ["技术", "编程", "MoonBit", "技术"])
  println("  SADD_ALL tags:post:1 {技术, 编程, MoonBit, 技术} = \{added} (自动去重)")
  println("  SCARD = \{db.scard("tags:post:1")}")
  println("  SISMEMBER MoonBit = \{db.sismember("tags:post:1", "MoonBit")}")
  let tags = db.smembers("tags:post:1")
  println("  SMEMBERS = [\{tags.join(", ")}]")
  db.sadd_all("tags:other", ["编程", "数据库"]) |> ignore
  println("  SINTERCARD 两标签集 = \{db.sintercard(["tags:post:1", "tags:other"], 0)}")
  let flags = db.smismember("tags:post:1", ["技术", "数据库"])
  println("  SMISMEMBER 技术/数据库 = [\{flags[0]}, \{flags[1]}]")
  println("  SRANDMEMBER tags:post:1 2 = \{db.srandmember_count("tags:post:1", 2).length()} 个成员")
  db.set("not_a_set", "plain string") |> ignore
  let inter = db.try_sinter(["tags:post:1", "not_a_set"])
  let inter_msg = match inter {
    Ok(values) => "[\{values.join(", ")}]"
    Err(error) => @lib.RedisError::message(error)
  }
  println("  TRY_SINTER 遇到错误类型 = \{inter_msg}")
  
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
  let top3 = db.zrevrange_with_scores("leaderboard", 0, 2)
  println("  TOP 3 (ZREVRANGE WITHSCORES):")
  for i = 0; i < top3.length(); i = i + 1 {
    println("    #\{i + 1} \{top3[i].0} - \{top3[i].1}分")
  }
  for name in ["alice", "bob", "carol", "dave"] {
    db.zadd("lex:names", 0.0, name) |> ignore
  }
  let lex = db.zrangebylex("lex:names", "[b", "(d")
  println("  ZRANGEBYLEX [b (d = [\{lex.join(", ")}]")
  println("  ZUNION leaderboard/lex:names = \{db.zunion(["leaderboard", "lex:names"]).length()} 个成员")
  println("  ZINTER leaderboard/lex:names = \{db.zinter(["leaderboard", "lex:names"]).length()} 个成员")
  println("  ZUNIONSTORE leaderboard:union = \{db.zunionstore("leaderboard:union", ["leaderboard", "lex:names"])} 个成员")
  println("  ZINTERCARD leaderboard/lex:names = \{db.zintercard(["leaderboard", "lex:names"], 0)}")
  
  println("\n📌 6. Key 过期与管理")
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
  let set_opt = db.set_with_options("session:opt", "v", false, false, false, Some(30), None, None, None, false)
  let set_opt_msg = match set_opt {
    Ok(_) => "OK"
    Err(error) => @lib.RedisError::message(error)
  }
  println("  SET session:opt EX 30 = \{set_opt_msg}, TTL = \{db.ttl("session:opt")}")
  println("  GETEX session:opt PERSIST = \{show_opt(db.getex("session:opt", None))}, TTL = \{db.ttl("session:opt")}")
  println("  EXPIRE session:opt NX 30 = \{db.expire_with_options("session:opt", 30, true, false, false, false)}, TTL = \{db.ttl("session:opt")}")
  db.set("session:backup", "user:1001") |> ignore
  println("  COPY session:backup -> session:copy = \{db.copy("session:backup", "session:copy")}")
  println("  TOUCH 3 keys = \{db.touch(["session:backup", "session:copy", "missing"])}")
  println("  UNLINK session:backup = \{db.unlink("session:backup")}")
  let (cursor, batch) = db.scan(0, 5)
  println("  SCAN 0 5 = cursor \{cursor}, [\{batch.join(", ")}]")
  
  println("\n📌 7. 批量操作与错误模型")
  println("----------------------------------------")
  db.mset(["config:theme", "config:lang", "config:tz"], ["dark", "zh-CN", "Asia/Shanghai"]) |> ignore
  println("  MSET 3个配置项")
  let nx_ok = db.msetnx(["config:theme", "config:new"], ["light", "x"])
  println("  MSETNX 含已存在 key = \{nx_ok}")
  let config_vals = db.mget(["config:theme", "config:lang", "config:tz", "config:nonexist"])
  println("  MGET 结果:")
  let keys = ["config:theme", "config:lang", "config:tz", "config:nonexist"]
  for i = 0; i < config_vals.length(); i = i + 1 {
    println("    \{keys[i]} = \{show_opt(config_vals[i])}")
  }
  let bad_mset = db.try_mset(["single_key"], [])
  let bad_mset_msg = match bad_mset {
    Ok(_) => "OK"
    Err(error) => @lib.RedisError::message(error)
  }
  println("  TRY_MSET 长度不一致 = \{bad_mset_msg}")
  db.set("bad_number", "abc")
  let bad_incr = db.try_incrby("bad_number", 1)
  let bad_incr_msg = match bad_incr {
    Ok(value) => "\{value}"
    Err(error) => @lib.RedisError::message(error)
  }
  println("  TRY_INCRBY 非整数 = \{bad_incr_msg}")
  
  println("\n📌 8. 服务器信息")
  println("----------------------------------------")
  println("  DBSIZE = \{db.dbsize()}")
  println("  PING = \{@lib.Database::ping()}")
  let rk = db.randomkey()
  println("  RANDOMKEY = \{show_opt(rk)}")
  let all_keys = db.keys()
  println("  KEYS * (\{all_keys.length()} 个): [\{all_keys.join(", ")}]")
  println("  支持命令数 = \{@lib.Database::command().length()}")

  println("\n📌 9. 现代语法与新命令")
  println("----------------------------------------")
  ignore(db.rpush("demo:list", "3"))
  ignore(db.rpush("demo:list", "1"))
  ignore(db.rpush("demo:list", "2"))
  let sorted_items = db.sort("demo:list", false, false, 0, 0, None)
  println("  SORT = [\{sorted_items.join(", ")}]")
  ignore(db.try_bitfield_set("demo:bits", 0, 8, false, 200))
  let bit_val = match db.try_bitfield_incrby_mode("demo:bits", 0, 8, false, 100, Some("SAT")) {
    Ok(Some(v)) => "\{v}"
    Ok(None) => "FAIL"
    Err(e) => @lib.RedisError::message(e)
  }
  println("  BITFIELD INCRBY u8 SAT = \{bit_val}")
  ignore(db.sadd("demo:set", "m1"))
  ignore(db.sadd("demo:set", "m2"))
  println("  SPOP_COUNT(1) = [\{db.spop_count("demo:set", 1).join(", ")}]")
  db.set("demo:gdel", "take-me")
  let gdel = db.try_getdel("demo:gdel")
  let gdel_text = match gdel {
    Ok(Some(v)) => v
    _ => "(nil)"
  }
  println("  GETDEL = \{gdel_text}")
  println("  EXPIRETIME(session:opt) = \{db.expiretime("session:opt").to_string()}")
  let mem_text = match db.memory_usage("demo:gdel") {
    Some(b) => "\{b}B"
    None => "(nil)"
  }
  println("  MEMORY_USAGE(demo:gdel) = \{mem_text}")

  println("\n========================================")
  println("  ✅ 演示完成！")
  println("========================================")
}
