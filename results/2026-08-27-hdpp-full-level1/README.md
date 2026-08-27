# HDPP 完整 Silesia `-1` 结果

## 范围

- 版本：`RANK1_HDP_PREFIX256_LSTM` 研究 executable。
- 输入：12 个完整 Silesia 文件。
- 压缩等级：`-1`。
- 验证：每个文件都完成 encode、decode、长度比较和 SHA-256 比较。
- 结果：`12/12 PASS`；总输入 `211,938,580 B`，12 个独立 archive 合计 `31,554,705 B`，合计 `1.191088664 bpb`。

## 文件说明

- `full_level1_results.csv`：逐文件大小、bpb、archive/source/decoded SHA-256、encode/decode 时间、峰值内存和 roundtrip 状态。
- `summary.json`：机器可读汇总与口径说明。

## 修复记录

旧批次在 `x-ray` 的 decoded 长度不匹配后停止，因此 `xml` 没有 decoded 文件，`x-ray` 旧 decoded 也不完整。为避免覆盖原始证据，两项都在独立目录中复用既有 archive 重解码：

- `xml`：`repaired-missing-decode`，通过长度和 SHA-256 校验。
- `x-ray`：`repaired-redecode`，通过长度和 SHA-256 校验。

## 解释边界

本目录证明 HDPP 研究版在完整 Silesia 上可进行 byte-exact lossless roundtrip；它不单独证明完整 Silesia 相比 Original v216 的总体压缩改进，因为目前只有 `sao` 有同协议的完整文件 Original v216 `-1` archive-size 对照。`sao` 的 HDPP archive 为 `3,767,793 B`，baseline 为 `3,769,793 B`，减少 `2,000 B`（`0.0531%`）。
