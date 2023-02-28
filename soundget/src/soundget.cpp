#include "soundget.hpp"
#include "math.hpp"
#include "exception"
#include "chrono"
#include "mutex"

#include "string.h"


const REFERENCE_TIME MINDELAY = 10000;
const long MICROSECOND = 1000000;
const long MAXFREQGET = 2000;
const int WAVE_FORMAT_PCM8 = 0x00010000 | WAVE_FORMAT_PCM;
const int WAVE_FORMAT_PCM16 = 0x00020000 | WAVE_FORMAT_PCM;
const int WINDOWSIZE = 1024;

void CallbackDump1(const float *a, int al, const WAVEFORMATEX *waveFormat){}
void CallbackDump2(SoundGet::SoundGetNotification_changeStatus status){}

#define RETURNIFERROR(hres) if(FAILED(hres)) return hres
#define BREAKIFERROR(hres) if(FAILED(hres)) break

#define SAFERELEASE(punk) if(!punk) punk->Release()



template<typename _num> _num mmin(_num a, _num b){
  return (((a) < (b)) ? (a) : (b));
}



/*        SoundGet::_NotificationHandling class       */
SoundGet::_NotificationHandling::_NotificationHandling(){
  _cb = CallbackDump2;
  _cRef = 1;
}


bool SoundGet::_NotificationHandling::_check_dataflow(LPCWSTR devId, EDataFlow flow){
  printf("check flow\n");
  IMMDevice *_curdev = NULL;
  IMMEndpoint *_curep = NULL;

  bool _res = false;
  while(true){
    HRESULT errcode;
    EDataFlow _currflow;
    errcode = _bound_soundget->pDeviceEnum->GetDevice(devId, &_curdev); BREAKIFERROR(errcode);
    errcode = _curdev->QueryInterface(IID_IMMEndpoint, (void**)&_curep); BREAKIFERROR(errcode);
    errcode = _curep->GetDataFlow(&_currflow); BREAKIFERROR(errcode);

    _res = (flow == _currflow);
  break;}

  SAFERELEASE(_curep);
  SAFERELEASE(_curdev);

  printf("return flow\n");

  return _res;
}

HRESULT SoundGet::_NotificationHandling::_onDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId){
  if(_using_default && (_bound_soundget->GetDataFlow() == flow) && (role == SOUNDGET_EROLE)){
    HRESULT errcode;

    _bound_soundget->StopListening();

    errcode = _bound_soundget->SetDevice();
    _bound_soundget->StartListening();

    _cb(SGN_STATUS_DEFAULTCHANGED);
  }

  return S_OK;
}

HRESULT SoundGet::_NotificationHandling::_onDeviceAdded(LPCWSTR pwstrDeviceId){
  if(!_check_dataflow(pwstrDeviceId, _bound_soundget->GetDataFlow()))
    return S_OK;

  for(size_t i = 0; i < _bound_soundget->soundDevices.size(); i++){
    SoundDevice *_currdev = &_bound_soundget->soundDevices[i];
    if(_currdev->deviceID && wcscmp(_currdev->deviceID, pwstrDeviceId) == 0)
      return S_OK;
  }

  auto devEnum = _bound_soundget->_getDeviceEnum();

  IMMDevice *newDev; devEnum->GetDevice(pwstrDeviceId, &newDev);
  DWORD devState; newDev->GetState(&devState);

  if(devState == DEVICE_STATE_ACTIVE){
    _bound_soundget->_addDevice(newDev);
    _cb(SGN_STATUS_ADDED);
  }
  
  newDev->Release();

  return S_OK;
}

HRESULT SoundGet::_NotificationHandling::_onDeviceRemoved(LPCWSTR pwstrDeviceId){
  if(!_check_dataflow(pwstrDeviceId, _bound_soundget->GetDataFlow()))
    return S_OK;

  for(size_t i = 0; i < _bound_soundget->soundDevices.size(); i++){
    SoundDevice *_currdev = &_bound_soundget->soundDevices[i];
    if(_currdev->deviceID && wcscmp(_currdev->deviceID, pwstrDeviceId) == 0){
      if(i == _bound_soundget->GetIndex() && !_using_default)
        _bound_soundget->StopListening();

      CoTaskMemFree(_currdev->deviceID);
      _bound_soundget->soundDevices.erase(_bound_soundget->soundDevices.begin() + i);

      _cb(SGN_STATUS_REMOVED);

      if(_bound_soundget->soundDevices.size() <= 0){
        _bound_soundget->StopListening();
        _bound_soundget->_RefreshDevices();
      }

      return S_OK;
    }
  }


  return E_NOTFOUND;
}

HRESULT SoundGet::_NotificationHandling::_onDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD newState){
  if(newState != DEVICE_STATE_ACTIVE)
    return _onDeviceRemoved(pwstrDeviceId);
  else
    return _onDeviceAdded(pwstrDeviceId);
}

void SoundGet::_NotificationHandling::_addNotificationData(_notificationType type, LPCWSTR devID, EDataFlow flow, ERole role, DWORD state, PROPERTYKEY key){
  _notificationData data;
  data.type = type;
  data.devID = NULL;
  data.flow = flow;
  data.role = role;
  data.state = state;
  data.key = key;

  if(devID){
    size_t idlen = wcslen(devID);
    LPWSTR id = (LPWSTR)CoTaskMemAlloc((idlen+1)*sizeof(WCHAR));

    wcscpy(id, devID);
    data.devID = id;
  }

  {std::lock_guard<std::mutex> _lg(_notifs_mutex);
    _notifs.insert(_notifs.end(), data);
    printf("adding notifs %d\n", _notifs.size());
  }

  PostMessage(_bindProc, _bindMsg, 0, 0);
}


void SoundGet::_NotificationHandling::BindSoundGet(SoundGet *_soundget){
  _bound_soundget = _soundget;
}

void SoundGet::_NotificationHandling::UseCallback_onDeviceChanged(SoundGetNotification_cb cb){
  _cb = cb;
}

void SoundGet::_NotificationHandling::Set_UsingDefault(bool b){
  _using_default = b;
}

void SoundGet::_NotificationHandling::CheckNotifications(){
  {std::lock_guard<std::mutex> _lg(_notifs_mutex);
    for(size_t i = 0; i < _notifs.size(); i++){
      _notificationData *data = &_notifs[i];

      switch(data->type){
        break; case _SGNT_DEFDEVCHANGED:
          _onDefaultDeviceChanged(data->flow, data->role, data->devID);

        break; case _SGNT_DEVADDED:
          _onDeviceAdded(data->devID);
        
        break; case _SGNT_DEVREMOVED:
          _onDeviceRemoved(data->devID);
        
        break; case _SGNT_DEVSTATECHANGED:
          _onDeviceStateChanged(data->devID, data->state);
      }

      CoTaskMemFree(data->devID);
    }

    _notifs.clear();
  }
}

void SoundGet::_NotificationHandling::BindProcess(HWND hWnd, UINT msg){
  _bindProc = hWnd;
  _bindMsg = msg;
}

ULONG SoundGet::_NotificationHandling::AddRef(){
  return InterlockedIncrement(&_cRef);
}

ULONG SoundGet::_NotificationHandling::Release(){
  ULONG ulRef = InterlockedDecrement(&_cRef);
  if(0 == ulRef)
    delete this;

  return ulRef;
}

HRESULT SoundGet::_NotificationHandling::QueryInterface(REFIID riid, VOID **ppvInterface){
  if(IID_IUnknown == riid){
    AddRef();
    *ppvInterface = (IUnknown*)this;
  }
  else if(__uuidof(IMMNotificationClient) == riid){
    AddRef();
    *ppvInterface = (IMMNotificationClient*)this;
  }
  else{
    *ppvInterface = NULL;
    return E_NOINTERFACE;
  }

  return S_OK;
}

HRESULT SoundGet::_NotificationHandling::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR devId){
  _addNotificationData(
    _SGNT_DEFDEVCHANGED,
    devId,
    flow,
    role,
    0,
    PROPERTYKEY()
  );
  
  return S_OK;
}

HRESULT SoundGet::_NotificationHandling::OnDeviceAdded(LPCWSTR pwstrDeviceId){
  _addNotificationData(
    _SGNT_DEVADDED,
    pwstrDeviceId,
    (EDataFlow)0,
    (ERole)0,
    0,
    PROPERTYKEY()
  );
  
  return S_OK;
}

HRESULT SoundGet::_NotificationHandling::OnDeviceRemoved(LPCWSTR pwstrDeviceId){
  _addNotificationData(
    _SGNT_DEVREMOVED,
    pwstrDeviceId,
    (EDataFlow)0,
    (ERole)0,
    0,
    PROPERTYKEY()
  );
  
  return S_OK;
}

HRESULT SoundGet::_NotificationHandling::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD newState){
  _addNotificationData(
    _SGNT_DEVSTATECHANGED,
    pwstrDeviceId,
    (EDataFlow)0,
    (ERole)0,
    newState,
    PROPERTYKEY()
  );
  
  return S_OK;
}

HRESULT SoundGet::_NotificationHandling::OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key){
  return S_OK;
}



/*        SoundGet class        */
SoundGet::SoundGet(int deviceIndex){
  _changedHandler = new _NotificationHandling();
  CallbackListener = CallbackDump1;
  CallbackListenerRaw = CallbackDump1;
  int errcode = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pDeviceEnum);

  if(FAILED(errcode))
    printf("Error. %d\n", errcode);

  _RefreshDevices();

  try{
    SetDevice(deviceIndex);
  }
  catch(std::system_error e){
    fprintf(stderr, "Catched an error. Class will still be constructed.\n");
  }

  _changedHandler->BindSoundGet(this);
  pDeviceEnum->RegisterEndpointNotificationCallback((IMMNotificationClient*)_changedHandler);
}

SoundGet::~SoundGet(){
  StopListening();

  pDeviceEnum->UnregisterEndpointNotificationCallback((IMMNotificationClient*)_changedHandler);
  delete _changedHandler;

  pAudioDevice->Release();
  pDeviceEnum->Release();

  _clear_soundDevices();
}

void SoundGet::_ThreadUpdate(){
  int samplesPerFrames = pWaveFormat->nChannels;
  int bytePerSample = pWaveFormat->wBitsPerSample/8;

  std::chrono::microseconds timeSleep;
  std::chrono::_V2::system_clock::time_point
    lastTime = std::chrono::high_resolution_clock::now();

  HRESULT errcode;

  while(isListening){
    //printf("listening\n");
    BYTE *apData = (BYTE*)malloc(0);
    size_t apDataSize = 0;
    size_t apDataSizeBytes = 0;
    BYTE *pData;
    UINT32 packetLength = 0;

    // getting all audio buffer data
    errcode = pCaptureClient->GetNextPacketSize(&packetLength);
    if(errcode != S_OK)
      return;

    while(packetLength > 0){
      //printf("getting packet\n");
      //printf("plen %d %d\n", packetLength, apDataSize);
      UINT32 numFramesAvailable = 0;
      DWORD flags;
      errcode = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
      if(errcode != S_OK)
        return;

      size_t plenInBytes = packetLength*samplesPerFrames*bytePerSample;
      apData = (BYTE*)realloc(apData, apDataSizeBytes+plenInBytes);
      memcpy(apData+apDataSizeBytes, pData, plenInBytes);
      apDataSize += packetLength;
      apDataSizeBytes += plenInBytes;
      
      pCaptureClient->ReleaseBuffer(numFramesAvailable);
      pCaptureClient->GetNextPacketSize(&packetLength);
    }

    // processing the data
    if(apDataSize > 0){ std::lock_guard<std::mutex> _lg(processingMutex);
      //printf("apDataSize %d\n", apDataSize);
      int freqbinlen = (int)round(
        (double)apDataSize
        *0.5f
        *mmin((float)MAXFREQGET/(pWaveFormat->nSamplesPerSec/2), 1.f));

      int _nidx = 0;
      int _n = (int)ceilf((float)freqbinlen/numOfBands);
      int freqbinarrlen = freqbinlen*samplesPerFrames;
      int freqbinarrmculen = numOfBands*samplesPerFrames;
      float *freqbinarr = new float[freqbinarrlen];
      float *freqbinarrmcu = new float[freqbinarrmculen]();
      //printf("freqbinlen %d\n", freqbinlen);
      for(int f_i = 0; f_i < freqbinlen; f_i++){
        //printf("fi %d\n", f_i);

        for(int c_i = 0; c_i < samplesPerFrames; c_i++){
          vec2d ri_pair;
          // FFT?
          // Radix 2-DIT
          for(int frames_i = 0; frames_i < apDataSize; frames_i += 2)
            ri_pair += DFT_partFunction(
              f_i,
              frames_i,
              apDataSize,
              _GetFloatInTime(&apData[(frames_i*samplesPerFrames+c_i)*bytePerSample])
            )+
            DFT_partFunction(
              f_i,
              frames_i+1,
              apDataSize,
              _GetFloatInTime(&apData[((frames_i+1)*samplesPerFrames+c_i)*bytePerSample])
            );

          ri_pair *= 2.f;
          float numf = (float)logf(ri_pair.magnitude());
          freqbinarr[f_i*samplesPerFrames+c_i] = numf;

          freqbinarrmcu[_nidx+c_i*numOfBands] += numf/_n;
          if(((f_i+1) % _n) == 0 && c_i == (samplesPerFrames-1))
            _nidx++;
        }
      }

      CallbackListenerRaw(freqbinarr, freqbinarrlen, pWaveFormat);
      CallbackListener(freqbinarrmcu, freqbinarrmculen, pWaveFormat);
      delete[] freqbinarr;
      delete[] freqbinarrmcu;

      timeSleep = std::chrono::microseconds((int)round((double)MICROSECOND/upPerSec));
    }

    free(apData);

    // timing stuff
    if(apDataSize > 0){
      auto finishTime = std::chrono::high_resolution_clock::now();
      _processingTime = ((std::chrono::duration<double>)(finishTime-lastTime)).count();
      std::this_thread::sleep_for(timeSleep-(finishTime-lastTime));

      lastTime = std::chrono::high_resolution_clock::now();
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


void SoundGet::_refresh_soundDevice(SoundDevice *dev, IMMDevice *idev){
  // getting device's ID
  idev->GetId(&dev->deviceID);

  IPropertyStore *pDeviceProperty;
  idev->OpenPropertyStore(STGM_READ, &pDeviceProperty);

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

    dev->deviceName = fnamestr;

    free(fnamestr);
  }

  if(dev->deviceID == NULL)
    printf("Device ID is null\n");

  PropVariantClear(&var);
  pDeviceProperty->Release();
}

void SoundGet::_addDevice(IMMDevice *dev){
  soundDevices.insert(soundDevices.end(), SoundDevice());
  SoundDevice *currDevData = &soundDevices[soundDevices.size()-1];

  _refresh_soundDevice(currDevData, dev);
}

void SoundGet::_clear_soundDevices(){
  for(size_t i = 0; i < soundDevices.size(); i++){
    if(soundDevices[i].deviceID)
      CoTaskMemFree(soundDevices[i].deviceID);
  }

  soundDevices.resize(0);
}

void SoundGet::_RefreshDevices(){
  IMMDeviceCollection *pDeviceCol;

  pDeviceEnum->EnumAudioEndpoints(_devicedataFlow, DEVICE_STATE_ACTIVE, &pDeviceCol);

  UINT numDev = 0;
  //should add checking if number of device is below 1
  pDeviceCol->GetCount(&numDev);
  
  _clear_soundDevices();
  if(numDev > 0){
    soundDevices.reserve(numDev);
    for(int i = 0; i < numDev; i++){
      IMMDevice *currDev;
      auto _errcode = pDeviceCol->Item(i, &currDev);
      if(FAILED(_errcode))
        continue;

      _addDevice(currDev);
      currDev->Release();
    }
  }
  else if(pAudioDevice){
    StopListening();

    pAudioDevice->Release();
    pAudioDevice = NULL;
  }

  pDeviceCol->Release();
}


HRESULT SoundGet::StartListening(int updatePerSec){
  if(isListening)
    return S_FALSE;
  
  if(isFault_setDevice){
    printf("A fault has happened when setting the device. Soundget will not starting.\n");
    return S_FALSE;
  }

  upPerSec = updatePerSec <= -1? upPerSec: updatePerSec;

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
    if(pWaveFormatEx){
      CoTaskMemFree(pWaveFormatEx);
      pWaveFormatEx = NULL;
    }

    CoTaskMemFree(pWaveFormat);
    pWaveFormat = NULL;
    pAudioClient->Release();
    printf("\n");
    return WAVE_FORMAT_UNKNOWN;
  }

  errcode = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, _devicedataFlow == eRender? AUDCLNT_STREAMFLAGS_LOOPBACK: 0, MINDELAY, 0, pWaveFormat, NULL);
  RETURNIFERROR(errcode);

  errcode = pAudioClient->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClient);
  RETURNIFERROR(errcode);
  
  errcode = pAudioClient->Start();
  RETURNIFERROR(errcode);

  isListening = true;
  audioThread = std::thread(&SoundGet::_ThreadUpdate, this);
  return S_OK;
}

void SoundGet::StopListening(){
  if(isListening){
    isListening = false;

    if(audioThread.joinable())
      audioThread.join();

    pAudioClient->Stop();
    pCaptureClient->Release();
    if(pWaveFormatEx){
      CoTaskMemFree(pWaveFormatEx);
      pWaveFormatEx = NULL;
    }

    CoTaskMemFree(pWaveFormat);
    pWaveFormat = NULL;

    pAudioClient->Release();
  }
}

const std::vector<SoundDevice> *SoundGet::GetDevices(){
  return &soundDevices;
}

const SoundDevice *SoundGet::GetDevice(int index){
  if(index < 0){
    if(!pAudioDevice)
      return NULL;
    
    LPWSTR _devid;
    pAudioDevice->GetId(&_devid);
    if(!_devid)
      return NULL;

    index = GetIndex(_devid);
    CoTaskMemFree(_devid);
    if(index < 0)
      return NULL;
  }

  return &soundDevices[index];
}

const WAVEFORMATEX *SoundGet::GetWaveFormat(){
  return pWaveFormat;
}

int SoundGet::GetIndex(LPCWSTR devID){
  if(devID == NULL)
    return -1;

  for(size_t i = 0; i < soundDevices.size(); i++){
    SoundDevice *_currdev = &soundDevices[i];
    if(_currdev->deviceID && wcscmp(_currdev->deviceID, devID) == 0)
      return i;
  }

  return -1;
}

int SoundGet::GetIndex(){
  return currentIndex;
}

HRESULT SoundGet::SetDevice(int deviceIndex){
  if(isListening){
    fprintf(stderr, "Class SoundGet still getting audio data from current device. Stop it first.\n");
    return S_FALSE;
  }

  HRESULT errcode = S_FALSE;
  if(pAudioDevice){
    pAudioDevice->Release();
    pAudioDevice = NULL;
  }

  if(deviceIndex < 0)
    errcode = pDeviceEnum->GetDefaultAudioEndpoint(_devicedataFlow, SOUNDGET_EROLE, &pAudioDevice);
  else if(deviceIndex < soundDevices.size())
    errcode = pDeviceEnum->GetDevice(soundDevices[deviceIndex].deviceID, &pAudioDevice);

  if(FAILED(errcode)){
    fprintf(stderr, "Cannot use sound device of index (%d).\nErrcode 0x%X\n", deviceIndex, errcode);
    isFault_setDevice = true;

    return errcode;
  }
  else{
    _changedHandler->Set_UsingDefault(deviceIndex < 0);
    currentIndex = deviceIndex;
    isFault_setDevice = false;
    
    return S_OK;
  }
}

HRESULT SoundGet::SetDevice(LPCWSTR devId){
  if(isListening){
    fprintf(stderr, "Class SoundGet still getting audio data from current device. Stop it first.\n");
    return S_FALSE;
  }

  if(!devId)
    return E_NOTFOUND;

  HRESULT errcode = S_FALSE;
  IMMDevice *_tmp;
  errcode = pDeviceEnum->GetDevice(devId, &_tmp);
  RETURNIFERROR(errcode);

  if(pAudioDevice){
    pAudioDevice->Release();
    pAudioDevice = NULL;
  }

  pAudioDevice = _tmp;

  _changedHandler->Set_UsingDefault(false);

  int devidx = GetIndex(devId);
  if(devidx)
    currentIndex = devidx;
  else{
    // TODO error message here
  }

  isFault_setDevice = false;
  return S_OK;
}

HRESULT SoundGet::SetDeviceFlow(EDataFlow flow){
  if(isListening){
    fprintf(stderr, "Class SoundGet still getting audio data from current device. Stop it first.\n");
    return S_FALSE;
  }

  if(flow >= 0 && flow < EDataFlow_enum_count){
    _devicedataFlow = flow;
    _RefreshDevices();

    // TODO returning certain error code
    return (soundDevices.size() <= 0)? S_FALSE: S_OK;
  }

  return S_FALSE;
}

void SoundGet::SetUpdate(int updatePerSec){
  std::lock_guard<std::mutex> _lg(processingMutex);
  upPerSec = updatePerSec > 0? updatePerSec: upPerSec;
}

void SoundGet::SetNumBands(int num){
  std::lock_guard<std::mutex> _lg(processingMutex);
  numOfBands = num;
}

SoundGet::_NotificationHandling *SoundGet::GetNotificationHandler(){
  return _changedHandler;
}