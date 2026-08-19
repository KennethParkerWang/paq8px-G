#pragma once

#include "../ResidualMap.hpp"
#include "RecordModel.hpp"

class RecordResidualModel {
private:
  static constexpr int ERROR_SHIFT = 4;
  static constexpr int ERROR_HISTOGRAMS = 16;
  static constexpr int UNCONFIRMED_HISTOGRAM = ERROR_HISTOGRAMS;
  const Shared* const shared;
  const RecordModel* const recordModel;
  ResidualMap residualMap;
  short predictions[RecordModel::NUMERIC_PREDICTORS] {};
  uint32_t predictionErrors[RecordModel::NUMERIC_PREDICTORS] {};
  bool previousPredictionIsValid = false;
  bool recordLengthConfirmed = false;

  void resetReliability();

public:
  static constexpr int MIXERINPUTS =
    RecordModel::NUMERIC_PREDICTORS * ResidualMap::MIXERINPUTS;
  static constexpr int MIXERCONTEXTS = 0;
  static constexpr int MIXERCONTEXTSETS = 0;

  RecordResidualModel(const Shared* sh, const RecordModel* recordModel);
  bool hasConfirmedRecordLength() const { return recordLengthConfirmed; }
  void mix(Mixer& m);
};
