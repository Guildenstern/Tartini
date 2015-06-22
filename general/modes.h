#ifndef MODES_H
#define MODES_H

enum AnalysisModes { MPM, AUTOCORRELATION, MPM_MODIFIED_CEPSTRUM };

enum AmplitudeModes { AMPLITUDE_RMS, AMPLITUDE_MAX_INTENSITY, AMPLITUDE_CORRELATION, FREQ_CHANGENESS, DELTA_FREQ_CENTROID, NOTE_SCORE, NOTE_CHANGE_SCORE };
#define NUM_AMP_MODES 7
extern const char *amp_mode_names[NUM_AMP_MODES];
extern const char *amp_display_string[NUM_AMP_MODES];
extern double(*amp_mode_func[NUM_AMP_MODES])(double, double);
extern double(*amp_mode_inv_func[NUM_AMP_MODES])(double, double);

#endif // MODES_H
