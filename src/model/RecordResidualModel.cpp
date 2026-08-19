#include "RecordResidualModel.hpp"

RecordResidualModel::RecordResidualModel(const Shared* const sh, const RecordModel* const record) :
    shared(sh),
    recordModel(record),
    residualMap(sh, RecordModel::NUMERIC_PREDICTORS, UNCONFIRMED_HISTOGRAM + 1, 64) {
}

void RecordResidualModel::resetReliability() {
  for (uint32_t& error : predictionErrors) {
    error = 0;
  }
  previousPredictionIsValid = false;
  recordLengthConfirmed = false;
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
      recordLengthConfirmed |= recordModel->wasRecordLengthAcceptedThisByte();
      if (previousPredictionIsValid) {
        INJECT_SHARED_c1
        for (int i = 0; i < RecordModel::NUMERIC_PREDICTORS; ++i) {
          const uint8_t error = rabs(c1, predictions[i]);
          predictionErrors[i] = ((predictionErrors[i] * 15) >> 4) + error;
        }
      }

      recordModel->getNumericPredictions(predictions);
      for (int i = 0; i < RecordModel::NUMERIC_PREDICTORS; ++i) {
        const int errorClass = min(static_cast<int>(predictionErrors[i] >> ERROR_SHIFT), ERROR_HISTOGRAMS - 1);
        residualMap.set(
          predictions[i],
          recordLengthConfirmed ? errorClass : UNCONFIRMED_HISTOGRAM
        );
      }
      previousPredictionIsValid = true;
    }
    else {
      previousPredictionIsValid = false;
      recordLengthConfirmed = false;
      for (int i = 0; i < RecordModel::NUMERIC_PREDICTORS; ++i) {
        residualMap.skip();
      }
    }
  }

  residualMap.mix(m);
}
