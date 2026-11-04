test "sorted_set_operations" {
  let db = Database::new()
  
  db.zadd("scores", 100.0, "Alice") |> ignore
  db.zadd("scores", 90.0, "Bob") |> ignore
  db.zadd("scores", 95.0, "Charlie") |> ignore
  
  @test.assert_eq(db.zcard("scores"), 3)
  @test.assert_eq(db.zscore("scores", "Alice"), Some(100))
  
  let top_two = db.zrange("scores", 1, 2)
  @test.assert_eq(top_two.length(), 2)
  
  db.zrem("scores", "Bob") |> ignore
  @test.assert_eq(db.zcard("scores"), 2)
}

test "sorted_set_boundary_cases" {
  let db = Database::new()
  
  @test.assert_eq(db.zcard("nonexistent"), 0)
  @test.assert_eq(db.zscore("nonexistent", "member"), None)
  @test.assert_eq(db.zrem("nonexistent", "member"), false)
  
  let empty_range = db.zrange("nonexistent", 0, -1)
  @test.assert_eq(empty_range.length(), 0)
  
  @test.assert_eq(db.zadd("zset1", 10.0, "a"), true)
  @test.assert_eq(db.zadd("zset1", 20.0, "b"), true)
  @test.assert_eq(db.zadd("zset1", 10.0, "a"), false)
  
  @test.assert_eq(db.zcard("zset1"), 2)
  @test.assert_eq(db.zscore("zset1", "a"), Some(10))
  
  @test.assert_eq(db.zadd("zset1", 15.0, "a"), false)
  @test.assert_eq(db.zscore("zset1", "a"), Some(15))
  
  let all_range = db.zrange("zset1", 0, -1)
  @test.assert_eq(all_range.length(), 2)
  
  let first = db.zrange("zset1", 0, 0)
  @test.assert_eq(first.length(), 1)
  
  let last = db.zrange("zset1", -1, -1)
  @test.assert_eq(last.length(), 1)
  
  @test.assert_eq(db.zrem("zset1", "c"), false)
  @test.assert_eq(db.zrem("zset1", "a"), true)
  
  @test.assert_eq(db.zcard("zset1"), 1)
  @test.assert_eq(db.zscore("zset1", "a"), None)
  
  db.zadd("neg_scores", -5.0, "low") |> ignore
  db.zadd("neg_scores", -10.0, "lower") |> ignore
  db.zadd("neg_scores", 0.0, "zero") |> ignore
  
  let sorted_neg = db.zrange("neg_scores", 0, -1)
  @test.assert_eq(sorted_neg.length(), 3)
}

test "zrangebyscore_zcount_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  db.zadd("zset1", 5.0, "e") |> ignore
  
  let range1 = db.zrangebyscore("zset1", 2.0, 4.0)
  @test.assert_eq(range1.length(), 3)
  @test.assert_eq(range1[0], "b")
  @test.assert_eq(range1[1], "c")
  @test.assert_eq(range1[2], "d")
  
  let range2 = db.zrangebyscore("zset1", 0.0, 100.0)
  @test.assert_eq(range2.length(), 5)
  
  let range3 = db.zrangebyscore("zset1", 10.0, 20.0)
  @test.assert_eq(range3.length(), 0)
  
  let count1 = db.zcount("zset1", 2.0, 4.0)
  @test.assert_eq(count1, 3)
  
  let count2 = db.zcount("zset1", 0.0, 100.0)
  @test.assert_eq(count2, 5)
  
  let count3 = db.zcount("nonexistent", 0.0, 100.0)
  @test.assert_eq(count3, 0)
}

test "zrevrange_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  
  let rev1 = db.zrevrange("zset1", 0, -1)
  @test.assert_eq(rev1.length(), 4)
  @test.assert_eq(rev1[0], "d")
  @test.assert_eq(rev1[1], "c")
  @test.assert_eq(rev1[2], "b")
  @test.assert_eq(rev1[3], "a")
  
  let rev2 = db.zrevrange("zset1", 0, 1)
  @test.assert_eq(rev2.length(), 2)
  @test.assert_eq(rev2[0], "d")
  @test.assert_eq(rev2[1], "c")
  
  let rev3 = db.zrevrange("zset1", -2, -1)
  @test.assert_eq(rev3.length(), 2)
  @test.assert_eq(rev3[0], "b")
  @test.assert_eq(rev3[1], "a")
}

test "zrevrangebyscore_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  
  let rev1 = db.zrevrangebyscore("zset1", 4.0, 2.0)
  @test.assert_eq(rev1.length(), 3)
  @test.assert_eq(rev1[0], "d")
  @test.assert_eq(rev1[1], "c")
  @test.assert_eq(rev1[2], "b")
  
  let rev2 = db.zrevrangebyscore("zset1", 3.0, 1.0)
  @test.assert_eq(rev2.length(), 3)
  @test.assert_eq(rev2[0], "c")
  
  let rev3 = db.zrevrangebyscore("zset1", 10.0, 5.0)
  @test.assert_eq(rev3.length(), 0)
}

test "zrank_zrevrank_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 10.0, "a") |> ignore
  db.zadd("zset1", 20.0, "b") |> ignore
  db.zadd("zset1", 30.0, "c") |> ignore
  db.zadd("zset1", 40.0, "d") |> ignore
  
  @test.assert_eq(db.zrank("zset1", "a"), Some(0))
  @test.assert_eq(db.zrank("zset1", "b"), Some(1))
  @test.assert_eq(db.zrank("zset1", "d"), Some(3))
  @test.assert_eq(db.zrank("zset1", "nonexistent"), None)
  @test.assert_eq(db.zrank("nonexistent", "a"), None)
  
  @test.assert_eq(db.zrevrank("zset1", "a"), Some(3))
  @test.assert_eq(db.zrevrank("zset1", "b"), Some(2))
  @test.assert_eq(db.zrevrank("zset1", "d"), Some(0))
  @test.assert_eq(db.zrevrank("zset1", "nonexistent"), None)
}

test "zincrby_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 10.0, "a") |> ignore
  
  let score1 = db.zincrby("zset1", 5.0, "a")
  @test.assert_eq(score1, 15)
  
  @test.assert_eq(db.zscore("zset1", "a"), Some(15))
  
  let score2 = db.zincrby("zset1", -3.0, "a")
  @test.assert_eq(score2, 12)
  
  let score3 = db.zincrby("zset1", 100.0, "new_member")
  @test.assert_eq(score3, 100)
  @test.assert_eq(db.zscore("zset1", "new_member"), Some(100))
  
  let score4 = db.zincrby("nonexistent", 5.0, "x")
  @test.assert_eq(score4, 5)
}

test "zremrangebyrank_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  db.zadd("zset1", 5.0, "e") |> ignore
  
  let removed1 = db.zremrangebyrank("zset1", 0, 1)
  @test.assert_eq(removed1, 2)
  @test.assert_eq(db.zcard("zset1"), 3)
  @test.assert_eq(db.zrange("zset1", 0, -1)[0], "c")
  
  let removed2 = db.zremrangebyrank("zset1", -2, -1)
  @test.assert_eq(removed2, 2)
  @test.assert_eq(db.zcard("zset1"), 1)
  
  let removed3 = db.zremrangebyrank("nonexistent", 0, 10)
  @test.assert_eq(removed3, 0)
}

test "zremrangebyscore_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 1.0, "a") |> ignore
  db.zadd("zset1", 2.0, "b") |> ignore
  db.zadd("zset1", 3.0, "c") |> ignore
  db.zadd("zset1", 4.0, "d") |> ignore
  db.zadd("zset1", 5.0, "e") |> ignore
  
  let removed1 = db.zremrangebyscore("zset1", 2.0, 4.0)
  @test.assert_eq(removed1, 3)
  @test.assert_eq(db.zcard("zset1"), 2)
  @test.assert_eq(db.zscore("zset1", "b"), None)
  @test.assert_eq(db.zscore("zset1", "a"), Some(1))
  
  let removed2 = db.zremrangebyscore("zset1", 0.0, 100.0)
  @test.assert_eq(removed2, 2)
  @test.assert_eq(db.zcard("zset1"), 0)
  
  let removed3 = db.zremrangebyscore("nonexistent", 0.0, 100.0)
  @test.assert_eq(removed3, 0)
}

test "zpopmin_zpopmax_operations" {
  let db = Database::new()
  
  db.zadd("zset1", 10.0, "a") |> ignore
  db.zadd("zset1", 20.0, "b") |> ignore
  db.zadd("zset1", 30.0, "c") |> ignore
  db.zadd("zset1", 40.0, "d") |> ignore
  
  let min1 = db.zpopmin("zset1")
  @test.assert_eq(min1 is None, false)
  
  let min_val = match min1 {
    Some(v) => v
    None => ("", 0.0)
  }
  @test.assert_eq(min_val.0, "a")
  @test.assert_eq(db.zcard("zset1"), 3)
  
  let max1 = db.zpopmax("zset1")
  @test.assert_eq(max1 is None, false)
  
  let max_val = match max1 {
    Some(v) => v
    None => ("", 0.0)
  }
  @test.assert_eq(max_val.0, "d")
  @test.assert_eq(db.zcard("zset1"), 2)
  
  db.zpopmin("zset1") |> ignore
  db.zpopmin("zset1") |> ignore
  
  @test.assert_eq(db.zcard("zset1"), 0)
  
  let empty_min = db.zpopmin("zset1")
  @test.assert_eq(empty_min is None, true)
  
  let empty_max = db.zpopmax("zset1")
  @test.assert_eq(empty_max is None, true)
  
  let nonexistent = db.zpopmin("nonexistent")
  @test.assert_eq(nonexistent is None, true)
}

test "zset_extended_edge_cases" {
  let db = Database::new()
  
  db.zadd("zset", 1.0, "a") |> ignore
  @test.assert_eq(db.zadd("zset", 5.0, "a"), false)
  @test.assert_eq(db.zscore("zset", "a"), Some(5))
  
  @test.assert_eq(db.zscore("zset", "nonexistent"), None)
  @test.assert_eq(db.zscore("nonexistent", "a"), None)
  
  @test.assert_eq(db.zcard("nonexistent"), 0)
  
  let single = db.zrange("zset", 0, -1)
  @test.assert_eq(single.length(), 1)
  
  db.zadd("zset", 3.0, "b") |> ignore
  db.zadd("zset", 3.0, "c") |> ignore
  
  let score_range = db.zrangebyscore("zset", 3.0, 3.0)
  @test.assert_eq(score_range.length(), 2)
  
  let count_exact = db.zcount("zset", 3.0, 3.0)
  @test.assert_eq(count_exact, 2)
  
  @test.assert_eq(db.zrank("zset", "a"), Some(2))
  @test.assert_eq(db.zrank("zset", "b"), Some(0))
  
  db.zincrby("zset", -10.0, "a") |> ignore
  @test.assert_eq(db.zscore("zset", "a"), Some(-5))
  
  let rev = db.zrange("zset", 0, -1)
  @test.assert_eq(rev[0], "a")
  
  @test.assert_eq(db.zrem("zset", "nonexistent"), false)
  @test.assert_eq(db.zrem("nonexistent", "a"), false)
  
  @test.assert_eq(db.zremrangebyrank("zset", 100, 200), 0)
  @test.assert_eq(db.zremrangebyscore("zset", 100.0, 200.0), 0)
}

test "zset_cache_invalidated_on_delete_and_overwrite" {
  let db = Database::new()
  db.zadd("z", 1.0, "a") |> ignore
  db.zrange("z", 0, -1) |> ignore
  db.del("z") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  let r = db.zrange("z", 0, -1)
  @test.assert_eq(r.length(), 1)
  @test.assert_eq(r[0], "b")

  db.zadd("z2", 1.0, "a") |> ignore
  db.zrange("z2", 0, -1) |> ignore
  db.set("z2", "str") |> ignore
  db.del("z2") |> ignore
  db.zadd("z2", 5.0, "c") |> ignore
  @test.assert_eq(db.zrange("z2", 0, -1)[0], "c")
}

test "zset_score_tie_lexicographic" {
  let db = Database::new()
  db.zadd("z", 1.0, "b") |> ignore
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 1.0, "c") |> ignore
  db.zadd("z", 0.0, "z") |> ignore
  let asc = db.zrange("z", 0, -1)
  @test.assert_eq(asc.length(), 4)
  @test.assert_eq(asc[0], "z")
  @test.assert_eq(asc[1], "a")
  @test.assert_eq(asc[2], "b")
  @test.assert_eq(asc[3], "c")
  let desc = db.zrevrange("z", 0, 2)
  @test.assert_eq(desc[0], "c")
  @test.assert_eq(desc[1], "b")
  @test.assert_eq(desc[2], "a")
}

test "zset_range_by_score_bounds" {
  let db = Database::new()
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 2.0, "c") |> ignore
  db.zadd("z", 3.0, "d") |> ignore
  let mid = db.zrangebyscore("z", 2.0, 2.0)
  @test.assert_eq(mid.length(), 2)
  @test.assert_eq(mid[0], "b")
  @test.assert_eq(mid[1], "c")
  @test.assert_eq(db.zcount("z", 2.0, 2.0), 2)
  @test.assert_eq(db.zcount("z", 1.5, 2.5), 2)
  let rev = db.zrevrangebyscore("z", 2.0, 2.0)
  @test.assert_eq(rev[0], "c")
  @test.assert_eq(rev[1], "b")
  @test.assert_eq(db.zrangebyscore("z", 4.0, 5.0).length(), 0)
  @test.assert_eq(db.zcount("z", 5.0, 1.0), 0)
}

test "zset_score_edge_cases" {
  let db = Database::new()
  
  db.zadd("zset", 0.0, "zero") |> ignore
  db.zadd("zset", -1.0, "negative") |> ignore
  db.zadd("zset", 3.14, "pi") |> ignore
  db.zadd("zset", -0.5, "neg_half") |> ignore
  
  let sorted = db.zrange("zset", 0, -1)
  @test.assert_eq(sorted[0], "negative")
  @test.assert_eq(sorted[1], "neg_half")
  @test.assert_eq(sorted[2], "zero")
  @test.assert_eq(sorted[3], "pi")
  
  let neg_range = db.zrangebyscore("zset", -1.0, 0.0)
  @test.assert_eq(neg_range.length(), 3)
  
  let pos_count = db.zcount("zset", 0.0, 100.0)
  @test.assert_eq(pos_count, 2)
  
  db.zincrby("zset", 1.5, "zero") |> ignore
  @test.assert_eq(db.zscore("zset", "zero"), Some(1.5))
  
  let min_pop = db.zpopmin("zset")
  let min_val = match min_pop {
    Some(v) => v
    None => ("", 0.0)
  }
  @test.assert_eq(min_val.0, "negative")
  
  let max_pop = db.zpopmax("zset")
  let max_val = match max_pop {
    Some(v) => v
    None => ("", 0.0)
  }
  @test.assert_eq(max_val.0, "pi")
}

test "zmscore_operations" {
  let db = Database::new()
  db.zadd("z", 1.5, "a") |> ignore
  db.zadd("z", 2.5, "b") |> ignore
  let scores = db.zmscore("z", ["a", "x", "b"])
  @test.assert_eq(scores.length(), 3)
  @test.assert_eq(scores[0], Some(1.5))
  @test.assert_eq(scores[1], None)
  @test.assert_eq(scores[2], Some(2.5))
  let missing = db.zmscore("nonexistent", ["a"])
  @test.assert_eq(missing[0] is None, true)
}

test "zset_cache_consistency" {
  let db = Database::new()
  db.zadd("rank", 10.0, "alice") |> ignore
  db.zadd("rank", 20.0, "bob") |> ignore
  let r1 = db.zrange("rank", 0, -1)
  @test.assert_eq(r1[0], "alice")

  db.zadd("rank", 1.0, "alice") |> ignore
  let r2 = db.zrange("rank", 0, -1)
  @test.assert_eq(r2[0], "alice")
  @test.assert_eq(r2[1], "bob")
  @test.assert_eq(db.zrank("rank", "alice"), Some(0))
  @test.assert_eq(db.zrank("rank", "bob"), Some(1))

  db.zincrby("rank", 100.0, "alice") |> ignore
  @test.assert_eq(db.zrank("rank", "bob"), Some(0))
  @test.assert_eq(db.zrank("rank", "alice"), Some(1))

  db.zpopmax("rank") |> ignore
  @test.assert_eq(db.zrange("rank", 0, -1).length(), 1)

  db.zadd("rank", 5.0, "carol") |> ignore
  db.zrem("rank", "bob") |> ignore
  let r3 = db.zrange("rank", 0, -1)
  @test.assert_eq(r3.length(), 1)
  @test.assert_eq(r3[0], "carol")

  db.set("other", "v") |> ignore
  db.rename("rank", "rank2") |> ignore
  @test.assert_eq(db.zrange("rank2", 0, -1)[0], "carol")
}

test "zset_with_scores_operations" {
  let db = Database::new()
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 3.0, "c") |> ignore

  let asc = db.zrange_with_scores("z", 0, -1)
  @test.assert_eq(asc.length(), 3)
  @test.assert_eq(asc[0].0, "a")
  @test.assert_eq(asc[0].1, 1)
  @test.assert_eq(asc[2].0, "c")
  @test.assert_eq(asc[2].1, 3)

  let desc = db.zrevrange_with_scores("z", 0, 1)
  @test.assert_eq(desc.length(), 2)
  @test.assert_eq(desc[0].0, "c")
  @test.assert_eq(desc[0].1, 3)
  @test.assert_eq(desc[1].0, "b")
}

test "zrangebylex_operations" {
  let db = Database::new()
  for item in ["a", "b", "c", "d"] {
    db.zadd("z", 0.0, item) |> ignore
  }

  let all = db.zrangebylex("z", "-", "+")
  @test.assert_eq(all.length(), 4)
  @test.assert_eq(all[0], "a")
  @test.assert_eq(all[3], "d")

  let incl = db.zrangebylex("z", "[b", "[c")
  @test.assert_eq(incl.length(), 2)
  @test.assert_eq(incl[0], "b")
  @test.assert_eq(incl[1], "c")

  let excl = db.zrangebylex("z", "(b", "(d")
  @test.assert_eq(excl.length(), 1)
  @test.assert_eq(excl[0], "c")

  let upper = db.zrangebylex("z", "-", "(c")
  @test.assert_eq(upper.length(), 2)
  @test.assert_eq(db.zrangebylex("z", "+", "+").length(), 0)
  @test.assert_eq(db.zrangebylex("z", "b", "+").length(), 0)
}

test "zset_rank_and_lex_cache" {
  let db = Database::new()
  db.zadd("z", 1.0, "a") |> ignore
  db.zadd("z", 2.0, "b") |> ignore
  db.zadd("z", 3.0, "c") |> ignore
  @test.assert_eq(db.zrank("z", "c"), Some(2))
  @test.assert_eq(db.zrevrank("z", "a"), Some(2))

  db.zadd("z", 0.0, "c") |> ignore
  @test.assert_eq(db.zrank("z", "c"), Some(0))
  @test.assert_eq(db.zrank("z", "b"), Some(2))

  let lex1 = db.zrangebylex("z", "-", "+")
  let lex2 = db.zrangebylex("z", "-", "+")
  @test.assert_eq(lex1.length(), 3)
  @test.assert_eq(lex2[0], "a")

  db.zrem("z", "b") |> ignore
  let lex3 = db.zrangebylex("z", "-", "+")
  @test.assert_eq(lex3.length(), 2)
  @test.assert_eq(db.zrank("z", "c"), Some(0))

  db.rename("z", "z2") |> ignore
  @test.assert_eq(db.zrank("z2", "c"), Some(0))
  @test.assert_eq(db.zrange("z2", 0, -1)[0], "c")
}

test "zset_write_through_removals" {
  let db = Database::new()
  for member_val in ["b", "aa", "c", "a"] {
    db.zadd("z", 0.0, member_val) |> ignore
  }
  @test.assert_eq(db.zrangebylex("z", "-", "+"), ["a", "aa", "b", "c"])

  db.zadd("z", 0.0, "ab") |> ignore
  @test.assert_eq(db.zrangebylex("z", "-", "+"), ["a", "aa", "ab", "b", "c"])
  @test.assert_eq(db.zrange("z", 0, -1), ["a", "aa", "ab", "b", "c"])

  db.zrem("z", "aa") |> ignore
  @test.assert_eq(db.zrange("z", 0, -1), ["a", "ab", "b", "c"])
  @test.assert_eq(db.zrangebylex("z", "-", "+"), ["a", "ab", "b", "c"])

  db.zrange("z", 0, -1) |> ignore
  db.zremrangebyrank("z", 0, 0) |> ignore
  @test.assert_eq(db.zrange("z", 0, -1), ["ab", "b", "c"])

  db.zrange("z", 0, -1) |> ignore
  db.zremrangebyscore("z", 0.0, 0.0) |> ignore
  @test.assert_eq(db.zcard("z"), 0)
  @test.assert_eq(db.exists("z"), false)
}

test "zset_pop_write_through_cache" {
  let db = Database::new()
  for i = 0; i < 6; i = i + 1 {
    db.zadd("z", Float::from_int(i), "m:\{i}") |> ignore
  }
  @test.assert_eq(db.zrange("z", 0, -1).length(), 6)
  @test.assert_eq(
    match db.zpopmin("z") {
      Some((member_val, _)) => member_val
      None => ""
    },
    "m:0",
  )
  @test.assert_eq(db.zrange("z", 0, -1).length(), 5)
  @test.assert_eq(
    match db.zpopmax("z") {
      Some((member_val, _)) => member_val
      None => ""
    },
    "m:5",
  )
  @test.assert_eq(db.zrange("z", 0, -1), ["m:1", "m:2", "m:3", "m:4"])
  @test.assert_eq(db.zrangebylex("z", "-", "+").length(), 4)
}

test "zset_extended_commands" {
  let db = Database::new()
  db.zadd("z1", 1.0, "a") |> ignore
  db.zadd("z1", 2.0, "b") |> ignore
  db.zadd("z1", 3.0, "c") |> ignore
  db.zadd("z2", 10.0, "b") |> ignore
  db.zadd("z2", 20.0, "c") |> ignore
  db.zadd("z2", 30.0, "d") |> ignore

  @test.assert_eq(
    db.zunion(["z1", "z2"]),
    [("a", 1.0), ("b", 12.0), ("c", 23.0), ("d", 30.0)],
  )
  @test.assert_eq(db.zinter(["z1", "z2"]), [("b", 12.0), ("c", 23.0)])
  @test.assert_eq(db.zdiff_with_scores(["z1", "z2"]), [("a", 1.0)])
  @test.assert_eq(db.zdiff_with_scores(["z1", "missing"]), [("a", 1.0), ("b", 2.0), ("c", 3.0)])
  @test.assert_eq(
    match db.zrandmember("z1") {
      Some(_) => true
      None => false
    },
    true,
  )

  for member_val in ["a", "aa", "b", "c"] {
    db.zadd("lex", 0.0, member_val) |> ignore
  }
  @test.assert_eq(db.zrevrangebylex("lex", "+", "-"), ["c", "b", "aa", "a"])
  @test.assert_eq(db.zlexcount("lex", "[a", "(c"), 3)
  @test.assert_eq(db.zremrangebylex("lex", "[a", "(c"), 3)
  @test.assert_eq(db.zrangebylex("lex", "-", "+"), ["c"])

  for i = 0; i < 6; i = i + 1 {
    db.zadd("zs", Float::from_int(i), "m:\{i}") |> ignore
  }
  let (next, batch) = db.zscan("zs", 0, 2)
  @test.assert_eq(batch.length(), 2)
  @test.assert_eq(next, 2)
  let (next2, batch2) = db.zscan("zs", next, 100)
  @test.assert_eq(batch2.length(), 4)
  @test.assert_eq(next2, 0)
}

test "zrangebylex_binary_bounds" {
  let db = Database::new()
  for member_val in ["a", "aa", "ab", "b", "ba", "c"] {
    db.zadd("z", 0.0, member_val) |> ignore
  }
  @test.assert_eq(db.zrangebylex("z", "[aa", "(b"), ["aa", "ab"])
  @test.assert_eq(db.zrangebylex("z", "-", "(b"), ["a", "aa", "ab"])
  @test.assert_eq(db.zrangebylex("z", "(b", "+"), ["ba", "c"])
  @test.assert_eq(db.zrevrangebylex("z", "+", "[aa"), ["c", "ba", "b", "ab", "aa"])
  @test.assert_eq(db.zlexcount("z", "[aa", "(b"), 2)
  @test.assert_eq(db.zremrangebylex("z", "[aa", "(b"), 2)
  @test.assert_eq(db.zrangebylex("z", "-", "+"), ["a", "b", "ba", "c"])
}

test "zset_store_and_card_commands" {
  let db = Database::new()
  db.zadd("z1", 1.0, "a") |> ignore
  db.zadd("z1", 2.0, "b") |> ignore
  db.zadd("z1", 3.0, "c") |> ignore
  db.zadd("z2", 10.0, "b") |> ignore
  db.zadd("z2", 20.0, "c") |> ignore
  db.zadd("z2", 30.0, "d") |> ignore
  db.set("str", "v") |> ignore

  @test.assert_eq(db.zunionstore("u", ["z1", "z2"]), 4)
  @test.assert_eq(
    db.zrange_with_scores("u", 0, -1),
    [("a", 1.0), ("b", 12.0), ("c", 23.0), ("d", 30.0)],
  )
  @test.assert_eq(db.zinterstore("i", ["z1", "z2"]), 2)
  @test.assert_eq(db.zrange_with_scores("i", 0, -1), [("b", 12.0), ("c", 23.0)])
  @test.assert_eq(db.zdiffstore("d", ["z1", "z2"]), 1)
  @test.assert_eq(db.zrange_with_scores("d", 0, -1), [("a", 1.0)])
  @test.assert_eq(db.zrangestore("r", "z1", 0, 1), 2)
  @test.assert_eq(db.zrange_with_scores("r", 0, -1), [("a", 1.0), ("b", 2.0)])
  @test.assert_eq(db.zintercard(["z1", "z2"], 0), 2)
  @test.assert_eq(db.zintercard(["z1", "z2"], 1), 1)
  @test.assert_eq(db.zintercard(["z1", "str"], 0), -1)
}

test "zset_store_weights_aggregate" {
  let db = Database::new()
  db.zadd("z1", 1.0, "a") |> ignore
  db.zadd("z1", 2.0, "b") |> ignore
  db.zadd("z2", 10.0, "a") |> ignore
  db.zadd("z2", 20.0, "b") |> ignore

  @test.assert_eq(
    match db.try_zunionstore_with_options("u", ["z1", "z2"], Some([2.0, 3.0]), None) {
      Ok(count) => {
        @test.assert_eq(count, 2)
        @test.assert_eq(
          db.zrange_with_scores("u", 0, -1),
          [("a", 32.0), ("b", 64.0)],
        )
        true
      }
      Err(_) => false
    },
    true,
  )

  @test.assert_eq(
    match db.try_zunionstore_with_options("u2", ["z1", "z2"], None, Some("min")) {
      Ok(count) => {
        @test.assert_eq(count, 2)
        @test.assert_eq(
          db.zrange_with_scores("u2", 0, -1),
          [("a", 1.0), ("b", 2.0)],
        )
        true
      }
      Err(_) => false
    },
    true,
  )

  @test.assert_eq(
    match db.try_zunionstore_with_options("u3", ["z1", "z2"], None, Some("max")) {
      Ok(count) => {
        @test.assert_eq(count, 2)
        @test.assert_eq(
          db.zrange_with_scores("u3", 0, -1),
          [("a", 10.0), ("b", 20.0)],
        )
        true
      }
      Err(_) => false
    },
    true,
  )

  @test.assert_eq(
    match db.try_zinterstore_with_options("i", ["z1", "z2"], Some([2.0, 3.0]), None) {
      Ok(count) => {
        @test.assert_eq(count, 2)
        @test.assert_eq(
          db.zrange_with_scores("i", 0, -1),
          [("a", 32.0), ("b", 64.0)],
        )
        true
      }
      Err(_) => false
    },
    true,
  )

  @test.assert_eq(
    match db.try_zinterstore_with_options("i2", ["z1", "z2"], None, Some("min")) {
      Ok(count) => {
        @test.assert_eq(count, 2)
        @test.assert_eq(
          db.zrange_with_scores("i2", 0, -1),
          [("a", 1.0), ("b", 2.0)],
        )
        true
      }
      Err(_) => false
    },
    true,
  )

  @test.assert_eq(
    match db.try_zinterstore_with_options("i3", ["z1", "z2"], None, Some("max")) {
      Ok(count) => {
        @test.assert_eq(count, 2)
        @test.assert_eq(
          db.zrange_with_scores("i3", 0, -1),
          [("a", 10.0), ("b", 20.0)],
        )
        true
      }
      Err(_) => false
    },
    true,
  )
}

test "zset_error_paths_more" {
  let db = Database::new()
  db.set("str", "v") |> ignore
  db.zadd("z", 1.0, "a") |> ignore

  @test.assert_eq(
    match db.try_zscan("missing", 0, 10) {
      Ok((cursor, items)) => cursor == 0 && items.length() == 0
      Err(_) => false
    },
    true,
  )
  @test.assert_eq(
    match db.try_zunion(["missing"]) {
      Ok(items) => items.length()
      Err(_) => -1
    },
    0,
  )
  @test.assert_eq(
    match db.try_zinter(["missing"]) {
      Ok(items) => items.length()
      Err(_) => -1
    },
    0,
  )
  @test.assert_eq(
    match db.try_zdiff(["missing"]) {
      Ok(items) => items.length()
      Err(_) => -1
    },
    0,
  )
  @test.assert_eq(
    match db.try_zrandmember("missing") {
      Err(RedisError::NotFound) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.zrandmember_count("missing", 2).length(), 0)
  @test.assert_eq(db.zrandmember_count("z", 0).length(), 0)
  @test.assert_eq(db.zpopmin_count("missing", 2).length(), 0)
  @test.assert_eq(db.zpopmax_count("z", 0).length(), 0)
  @test.assert_eq(db.zpopmin_count("str", 2).length(), 0)
  @test.assert_eq(db.zintercard([], 0), 0)
  @test.assert_eq(db.zintercard(["missing"], 0), 0)
  @test.assert_eq(
    match db.try_zunionstore("d", ["str"]) {
      Err(RedisError::WrongType) => true
      _ => false
    },
    true,
  )
  @test.assert_eq(db.zunionstore("d", ["str"]), -1)
  @test.assert_eq(
    match db.try_zrangestore("d", "missing", 0, -1) {
      Ok(count) => count
      Err(_) => -1
    },
    0,
  )
  @test.assert_eq(db.zrangestore("d", "str", 0, -1), -1)
}

test "transaction_zset_operations" {
  let db = Database::new()
  
  ignore(db.multi())
  ignore(db.zadd("zset_key", 10.0, "member1"))
  ignore(db.zadd("zset_key", 20.0, "member2"))
  ignore(db.zrem("zset_key", "member1"))
  @test.assert_eq(db.exec(), "OK")
  
  @test.assert_eq(db.zrange("zset_key", 0, -1), ["member2"])
}
