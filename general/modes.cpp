#include "modes.h"
#include "conversions.h"

const char *amp_mode_names[NUM_AMP_MODES] = { "RMS Amplitude (dB)", "Max Amplitude (dB)", "Amplitude Correlation", "Frequency Changness", "Delta Frequency Centroid", "Note Score", "Note Change Score" };
const char *amp_display_string[NUM_AMP_MODES] = { "RMS Amp Threshold = %0.2f, %0.2f", "Max Amp Threshold = %0.2f, %0.2f", "Amp Corr Threshold = %0.2f, %0.2f", "Freq Changeness Threshold = %0.2f, %0.2f", "Delta Freq Centroid Threshold = %0.2f, %0.2f", "Note Score Threshold = %0.2f, %0.2f", "Note Change Score Threshold = %0.2f, %0.2f" };
double(*amp_mode_func[NUM_AMP_MODES])(double, double) = { &dB2Normalised, &dB2Normalised, &same, &oneMinus, &same, &same, &same };
double(*amp_mode_inv_func[NUM_AMP_MODES])(double, double) = { &normalised2dB, &normalised2dB, &same, &oneMinus, &same, &same, &same };

