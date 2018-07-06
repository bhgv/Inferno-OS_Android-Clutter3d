#pragma once

extern "C" {
typedef void (*AndroidAudioCallback)(short *buffer, int num_samples);

/*bool*/unsigned OpenSLWrap_Init(AndroidAudioCallback cb);
void OpenSLWrap_Shutdown();
}