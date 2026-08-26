#pragma once

#include "IPredictor.hpp"
#include "Shared.hpp"

#include <array>
#include <cstdint>
#include <vector>

#ifndef HDPP_MODE
#define HDPP_MODE 1
#endif

/**
 * Causal dual-domain posterior used by the Rank 1 experiments.
 *
 * The legacy ContextModel/SSE remains the primary expert.  This class adds a
 * byte-domain 256-way posterior (represented as binary prefix counts), a
 * fixed-point recurrent long-context branch, and an online mixture/calibration
 * layer.  It is an IPredictor so that observation happens after Shared has
 * committed the decoded bit and, at bitPosition == 0, the completed byte.
 */
class HierarchicalPosterior final : public IPredictor {
public:
  explicit HierarchicalPosterior(Shared* const sh);
  ~HierarchicalPosterior() override;

  /** Fuse the causal posterior with the already calibrated legacy result. */
  uint32_t predict(uint32_t legacyP);

  /** Receive the post-bit update broadcast from Shared. */
  void update() override;

private:
  static constexpr uint32_t CODER_MAX = (1u << 31) - 1;
  static constexpr uint32_t SYMBOL_MAX = 4095;

  struct ByteContext {
    uint8_t previous1 = 0;
    uint8_t previous2 = 0;
    uint8_t blockType = 0;
    uint64_t rollingHash = 0;
  };

  struct Prediction {
    uint32_t p12 = 2048;
    uint32_t support = 0;
  };

  /** A compact binary trie for a 256-way symbol distribution. */
  class ByteTrie {
  public:
    explicit ByteTrie(uint32_t contexts);

    Prediction predict(uint32_t context, uint8_t prefix, uint8_t bits) const;
    void observe(uint32_t context, uint8_t symbol);

  private:
    static constexpr uint32_t INTERNAL_NODES = 255;
    static constexpr uint16_t DECAY_LIMIT = 60000;

    uint32_t contextCount;
    std::vector<uint16_t> zero;
    std::vector<uint16_t> one;
    std::vector<uint16_t> totals;

    void decay(uint32_t context);
  };

  class StatisticalBranch {
  public:
    explicit StatisticalBranch(uint8_t level);

    ByteContext beginByte(uint8_t blockType);
    Prediction predict(const ByteContext& context, uint8_t prefix, uint8_t bits) const;
    void observe(const ByteContext& context, uint8_t symbol);

  private:
    ByteTrie order0;
    ByteTrie order1;
    ByteTrie order2;
    uint32_t order2Mask;
    ByteContext nextContext;

    static uint64_t mix64(uint64_t x);
    uint32_t order2Key(const ByteContext& context) const;
  };

  /**
   * Small deterministic recurrent residual model.  It is intentionally
   * integer-only: no weights or side information are transmitted, and both
   * encoder and decoder rebuild the same state from decoded bytes.
   */
  class LongContextNeuralBranch {
  public:
    explicit LongContextNeuralBranch(uint8_t level);

    void beginByte(const ByteContext& context);
    Prediction predict(uint8_t prefix, uint8_t bits);
    void observeBit(uint8_t bit);
    void observeByte(uint8_t symbol, const ByteContext& context);

  private:
    static constexpr uint32_t HIDDEN = 16;
    static constexpr uint32_t BIT_POSITIONS = 8;

    std::vector<int16_t> contextLogits;
    std::vector<uint16_t> contextCounts;
    uint32_t contextMask;
    uint32_t currentKey = 0;
    uint8_t currentBitPosition = 0;
    int16_t currentLogit = 0;
    std::array<int16_t, HIDDEN> fastState{};
    std::array<int16_t, HIDDEN> slowState{};
    std::array<std::array<int16_t, HIDDEN>, BIT_POSITIONS> outputWeights{};
    std::array<int16_t, BIT_POSITIONS> outputBias{};

    static uint64_t mix64(uint64_t x);
    static int16_t clamp16(int32_t x);
    static uint32_t squashToSymbol(int32_t logit);
  };

  Shared* const shared;
  StatisticalBranch statistical;
  LongContextNeuralBranch neural;

  ByteContext currentContext{};
  bool byteActive = false;
  uint8_t lastBitPosition = 0;
  uint32_t lastLegacyP = 2048u << 19;
  uint32_t lastStatP = 2048u << 19;
  uint32_t lastNeuralP = 2048u << 19;
  uint32_t lastSymbolP = 2048u << 19;
  uint32_t lastFinalP = 2048u << 19;
  uint32_t lastStatSupport = 0;
  uint32_t lastNeuralSupport = 0;

  uint64_t bitCount = 0;
  uint64_t byteCount = 0;
  uint64_t disagreementCount = 0;
  uint64_t legacyLossSum = 0;
  uint64_t statLossSum = 0;
  uint64_t neuralLossSum = 0;
  uint64_t symbolLossSum = 0;
  uint64_t finalLossSum = 0;
  uint32_t legacyLossEwma = 2048;
  uint32_t statLossEwma = 2048;
  uint32_t neuralLossEwma = 2048;
  uint32_t symbolLossEwma = 2048;
  uint32_t fusionWeight = 32; // 0..128 in 1/256 probability-mass units
  uint32_t neuralWeight = 32; // stat/neural split, also 0..128

  static uint32_t toSymbol(uint32_t coderP);
  static uint32_t toCoder(uint32_t symbolP);
  static uint32_t blend(uint32_t a, uint32_t b, uint32_t weight);
  static uint32_t bitLoss(uint32_t p12, uint8_t bit);
  static uint32_t clampProbability(uint32_t p);
  static uint32_t clampWeight(int32_t weight);
  void updateReliability(uint8_t bit);
  void observeCompletedByte();
};
