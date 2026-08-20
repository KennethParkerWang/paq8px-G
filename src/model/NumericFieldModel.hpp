#pragma once

#include "../ResidualMap.hpp"
#include "RecordModel.hpp"
#include "RecordResidualModel.hpp"

class NumericFieldModel {
private:
  static constexpr int ERROR_SHIFT = 4;
  static constexpr int ERROR_HISTOGRAMS = 16;
  static constexpr int COLD_HISTOGRAM = ERROR_HISTOGRAMS;
  const Shared* const shared;
  const RecordModel* const recordModel;
  const RecordResidualModel* const recordResidualModel;
  ResidualMap residualMap;
  short prediction = 0;
  uint32_t predictionError = 0;
  bool previousPredictionIsValid = false;
  bool predictionErrorIsValid = false;

  void resetReliability();

public:
  static constexpr int MIXERINPUTS = ResidualMap::MIXERINPUTS;
  static constexpr int MIXERCONTEXTS = 0;
  static constexpr int MIXERCONTEXTSETS = 0;

  NumericFieldModel(const Shared* sh, const RecordModel* recordModel,
                    const RecordResidualModel* recordResidualModel);
  void mix(Mixer& m);
};
