#include <winsock2.h>
#include <ws2tcpip.h>
#include "Windows.h"
#include "initguid.h"
#include "Audioclient.h"
#include "mmdeviceapi.h"
#include "mmreg.h"
#include "functiondiscoverykeys_devpkey.h"

#include "stdio.h"
#include "math.h"

#include "utility"
#include "fstream"
#include "chrono"
#include "thread"

#define min(a,b)            (((a) < (b)) ? (a) : (b))


#define REFTIMES_PER_SEC 1000000
#define REFTIMES_PER_MILLISEC 1000

const float PI_NUM = 22.f/7;


WSAData *wsadat = NULL;

const int datasize = 50;
const uint16_t port = 3022;
sockaddr_in *localHostAddress = new sockaddr_in(); 

void closeSocket(){
  delete wsadat;
  WSACleanup();
}

void initSocket(){
  wsadat = new WSADATA();
  int errcode = WSAStartup(MAKEWORD(2,2), wsadat);
  if(errcode != 0){
    throw;
  }

  localHostAddress->sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
  localHostAddress->sin_family = AF_INET;
  localHostAddress->sin_port = htons(port);
}


float halfFloatToFloat(UINT16 data){
  int significand = (0xffff >> 6) & data;
  int exponent = ((0xff >> 3) & (data >> 10))-15;
  int sign = (data >> 15) > 0;

  if(exponent > 15)
    return INFINITE;
  
  float res;
  
  if(exponent == -15)
    res = powf(-1, sign) * powf(2, -14) * (0 + (float)significand/1024);
  else
    res = powf(-1, sign) * powf(2, exponent) * (1 + (float)significand/1024);

  return res;
}

float f_max(float *arr, int from, int to){
  float res = arr[from];
  for(int i = from+1; i < to; i++){
    if(res < arr[i])
      res = arr[i];
  }

  return res;
}

float f_avg(float *arr, int from, int to){
  float res = arr[from];
  for(int i = from+1; i < to; i++){
    res += arr[i];
  }

  return res / (to-from);
}

// return a pair of real and imaginary number
std::pair<float, float> eulerFunction(int k, int n, int manySamples){
  float real = 0.f, imag = 0.f;

  float part1 = -((float)2*PI_NUM*k*n)/manySamples;
  real = cosf(part1);
  imag = sinf(part1);

  std::pair<float, float> res = {real, imag};
  return res;
}

// return a pair of real and imaginary number time Xn
std::pair<float, float> DFT_partFunction(int k, int n, int manySamples, float Xn){
  auto pres = eulerFunction(k, n, manySamples);
  return std::pair<float, float>(pres.first * Xn, pres.second * Xn);
}

float magnitude_pair(const std::pair<float, float> &p){
  return powf(powf(p.first, 2) + powf(p.second, 2), 0.5f);
}

void add_pair(std::pair<float, float> &p_to, const std::pair<float, float> &p_from){
  p_to = std::pair<float, float>(p_to.first + p_from.first, p_to.second + p_from.second);
}

void exitIfFailed(HRESULT errcode){
  if(FAILED(errcode)){
    printf("Error occurred, exiting.\nErrcode: 0x%X\n", errcode);
    exit(errcode);
  }
}

unsigned short test_floatdata[] = {0b0000000000000000, 0b0000000000000001, 0b0000001111111111, 0b0000010000000000, 0b0011010101010101, 0b0011101111111111, 0b0011110000000000, 0b0011110000000001, 0b0111101111111111, 0b1000000000000000, 0b1100000000000000, 0b1111110000000000};
int test_floatdatalen = 12;

void dft_test(){
  float fData[] = {0.f, 0.707f, 1.f, 0.707f, 0.f, -0.707f, -1.f, -0.707f, 0.f, 0.707f, 1.f, 0.707f, 0.f, -0.707f, -1.f, -0.707f};

  int freqbinlen = 8/2;
  float *freqbinarr = new float[freqbinlen];
  freqbinarr[0] = 0.f;

  for(int f_i = 1; f_i < freqbinlen; f_i++){
    std::pair<float, float> ri_pair(0.f, 0.f);

    // for now calculate one of the channels
    for(int frames_i = 0; frames_i < 16; frames_i++){
      add_pair(ri_pair, DFT_partFunction(f_i, frames_i, 16, fData[frames_i]));
    }

    ri_pair = std::pair<float, float>(ri_pair.first * 2, ri_pair.second * 2);
    freqbinarr[f_i] = magnitude_pair(ri_pair)/8;

    printf("Frequency %dHz, amplitude: %f\n", f_i, freqbinarr[f_i]);
  }

  delete freqbinarr;
}

int main(){
  printf("dft test\n");
  dft_test();
  printf("\n");

  initSocket();

  HRESULT errcode;
  errcode = CoInitialize(NULL);
  if(FAILED(errcode)){
    printf("Failed to initialize COM objects.\nErrcode:0x%X\n", errcode);
    return -1;
  }

  IMMDeviceEnumerator *pDeviceEnum;
  CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pDeviceEnum);
  if(FAILED(errcode)){
    printf("Failed to get device enumerator for the audio.\nErrcode:0x%X\n", errcode);
    return -1;
  }

  IMMDeviceCollection *pDevices;
  pDeviceEnum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pDevices);

  UINT n = 0;
  pDevices->GetCount(&n);

  IMMDevice *pDevice;

  printf("List of devices (%d):\n", n);
  for(int i = 0; i < n; i++){
    errcode = pDevices->Item(i, &pDevice);

    if(FAILED(errcode))
      continue;

    IPropertyStore *pDeviceProperty;
    errcode = pDevice->OpenPropertyStore(STGM_READ, &pDeviceProperty);

    PROPVARIANT name;
    PropVariantInit(&name);
    errcode = pDeviceProperty->GetValue(PKEY_Device_FriendlyName, &name);
    
    LPWSTR namewstr = name.pwszVal;
    LPSTR namestr;

    int length = 0;
    length = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, namewstr, -1, NULL, length, NULL, NULL);

    namestr = (char*)malloc(length+1);
    namestr[length] = '\0';

    WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, namewstr, -1, namestr, length, NULL, NULL);

    printf("%d. %s\n", i, namestr);

    free(namestr);
    PropVariantClear(&name);
    pDevice->Release();
  }

  printf("\nChoose which capture device: ");
  int _intinput = 0;
  scanf("%d", &_intinput);

  printf("\n");

  if(_intinput < n && _intinput >= 0){
    errcode = pDevices->Item(_intinput, &pDevice);
    if(!FAILED(errcode)){
      REFERENCE_TIME hnsRequestedDuration = 10000;
      REFERENCE_TIME hnsActualDuration;

      WAVEFORMATEX *pwfx = NULL;
      WAVEFORMATEXTENSIBLE *pwfxtns = NULL;
      IAudioClient *pAudioClient = NULL;
      IAudioCaptureClient *pCaptureClient = NULL;

      errcode = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
      exitIfFailed(errcode);

      errcode = pAudioClient->GetMixFormat(&pwfx);
      exitIfFailed(errcode);
      // then get the extensible if it is FFFE
      // the current implementation is for float and pcm
      // if there's no format that support, just throw an error
      if(pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE){
        errcode = pAudioClient->GetMixFormat((WAVEFORMATEX**)&pwfxtns);
        exitIfFailed(errcode);

        if(pwfxtns->SubFormat == KSDATAFORMAT_SUBTYPE_PCM){
          printf("PcmType\n");
        }
        else if(pwfxtns->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT){
          printf("Float type\n");
        }
      }

      errcode = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, hnsRequestedDuration, 0, pwfx, NULL);
      exitIfFailed(errcode);

      printf("wtagformat %d\nbitshift %d\nchannels %d\nsamples per sec %d\nbytes per sec %d\nbits per sample %d\n", pwfx->wFormatTag, pwfx->nBlockAlign, pwfx->nChannels, pwfx->nSamplesPerSec, pwfx->nAvgBytesPerSec, pwfx->wBitsPerSample);

      UINT32 bufferSize = 0;
      errcode = pAudioClient->GetBufferSize(&bufferSize);
      exitIfFailed(errcode);

      printf("buffersize %d\n", bufferSize);
      exit(0);

      errcode = pAudioClient->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClient);
      exitIfFailed(errcode);

      hnsActualDuration = (double)REFTIMES_PER_SEC * bufferSize / pwfx->nSamplesPerSec;

      errcode = pAudioClient->Start();
      exitIfFailed(errcode);

      const int minWindowSize = 1024;
      const int maxFrequency = 5000;

      printf("%d\n", hnsActualDuration);
      pAudioClient->GetStreamLatency(&hnsActualDuration);
      printf("%d\n", hnsActualDuration);
      while(true){
        BYTE *pData;
        UINT32 packetLength = 0;

        int currentWindowSize = 0;

        errcode = pCaptureClient->GetNextPacketSize(&packetLength);

        // this is where the noise picking come from, it should run asyncronously, since we should send
        while(packetLength > 0){
          UINT32 numFramesAvailable = 0;
          DWORD flags;
          errcode = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);

          float *newfdata = reinterpret_cast<float*>(pData);
          int freqbinlen = numFramesAvailable/2;
          float *freqbinarr = new float[freqbinlen];

          for(int f_i = 0; f_i < freqbinlen; f_i++){
            std::pair<float, float> ri_pair(0.f, 0.f);

            // for now calculate one of the channels
            for(int frames_i = 0; frames_i < numFramesAvailable; frames_i++){
              add_pair(ri_pair, DFT_partFunction(f_i, frames_i, numFramesAvailable, newfdata[frames_i*2]));
            }

            ri_pair = std::pair<float, float>(ri_pair.first * 2, ri_pair.second * 2);

            // to get the amplitude, divide by number of samples
            //freqbinarr[f_i] = 20*log10f(magnitude_pair(ri_pair)/numFramesAvailable);
            freqbinarr[f_i] = magnitude_pair(ri_pair)/numFramesAvailable;
          }

          int incr = 1;
          int arrsize = freqbinlen;
          if(freqbinlen > datasize){
            incr = (int)ceilf((float)freqbinlen/datasize);
            arrsize = (int)ceilf((float)arrsize/incr);
          }

          SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
          if(connect(s, (sockaddr*)localHostAddress, sizeof(*localHostAddress)) < 0){
            printf("cannot use socket.\n");
          }
          else{
            send(s, reinterpret_cast<char*>(&arrsize), sizeof(int), 0);

            int data_i = 0;
            float *dataarr = new float[arrsize];
            
            for(int in = 0; in < freqbinlen; in += incr){
              float num = f_max(freqbinarr, in, in+incr) * 1000;
              dataarr[data_i] = num;
              data_i++;
            }

            send(s, reinterpret_cast<char*>(dataarr), sizeof(float) * arrsize, 0);

            delete dataarr;
          }
          
          delete freqbinarr;
          
          errcode = pCaptureClient->ReleaseBuffer(numFramesAvailable);
          errcode = pCaptureClient->GetNextPacketSize(&packetLength);

          std::this_thread::sleep_for(std::chrono::microseconds(1000000/30));
        }
      }
    }
    else{
      printf("Cannot get device.\nErrcode:%X\n", errcode);
    }
  }
  else
    printf("Wrong choice.\n");

  pDevices->Release();
  pDeviceEnum->Release();
  closeSocket();

  return 0;
}