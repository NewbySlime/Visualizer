#include "soundget.hpp"
#include "math.hpp"
#include "exception"

#include "chrono"

const REFERENCE_TIME MINDELAY = 10000;
const long MICROSECOND = 1000000;
const long MAXFREQGET = 2000;
const int WAVE_FORMAT_PCM8 = 0x00010000 | WAVE_FORMAT_PCM;
const int WAVE_FORMAT_PCM16 = 0x00020000 | WAVE_FORMAT_PCM;
const int WINDOWSIZE = 1024;

void CallbackDump1(const float *a, int al, const WAVEFORMATEX *waveFormat){}

#define RETURNIFERROR(hres) if(FAILED(hres)) return hres;

template<typename _num> _num mmin(_num a, _num b){
  return (((a) < (b)) ? (a) : (b));
}

SoundGet::SoundGet(int deviceIndex){
  CallbackListener = CallbackDump1;
  CallbackListenerRaw = CallbackDump1;
  int errcode = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pDeviceEnum);

  if(FAILED(errcode))
    printf("Error. %d\n", errcode);

  RefreshDevices();

  try{
    SetDevice(deviceIndex);
  }
  catch(std::system_error e){
    fprintf(stderr, "Catched an error. Class will still be constructed.\n");
  }
}

SoundGet::~SoundGet(){
  StopListening();
}


// i might need to get all of the buffer data so it doesn't have delay (hope so)
void SoundGet::_ThreadUpdate(){
  auto timeSleep = std::chrono::microseconds((int)round((double)MICROSECOND/upPerSec));
  int samplesPerFrames = pWaveFormat->nChannels;
  int bytePerSample = pWaveFormat->wBitsPerSample/8;
  while(isListening){
    //printf("test");
    //Sleep(hnsActualDuration/1000/2);
    BYTE *pData;
    UINT32 packetLength = 0;

    pCaptureClient->GetNextPacketSize(&packetLength);
    while(packetLength > 0 && isListening){
      UINT32 numFramesAvailable = 0;
      DWORD flags;
      pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);

      // TODO the delay is pretty big (maybe the cause of it was reading small portion of the audio data, hence the delay)
      // i might need to use all up, but it might need fft
      int freqbinlen = (int)round(
          (double)numFramesAvailable
          *0.5f
          *mmin((float)MAXFREQGET/(pWaveFormat->nSamplesPerSec/2), 1.f));

      int freqbinarrlen = freqbinlen*samplesPerFrames;
      float *freqbinarr = new float[freqbinarrlen];
      for(int f_i = 0; f_i < freqbinlen; f_i++){
        for(int c_i = 0; c_i < samplesPerFrames; c_i++){
          vec2d ri_pair;
          for(int frames_i = 0; frames_i < numFramesAvailable; frames_i++)
            ri_pair += DFT_partFunction(
              f_i,
              frames_i,
              numFramesAvailable,
              _GetFloatInTime(&pData[(frames_i*samplesPerFrames+c_i)*bytePerSample])
            );

          ri_pair *= 2.f;
          freqbinarr[f_i*samplesPerFrames+c_i] = (float)ri_pair.magnitude() * 5;
          //printf("%f\n", freqbinarr[f_i*samplesPerFrames+c_i]);
        }
      }

      CallbackListenerRaw(freqbinarr, freqbinarrlen, pWaveFormat);
      delete freqbinarr;

      pCaptureClient->ReleaseBuffer(numFramesAvailable);
      pCaptureClient->GetNextPacketSize(&packetLength);
      std::this_thread::sleep_for(timeSleep);
    }
  }
}

float SoundGet::_GetFloatInTime(BYTE *data){
  float res = 0.f;
  switch(currentWaveFormat){
    break; case WAVE_FORMAT_IEEE_FLOAT:
      res = *reinterpret_cast<float*>(data);

    break; case WAVE_FORMAT_PCM16:{
      UINT16 num = *reinterpret_cast<UINT16*>(data);
      res = ((float)num/0x7FFF)-1.f;
    }
    
    break; case WAVE_FORMAT_PCM8:{
      UINT16 num = *reinterpret_cast<UINT8*>(data);
      res = ((float)num/0x7FFF)-1.f;
    }
  }

  return res;
}

void SoundGet::RefreshDevices(){
  IMMDeviceCollection *pDeviceCol;

  pDeviceEnum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pDeviceCol);

  UINT numDev = 0;
  //should add checking if number of device is below 1
  pDeviceCol->GetCount(&numDev);
  soundDevices.resize(numDev);
  for(int i = 0; i < numDev; i++){
    SoundDevice *currDevData = &soundDevices[i];

    IMMDevice *currDev;
    auto _errcode = pDeviceCol->Item(i, &currDev);
    if(FAILED(_errcode))
      continue;

    // getting device's ID
    currDev->GetId(&currDevData->deviceID);

    IPropertyStore *pDeviceProperty;
    currDev->OpenPropertyStore(STGM_READ, &pDeviceProperty);

    PROPVARIANT var;
    PropVariantInit(&var);

    // getting device's friendly name
    { 
      pDeviceProperty->GetValue(PKEY_Device_FriendlyName, &var);

      LPWSTR fnamewstr = var.pwszVal;
      LPSTR fnamestr;

      int namelen = 0;
      namelen = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, fnamewstr, -1, NULL, namelen, NULL, NULL);

      fnamestr = (char*)malloc(namelen+1);
      fnamestr[namelen] = '\0';

      WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, fnamewstr, -1, fnamestr, namelen, NULL, NULL);

      currDevData->deviceName = fnamestr;

      free(fnamestr);
    }

    PropVariantClear(&var);
    pDeviceProperty->Release();
    currDev->Release();
  }

  pDeviceCol->Release();
}

HRESULT SoundGet::StartListening(int updatePerSec){
  if(isListening)
    return S_FALSE;

  HRESULT errcode;
  
  errcode = pAudioDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
  RETURNIFERROR(errcode);

  pWaveFormatEx = NULL;
  errcode = pAudioClient->GetMixFormat(&pWaveFormat);

  currentWaveFormat = pWaveFormat->wFormatTag;
  if(currentWaveFormat == WAVE_FORMAT_EXTENSIBLE){
    errcode = pAudioClient->GetMixFormat((WAVEFORMATEX**)&pWaveFormatEx);
    GUID subformat = pWaveFormatEx->SubFormat;
    if(subformat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
      currentWaveFormat = WAVE_FORMAT_IEEE_FLOAT;
    else if(subformat == KSDATAFORMAT_SUBTYPE_PCM)
      currentWaveFormat = WAVE_FORMAT_PCM;
    else if(subformat == KSDATAFORMAT_SUBTYPE_DRM)
      currentWaveFormat = WAVE_FORMAT_DRM;
    else if(subformat == KSDATAFORMAT_SUBTYPE_ALAW)
      currentWaveFormat = WAVE_FORMAT_ALAW;
    else if(subformat == KSDATAFORMAT_SUBTYPE_MULAW)
      currentWaveFormat = WAVE_FORMAT_MULAW;
    else if(subformat == KSDATAFORMAT_SUBTYPE_ADPCM)
      currentWaveFormat = WAVE_FORMAT_ADPCM;
    else
      printf("Unsupported wave format. Cancelling to start listening.\n");
  }

  bool _doReturn = false;
  switch(currentWaveFormat){
    break; case WAVE_FORMAT_PCM:
      switch(pWaveFormat->wBitsPerSample){
        break; case 8:
          currentWaveFormat = WAVE_FORMAT_PCM8;
        break; case 16:
          currentWaveFormat = WAVE_FORMAT_PCM16;
        break; default:
          printf("Bits per sample for PCM type exceeded or not right. Bit length: %d\n", pWaveFormat->wBitsPerSample);
          // set it to WAVE_FORMAT_EXTENSIBLE so the error message doesn't print again
          currentWaveFormat = WAVE_FORMAT_EXTENSIBLE;
          _doReturn = true;
      }
    break; case WAVE_FORMAT_IEEE_FLOAT:
      if(pWaveFormat->wBitsPerSample != 32){
        printf("Bits per sample for Float type exceeded or not right. Bit length: %d\n", pWaveFormat->wBitsPerSample);
        // set it to WAVE_FORMAT_EXTENSIBLE so the error message doesn't print again
        currentWaveFormat = WAVE_FORMAT_EXTENSIBLE;
        _doReturn = true;
      }
    
    break; default:
      _doReturn = true;
  }
  
  if(_doReturn){
    if(currentWaveFormat != WAVE_FORMAT_EXTENSIBLE)
      printf("Unsupported type. Type: 0x%X\n", currentWaveFormat);
    if(pWaveFormatEx)
      CoTaskMemFree(pWaveFormatEx);
    CoTaskMemFree(pWaveFormat);
    pAudioClient->Release();
    printf("\n");
    return WAVE_FORMAT_UNKNOWN;
  }

  errcode = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, MINDELAY, 0, pWaveFormat, NULL);
  RETURNIFERROR(errcode);

  errcode = pAudioClient->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClient);
  RETURNIFERROR(errcode);
  
  errcode = pAudioClient->Start();
  RETURNIFERROR(errcode);

  upPerSec = updatePerSec;
  isListening = true;
  _ThreadUpdate();
  //audioThread = std::thread(&SoundGet::_ThreadUpdate, this);
  return S_OK;
}

void SoundGet::StopListening(){
  if(isListening){
    isListening = false;
    audioThread.join();
    pAudioClient->Stop();
    pCaptureClient->Release();
    if(pWaveFormatEx)
      CoTaskMemFree(pWaveFormatEx);
    CoTaskMemFree(pWaveFormat);
    pAudioClient->Release();
  }
}

const std::vector<SoundDevice> *SoundGet::GetDevices(){
  return &soundDevices;
}

void SoundGet::SetDevice(int deviceIndex){
  if(isListening){
    fprintf(stderr, "Class SoundGet still getting audio data from current device. Stop it first.\n");
    return;
  }

  HRESULT errcode = S_FALSE;
  if(pAudioDevice)
    pAudioDevice->Release();

  if(deviceIndex < 0)
    errcode = pDeviceEnum->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pAudioDevice);
  else if(deviceIndex < soundDevices.size())
    errcode = pDeviceEnum->GetDevice(soundDevices[deviceIndex].deviceID, &pAudioDevice);

  if(FAILED(errcode)){
    fprintf(stderr, "Cannot use sound device of index (%d).\nErrcode 0x%X\n", deviceIndex, errcode);
  }
}