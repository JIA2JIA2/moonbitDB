struct Product {
  id : String
  name : String
  price : Float
  stock : Int
}

fn Product::new(id : String, name : String, price : Float, stock : Int) -> Product {
  { id, name, price, stock }
}

fn main() -> Unit {
  println("========================================")
  println("  电商购物车 & 缓存系统 - 部署示例")
  println("========================================")
  
  let db = Database::new()
  
  let products = [
    Product::new("p1001", "MoonBit编程指南", 99.0, 100),
    Product::new("p1002", "高性能服务器", 2999.0, 50),
    Product::new("p1003", "机械键盘", 399.0, 200),
    Product::new("p1004", "4K显示器", 1999.0, 30),
    Product::new("p1005", "无线鼠标", 129.0, 500)
  ]
  
  println("\n📦 阶段1：商品信息缓存（String + 过期）")
  println("----------------------------------------")
  for product in products {
    let key = "product:\{product.id}"
    db.hset(key, "name", product.name) |> ignore
    db.hset(key, "price", float_to_string(product.price)) |> ignore
    db.hset(key, "stock", int_to_string(product.stock)) |> ignore
    db.expire(key, 3600) |> ignore
    println("  缓存商品: \{product.name} (ID: \{product.id}, TTL: 3600s)")
  }
  println("  缓存商品数: \{db.dbsize()}")
  
  println("\n🛒 阶段2：用户购物车（Hash）")
  println("----------------------------------------")
  let user_id = "user:1001"
  let cart_key = "cart:\{user_id}"
  
  db.hset(cart_key, "p1001", "2") |> ignore
  db.hset(cart_key, "p1003", "1") |> ignore
  db.hset(cart_key, "p1005", "3") |> ignore
  
  println("  用户 \{user_id} 的购物车:")
  let cart_items = db.hgetall(cart_key)
  let mut total = 0.0
  for (pid, qty_str) in cart_items {
    let qty = match string_to_int(qty_str) {
      Some(q) => q
      None => 0
    }
    let price = match db.hget("product:\{pid}", "price") {
      Some(p) => match string_to_float(p) {
        Some(f) => f
        None => 0.0
      }
      None => 0.0
    }
    let name = match db.hget("product:\{pid}", "name") {
      Some(n) => n
      None => "Unknown"
    }
    let subtotal = price * qty.to_float()
    total = total + subtotal
    println("    \{name} x \{qty} = ¥\{subtotal}")
  }
  println("  购物车总价: ¥\{total}")
  println("  商品种类: \{db.hlen(cart_key)}")
  
  println("\n📝 阶段3：浏览历史（List）")
  println("----------------------------------------")
  let history_key = "history:\{user_id}"
  
  let viewed = ["p1002", "p1001", "p1004", "p1003", "p1005", "p1001", "p1002"]
  for pid in viewed {
    db.lpush(history_key, pid) |> ignore
  }
  
  println("  最近浏览的5个商品:")
  let recent = db.lrange(history_key, 0, 4)
  for i = 0; i < recent.length(); i = i + 1 {
    let pid = recent[i]
    let name = match db.hget("product:\{pid}", "name") {
      Some(n) => n
      None => "Unknown"
    }
    println("    \{i + 1}. \{name} (\{pid})")
  }
  println("  历史记录总数: \{db.llen(history_key)}")
  
  println("\n⭐ 阶段4：商品收藏（Set）")
  println("----------------------------------------")
  let fav_key = "favorites:\{user_id}"
  
  db.sadd(fav_key, "p1001") |> ignore
  db.sadd(fav_key, "p1002") |> ignore
  db.sadd(fav_key, "p1004") |> ignore
  db.sadd(fav_key, "p1001") |> ignore
  
  println("  收藏商品数: \{db.scard(fav_key)}")
  let favs = db.smembers(fav_key)
  for pid in favs {
    let name = match db.hget("product:\{pid}", "name") {
      Some(n) => n
      None => "Unknown"
    }
    println("    - \{name}")
  }
  
  println("\n🔗 阶段5：共同收藏（Set交集）")
  println("----------------------------------------")
  let user2_fav = "favorites:user:1002"
  db.sadd(user2_fav, "p1001") |> ignore
  db.sadd(user2_fav, "p1003") |> ignore
  db.sadd(user2_fav, "p1005") |> ignore
  
  let common = db.sinter([fav_key, user2_fav])
  println("  两位用户共同收藏: \{common.length()} 件")
  for pid in common {
    let name = match db.hget("product:\{pid}", "name") {
      Some(n) => n
      None => "Unknown"
    }
    println("    - \{name}")
  }
  
  println("\n🏷️ 阶段6：商品标签（Set）")
  println("----------------------------------------")
  db.sadd("tag:编程", "p1001") |> ignore
  db.sadd("tag:硬件", "p1002") |> ignore
  db.sadd("tag:硬件", "p1003") |> ignore
  db.sadd("tag:硬件", "p1004") |> ignore
  db.sadd("tag:外设", "p1003") |> ignore
  db.sadd("tag:外设", "p1005") |> ignore
  db.sadd("tag:显示器", "p1004") |> ignore
  
  let hardware = db.smembers("tag:硬件")
  println("  '硬件'标签商品数: \{hardware.length()}")
  for pid in hardware {
    let name = match db.hget("product:\{pid}", "name") {
      Some(n) => n
      None => "Unknown"
    }
    println("    - \{name}")
  }
  
  let hw_and_periph = db.sinter(["tag:硬件", "tag:外设"])
  println("  既是'硬件'又是'外设': \{hw_and_periph.length()} 件")
  
  let all_tags = db.sunion(["tag:硬件", "tag:编程", "tag:外设"])
  println("  所有标签商品(去重): \{all_tags.length()} 件")
  
  println("\n📊 阶段7：销量排行榜（Sorted Set）")
  println("----------------------------------------")
  let sales_key = "sales:ranking"
  db.zadd(sales_key, 150.0, "p1001") |> ignore
  db.zadd(sales_key, 80.0, "p1002") |> ignore
  db.zadd(sales_key, 320.0, "p1003") |> ignore
  db.zadd(sales_key, 45.0, "p1004") |> ignore
  db.zadd(sales_key, 500.0, "p1005") |> ignore
  
  println("  销量 TOP 3:")
  let top3 = db.zrevrange(sales_key, 0, 2)
  for i = 0; i < top3.length(); i = i + 1 {
    let pid = top3[i]
    let name = match db.hget("product:\{pid}", "name") {
      Some(n) => n
      None => "Unknown"
    }
    let sales = match db.zscore(sales_key, pid) {
      Some(s) => s
      None => 0.0
    }
    let rank = match db.zrevrank(sales_key, pid) {
      Some(r) => r + 1
      None => 0
    }
    println("    第\{rank}名: \{name} - \{sales} 件")
  }
  
  let mid_sales = db.zrangebyscore(sales_key, 100.0, 300.0)
  println("  销量在100-300之间的商品: \{mid_sales.length()} 件")
  
  println("\n⏱️ 阶段8：会话管理（String + 过期）")
  println("----------------------------------------")
  let session_id = "sess:abc123xyz"
  db.set(session_id, "user:1001")
  db.expire(session_id, 1800) |> ignore
  
  println("  会话ID: \{session_id}")
  println("  关联用户: \{db.get(session_id)}")
  println("  剩余时间: \{db.ttl(session_id)} 秒")
  
  db.advance_time(1000000)
  println("  1000秒后...")
  println("  会话是否有效: \{db.exists(session_id)}")
  println("  TTL: \{db.ttl(session_id)}")
  
  db.advance_time(1000000)
  println("  2000秒后...")
  println("  会话是否有效: \{db.exists(session_id)}")
  
  println("\n🔄 阶段9：缓存预热 & 批量操作")
  println("----------------------------------------")
  db.flushdb()
  
  let hot_keys = ["hot:1", "hot:2", "hot:3", "hot:4", "hot:5"]
  let hot_vals = ["value1", "value2", "value3", "value4", "value5"]
  db.mset(hot_keys, hot_vals) |> ignore
  println("  批量预热 5 个热点key")
  
  let batch_result = db.mget(hot_keys)
  println("  批量读取结果:")
  for i = 0; i < batch_result.length(); i = i + 1 {
    match batch_result[i] {
      Some(v) => println("    \{hot_keys[i]} = \{v}")
      None => println("    \{hot_keys[i]} = (nil)")
    }
  }
  
  println("\n📈 阶段10：系统状态总览")
  println("----------------------------------------")
  println("  总 Key 数: \{db.dbsize()}")
  let info = db.info()
  let lines = info.split("\n")
  for line in lines {
    if line.starts_with("#") || line.starts_with("db0") || line.starts_with("uptime") {
      println("  \{line}")
    }
  }
  
  println("\n========================================")
  println("  ✅ 购物车 & 缓存系统示例完成！")
  println("========================================")
}
