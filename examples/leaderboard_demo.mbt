struct Player {
  id : String
  name : String
}

fn Player::new(id : String, name : String) -> Player {
  { id, name }
}

fn main() -> Unit {
  let db = Database::new()
  
  println("========================================")
  println("  游戏排行榜系统 - 部署示例")
  println("========================================")
  
  let players = [
    Player::new("p1", "Alice"),
    Player::new("p2", "Bob"),
    Player::new("p3", "Charlie"),
    Player::new("p4", "David"),
    Player::new("p5", "Eve"),
    Player::new("p6", "Frank"),
    Player::new("p7", "Grace"),
    Player::new("p8", "Henry")
  ]
  
  println("\n📊 阶段1：初始化玩家分数")
  println("----------------------------------------")
  for i = 0; i < players.length(); i = i + 1 {
    let player = players[i]
    let score = (i * 100 + 50).to_float()
    db.zadd("leaderboard:global", score, player.id) |> ignore
    db.hset("player:\{player.id}", "name", player.name) |> ignore
    db.hset("player:\{player.id}", "score", int_to_string(i * 100 + 50)) |> ignore
    println("  玩家 \{player.name} (ID: \{player.id}): \{i * 100 + 50} 分")
  }
  
  println("\n🏆 阶段2：查询排行榜 TOP 5")
  println("----------------------------------------")
  let top5_ids = db.zrevrange("leaderboard:global", 0, 4)
  println("  排名 | 玩家ID | 玩家名称 | 分数")
  println("  -----|--------|----------|------")
  for i = 0; i < top5_ids.length(); i = i + 1 {
    let pid = top5_ids[i]
    let pname = match db.hget("player:\{pid}", "name") {
      Some(n) => n
      None => "Unknown"
    }
    let score = db.zscore("leaderboard:global", pid)
    let score_str = match score {
      Some(s) => "\{s}"
      None => "N/A"
    }
    println("   #\{i + 1}  | \{pid}     | \{pname}    | \{score_str}")
  }
  
  println("\n🔄 阶段3：模拟游戏对局更新分数")
  println("----------------------------------------")
  let score_updates = [
    ("p1", 250.0),
    ("p3", 180.0),
    ("p5", 320.0),
    ("p7", 410.0)
  ]
  for i = 0; i < score_updates.length(); i = i + 1 {
    let (pid, add_score) = score_updates[i]
    let new_score = db.zincrby("leaderboard:global", add_score, pid)
    let pname = match db.hget("player:\{pid}", "name") {
      Some(n) => n
      None => "Unknown"
    }
    println("  \{pname} 增加 \{add_score} 分，新总分: \{new_score}")
  }
  
  println("\n🏆 阶段4：更新后的完整排行榜")
  println("----------------------------------------")
  let all_ids = db.zrevrange("leaderboard:global", 0, -1)
  println("  排名 | 玩家ID | 玩家名称 | 分数 | 排名变化")
  println("  -----|--------|----------|------|----------")
  for i = 0; i < all_ids.length(); i = i + 1 {
    let pid = all_ids[i]
    let pname = match db.hget("player:\{pid}", "name") {
      Some(n) => n
      None => "Unknown"
    }
    let score = match db.zscore("leaderboard:global", pid) {
      Some(s) => "\{s}"
      None => "N/A"
    }
    let new_rank = db.zrevrank("leaderboard:global", pid)
    let rank_str = match new_rank {
      Some(r) => "#\{r + 1}"
      None => "N/A"
    }
    let old_idx = find_player_index(players, pid)
    let change = if old_idx >= 0 { old_idx - i } else { 0 }
    let change_str = if change > 0 { "↑\{change}" } else if change < 0 { "↓\{-change}" } else { "—" }
    println("   \{rank_str}  | \{pid}     | \{pname}    | \{score} | \{change_str}")
  }
  
  println("\n📈 阶段5：分数区间统计")
  println("----------------------------------------")
  let count_200_400 = db.zcount("leaderboard:global", 200.0, 400.0)
  println("  200-400 分区间玩家数: \{count_200_400}")
  
  let mid_players = db.zrangebyscore("leaderboard:global", 100.0, 300.0)
  println("  100-300 分区间玩家: \{mid_players.join(", ")}")
  
  println("\n🎮 阶段6：每日排行榜（带过期）")
  println("----------------------------------------")
  db.zadd("leaderboard:daily", 500.0, "p1") |> ignore
  db.zadd("leaderboard:daily", 400.0, "p2") |> ignore
  db.zadd("leaderboard:daily", 300.0, "p3") |> ignore
  db.expire("leaderboard:daily", 86400) |> ignore
  println("  每日排行榜已创建，24小时后自动过期")
  println("  TTL: \{db.ttl("leaderboard:daily")} 秒")
  
  println("\n👑 阶段7：弹出每日前三名领奖")
  println("----------------------------------------")
  for i = 0; i < 3; i = i + 1 {
    let winner = db.zpopmax("leaderboard:daily")
    match winner {
      Some((pid, score)) => {
        let pname = match db.hget("player:\{pid}", "name") {
          Some(n) => n
          None => "Unknown"
        }
        println("  第 \{i + 1} 名: \{pname} (\{pid}) - \{score} 分")
      }
      None => println("  第 \{i + 1} 名: 无")
    }
  }
  
  println("\n📦 阶段8：使用Set管理好友关系")
  println("----------------------------------------")
  db.sadd("friends:p1", "p2") |> ignore
  db.sadd("friends:p1", "p3") |> ignore
  db.sadd("friends:p1", "p4") |> ignore
  db.sadd("friends:p2", "p1") |> ignore
  db.sadd("friends:p2", "p3") |> ignore
  db.sadd("friends:p2", "p5") |> ignore
  
  let mutual = db.sinter(["friends:p1", "friends:p2"])
  println("  Alice 和 Bob 的共同好友数: \{mutual.length()}")
  println("  共同好友ID: \{mutual.join(", ")}")
  
  let all_friends = db.sunion(["friends:p1", "friends:p2"])
  println("  两人好友总数（去重）: \{all_friends.length()}")
  
  let only_p1 = db.sdiff(["friends:p1", "friends:p2"])
  println("  仅 Alice 有的好友: \{only_p1.join(", ")}")
  
  println("\n📋 阶段9：使用List管理消息队列")
  println("----------------------------------------")
  let messages = [
    "系统维护通知",
    "新版本发布",
    "活动奖励发放",
    "赛季更新预告"
  ]
  for msg in messages {
    db.rpush("notifications:global", msg) |> ignore
  }
  println("  消息队列长度: \{db.llen("notifications:global")}")
  
  println("  逐条消费消息:")
  while db.llen("notifications:global") > 0 {
    let msg = db.lpop("notifications:global")
    match msg {
      Some(m) => println("    -> 处理: \{m}")
      None => break
    }
  }
  println("  队列剩余长度: \{db.llen("notifications:global")}")
  
  println("\n📊 阶段10：服务器状态统计")
  println("----------------------------------------")
  println("  总 Key 数: \{db.dbsize()}")
  println("  随机 Key: \{db.randomkey()}")
  println("  支持命令数: \{Database::command().length()}")
  
  let info = db.info()
  let info_lines = info.split("\n")
  for line in info_lines {
    if line.starts_with("db0") {
      println("  数据库: \{line}")
    }
  }
  
  println("\n========================================")
  println("  ✅ 排行榜系统示例运行完成！")
  println("========================================")
}

fn find_player_index(players : Array[Player], id : String) -> Int {
  for i = 0; i < players.length(); i = i + 1 {
    if players[i].id == id {
      return i
    }
  }
  -1
}
