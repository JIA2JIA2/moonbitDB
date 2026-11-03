test "bench zset read paths" (it : @bench.T) {
  let db = @lib.Database::new()
  for i = 0; i < 1000; i = i + 1 {
    db.zadd("rank", Float::from_int(i), "member:\{i}") |> ignore
  }
  db.zrange("rank", 0, -1) |> ignore
  db.zrank("rank", "member:500") |> ignore
  db.zrangebylex("rank", "-", "+") |> ignore
  it.bench(name="zrange 1000", () => {
    db.zrange("rank", 0, -1) |> ignore
  })
  it.bench(name="zrangebyscore 1000", () => {
    db.zrangebyscore("rank", 250.0, 750.0) |> ignore
  })
  it.bench(name="zcount 1000", () => {
    db.zcount("rank", 250.0, 750.0) |> ignore
  })
  it.bench(name="zrank 1000", () => {
    db.zrank("rank", "member:500") |> ignore
  })
  it.bench(name="zrangebylex 1000", () => {
    db.zrangebylex("rank", "-", "+") |> ignore
  })
}

test "bench scan" (it : @bench.T) {
  let db = @lib.Database::new()
  for i = 0; i < 1000; i = i + 1 {
    db.set("scan:\{i}", "v") |> ignore
  }
  let (cursor, _) = db.scan(0, 100)
  it.bench(name="scan 1000 batch 100", () => {
    db.scan(cursor, 100) |> ignore
  })
}

test "bench list lrem" (it : @bench.T) {
  let db = @lib.Database::new()
  for i = 0; i < 1000; i = i + 1 {
    db.rpush("tasks", "task:\{i % 10}") |> ignore
  }
  it.bench(name="lrem 1000", () => {
    db.lrem("tasks", 10, "task:5") |> ignore
  })
}

test "bench keys pattern" (it : @bench.T) {
  let db = @lib.Database::new()
  for i = 0; i < 500; i = i + 1 {
    db.set("user:\{i}", "v") |> ignore
  }
  it.bench(name="keys_pattern 500", () => {
    db.keys_pattern("user:1*") |> ignore
  })
}