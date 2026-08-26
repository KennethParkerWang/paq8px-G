# PAQ8px-G：PAQ8px 通用无损压缩改进研究

这是在 PAQ8px v216 基础上进行的个人研究版本。仓库首页只记录本项目已经实现并测试过的改动，不代表上游 PAQ8px 官方版本。

## 当前版本

`main` 当前对应 `experiment/rank1-hdp`，核心版本为：

```text
PAQ8px v216 baseline
  -> RecordResidual / reliability evidence
  -> legacy evidence pruning
  -> conditional Numeric16 carry prediction
  -> HDPP_MODE=1
```

当前代码的最终改动主要位于：

- `src/model/RecordModel.*`：沿用并改进 record predictor 的 residual evidence。
- `src/model/LinearPredictionModel.*`、`src/model/SimilarityModel.*`：保留已有通用预测模型。
- `src/model/ContextModelGeneric.*`：Generic binary 路径中的模型组合。
- `src/HierarchicalPosterior.cpp/.hpp`：新增 HDPP posterior。
- `src/PredictorMain.cpp/.hpp`：将 HDPP 接到 Legacy ContextModel + SSE 的最终概率之后。
- `CMakeLists.txt`：增加 `HDPP_MODE` 编译选项。

## 做了什么

### 1. RecordResidual

PAQ 原有 `RecordModel` 已经可以发现重复 record，并计算跨 record 预测。新增路径使用 `ResidualMap` 和 prediction-error EWMA 表达“这个预测当前有多可信”，再交给 Mixer 使用。

### 2. Legacy evidence pruning

对 RecordModel 中可能重叠的旧 maps / indirect maps 逐项做消融。只有在总结果和目标文件集合都没有退化时才保留，避免简单删除有效模型。

### 3. Conditional Numeric16

仅当检测到稳定的 `rLength=2` record 时，使用已解码 low byte 修正 16-bit little-endian high-byte 的 carry/borrow 预测。该改动不写入额外 metadata，encoder 和 decoder 都能从已解码历史同步重建。

### 4. HDPP（Hierarchical Dual-Domain Posterior）

HDPP 在 Legacy bit posterior 之外增加 byte-domain posterior：

- order-0 / order-1 / hashed order-2 byte context；
- 256-way byte distribution 的 binary-prefix counts；
- low-support shrinkage；
- statistical branch 与 recurrent branch 的 reliability fusion；
- 通过 `UpdateBroadcaster` 在 encoder / decoder 两端保持因果同步。

Arithmetic coder 没有改动。HDPP 直接参与 `PredictorMain::p()` 的最终概率计算。

## 已验证结果

正式协议为 Silesia 12 个文件的前 `32/64/128 KiB`，`-8`，Repeat=1。前缀是嵌套范围，不能当作 36 个独立文件样本。

### Generic `-8`

| 版本 | 总 archive bytes | 相对 EXP00 |
|---|---:|---:|
| EXP00 原始 v216 rebuild | 612,114 | 0 B |
| EXP01A RecordResidual | 611,913 | -201 B |
| EXP01B Reliability | 611,909 | -205 B |
| EXP03B Numeric16 Conditional | 611,657 | -457 B |
| HDPP | 611,641 | -473 B |

HDPP 相对同设置的 EXP03B 减少 `16 B`。在 36 个 `-8` cases 中，14 个变好、22 个持平、没有退化。

### `-8L` attribution control

| 版本 | 总 archive bytes |
|---|---:|
| EXP03B + existing LSTM | 609,101 B |
| HDPP + existing LSTM | 609,083 B |

在相同 `-8L` 设置下，HDPP 的独立边际为 `18 B`。`-8L` 相比 `-8` 的大部分收益来自 PAQ v216 已有 LSTM，不能全部归因于 HDPP。

所有正式 Rank 1 cases 均完成 encode、decode、长度和 SHA-256 校验：`144/144 PASS`。

## 编译

需要 C++17、CMake 和可用的 C++ 编译器。Windows 可使用 Visual Studio 工程，或使用 CMake：

```powershell
cmake -S . -B build-mode1 -DHDPP_MODE=1 -DCMAKE_BUILD_TYPE=Release
cmake --build build-mode1 --config Release --parallel
```

`HDPP_MODE`：

- `0`：关闭 HDPP，作为对照；
- `1`：当前保留的 statistical posterior 模式；
- 其他模式属于研究试验配置，不能直接当作已验证的最佳版本。

## 使用

```powershell
paq8px.exe -8 input.bin
paq8px.exe -8L input.bin
paq8px.exe -d input.bin.paq8px216
```

改过概率模型的 archive 必须使用匹配的研究版 executable 解码，不能宣称与原始 PAQ8px v216 bitstream 兼容。

## 实验分支

`experiment/exp01a-record-rm`、`experiment/exp01b-record-reliability`、`experiment/exp02a-structural-mixer`、`experiment/exp03a-numeric16-raw`、`experiment/exp03b-numeric16-conditional` 和 `experiment/rank1-hdp` 保存了各阶段源码历史。`ABL_*` 分支用于验证 Record、Linear、Similarity 等已有模型的边际价值。

## 当前结论与限制

- 已确认 RecordModel、LinearPredictionModel、SimilarityModel 都有真实压缩价值。
- `EXP03B_NUMERIC16_CONDITIONAL` 是 Generic `-8` 的当前研究基线。
- HDPP 有稳定但很小的独立收益：`-16 B`（`-8`）和 `-18 B`（`-8L`）。
- HDPP 的收益不能写成“整个 Silesia 全面提升”；当前正式协议是前缀测试，且只有一次重复。
- 最大观测 Peak RAM 约 3.1 GiB；HDPP 相对匹配 `-8L` 对照的压缩时间约增加 0.85%。
- 当前版本适合研究和复现实验，不是面向生产的快速压缩器。

## 许可证

代码沿用 PAQ8px 的 GPL 许可。程序启动信息和上游项目历史中保留了许可声明；本研究分支没有另行改变许可条款。
