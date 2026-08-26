#include "HierarchicalPosterior.hpp"

#include "Squash.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>

namespace {

constexpr uint32_t kInitialHistoryHash = UINT32_C(0x9e3779b9);

inline uint32_t clampU32(const uint32_t value, const uint32_t lo, const uint32_t hi) {
  return value < lo ? lo : value > hi ? hi : value;
}

inline int32_t clampI32(const int32_t value, const int32_t lo, const int32_t hi) {
  return value < lo ? lo : value > hi ? hi : value;
}

inline uint32_t symbolPToCoder(const uint32_t p12) {
  const uint32_t q = clampU32(p12, 1, 4095);
  return clampU32(static_cast<uint32_t>((uint64_t(q) * ((1u << 31) - 1) + 2047) / 4095), 1, (1u << 31) - 1);
}

} // namespace

HierarchicalPosterior::ByteTrie::ByteTrie(const uint32_t contexts)
  : contextCount(std::max<uint32_t>(1, contexts)),
    zero(static_cast<size_t>(contextCount) * INTERNAL_NODES, 0),
    one(static_cast<size_t>(contextCount) * INTERNAL_NODES, 0),
    totals(contextCount, 0) {
}

void HierarchicalPosterior::ByteTrie::decay(const uint32_t context) {
  const size_t base = static_cast<size_t>(context) * INTERNAL_NODES;
  for (uint32_t i = 0; i < INTERNAL_NODES; ++i) {
    zero[base + i] = static_cast<uint16_t>(zero[base + i] >> 1);
    one[base + i] = static_cast<uint16_t>(one[base + i] >> 1);
  }
  totals[context] = static_cast<uint16_t>(totals[context] >> 1);
}

HierarchicalPosterior::Prediction HierarchicalPosterior::ByteTrie::predict(
    const uint32_t context,
    const uint8_t prefix,
    const uint8_t bits) const {
  assert(context < contextCount);
  assert(bits < 8);
  const size_t base = static_cast<size_t>(context) * INTERNAL_NODES;
  uint32_t node = 0;
  for (uint8_t i = 0; i < bits; ++i) {
    const uint8_t bit = static_cast<uint8_t>((prefix >> (bits - 1 - i)) & 1u);
    node = node * 2u + 1u + bit;
  }
  assert(node < INTERNAL_NODES);
  const uint32_t zeros = zero[base + node];
  const uint32_t ones = one[base + node];
  const uint32_t support = zeros + ones;
  // A small Dirichlet prior prevents a cold context from making a hard
  // decision before it has observed both sides of the prefix.
  const uint32_t prior = support < 8 ? 4 : 1;
  const uint32_t p = static_cast<uint32_t>((uint64_t(ones + prior) * 4095 + (support + 2 * prior) / 2) /
                                           (support + 2 * prior));
  return {clampU32(p, 1, 4095), support};
}

void HierarchicalPosterior::ByteTrie::observe(const uint32_t context, const uint8_t symbol) {
  assert(context < contextCount);
  if (totals[context] >= DECAY_LIMIT) {
    decay(context);
  }
  ++totals[context];
  const size_t base = static_cast<size_t>(context) * INTERNAL_NODES;
  uint32_t node = 0;
  for (uint8_t depth = 0; depth < 8; ++depth) {
    const uint8_t bit = static_cast<uint8_t>((symbol >> (7 - depth)) & 1u);
    uint16_t& count = bit == 0 ? zero[base + node] : one[base + node];
    if (count != UINT16_MAX) {
      ++count;
    }
    node = node * 2u + 1u + bit;
  }
}

uint64_t HierarchicalPosterior::StatisticalBranch::mix64(uint64_t x) {
  x += UINT64_C(0x9e3779b97f4a7c15);
  x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
  return x ^ (x >> 31);
}

HierarchicalPosterior::StatisticalBranch::StatisticalBranch(const uint8_t level)
  : order0(1),
    order1(256),
    order2(1u << std::min<uint8_t>(12, std::max<uint8_t>(8, static_cast<uint8_t>(level + 3)))),
    order2Mask((1u << std::min<uint8_t>(12, std::max<uint8_t>(8, static_cast<uint8_t>(level + 3)))) - 1),
    nextContext{0, 0, 0, UINT64_C(0x9e3779b97f4a7c15)} {
}

uint32_t HierarchicalPosterior::StatisticalBranch::order2Key(const ByteContext& context) const {
  const uint64_t packed = static_cast<uint64_t>(context.previous2) |
                          (static_cast<uint64_t>(context.previous1) << 8) |
                          (static_cast<uint64_t>(context.blockType) << 16);
  return static_cast<uint32_t>(mix64(packed)) & order2Mask;
}

HierarchicalPosterior::ByteContext HierarchicalPosterior::StatisticalBranch::beginByte(const uint8_t blockType) {
  nextContext.blockType = blockType;
  return nextContext;
}

HierarchicalPosterior::Prediction HierarchicalPosterior::StatisticalBranch::predict(
    const ByteContext& context,
    const uint8_t prefix,
    const uint8_t bits) const {
  const Prediction p0 = order0.predict(0, prefix, bits);
  const Prediction p1 = order1.predict(context.previous1, prefix, bits);
  const Prediction p2 = order2.predict(order2Key(context), prefix, bits);

  // Hierarchical interpolation: order-2 evidence is admitted only after it
  // has support, while order-0 remains a stable backoff distribution.
  const uint32_t w0 = 32;
  const uint32_t w1 = std::min<uint32_t>(96, 8 + p1.support);
  const uint32_t w2 = p2.support < 2 ? 0 : std::min<uint32_t>(160, p2.support * 2);
  const uint32_t totalWeight = w0 + w1 + w2;
  const uint32_t p = static_cast<uint32_t>((uint64_t(p0.p12) * w0 + uint64_t(p1.p12) * w1 +
                                            uint64_t(p2.p12) * w2 + totalWeight / 2) / totalWeight);
  return {clampU32(p, 1, 4095), std::max(p1.support, p2.support)};
}

void HierarchicalPosterior::StatisticalBranch::observe(const ByteContext& context, const uint8_t symbol) {
  const uint32_t key1 = context.previous1;
  const uint32_t key2 = order2Key(context);
  order0.observe(0, symbol);
  order1.observe(key1, symbol);
  order2.observe(key2, symbol);

  nextContext.previous2 = context.previous1;
  nextContext.previous1 = symbol;
  nextContext.rollingHash = mix64(context.rollingHash ^ (static_cast<uint64_t>(symbol) + UINT64_C(0x100)));
}

uint64_t HierarchicalPosterior::LongContextNeuralBranch::mix64(uint64_t x) {
  x += UINT64_C(0x9e3779b97f4a7c15);
  x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
  return x ^ (x >> 31);
}

int16_t HierarchicalPosterior::LongContextNeuralBranch::clamp16(const int32_t x) {
  return static_cast<int16_t>(clampI32(x, -32768, 32767));
}

uint32_t HierarchicalPosterior::LongContextNeuralBranch::squashToSymbol(const int32_t logit) {
  return static_cast<uint32_t>(squash(clampI32(logit, -2047, 2047)));
}

HierarchicalPosterior::LongContextNeuralBranch::LongContextNeuralBranch(const uint8_t level)
  : contextLogits(static_cast<size_t>(1u << std::min<uint8_t>(13, std::max<uint8_t>(9, static_cast<uint8_t>(level + 2)))) * BIT_POSITIONS, 0),
    contextCounts(static_cast<size_t>(1u << std::min<uint8_t>(13, std::max<uint8_t>(9, static_cast<uint8_t>(level + 2)))), 0),
    contextMask((1u << std::min<uint8_t>(13, std::max<uint8_t>(9, static_cast<uint8_t>(level + 2)))) - 1) {
  for (uint32_t bit = 0; bit < BIT_POSITIONS; ++bit) {
    outputBias[bit] = static_cast<int16_t>((static_cast<int32_t>(bit) - 3) * 3);
    for (uint32_t h = 0; h < HIDDEN; ++h) {
      const uint64_t seed = mix64(UINT64_C(0x123456789abcdef0) ^ (uint64_t(bit) << 16) ^ h);
      outputWeights[bit][h] = static_cast<int16_t>(static_cast<int32_t>(seed & 31) - 16);
    }
  }
}

void HierarchicalPosterior::LongContextNeuralBranch::beginByte(const ByteContext& context) {
  currentKey = static_cast<uint32_t>(mix64(context.rollingHash ^ (uint64_t(context.blockType) << 48))) & contextMask;
  currentBitPosition = 0;
  currentLogit = 0;
}

HierarchicalPosterior::Prediction HierarchicalPosterior::LongContextNeuralBranch::predict(
    const uint8_t prefix,
    const uint8_t bits) {
  const size_t index = static_cast<size_t>(currentKey) * BIT_POSITIONS + bits;
  int32_t logit = contextLogits[index] + outputBias[bits];
  for (uint32_t h = 0; h < HIDDEN; ++h) {
    logit += (static_cast<int32_t>(fastState[h]) * outputWeights[bits][h]) >> 9;
    logit += (static_cast<int32_t>(slowState[h]) * outputWeights[bits][h]) >> 11;
  }
  // Prefix-dependent correction makes the branch a proper bit-conditioned
  // posterior instead of eight independent byte-position classifiers.
  if (bits != 0) {
    const int32_t prefixSignal = static_cast<int32_t>((prefix * 17u + bits * 29u) & 63u) - 31;
    logit += prefixSignal;
  }
  currentBitPosition = bits;
  currentLogit = static_cast<int16_t>(clampI32(logit, -2047, 2047));
  return {squashToSymbol(logit), contextCounts[currentKey]};
}

void HierarchicalPosterior::LongContextNeuralBranch::observeBit(const uint8_t bit) {
  const size_t index = static_cast<size_t>(currentKey) * BIT_POSITIONS + currentBitPosition;
  const uint32_t p12 = squashToSymbol(currentLogit);
  const int32_t target = bit == 0 ? 0 : static_cast<int32_t>(SYMBOL_MAX);
  const int32_t error = target - static_cast<int32_t>(p12);
  contextLogits[index] = clamp16(static_cast<int32_t>(contextLogits[index]) + clampI32(error >> 7, -24, 24));
  outputBias[currentBitPosition] = clamp16(static_cast<int32_t>(outputBias[currentBitPosition]) + clampI32(error >> 9, -4, 4));
  for (uint32_t h = 0; h < HIDDEN; ++h) {
    const int32_t delta = (error * static_cast<int32_t>(fastState[h])) >> 18;
    outputWeights[currentBitPosition][h] = clamp16(static_cast<int32_t>(outputWeights[currentBitPosition][h]) + clampI32(delta, -3, 3));
  }
}

void HierarchicalPosterior::LongContextNeuralBranch::observeByte(const uint8_t symbol, const ByteContext& context) {
  const uint64_t seed = mix64(context.rollingHash ^ (uint64_t(symbol) << 24) ^ UINT64_C(0x517cc1b727220a95));
  for (uint32_t h = 0; h < HIDDEN; ++h) {
    const int32_t input = static_cast<int32_t>((seed >> ((h * 3) & 63)) & 0xffu) - 128;
    fastState[h] = clamp16((static_cast<int32_t>(fastState[h]) * 224 + input * 64) >> 8);
    slowState[h] = clamp16((static_cast<int32_t>(slowState[h]) * 252 + input * 16) >> 8);
  }
  if (contextCounts[currentKey] != UINT16_MAX) {
    ++contextCounts[currentKey];
  }
}

HierarchicalPosterior::HierarchicalPosterior(Shared* const sh)
  : shared(sh), statistical(sh->level), neural(sh->level) {
  // The branch starts with conservative mixture weights.  The online loss
  // tracker can increase them only when the new evidence beats Legacy.
}

HierarchicalPosterior::~HierarchicalPosterior() {
#if HDPP_MODE != 0
  if (bitCount != 0) {
    fprintf(stderr,
      "[HDPP] mode=%d bytes=%llu bits=%llu fusionWeight=%u neuralWeight=%u "
      "loss12 legacy=%llu stat=%llu neural=%llu symbol=%llu final=%llu "
      "disagreement=%llu\n",
      HDPP_MODE,
      static_cast<unsigned long long>(byteCount),
      static_cast<unsigned long long>(bitCount),
      fusionWeight,
      neuralWeight,
      static_cast<unsigned long long>(legacyLossSum),
      static_cast<unsigned long long>(statLossSum),
      static_cast<unsigned long long>(neuralLossSum),
      static_cast<unsigned long long>(symbolLossSum),
      static_cast<unsigned long long>(finalLossSum),
      static_cast<unsigned long long>(disagreementCount));
  }
#endif
}

uint32_t HierarchicalPosterior::toSymbol(const uint32_t coderP) {
  return clampU32(static_cast<uint32_t>((uint64_t(clampProbability(coderP)) * SYMBOL_MAX + CODER_MAX / 2) / CODER_MAX), 1, SYMBOL_MAX);
}

uint32_t HierarchicalPosterior::toCoder(const uint32_t symbolP) {
  return symbolPToCoder(symbolP);
}

uint32_t HierarchicalPosterior::blend(const uint32_t a, const uint32_t b, const uint32_t weight) {
  const uint32_t w = std::min<uint32_t>(weight, 256);
  return static_cast<uint32_t>((uint64_t(a) * (256 - w) + uint64_t(b) * w + 128) >> 8);
}

uint32_t HierarchicalPosterior::bitLoss(const uint32_t p12, const uint8_t bit) {
  const uint32_t p = clampU32(p12, 1, SYMBOL_MAX - 1);
  return bit == 0 ? p : SYMBOL_MAX - p;
}

uint32_t HierarchicalPosterior::clampProbability(const uint32_t p) {
  return clampU32(p, 1, CODER_MAX - 1);
}

uint32_t HierarchicalPosterior::clampWeight(const int32_t weight) {
  return static_cast<uint32_t>(clampI32(weight, 0, 128));
}

uint32_t HierarchicalPosterior::predict(const uint32_t legacyP) {
#if HDPP_MODE == 0
  return legacyP;
#else
  INJECT_SHARED_bpos
  INJECT_SHARED_c0
  INJECT_SHARED_blockType

  if (bpos == 0) {
    currentContext = statistical.beginByte(static_cast<uint8_t>(blockType));
    neural.beginByte(currentContext);
    byteActive = true;
  }
  assert(byteActive);

  const uint8_t prefix = static_cast<uint8_t>(c0 & ((1u << bpos) - 1u));
  const Prediction stat = statistical.predict(currentContext, prefix, bpos);
  const Prediction recur = neural.predict(prefix, bpos);
  const uint32_t statP = toCoder(stat.p12);
  const uint32_t neuralP = toCoder(recur.p12);
  const uint32_t stat12 = toSymbol(statP);
  const uint32_t neural12 = toSymbol(neuralP);
  uint32_t symbol12 = stat12;
  if (HDPP_MODE == 2) {
    symbol12 = neural12;
  }
  else if (HDPP_MODE == 3) {
    symbol12 = blend(stat12, neural12, neuralWeight);
  }

  // Reliability is based on actual prefix support, not a file/type oracle.
  const uint32_t support = std::max(stat.support, recur.support);
  const uint32_t confidence = std::min<uint32_t>(256, 16 + support * 4);
  symbol12 = blend(SYMBOL_MAX / 2, symbol12, confidence);
  const uint32_t symbolP = toCoder(symbol12);

  const uint32_t finalP = clampProbability(blend(legacyP, symbolP, (fusionWeight * confidence) >> 8));

  lastLegacyP = legacyP;
  lastStatP = statP;
  lastNeuralP = neuralP;
  lastSymbolP = symbolP;
  lastFinalP = finalP;
  lastStatSupport = stat.support;
  lastNeuralSupport = recur.support;
  if (std::abs(static_cast<int>(stat12) - static_cast<int>(neural12)) > 256) {
    ++disagreementCount;
  }
  // UpdateBroadcaster subscriptions are per-bit: Shared clears the list
  // after broadcasting.  Register this posterior after producing the current
  // prediction so its state is updated with the same causal bit as Legacy.
  shared->GetUpdateBroadcaster()->subscribe(this);
  return finalP;
#endif
}

void HierarchicalPosterior::updateReliability(const uint8_t bit) {
  const uint32_t legacy12 = toSymbol(lastLegacyP);
  const uint32_t stat12 = toSymbol(lastStatP);
  const uint32_t neural12 = toSymbol(lastNeuralP);
  const uint32_t symbol12 = toSymbol(lastSymbolP);
  const uint32_t final12 = toSymbol(lastFinalP);
  const uint32_t eLegacy = bitLoss(legacy12, bit);
  const uint32_t eStat = bitLoss(stat12, bit);
  const uint32_t eNeural = bitLoss(neural12, bit);
  const uint32_t eSymbol = bitLoss(symbol12, bit);
  const uint32_t eFinal = bitLoss(final12, bit);

  legacyLossSum += eLegacy;
  statLossSum += eStat;
  neuralLossSum += eNeural;
  symbolLossSum += eSymbol;
  finalLossSum += eFinal;
  const auto updateEwma = [](uint32_t& state, const uint32_t value) {
    const int32_t delta = static_cast<int32_t>(value) - static_cast<int32_t>(state);
    state = static_cast<uint32_t>(static_cast<int32_t>(state) + (delta >> 5));
  };
  updateEwma(legacyLossEwma, eLegacy);
  updateEwma(statLossEwma, eStat);
  updateEwma(neuralLossEwma, eNeural);
  updateEwma(symbolLossEwma, eSymbol);

  if ((bitCount & 31u) == 31u) {
    const int32_t symbolAdvantage = static_cast<int32_t>(legacyLossEwma) - static_cast<int32_t>(symbolLossEwma);
    fusionWeight = clampWeight(static_cast<int32_t>(fusionWeight) + clampI32(symbolAdvantage >> 6, -4, 4));
    if (HDPP_MODE == 3) {
      const int32_t neuralAdvantage = static_cast<int32_t>(statLossEwma) - static_cast<int32_t>(neuralLossEwma);
      neuralWeight = clampWeight(static_cast<int32_t>(neuralWeight) + clampI32(neuralAdvantage >> 6, -4, 4));
    }
  }
}

void HierarchicalPosterior::observeCompletedByte() {
  INJECT_SHARED_c1
  statistical.observe(currentContext, c1);
  neural.observeByte(c1, currentContext);
  ++byteCount;
  byteActive = false;
}

void HierarchicalPosterior::update() {
#if HDPP_MODE != 0
  INJECT_SHARED_y
  ++bitCount;
  updateReliability(y);
  neural.observeBit(y);
  if (shared->State.bitPosition == 0) {
    observeCompletedByte();
  }
#endif
}
