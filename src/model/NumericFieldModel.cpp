#include "NumericFieldModel.hpp"

NumericFieldModel::NumericFieldModel(
    const Shared* const sh,
    const RecordModel* const record,
    const RecordResidualModel* const recordResidual) :
    shared(sh),
    recordModel(record),
    recordResidualModel(recordResidual),
    residualMap(sh, 1, COLD_HISTOGRAM + 1, 64) {
}

void NumericFieldModel::resetReliability() {
  prediction = 0;
  predictionError = 0;
  previousPredictionIsValid = false;
  predictionErrorIsInitialized = false;
}

void NumericFieldModel::mix(Mixer& m) {
  INJECT_SHARED_bpos
  if (bpos == 0) {
    INJECT_SHARED_blockPos
    if (blockPos == 0) {
      resetReliability();
    }

    if (previousPredictionIsValid) {
      INJECT_SHARED_c1
      const uint8_t error = rabs(c1, prediction);
      predictionError = ((predictionError * 15) >> 4) + error;
      previousPredictionIsValid = false;
      predictionErrorIsInitialized = true;
    }

    INJECT_SHARED_blockType
    if (blockType == BlockType::DEFAULT &&
        recordResidualModel->hasConfirmedRecordLength() &&
        recordModel->getRecordLength() == 2 &&
        recordModel->getColumn() == 1 &&
        blockPos >= 6) {
      INJECT_SHARED_buf
      const uint8_t currentLow = buf(1);
      const uint8_t h1 = buf(2);
      const uint8_t l1 = buf(3);
      const uint8_t h2 = buf(4);
      const uint8_t l2 = buf(5);
      const uint16_t v1 = static_cast<uint16_t>(l1 | (static_cast<uint16_t>(h1) << 8));
      const uint16_t v2 = static_cast<uint16_t>(l2 | (static_cast<uint16_t>(h2) << 8));
      const uint16_t q = static_cast<uint16_t>(2 * static_cast<int32_t>(v1) - static_cast<int32_t>(v2));
      const uint8_t rawDelta = static_cast<uint8_t>(currentLow - static_cast<uint8_t>(q));
      const int32_t signedDelta = rawDelta < 128 ? rawDelta : static_cast<int32_t>(rawDelta) - 256;
      const uint16_t adjusted = static_cast<uint16_t>(static_cast<int32_t>(q) + signedDelta);
      prediction = static_cast<short>(adjusted >> 8);

      const uint32_t histogram = predictionErrorIsInitialized ?
        min(static_cast<int>(predictionError >> ERROR_SHIFT), ERROR_HISTOGRAMS - 1) :
        COLD_HISTOGRAM;
      residualMap.set(prediction, histogram);
      previousPredictionIsValid = true;
    }
    else {
      residualMap.skip();
    }
  }

  residualMap.mix(m);
}
