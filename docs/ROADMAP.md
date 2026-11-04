# 后续路线图（高成本延后项评估）

以下项经评估为结构性改造，暂列延后（按优先级）：

| 项 | 评估结论 |
|----|---------|
| 事务全量排队 | TransactionOp 需扩 30+ 变体、约 50 个写命令补 in_transaction 分支；当前 MULTI 仅 17 种操作排队（已在命令表声明差异） |
| 多 DB 架构 | data/expires 直接引用约 210 处、11 个附属状态需 per-db 化；解锁 SELECT/SWAPDB/MIGRATE 真实语义 |
| ZSet 跳表 | 替换 Map+排序缓存（zadd 写穿 O(n)→O(log n)），所有 ZSet 路径改造 |
| Float→Double | 全链路破坏性签名变更（分数/浮点精度根治，见"已知差异（精度）"） |
| keyspace 事件通知 | 需事件订阅基础设施 |
| APPEND rope | 字符串存储模型变更（大值 append 优化） |
| 随机索引 | 写路径净负收益（sadd O(1)→O(n)），除非读多写少场景强需求，倾向否决 |
