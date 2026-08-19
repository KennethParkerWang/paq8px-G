#include "RecordResidualModel.hpp"

RecordResidualModel::RecordResidualModel(const Shared* const sh, const RecordModel* const record) :
    shared(sh),
    recordModel(record),
    residualMap(sh, RecordModel::NUMERIC_PREDICTORS, 16, 64) {
}

void RecordResidualModel::resetReliability() {
  for (uint32_t& error : predictionErrors) {
    error = 0;
  }
  previousPredictionIsValid = false;
}

void RecordResidualModel::mix(Mixer& m) {
  INJECT_SHARED_bpos
  if (bpos == 0) {
    INJECT_SHARED_blockPos
    INJECT_SHARED_blockType

    if (blockPos == 0) {
      resetReliability();
    }

    if (blockType == BlockType::DEFAULT) {
      if (previousPredictionIsValid) {
        INJECT_SHARED_c1
        for (int i = 0; i < RecordModel::NUMERIC_PREDICTORS; ++i) {
          const uint8_t error = rabs(c1, predictions[i]);
          predictionErrors[i] = ((predictionErrors[i] * 15) >> 4) + error;
        }
      }

      recordModel->getNumericPredictions(predictions);
      for (int i = 0; i < RecordModel::NUMERIC_PREDICTORS; ++i) {
        residualMap.set(
          predictions[i],
          min(static_cast<int>(predictionErrors[i] >> ERROR_SHIFT), 15)
        );
      }
      previousPredictionIsValid = true;
    }
    else {
      previousPredictionIsValid = false;
      for (int i = 0; i < RecordModel::NUMERIC_PREDICTORS; ++i) {
        residualMap.skip();
      }
    }
  }

  residualMap.mix(m);
}
