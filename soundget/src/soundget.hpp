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
#include "mutex"

#define DEFAULT_UPDATERATE 30
#define SOUNDGET_EROLE eConsole


struct SoundDevice{
  LPWSTR deviceID;
  std::string deviceName;
};

class SoundGet{
  public:
    enum SoundGetNotification_changeStatus{
      SGN_STATUS_DEFAULTCHANGED,
      SGN_STATUS_ADDED,
      SGN_STATUS_REMOVED
    };

    typedef void(*SoundGetNotification_cb)(SoundGetNotification_changeStatus status);

    class _NotificationHandling: IMMNotificationClient{
      protected:
        enum _notificationType{
          _SGNT_DEFDEVCHANGED,
          _SGNT_DEVADDED,
          _SGNT_DEVREMOVED,
          _SGNT_DEVSTATECHANGED
        };

        struct _notificationData{
          _notificationType type;
          LPWSTR devID;
          EDataFlow flow;
          ERole role;
          DWORD state;
          PROPERTYKEY key;
        };

        HWND _bindProc;
        UINT _bindMsg;
        ULONG _cRef;

        SoundGet *_bound_soundget;
        SoundGetNotification_cb _cb;

        std::mutex _notifs_mutex;
        std::vector<_notificationData> _notifs;

        bool _using_default = true;

        bool _check_dataflow(LPCWSTR devId, EDataFlow flow);

        HRESULT _onDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId);
        HRESULT _onDeviceAdded(LPCWSTR pwstrDeviceId);
        HRESULT _onDeviceRemoved(LPCWSTR pwstrDeviceId);
        HRESULT _onDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD newState);

        void _addNotificationData(_notificationType type, LPCWSTR devID, EDataFlow flow, ERole role, DWORD state, PROPERTYKEY key);
      
      public:
        _NotificationHandling();
        void BindSoundGet(SoundGet *_soundget);
        void UseCallback_onDeviceChanged(SoundGetNotification_cb cb);
        void Set_UsingDefault(bool b);

        void CheckNotifications();
        void BindProcess(HWND hWnd, UINT msg);

        ULONG STDMETHODCALLTYPE AddRef();
        ULONG STDMETHODCALLTYPE Release();
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **ppvInterface);

        HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId);
        HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId);
        HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId);
        HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD newState);
        HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key);
    };

    friend class SoundGet::_NotificationHandling;


  protected:
    IMMDeviceEnumerator *pDeviceEnum = NULL;
    IMMDevice *pAudioDevice = NULL;
    IAudioClient *pAudioClient = NULL;
    IAudioCaptureClient *pCaptureClient = NULL;
    WAVEFORMATEX *pWaveFormat = NULL;
    WAVEFORMATEXTENSIBLE *pWaveFormatEx = NULL;

    _NotificationHandling *_changedHandler;

    int numOfBands = 8;
    int currentWaveFormat;

    EDataFlow _devicedataFlow = eRender;

    bool isFault_setDevice = false;
    bool isListening = false;
    int upPerSec = DEFAULT_UPDATERATE;
    float _processingTime = 0.0f;
    std::thread audioThread;
    std::mutex processingMutex;

    void _ThreadUpdate();
    float _GetFloatInTime(BYTE *data);
    

  private:
    std::vector<SoundDevice> soundDevices{};
    int currentIndex = -1;
    
    IMMDeviceEnumerator *_getDeviceEnum(){
      return pDeviceEnum;
    }


    void _refresh_soundDevice(SoundDevice *dev, IMMDevice *idev);
    void _addDevice(IMMDevice *dev);
    void _clear_soundDevices();
    void _RefreshDevices();

  public:
    // this function will send array that is in the same length as band number times number of channels
    void (*CallbackListener)(const float *amplitudes, int arrlen, const WAVEFORMATEX *waveformat);

    // this function will send raw array, which the same length as window size
    void (*CallbackListenerRaw)(const float *amplitudes, int arrlen, const WAVEFORMATEX *waveformat);

    SoundGet(int deviceIndex = -1);
    ~SoundGet();

    HRESULT StartListening(int updatePerSec = -1);
    bool IsListening(){
      return isListening;
    }

    void StopListening();
    const std::vector<SoundDevice> *GetDevices();
    const SoundDevice *GetDevice(int index = -1);
    const WAVEFORMATEX* GetWaveFormat();
    int GetIndex(LPCWSTR devID);
    int GetIndex();
    HRESULT SetDevice(int deviceIndex = -1);
    HRESULT SetDevice(LPCWSTR devId);
    HRESULT SetDeviceFlow(EDataFlow flow);
    void SetUpdate(int updatePerSec);
    void SetNumBands(int num);
    float GetTime(){
      return _processingTime;
    }
    
    EDataFlow GetDataFlow(){
      return _devicedataFlow;
    }

    _NotificationHandling *GetNotificationHandler();
};

#endif