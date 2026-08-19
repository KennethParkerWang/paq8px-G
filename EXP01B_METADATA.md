# EXP01B - Record detector 可靠性条件化

## 实验说明

本实验在 EXP01A 上增加 coarse record reliability，不新增 predictor。它将 RecordModel 接受当前自动检测 record length 之前与之后产生的 residual evidence 分开建模。

## 模型结构说明

- 原有 predictors：`P0 = N + NN - NNN`、`P1 = 2N - NN`、`P2 = 3N - 3NN + NNN`。
- 原有 prediction-error classes：0 至 15。
- 新增状态：每个 DEFAULT block 一个 sticky `recordLengthConfirmed` flag。
- 新增 histogram：所有未确认样本使用 ID 16。
- 已确认样本继续使用原 error-class histogram ID 0 至 15。
- Mixer inputs 保持 6 个不变。

## 在什么基础上改了什么

基础版本是 EXP01A Record ResidualMap。RecordModel 新增只读事件，表示现有 detector 是否在当前 byte 真正接受或再次确认 record length。RecordResidualModel 将该事件在当前 DEFAULT block 内保持为 sticky 状态，仅用于选择 residual histogram。

## 假设

将 detector 的 cold/unconfirmed regime 单独分配 histogram，可以避免其 residual distribution 干扰已确认 record evidence，同时保留确认前可学习的 evidence。

## 基线实验说明

EXP01A Record ResidualMap，source revision `219bb41a5a02036decc58c7d0ae3fffed8a49a51`，原始 PAQ8px v216，`-8` auto mode。

## 测试范围

Silesia 每个文件的前 32、64、128 KiB，不测试完整文件。每个 case 必须完成 compress、decompress、长度检查和 SHA-256 byte-exact 检查。

## 构建与运行信息

- Source revision：等待本次 commit。
- Release codec：`F:\paq8px\paq-default-research-20260820\builds\EXP01B\paq8px.exe`
- Release codec SHA-256：`666C2CA9EDF43AF88A0124F7FC3A1F58786FF82BF4ACD142F4E98AA2D1476988`
- Assertion-enabled Debug codec：`F:\paq8px\paq-default-research-20260820\builds\EXP01B_DEBUG\paq8px.exe`
- Debug codec SHA-256：`6B1BE1BFAE7450CC4E1357AE3BCB49BEA1A41461FCE335A48728E361321C9E3B`
- Smoke：`sao`、`x-ray`、`osdb`、`xml` 的前 32 KiB；Release 与 Debug 均通过 4/4 byte-exact roundtrip，且对应 archives 完全一致。
- 正式 36-case run：待运行。
