#ifndef SOUNDGETHEADER
#define SOUNDGETHEADER

#include "Windows.h"
#include "initguid.h"
#include "Audioclient.h"
#include "mmdeviceapi.h"
#include "mmreg.h"
#include "functiondiscoverykeys_devpkey.h"

#include "iostream"
#include "string"
#include "vector"
#include "thread"

struct SoundDevice{
  LPWSTR deviceID;
  std::string deviceName;
};

class SoundGet{
  private:
    IMMDeviceEnumerator *pDeviceEnum;
    IMMDevice *pAudioDevice = NULL;
    IAudioClient *pAudioClient;
    IAudioCaptureClient *pCaptureClient;
    WAVEFORMATEX *pWaveFormat;
    WAVEFORMATEXTENSIBLE *pWaveFormatEx = NULL;

    std::vector<SoundDevice> soundDevices{};
    int numOfBands = 8;
    int currentWaveFormat;

    bool isListening = false;
    int upPerSec;
    std::thread audioThread;

    void _ThreadUpdate();

    float _GetFloatInTime(BYTE *data);


  public:
    // this function will send array that is in the same length as band number times number of channels
    void (*CallbackListener)(const float *amplitudes, int arrlen, const WAVEFORMATEX *waveformat);

    // this function will send raw array, which the same length as window size
    void (*CallbackListenerRaw)(const float *amplitudes, int arrlen, const WAVEFORMATEX *waveformat);

    SoundGet(int deviceIndex = -1);
    ~SoundGet();

    void RefreshDevices();
    HRESULT StartListening(int updatePerSec);
    void StopListening();
    const std::vector<SoundDevice> *GetDevices();
    void SetDevice(int deviceIndex);
};

#endif