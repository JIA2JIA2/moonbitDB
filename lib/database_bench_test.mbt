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

test "bench copy zset" (it : @bench.T) {
  let db = @lib.Database::new()
  for i = 0; i < 500; i = i + 1 {
    db.zadd("src", Float::from_int(i), "member:\{i}") |> ignore
  }
  it.bench(name="copy zset 500", () => {
    db.copy_with_replace("src", "dst", true) |> ignore
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

test "bench write paths" (it : @bench.T) {
  let chunk = "abcdefghijklmnop"
  it.bench(name="append 256B", () => {
    let db = @lib.Database::new()
    for i = 0; i < 16; i = i + 1 {
      db.append("s", chunk) |> ignore
    }
  })
  it.bench(name="zadd 1000", () => {
    let db = @lib.Database::new()
    for i = 0; i < 1000; i = i + 1 {
      db.zadd("z", Float::from_int(i), "member:\{i}") |> ignore
    }
  })
  it.bench(name="expire 1000", () => {
    let db = @lib.Database::new()
    for i = 0; i < 1000; i = i + 1 {
      db.set("ttl:\{i}", "v") |> ignore
      db.pexpire("ttl:\{i}", 1000) |> ignore
    }
    db.advance_time(1000)
    db.dbsize() |> ignore
  })
}

test "bench new feature paths" (it : @bench.T) {
  let db = @lib.Database::new()
  for i = 0; i < 1000; i = i + 1 {
    ignore(db.zadd("zs", Float::from_int(i), "member:\{i}"))
    ignore(db.set("s:\{i}", "value-\{i}"))
  }
  ignore(db.hset("weights", "w_member:1", "1.0"))
  ignore(db.set("bitkey", ""))
  it.bench(name="sort by 1000", () => {
    ignore(db.try_sort_advanced("zs", Some("s_*"), [], false, false, 0, 0, None))
  })
  it.bench(name="bitfield get 16", () => {
    ignore(db.try_bitfield_get("bitkey", 0, 16, false))
  })
  it.bench(name="bitfield set 16", () => {
    ignore(db.try_bitfield_set("bitkey", 0, 16, false, 12345))
  })
  it.bench(name="setbit 64", () => {
    for i = 0; i < 64; i = i + 1 {
      ignore(db.try_setbit("bitkey", i, 1))
    }
  })
  it.bench(name="zrangestore byscore 250", () => {
    ignore(db.try_zrangestore_byscore("zdst", "zs", "0", "250", 0, 0))
  })
  it.bench(name="zunionstore 1000", () => {
    ignore(db.zunionstore("zu", ["zs", "zs"]))
  })
  it.bench(name="exists_many 100", () => {
    let keys : Array[String] = []
    for i = 0; i < 100; i = i + 1 {
      keys.push("s:\{i}")
    }
    ignore(db.exists_many(keys))
  })
  it.bench(name="lpos_options 100", () => {
    ignore(db.try_lpos_options("zs", "member:1", 1, 0, 0))
  })
}
