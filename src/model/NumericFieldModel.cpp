#include "NumericFieldModel.hpp"

NumericFieldModel::NumericFieldModel(const Shared* const sh, const RecordModel* const record,
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
  predictionErrorIsValid = false;
}

void NumericFieldModel::mix(Mixer& m) {
  INJECT_SHARED_bpos
  if (bpos == 0) {
    INJECT_SHARED_blockPos
    INJECT_SHARED_blockType

    if (blockPos == 0) {
      resetReliability();
    }

    if (previousPredictionIsValid) {
      INJECT_SHARED_c1
      const uint8_t error = rabs(c1, prediction);
      predictionError = ((predictionError * 15) >> 4) + error;
      previousPredictionIsValid = false;
      predictionErrorIsValid = true;
    }

    const bool isActive =
      blockType == BlockType::DEFAULT &&
      recordResidualModel->hasConfirmedRecordLength() &&
      recordModel->getRecordLength() == 2 &&
      recordModel->getColumn() == 1 &&
      blockPos >= 6;

    if (isActive) {
      INJECT_SHARED_buf
      const uint16_t v1 = static_cast<uint16_t>(
        buf(3) | (static_cast<uint16_t>(buf(2)) << 8)
      );
      const uint16_t v2 = static_cast<uint16_t>(
        buf(5) | (static_cast<uint16_t>(buf(4)) << 8)
      );
      const uint16_t q = static_cast<uint16_t>(2 * v1 - v2);
      prediction = static_cast<short>(q >> 8);

      const int errorClass = min(
        static_cast<int>(predictionError >> ERROR_SHIFT),
        ERROR_HISTOGRAMS - 1
      );
      residualMap.set(
        prediction,
        predictionErrorIsValid ? errorClass : COLD_HISTOGRAM
      );
      previousPredictionIsValid = true;
    }
    else {
      residualMap.skip();
    }
  }

  residualMap.mix(m);
}
