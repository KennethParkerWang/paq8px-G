#pragma once

#include "../ResidualMap.hpp"
#include "RecordModel.hpp"

class RecordResidualModel {
private:
  static constexpr int ERROR_SHIFT = 4;
  const Shared* const shared;
  const RecordModel* const recordModel;
  ResidualMap residualMap;
  short predictions[RecordModel::NUMERIC_PREDICTORS] {};
  uint32_t predictionErrors[RecordModel::NUMERIC_PREDICTORS] {};
  bool previousPredictionIsValid = false;

  void resetReliability();

public:
  static constexpr int MIXERINPUTS =
    RecordModel::NUMERIC_PREDICTORS * ResidualMap::MIXERINPUTS;
  static constexpr int MIXERCONTEXTS = 0;
  static constexpr int MIXERCONTEXTSETS = 0;

  RecordResidualModel(const Shared* sh, const RecordModel* recordModel);
  void mix(Mixer& m);
};
