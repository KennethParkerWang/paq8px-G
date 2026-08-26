# PAQ8px-G：PAQ8px 通用无损压缩改进研究

这是基于 PAQ8px v216 的个人研究版本。本文档只记录本项目已经实现、编译并用真实 archive 完成验证的内容；它不是上游 PAQ8px 官方说明。默认成果基线是 **`-1`**，`-8/-8L` 结果单独作为机制对照保存。

## 当前结论

在统一的 `-1`、12 个 Silesia 文件、每个文件前 `32/64/128 KiB`、Repeat=1 的正式协议下：

| 版本 | Archive bytes | bpb | 相对 Original v216 |
|---|---:|---:|---:|
| Original v216 `-1` | 647,334 | 1.881434849 | 0 B |
| EXP03B Numeric16 Conditional `-1` | 646,635 | 1.879403251 | -699 B（-0.1080%） |
| Rank 1 HDPP `-1` | **646,628** | **1.879382906** | **-706 B（-0.1091%）** |

因此，当前研究版在这组前缀测试上相对原始 v216 `-1` 有客观但较小的总收益；HDPP 在 EXP03B 之上只增加了 `7 B` 的边际收益。这个结论不能外推成“完整 Silesia 全部提升”。

## 评测协议（默认成果口径）

- 数据集：Silesia corpus 中的 12 个文件：`dickens`、`mozilla`、`mr`、`nci`、`ooffice`、`osdb`、`reymont`、`samba`、`sao`、`webster`、`x-ray`、`xml`。
- 每个文件分别取前 `32 KiB`、前 `64 KiB`、前 `128 KiB`。
- `64 KiB` 包含前 `32 KiB`，`128 KiB` 包含前 `64 KiB`；它们是嵌套前缀，不是 36 个互相独立的文件。
- 压缩等级：`-1`。
- processing：`auto`；Repeat=1。
- 总 case 数：`12 × 3 = 36`。
- 每个版本都执行 encode、decode、解压长度检查和 SHA-256 重算。
- 总输入字节数：`2,752,512 B`。
- Original、EXP03B 和 HDPP 均为 `36/36 PASS`。

正式结果文件：

```text
F:/paq8px/paq-default-research-20260820/runs-prefix-level1/comparison_20260820/level1_prefix_summary.csv
F:/paq8px/paq-default-research-20260820/runs-prefix-level1/comparison_20260820/level1_prefix_comparison.csv
F:/paq8px/paq-default-research-20260820/runs-prefix-level1/comparison_20260821_level1_historical_variants.csv
```

## `-1` 按前缀范围汇总

| 范围 | Original v216 | EXP03B | HDPP | HDPP 相对 Original |
|---|---:|---:|---:|---:|
| 前 32 KiB（12 文件合计） | 100,534 B | 100,404 B | **100,402 B** | -132 B |
| 前 64 KiB（12 文件合计） | 188,536 B | 188,315 B | **188,312 B** | -224 B |
| 前 128 KiB（12 文件合计） | 358,264 B | 357,916 B | **357,914 B** | -350 B |
| **三种范围合计** | **647,334 B** | **646,635 B** | **646,628 B** | **-706 B** |

## `-1` 逐文件结果（三个范围相加）

下表不是完整文件大小，而是同一文件前 `32/64/128 KiB` 三个 archive 的合计。这样可以看出改进来自哪些文件，同时避免把前缀合计误写成完整文件结果。

| 文件 | Original v216 | EXP03B | HDPP | HDPP 相对 Original |
|---|---:|---:|---:|---:|
| dickens | 60,557 | 60,557 | 60,557 | 0 B |
| mozilla | 67,001 | 66,982 | 66,981 | -20 B |
| mr | 51,968 | 51,806 | 51,806 | -162 B |
| nci | 9,624 | 9,624 | 9,624 | 0 B |
| ooffice | 69,805 | 69,801 | 69,800 | -5 B |
| osdb | 63,416 | 63,414 | 63,413 | -3 B |
| reymont | 36,461 | 36,461 | 36,460 | -1 B |
| samba | 6,881 | 6,881 | 6,881 | 0 B |
| sao | 129,041 | 128,803 | 128,803 | -238 B |
| webster | 44,125 | 44,125 | 44,124 | -1 B |
| x-ray | 104,313 | 104,039 | 104,037 | -276 B |
| xml | 4,142 | 4,142 | 4,142 | 0 B |
| **合计** | **647,334** | **646,635** | **646,628** | **-706 B** |

改善主要集中在 `sao`、`x-ray` 和 `mr`；`dickens`、`nci`、`samba`、`xml` 在这组前缀口径下没有变化。这是按文件的观测结果，不是文件名特判，也不是算法路由规则。

## 已完成的改进版本

### 1. RecordResidual / Reliability

位置：`src/model/RecordModel.*` 以及 Generic 模型组合路径。

原始问题：PAQ 原有 `RecordModel` 能发现重复 record 并产生跨 record 预测，但旧式 `StationaryMap` / `IndirectMap` evidence 没有明确表达预测当前是否可靠。

修改：保留原有 record detector 和跨 record predictor，增加基于 `ResidualMap` 的 residual evidence，并用 prediction-error EWMA 对 error class / reliability 做条件化。

目的：把“已有预测值”转换成更容易被 Mixer 利用的概率证据，而不是简单再增加一个重复 predictor。

`-1` 结果：

| 版本 | Archive bytes | 相对 Original |
|---|---:|---:|
| EXP01A RecordResidual | 647,021 | -313 B |
| EXP01B RecordReliability | 647,003 | -331 B |

判断：**有收益**。Reliability 在 RecordResidual 基础上进一步减少 `18 B`。

### 2. Legacy evidence pruning

位置：`RecordModel` 的旧 `maps[]` / `iMap[]` evidence 组合。

原始问题：新增 residual path 后，旧 evidence 可能与新路径重复；逐项关闭可以测量边际贡献，但不能预先假设“旧模型都没用”。

修改：逐项消融 `map4`、`iMap2`、`map3`、`iMap1`、`map2`、`iMap0`，每次都重新压缩并做 roundtrip。

`-1` 结果：

| 版本 | Archive bytes | 相对 Original |
|---|---:|---:|
| EXP01C prune map4 | 647,108 | -226 B |
| EXP01D prune iMap2 | 647,039 | -295 B |
| EXP01E prune map3 | 647,026 | -308 B |
| EXP01F prune iMap1 | 647,006 | -328 B |
| EXP01G prune map2 | 647,009 | -325 B |
| EXP01H prune iMap0 | 647,009 | -325 B |

判断：**没有一项超过 EXP01B**。它们是边际贡献诊断，不应被写成独立的最终优化；当前最佳仍保留必要的 legacy evidence。

### 3. Structural mixer

位置：Generic mixer 的结构状态输入。

原始问题：Record / Similarity 可以发现结构，但 Generic downstream 是否能从一个额外 structural regime 中获益需要验证。

修改：测试小型 structural mixer context，未使用文件名、格式解析或 benchmark 特判。

`-1` 结果：`647,008 B`，相对 Original `-326 B`，仍低于 EXP01B 的 `647,003 B`。

判断：**局部有收益但不是当前最佳；作为最终版本不保留。**

### 4. Numeric16 Raw 与 Conditional

位置：`src/model/LinearPredictionModel.*` / `src/model/ContextModelGeneric.*` 相关 Generic 路径。

原始问题：部分稳定 `rLength=2` 数据可能具有 little-endian 16-bit 数值的 carry / borrow 关系，逐 byte prediction 没有充分利用已解码 low byte。

修改：

- `EXP03A_NUMERIC16_RAW`：测试未经稳定条件约束的 Numeric16 路径。
- `EXP03B_NUMERIC16_CONDITIONAL`：仅在检测到稳定 `rLength=2` record 时启用，并使用因果可得的 low byte 修正 high-byte prediction。
- 不写入额外 metadata；encoder / decoder 都从已解码历史同步重建。

`-1` 结果：

| 版本 | Archive bytes | 相对 Original |
|---|---:|---:|
| EXP03A Numeric16 Raw | 646,929 | -405 B |
| EXP03B Numeric16 Conditional | **646,635** | **-699 B** |

判断：**Conditional 版本是当前 HDPP 的直接 baseline，并且是本周最有实际收益的前置改进。** 结果也说明 Numeric16 不能不加条件地泛化到所有输入。

### 5. HDPP（Hierarchical Dual-Domain Posterior）

位置：

- `src/HierarchicalPosterior.hpp/.cpp`：新增 posterior subsystem。
- `src/PredictorMain.hpp/.cpp`：在 Legacy bit posterior 与 SSE 输出之后接入 HDPP。
- `CMakeLists.txt`：提供 `HDPP_MODE` 编译开关。

机制：

- Legacy PAQ bit/context probability 继续保留；
- byte domain 增加 order-0、order-1、hashed order-2 byte context；
- 256-way byte distribution 转成 binary-prefix counts；
- low-support shrinkage 减少冷启动时的过度自信；
- statistical branch 与 recurrent branch 做 reliability fusion；
- `UpdateBroadcaster` 保证 encoder / decoder 以相同因果状态更新；
- arithmetic coder 没有改动，HDPP 参与的是最终概率计算。

`-1` 结果：HDPP 相对 EXP03B 再减少 `7 B`，总计相对 Original 减少 `706 B`。判断：**有小幅真实收益，但不是大幅改进。**

## `-1` 后续验证但未纳入当前最佳版本

### EXP04A Causal Regret / CRRG

该实验尝试用实际因果 log-loss 后悔值动态调整 Legacy 与 HDPP 的融合权重。

| 版本 | Archive bytes | 相对 HDPP | 状态 |
|---|---:|---:|---|
| HDPP parent | 646,628 | 0 B | 当前 parent |
| EXP04A causal regret v0 | 646,627 | -1 B | 预实验，未保留 |
| EXP04A CRRG global | 646,630 | +2 B | **REJECT** |

两种版本均为 `36/36 PASS`。v0 只少 `1 B`，CRRG global 反而多 `2 B`，因此不能写成稳定改进。详细记录见：

```text
F:/paq8px/paq-default-research-20260820/rank1-hdp-20260820/exp04a-causal-regret/EXP04A_RESULTS.md
F:/paq8px/paq-default-research-20260820/runs-prefix-level1/EXP04A_CRRG_GLOBAL_L1_MATCHED_20260826/results.csv
```

## 历史 `-8` / `-8L` 结果（与 `-1` 分开）

这些结果来自较高 compression level，不能替代用户默认的 `-1` baseline，也不能和上面的 `-1` bytes 直接比较。

| 版本 | `-8` Archive bytes | 相对 EXP00 `-8` |
|---|---:|---:|
| EXP00 原始 v216 rebuild | 612,114 | 0 B |
| EXP01A RecordResidual | 611,913 | -201 B |
| EXP01B Reliability | 611,909 | -205 B |
| EXP03B Numeric16 Conditional | 611,657 | -457 B |
| Rank 1 HDPP | **611,641** | **-473 B** |

在相同 `-8` 设置下，HDPP 相对 EXP03B 减少 `16 B`。`-8L` attribution control 为：EXP03B + existing LSTM `609,101 B`，HDPP + existing LSTM `609,083 B`，HDPP 的独立边际是 `18 B`。`-8L` 的大部分收益来自 PAQ v216 已有 LSTM，不能全部归因于 HDPP。

## 完整文件结果的边界

目前只有 `sao` 做过严格成对的完整文件 `-1` 对比：

| 文件 | Original v216 `-1` | HDPP `-1` | 改善 |
|---|---:|---:|---:|
| `sao` 完整文件 | 3,769,793 B | 3,767,793 B | -2,000 B（约 -0.0531%） |

该结果已通过 roundtrip，但不能写成“完整 Silesia 总体提升”。其余文件目前没有形成同一协议下完整成对结果，正式成果仍以 `32/64/128 KiB` 前缀协议为准。

## 源码与编译

当前研究版主要改动文件：

```text
src/model/RecordModel.*
src/model/LinearPredictionModel.*
src/model/SimilarityModel.*
src/model/ContextModelGeneric.*
src/HierarchicalPosterior.hpp
src/HierarchicalPosterior.cpp
src/PredictorMain.hpp
src/PredictorMain.cpp
CMakeLists.txt
```

编译要求 C++17、CMake 和可用的 C++ 编译器：

```powershell
cmake -S . -B build-mode1 -DHDPP_MODE=1 -DCMAKE_BUILD_TYPE=Release
cmake --build build-mode1 --config Release --parallel
```

`HDPP_MODE`：

- `0`：关闭 HDPP，用于对照；
- `1`：当前研究版 statistical posterior 模式；
- 其他值属于试验配置，不能直接当作已验证最佳版本。

运行示例：

```powershell
paq8px.exe -1 input.bin
paq8px.exe -d input.bin.paq8px216
```

改过概率模型的 archive 必须使用匹配的研究版 executable 解码，不能宣称与原始 PAQ8px v216 bitstream 兼容。

## 当前判断与限制

- `-1` 是本项目默认成果口径；当前最好版本是 `Rank 1 HDPP`，但相对原始 `-1` 的提升只有 `706 B / 0.1091%`。
- EXP03B Numeric16 Conditional 提供了主要收益（`-699 B`）；HDPP 在其上只有 `-7 B`，说明当前 posterior 新信息有限，不能夸大 HDPP 的贡献。
- 收益集中在 `sao`、`x-ray`、`mr` 等部分文件，多个文件持平；不存在已证明的“完整 Silesia 全面优于 Original -1”结论。
- 前缀数据是嵌套范围，不能把 32/64/128 KiB 当成 36 个独立样本进行统计显著性推断。
- 每次正式结果 Repeat=1；还缺少多次重复和完整 Silesia `-1` 成对实验，因此速度差异和小于数十 bytes 的变化应谨慎解读。
- 已观察最大 Peak RAM 约 `3.1 GiB`；HDPP 属于研究版，资源开销和压缩速度仍需进一步优化。

## 分支和实验记录

已保存的阶段分支包括：

```text
experiment/exp01a-record-rm
experiment/exp01b-record-reliability
experiment/exp02a-structural-mixer
experiment/exp03a-numeric16-raw
experiment/exp03b-numeric16-conditional
experiment/rank1-hdp
```

`ABL_*` 分支用于检查 Record、Linear、Similarity 等已有模型的边际价值。完整逐 case 数据和实验日志保存在 `paq-default-research-20260820/runs-prefix-level1/`，README 只摘录可复核的汇总。

## 研究节点：大改前快照（2026-08-27）

这是后续大规模重构前的可回溯节点，旧实现、实验结果和结论不应被覆盖：

- 节点名/tag：`checkpoint/pre-major-refactor-20260827`。
- 基准分支：`experiment/rank1-hdp`。
- README 更新前的代码提交：`8104a91`（HDPP 当前研究版）。本节点 tag 指向本次 README 更新后的提交。
- 默认成果口径固定为：PAQ8px v216、`-1`、12 个 Silesia 文件的前 `32/64/128 KiB`，共 36 cases，Repeat=1，并通过 encode/decode/SHA-256 校验。
- 当前前缀测试最佳结果：Rank 1 HDPP 为 `646,628 B`，比 Original v216 `-1` 少 `706 B`（`-0.1091%`）；EXP03B Numeric16 Conditional 是主要收益来源，HDPP 额外减少 `7 B`。
- 完整文件只把 `sao` 记为严格成对 PASS：`3,769,793 B → 3,767,793 B`，减少 `2,000 B`。其它完整文件没有形成同一协议下的完整成对 PASS，不能外推为完整 Silesia 总体提升。
- 后续阶段可以修改模型、Mixer、SSE/APM、coder、block/type 路径和 archive format；所有新增结果必须与本节点的 `-1` 前缀 baseline 及 roundtrip 校验对比。

回溯或查看该阶段：

```powershell
git switch --detach checkpoint/pre-major-refactor-20260827
git show --stat checkpoint/pre-major-refactor-20260827
```

## 许可证

代码沿用 PAQ8px 的 GPL 许可。本研究分支没有另行改变许可条款。
