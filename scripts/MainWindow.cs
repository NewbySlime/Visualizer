using Godot;
using System;
using System.Diagnostics;
using System.Collections.Generic;


public class MainWindow: Control{
  [Export]
  private PackedScene _popupEdit, _popupEdit_IPPort;
  [Export]
  private float _waitForSGConn;
  [Export]
  private string _errorSGDcMsg;
  [Export]
  private string _soundget_appname = "Soundget.exe";

  [Export]
  private float max_intensity = 20f;

  // constants
  private const int MaxPresetNameLength = 16, MaxSplits = 32;

  private enum _editContent{
    // general data
    PresetName,
    ManyLEDs,
    ColorMode,
    Brightness,

    // static
    ValuePosx,

    // RGB
    SpeedChange,
    WindowSlide,
    InUnion,

    // Sound
    ColorShifting,
    BrightnessMode,
    MaxIntensity,
    MinIntensity,

    // parts
    DeletePart,
    RangeEnd,
    RangeStart,
    Reverse,
    Channels
  }

  private enum _colorTypes{
    Static,
    RGB,
    Sound
  }

  private enum _brightnessMode{
    Static,
    Union,
    Individual
  }

  private enum _progCodes{
    // stop this soundget program
    STOP_PROG = 0x0001,
    // temporarily stop soundget (not program)
    STOP_SOUNDGET = 0x0002,
    START_SOUNDGET = 0x0003,
    // sending sound data (both mc and tool)
    // only used when it doesn't use the sound color type
    SOUNDDATA_DATA = 0x0010,
    // forwarding to mcu / to the tool
    FORWARD = 0x0020,
    // configuring the soundget system
    SET_CONFIG_SG = 0x0021,
    GET_CONFIG_SG = 0x0022,
    // vis tool -> soundget prog
    //    asking for state, or asking to reconnect or disconnect
    // soundget prog -> vis tool
    //    telling the vis tool current connection state of mcu
    CONNECT_MCU_STATE = 0x0030,
    // setting mcu ip address and port
    SETMCU_ADDRESS = 0x0031,
    GETMCU_ADDRESS = 0x0032
  }

  private enum _sg_configCodes{
    // [SET] the input device (index based)
    CFG_SG_INPUTDEVICE = 0x0001,
    // [GET] asking and send input device name
    CFG_SG_INPUTDEVICESNAME = 0x0002,
    // [GET] asking and send current or specified input device data (name, channels, and such)
    //    if the index specified is -1, then it will send the current device data
    //    if the index is above or same as 0, then it will send a device data based on the index
    CFG_SG_INPUTDEVICEDATA = 0x0003,
    // [SET] the sendrate
    CFG_SG_SENDRATE = 0x0005,
    // [SET] the delay of the sound
    CFG_SG_DELAYMS = 0x0006,
    // [GET] the numbers of channels
    CFG_SG_CHANNELS = 0x0007,
    // [SET] setting the flow
    // [GET] asking what the flow is
    CFG_SG_FLOW = 0x0008
  }

  private enum _mcu_connState{
    MCUCONNSTATE_CONNECTED = 0x01,
    MCUCONNSTATE_DISCONNECTED = 0x02,
    MCUCONNSTATE_RECONNECT = 0x03,
    MCUCONNSTATE_REQUESTSTATE = 0x04
  }

  private enum _mcu_forwardCodes{
    MCU_ASKALLPRESET = 0x01,
    MCU_UPDATEPRESETS = 0x02,
    MCU_USEPRESET = 0x03,

    MCU_SETPRESET = 0x100,
    MCU_SETPASSWORD = 0x101,
    MCU_SETREMOVEPASSWORD = 0x102,
    MCU_SETTIME = 0x103,

    MCU_GETPRESET = 0x200,
    MCU_GETPRESETLEN = 0x201,
    MCU_GETPASSWORDMAX = 0x202,
    MCU_GETPASSWORD = 0x203,
    MCU_GETTIME = 0x204,
    
    MCU_GETMCUINFO = 0x280,
    MCU_GETMCUINFO_BATTERYLVL = 0x281
  }

  private string[] _sg_flowtypes_str = new string[]{
    "Render Device",
    "Capture Device"
  };

  private enum _sgconnerrorbutton_codes{
    RUNSG,
    SANDBOX,
    QUIT
  }

  private enum _vdropdownpanel_codes{
    PASSWORD
  }

  private enum __pass_editcodes{
    SSID = 0,
    PASSWORD = 1,
    _DMP = 2,
    _LEN = 3,
    APPLY = -1
  }


  private struct _general_data{
    public int colorType;
    public float brightness;
  }

  private struct _static_data{
    public float valueposx;
  }

  private struct _RGB_data{
    public float rgb_speed;
    public float rgb_window;
    public bool in_union;
  }

  private struct _sound_data{
    public int color_shifting;
    public int brightness_mode;
    public float max_intensity;
    public float min_intensity;
  }

  private struct _split_data{
    public int start_led, end_led;
    public float range_start, range_end;
    public bool reversed;
    public int channel;
  }

  private class _presets_data{
    public string PresetName = "New Preset";
    public _general_data GeneralData;
    public _static_data StaticData;
    public _RGB_data RGBData;
    public _sound_data SoundData;
    public Color[] ColorDatas = new Color[1];
    public List<_split_data> SplitDatas = new List<_split_data>();

    public _presets_data Copy(){
      return (_presets_data)MemberwiseClone();
    }

    public static _presets_data DefaultCopy(){
      var _p = new _presets_data();
      _p.SplitDatas.Add(new _split_data());
      return _p;
    }
  }

  private int _manyLed = 0;
  private int _constManyLed = 0;

  private int currentPresetIdx = 0;

  private HBoxContainer _hbcont_apply;
  private VBoxContainer _vbcont_color, _vbcont_main;
  private SliderColor _sliderColor;
  private Splitter _splitter;
  private Visualizer _vis;
  private ShaderMaterial _vismat;
  private PopupEdit _pedit, _popupPassEdit;
  private PopupEdit_IPPort _pedit_IPPopup;
  private VisualizerDropdown _dropdown;

  private IEditInterface _presetChoice, _presetButtonAdd, _presetButtonRemove;
  private IEditInterface _presetNameEdit;

  private Image _split_datas_led = new Image(), _split_datas = new Image();

  private Timer _sgtimer;


  // this will not be changed until applied
  private List<_presets_data> _presets_list = new List<_presets_data>();
  // all the things that has been changed
  // for the first init, it's a copy of _presets_list
  private List<_presets_data> _changed_presets = new List<_presets_data>();
  private List<_presets_data> _reserved_presets = new List<_presets_data>();
  // the current data that's going to be changed
  private _presets_data _current_preset;
  private int _presetIdx = -1;
  private int _changedPresetSize = -1;

  private bool _alreadyConnToMCU = false;
  private ushort _set_manyPreset = 0;
  private bool _set_preset = true;

  private List<KeyValuePair<string, string>> 
    _mcu_passwordDatas = new List<KeyValuePair<string, string>>(),
    _mcu_passwordDatasEdit = new List<KeyValuePair<string, string>>();

  private bool _is_sandboxmode = false;
  private bool _is_sgconnected = false;


  private struct _sock_callbacks{
    public delegate void Callback(List<byte[]> datas);
    public Callback cb;
    public ushort manyParam;
  }

  private Dictionary<int, _sock_callbacks> _sock_callbackSet;

  private const ushort _def_mcuport = 3020;
  private const ushort _sockport = 3022;
  // means 127.0.0.1
  private const uint _sockip = 0x0100007F;
  private SocketListener _sockListener;


  private string _soundget_folderpath;



  private void _sendPasswordsDataMCU(){
    byte[] _data = new byte[(sizeof(ushort) * (_mcu_passwordDatas.Count * 2 + 1))];
    int _dataiter = 0;
    Array.Copy(BitConverter.GetBytes((ushort)_mcu_passwordDatas.Count), _data, sizeof(ushort)); _dataiter += sizeof(ushort);

    for(int i = 0; i < _mcu_passwordDatas.Count; i++){
      var _pair = _mcu_passwordDatas[i];
      Array.Resize<byte>(ref _data, _data.Length + _pair.Key.Length + _pair.Key.Length);

      Array.Copy(BitConverter.GetBytes((ushort)_pair.Key.Length), 0, _data, _dataiter, sizeof(ushort));
      _dataiter += sizeof(ushort);
      Array.Copy(_pair.Key.ToCharArray(), 0, _data, _dataiter, _pair.Key.Length);
      _dataiter += _pair.Key.Length;

      Array.Copy(BitConverter.GetBytes((ushort)_pair.Value.Length), 0, _data, _dataiter, sizeof(ushort));
      _dataiter += sizeof(ushort);
      Array.Copy(_pair.Value.ToCharArray(), 0, _data, _dataiter, _pair.Value.Length);
      _dataiter += _pair.Value.Length;
    }

    _sendMsg_forward(_mcu_forwardCodes.MCU_SETPASSWORD, _data);
  }

  private void _popuppassedit_onoptchanged(int __dmp, int id, object obj, PopupEdit popup){
    int _idx = id / (int)__pass_editcodes._LEN;
    switch((__pass_editcodes)(id % (int)__pass_editcodes._LEN)){
      case __pass_editcodes.SSID:{
        EditText.get_data data = obj as EditText.get_data;
        var _pair = _mcu_passwordDatasEdit[_idx];

        if(data.confirmed && data.strdata.Length == 0)
          data.strdata = _mcu_passwordDatas[_idx].Key;

        _mcu_passwordDatasEdit[_idx] = new KeyValuePair<string, string>(data.strdata, _pair.Value);
      }break;

      case __pass_editcodes.PASSWORD:{
        EditText.get_data data = obj as EditText.get_data;
        var _pair = _mcu_passwordDatasEdit[_idx];

        string _pass = _pair.Value;
        string _passhide = "";

        // erase
        if(data.strdata.Length < _pass.Length){
          _pass = _pass.Substr(0, data.strdata.Length);
        }

        // adding
        else if(data.strdata.Length > _pass.Length){
          _pass = _pass + data.strdata[data.strdata.Length-1];
        }

        for(int i = 0; i < _pass.Length; i++)
          _passhide += '*';
        
        _mcu_passwordDatasEdit[_idx] = new KeyValuePair<string, string>(_pair.Key, _pass);
        
        data.strdata = _passhide;
      }break;

      case __pass_editcodes.APPLY:{
        EditApplyCancel.get_param _cont = obj as EditApplyCancel.get_param;
        if(_cont.answer == EditApplyCancel.Answer.APPLY){
          bool _sendtomcu = true;
          const string _errformat = "Password #{0} has the same as Password #{1}.\nPassword #{2} will be reset.\n\n";
          string _errormsg = "";
          for(int i = 0; i < _mcu_passwordDatasEdit.Count; i++){
            var _pair = _mcu_passwordDatasEdit[i];
            if(_pair.Key == "")
              continue;

            for(int o = i+1; o < _mcu_passwordDatasEdit.Count; o++){
              var _pair2 = _mcu_passwordDatasEdit[o];
              if(_pair2.Key == "")
                continue;
              
              if(_pair.Key == _pair2.Key){
                _mcu_passwordDatasEdit[o] = new KeyValuePair<string, string>("", _pair2.Value);
                _errormsg += string.Format(_errformat, i, o, i);
                _sendtomcu = false;
              }
            }

            _mcu_passwordDatas[i] = _pair;
          }

          if(_sendtomcu)
            _sendPasswordsDataMCU();
          else{
            _errormsg += "Passwords will not be send to MCU.";
            ErrorShowAutoload.Autoload.DisplayError(_errormsg);
          }
        }

        popup.Exit();
      }break;
    }
  }

  private void _on_editPanelChanged(int id, object obj){
    switch((_vdropdownpanel_codes)id){
      case _vdropdownpanel_codes.PASSWORD:{
        IEditInterface_Create.EditInterfaceContent[] _passEdit = {
          new IEditInterface_Create.EditInterfaceContent{
            TitleName = "SSID {0}",
            EditType = IEditInterface_Create.InterfaceType.Text,
            ID = (int)__pass_editcodes.SSID
          },

          new IEditInterface_Create.EditInterfaceContent{
            TitleName = "Password",
            EditType = IEditInterface_Create.InterfaceType.Text,
            ID = (int)__pass_editcodes.PASSWORD
          },

          new IEditInterface_Create.EditInterfaceContent{
            TitleName = "",
            EditType = IEditInterface_Create.InterfaceType.StaticText,
            Properties = new EditStaticText.set_param{},
            ID = (int)__pass_editcodes._DMP
          }
        };

        IEditInterface_Create.EditInterfaceContent[] _edit
          = new IEditInterface_Create.EditInterfaceContent[(_mcu_passwordDatas.Count * _passEdit.Length)+1];

        for(int i = 0; i < _edit.Length-1; i++){
          int _passidx = i / _passEdit.Length;
          int _idx = i % _passEdit.Length;
          var _editcont = _passEdit[_idx];
          _editcont.ID = i;

          switch((__pass_editcodes)_idx){
            case __pass_editcodes.SSID:{
              GD.Print(_passidx, _mcu_passwordDatas[_passidx]);
              _editcont.TitleName = string.Format(_editcont.TitleName, _passidx+1);
              var _p = new EditText.set_param{strdata = _mcu_passwordDatas[_passidx].Key};

              _editcont.Properties = _p;
            }break;

            case __pass_editcodes.PASSWORD:{
              var _p = new EditText.set_param();

              _p.strdata = "";
              string _pass = _mcu_passwordDatas[_passidx].Value;
              for(int o = 0; o < _pass.Length; o++)
                _p.strdata += '*';

              _editcont.Properties = _p;
            }break;
          }

          _edit[i] = _editcont;
        }

        _edit[_edit.Length-1] = new IEditInterface_Create.EditInterfaceContent{
          TitleName = "",
          EditType = IEditInterface_Create.InterfaceType.ApplyCancel,
          Properties = new EditApplyCancel.set_param{
            alignment = HBoxContainer.AlignMode.End
          },

          ID = (int)__pass_editcodes.APPLY
        };


        _mcu_passwordDatasEdit.Clear();
        for(int i = 0; i < _mcu_passwordDatas.Count; i++)
          _mcu_passwordDatasEdit.Add(_mcu_passwordDatas[i]);

        _popupPassEdit.RectPosition = GetGlobalMousePosition();
        _popupPassEdit.SetInterfaceEdit(_edit);
        _popupPassEdit.Popup_();

      }break;
    }
  }

  private void _SGButtonPressed(){
    if(_is_sgconnected)
      _quit_sg();
    else
      _run_sg();
  }

  private void _sandboxButtonPressed(){
    if(_is_sandboxmode)
      _sandboxmode_quit();
    else
      _sandboxmode();
  }

  // this will not affect the actual preset, however, it will copy the actual preset for testing
  private void _sandboxmode(){
    if(!_is_sandboxmode){
      _is_sandboxmode = true;
      _dropdown.SetSandboxState(true);

      // copy all data to reserve
      _reserved_presets.Clear();
      for(int i = 0; i < _presets_list.Count; i++)
        _reserved_presets.Insert(i, _presets_list[i].Copy());

      _changePreset(_presetIdx);

      _sgtimer.Stop();
      _onEditorReadyToUse();

      _vis.SandboxMode = true;
    }
  }

  private void _sandboxmode_quit(){
    if(_is_sandboxmode){
      _is_sandboxmode = false;
      _dropdown.SetSandboxState(false);

      // copy reserve data to preset list and changed list
      _presets_list.Clear();
      _changed_presets.Clear();
      for(int i = 0; i < _reserved_presets.Count; i++){
        _presets_list.Insert(i, _reserved_presets[i].Copy());
        _changed_presets.Insert(i, _reserved_presets[i].Copy());
      }

      _reserved_presets.Clear();

      _manyLed = _constManyLed;
      _changePreset(_presetIdx);

      if(!_is_sgconnected)
        _dispSGError();
      
      _onEditorNotReadyToUse();

      _vis.SandboxMode = false;
    }
  }
  
  private void _timer_dconnsg(){
    _dispSGError();
  }

  private void _run_sg(){
    Process _sgproc = new Process{StartInfo = new ProcessStartInfo{
      FileName = _soundget_folderpath + _soundget_appname,
      UseShellExecute = false,
      RedirectStandardError = false,
      RedirectStandardOutput = false,
      CreateNoWindow = true
    }};
  }

  private void _quit_sg(){
    _sockListener.AppendData(BitConverter.GetBytes((ushort)_progCodes.STOP_PROG));
  }

  private void _dispSGError(){
    ErrorShowBlockAutoload.Autoload.DisplayError(
      _errorSGDcMsg, 
      new IEditInterface_Create.EditInterfaceContent[]{
        new IEditInterface_Create.EditInterfaceContent{
          TitleName = "Run Soundget",
          EditType = IEditInterface_Create.InterfaceType.Button,
          Properties = new EditButton.set_param{},
          ID = (int)_sgconnerrorbutton_codes.RUNSG
        },

        new IEditInterface_Create.EditInterfaceContent{
          TitleName = "Sandbox Mode",
          EditType = IEditInterface_Create.InterfaceType.Button,
          Properties = new EditButton.set_param{},
          ID = (int)_sgconnerrorbutton_codes.SANDBOX
        },
        
        new IEditInterface_Create.EditInterfaceContent{
          TitleName = "Quit",
          EditType = IEditInterface_Create.InterfaceType.Button,
          Properties = new EditButton.set_param{},
          ID = (int)_sgconnerrorbutton_codes.QUIT
        }
      },
      "_onpopup_SGError",
      this
    );
  }

  private void _onpopup_SGError(int id, object obj){
    switch((_sgconnerrorbutton_codes)id){
      case _sgconnerrorbutton_codes.RUNSG:{
        _sgtimer.Start();
        _run_sg();
      }break;

      case _sgconnerrorbutton_codes.SANDBOX:{
        _sandboxmode();
      }break;
    
      case _sgconnerrorbutton_codes.QUIT:{
        GetTree().Quit();
      }break;
    }

    ErrorShowBlockAutoload.Autoload.CloseError();
  }

  private void _onEditorReadyToUse(){
    _dropdown.LockPosition(false);
  }

  private void _onEditorNotReadyToUse(){
    if(!OS.IsDebugBuild() && !_is_sandboxmode && !_alreadyConnToMCU){
      _dropdown.DoAnimation(true);
      _dropdown.LockPosition(true);
    }
  }

  private void _onChangeDev(int idx){
    _sendSGSetConfig(_sg_configCodes.CFG_SG_INPUTDEVICE, BitConverter.GetBytes((Int16)(idx-1)));
  }

  private void _onChangeFlow(int idx){
    _sendSGSetConfig(_sg_configCodes.CFG_SG_FLOW, BitConverter.GetBytes((Int16)idx));
  }

  // this function will not send preset datas to mcu
  private void _updateNewPreset_nosend(){
    _hbcont_apply.Visible = false;
    _presets_list.Clear();

    for(int i = 0; i < _changed_presets.Count; i++)
      _presets_list.Add(_changed_presets[i]);
  }

  private void _updateNewPreset(){
    _updatePresetDataMCU();
    _updateNewPreset_nosend();
  }

  private void _cancelNewPreset(){
    _hbcont_apply.Visible = false;
    _changed_presets.Clear();
    for(int i = 0; i < _presets_list.Count; i++)
      _changed_presets.Add(_presets_list[i]);
    
    if(_presetIdx < _presets_list.Count)
      _changePreset(_presetIdx);
    else
      _changePreset(_presets_list.Count-1);
    
    _updateChoice_preset();
  }

  private void _checkIfChanged(){
    _hbcont_apply.Visible = true;
  }

  // this should setup the shader and reset the option
  private void _changePreset(int idx, object obj = null){
    idx = obj == null? ((idx < _changed_presets.Count)? idx: 0): (obj as EditChoice.get_data).choiceID;

    _current_preset = _changed_presets[idx];

    IEditInterface_Create.Autoload.RemoveAllChild(_vbcont_main);
    IEditInterface_Create.EditInterfaceContent[] _mainContent_general = {
      new IEditInterface_Create.EditInterfaceContent{
        TitleName = "Many LEDs",
        EditType = IEditInterface_Create.InterfaceType.Text,
        Properties = new EditText.set_param{
          strdata = _manyLed.ToString(),
          readOnly = !OS.IsDebugBuild() && !_is_sandboxmode
        },

        ID = (int)_editContent.ManyLEDs
      },

      new IEditInterface_Create.EditInterfaceContent{
        TitleName = "Color Type",
        EditType = IEditInterface_Create.InterfaceType.Choice,
        Properties = new EditChoice.set_param{
          choices = new KeyValuePair<string, int>[]{
            new KeyValuePair<string, int>("Static", (int)_colorTypes.Static),
            new KeyValuePair<string, int>("RGB", (int)_colorTypes.RGB),
            new KeyValuePair<string, int>("Sound", (int)_colorTypes.Sound)
          },

          choiceID = _current_preset.GeneralData.colorType
        },

        ID = (int)_editContent.ColorMode
      },

      new IEditInterface_Create.EditInterfaceContent{
        TitleName = "Brightness",
        EditType = IEditInterface_Create.InterfaceType.Slider,
        Properties = new EditSlider.set_param{
          value = _current_preset.GeneralData.brightness
        },

        ID = (int)_editContent.Brightness
      }
    };

    IEditInterface_Create.Autoload.CreateAndAdd(_vbcont_main, this, "_on_generalOptionChanged", _mainContent_general);

    _presetNameEdit.ChangeContent(new EditText.set_param{
      strdata = _current_preset.PresetName
    });

    _vis.Bands = _manyLed;
    _vismat.SetShaderParam("manyLed", _manyLed);
    _vismat.SetShaderParam("brightness", _current_preset.GeneralData.brightness);
    _changeColor((_colorTypes)_current_preset.GeneralData.colorType);

    // update slider color
    _sliderColor.SetColors(_current_preset.ColorDatas);
  
    // update splitter
    _split_datas_led.Create(_current_preset.SplitDatas.Count, 1, false, Image.Format.Rgf);
    _split_datas.Create(_current_preset.SplitDatas.Count, 1, false, Image.Format.Rgbf);

    _splitter.RemoveAllIndex();
    for(int i = 0; i < _current_preset.SplitDatas.Count; i++){
      int _startIndex = _current_preset.SplitDatas[i].start_led;
      _splitter.AddIndex(
        i,
        _startIndex,
        _current_preset.SplitDatas[i].end_led-_startIndex,
        false
      );
    }
    _splitter.Ticks = _manyLed;
    _updateSplits();
  
    _vis.ResetOffset();

    _presetIdx = idx;
  }

  private void _updateChoice_preset(){
    var _newchoices = new KeyValuePair<string, int>[_changed_presets.Count];
    for(int i = 0; i < _changed_presets.Count; i++)
      _newchoices[i] = new KeyValuePair<string, int>(_changed_presets[i].PresetName, i);
    
    _presetChoice.ChangeContent(new EditChoice.set_param{
      choices = _newchoices,
      choiceID = _presetIdx
    });
  }

  private void _addPreset(int _dmp1 = 0, int _dmp2 = 0){
    _checkIfChanged();
    _changed_presets.Add(_presets_data.DefaultCopy());
    _changePreset(_changed_presets.Count-1);

    _updateChoice_preset();
  }

  private void _deletePreset(int _dmp1 = 0, int _dmp2 = 0){
    if(_changed_presets.Count > 1){
      _checkIfChanged();
      _changed_presets.RemoveAt(_presetIdx);
      _updateChoice_preset();
    }
    else
      ErrorShowAutoload.Autoload.DisplayError("Cannot delete last preset.");
  }
  
  // putting the specified split to an array
  private void _updateOnSplit(int idx){
    var _data = _current_preset.SplitDatas[idx];

    _split_datas.Lock();
    Color _data1 = new Color(
      _data.range_start,
      _data.range_end,
      (float)(
        ((_data.reversed? 1: 0)
        | (_data.channel << 1))
        & 0xff 
      ) / 0xff
    );


    _split_datas.SetPixel(idx, 0, _data1);
    _split_datas.Unlock();

    _split_datas_led.Lock();
    Color _data2 = new Color{
      r = ((float)_data.start_led-0.5f)/_manyLed,
      g = (float)_data.end_led/_manyLed
    };

    _split_datas_led.SetPixel(idx, 0, _data2);
    _split_datas_led.Unlock();
  }
  
  private void _updateAllSplit(){
    _split_datas_led.Create(_current_preset.SplitDatas.Count, 1, false, Image.Format.Rgf);
    _split_datas.Create(_current_preset.SplitDatas.Count, 1, false, Image.Format.Rgbf);
    
    for(int i = 0; i < _current_preset.SplitDatas.Count; i++){
      var _data = _current_preset.SplitDatas[i];
      var _idxs = _splitter.GetButtonBoundIndex(i);
      _data.start_led = _idxs.Key;
      _data.end_led = _idxs.Value;
      _current_preset.SplitDatas[i] = _data;
      _updateOnSplit(i);
    }
    
    _updateSplits();
  }

  // sending the split datas to the shader
  private void _updateSplits(){
    ImageTexture texdata = new ImageTexture();
    texdata.CreateFromImage(_split_datas, 0);
    _vismat.SetShaderParam("splitsData", texdata);
    
    ImageTexture texdataled = new ImageTexture();
    texdataled.CreateFromImage(_split_datas_led, 0);
    _vismat.SetShaderParam("splitsDataLed", texdataled);
    _vismat.SetShaderParam("splitsDataLen", _current_preset.SplitDatas.Count);
  }

  // adding a split
  private void _addSplit(int idx, int tick, int ticksize){
    _checkIfChanged();
    _splitter.AddIndex(idx, tick, ticksize);
  }

  // removing a split
  private void _removeSplit(int idx){
    _checkIfChanged();
    _splitter.RemoveIndex(idx);
  }

  private void _popupedit_onoptchanged(int idPopup, int idOpt, object obj, Popup popup){
    _checkIfChanged();
    switch((_editContent)idOpt){
      case _editContent.DeletePart:{
        _removeSplit(idPopup);
        popup.Visible = false;
        break;
      }

      case _editContent.RangeEnd:{
        var param = obj as EditSlider.get_data;
        var _data = _current_preset.SplitDatas[idPopup];
        float _f = param.fdata/_manyLed;
        if(_f < _data.range_start)
          param.fdata = _data.range_start*_manyLed;

        _data.range_end = _f;
        _current_preset.SplitDatas[idPopup] = _data;
        _updateOnSplit(idPopup);
        _updateSplits();
        break;
      }

      case _editContent.RangeStart:{
        var param = obj as EditSlider.get_data;
        var _data = _current_preset.SplitDatas[idPopup];
        float _f = param.fdata/_manyLed;
        if(_f > _data.range_end)
          param.fdata = _data.range_end*_manyLed;

        _data.range_start = _f;
        _current_preset.SplitDatas[idPopup] = _data;
        _updateOnSplit(idPopup);
        _updateSplits();
        break;
      }

      case _editContent.Reverse:{
        var _data = _current_preset.SplitDatas[idPopup];
        var param = obj as EditCheck.get_data;
        _data.reversed = param.bdata;

        _current_preset.SplitDatas[idPopup] = _data;
        _updateOnSplit(idPopup);
        _updateSplits();
        
        break;
      }

      case _editContent.Channels:{
        var _data = _current_preset.SplitDatas[idPopup];
        var param = obj as EditChoice.get_data;

        _data.channel = param.choiceID;
        _current_preset.SplitDatas[idPopup] = _data;
        _updateOnSplit(idPopup);
        _updateSplits();
        break;
      }
    }
  }

  private void _splitter_onPressed(int idx){
    _pedit.ID = idx;

    // then setting up the edit
    List<KeyValuePair<string, int>> _channel_idx = new List<KeyValuePair<string, int>>();
    for(int i = 0; i < _vis.Channels; i++)
      _channel_idx.Add(new KeyValuePair<string, int>("Channel-" + i.ToString(), i));

    var _data = _current_preset.SplitDatas[idx];
    IEditInterface_Create.EditInterfaceContent[] _split_content = {
      new IEditInterface_Create.EditInterfaceContent{
        TitleName = "Range End",
        EditType = IEditInterface_Create.InterfaceType.Slider,
        Properties = new EditSlider.set_param{
          value = _data.range_end*_manyLed,
          max_value = _manyLed,
          rounded = true
        },

        ID = (int)_editContent.RangeEnd
      },

      new IEditInterface_Create.EditInterfaceContent{
        TitleName = "Range Start",
        EditType = IEditInterface_Create.InterfaceType.Slider,
        Properties = new EditSlider.set_param{
          value = _data.range_start*_manyLed,
          max_value = _manyLed,
          rounded = true
        },

        ID = (int)_editContent.RangeStart
      },

      new IEditInterface_Create.EditInterfaceContent{
        TitleName = "Reversed",
        EditType = IEditInterface_Create.InterfaceType.Check,
        Properties = new EditCheck.set_param{
          check = _data.reversed
        },

        ID = (int)_editContent.Reverse
      },

      new IEditInterface_Create.EditInterfaceContent{
        TitleName = "Channels",
        EditType = IEditInterface_Create.InterfaceType.Choice,
        Properties = new EditChoice.set_param{
          choices = _channel_idx.ToArray(),
          choiceID = _data.channel
        },

        ID = (int)_editContent.Channels
      },

      new IEditInterface_Create.EditInterfaceContent{
        TitleName = "Delete Part",
        EditType = IEditInterface_Create.InterfaceType.Button,
        Properties = new EditButton.set_param{
          id = idx
        },

        ID = (int)_editContent.DeletePart
      }
    };

    _pedit.SetInterfaceEdit(_split_content);
    
    _pedit.RectPosition = GetGlobalMousePosition();
    _pedit.Popup_();
  }

  private void _splitter_onAdded(int idx){
    _checkIfChanged();
    var _idx = _splitter.GetButtonBoundIndex(idx);
    _current_preset.SplitDatas.Insert(idx, new _split_data());
    _updateAllSplit();
  }

  private void _splitter_onDeleted(int idx){
    _checkIfChanged();
    var _data = _current_preset.SplitDatas[idx];
    _current_preset.SplitDatas.RemoveAt(idx);
    _updateAllSplit();
  }

  private void _splitter_adjusted(int idx, int t_1, int t_2, bool _end){
    _checkIfChanged();
    var _data = _current_preset.SplitDatas[idx];
    _data.start_led = t_1;
    _data.end_led = t_2;
    
    _current_preset.SplitDatas[idx] = _data;
    _updateOnSplit(idx);

    if(_end)
      _updateSplits();
  }

  private void _on_generalOptionChanged(int id, object obj){
    _checkIfChanged();
    switch((_editContent)id){
      case _editContent.PresetName:{
        EditText.get_data data = obj as EditText.get_data;
        if(data.strdata.Length > MaxPresetNameLength){
          ErrorShowAutoload.Autoload.DisplayError(string.Format("Preset name cannot have more than {0} characters.", MaxPresetNameLength));
          data.strdata = data.strdata.Substr(0, MaxPresetNameLength);
        }

        _current_preset.PresetName = data.strdata;
        (_presetChoice as EditChoice).ChangeTextItem(_presetIdx, data.strdata);
        break;
      }

      case _editContent.ManyLEDs:{
        EditText.get_data data = obj as EditText.get_data;
        int.TryParse(data.strdata, out _manyLed);
        data.strdata = _manyLed.ToString();

        _vis.Bands = _manyLed;
        _vismat.SetShaderParam("manyLed", _manyLed);
        _splitter.Ticks = _manyLed;
        break;
      }

      case _editContent.ColorMode:{
        EditChoice.get_data data = obj as EditChoice.get_data;
        _current_preset.GeneralData.colorType = data.choiceID;
        _changeColor((_colorTypes)data.choiceID);
        break;
      }

      case _editContent.Brightness:{
        EditSlider.get_data data = obj as EditSlider.get_data;
        _current_preset.GeneralData.brightness = data.fdata;
        _vismat.SetShaderParam("brightness", data.fdata);
        break;
      }
    }
  }

  private void _on_optionChanged(int id, object obj){
    _checkIfChanged();
    switch((_editContent)id){
      case _editContent.ValuePosx:{
        EditSlider.get_data data = obj as EditSlider.get_data;
        _current_preset.StaticData.valueposx = data.fdata;
        _vismat.SetShaderParam("static_valueposx", data.fdata);
        break;
      }

      case _editContent.SpeedChange:{
        EditText.get_data data = obj as EditText.get_data;
        float speed; float.TryParse(data.strdata, out speed);
        _current_preset.RGBData.rgb_speed = speed;
        _vis.OffsetMultiplier = speed;
        data.strdata = speed.ToString();
        break;
      }

      case _editContent.WindowSlide:{
        EditSlider.get_data data = obj as EditSlider.get_data;
        _current_preset.RGBData.rgb_window = data.fdata;
        _vismat.SetShaderParam("rgb_window", data.fdata);
        break;
      }

      case _editContent.InUnion:{
        EditCheck.get_data data = obj as EditCheck.get_data;
        _current_preset.RGBData.in_union = data.bdata;
        _vismat.SetShaderParam("rgb_inunion", data.bdata);
        break;
      }

      case _editContent.ColorShifting:{
        EditChoice.get_data data = obj as EditChoice.get_data;
        _current_preset.SoundData.color_shifting = data.choiceID;
        _vismat.SetShaderParam("color_shift", data.choiceID);
        break;
      }

      case _editContent.BrightnessMode:{
        EditChoice.get_data data = obj as EditChoice.get_data;
        _current_preset.SoundData.brightness_mode = data.choiceID;
        _vismat.SetShaderParam("brightness_mode", data.choiceID);
        break;
      }

      case _editContent.MaxIntensity:
      case _editContent.MinIntensity:{
        EditSlider.get_data data = obj as EditSlider.get_data;
        if(id == (int)_editContent.MaxIntensity){
          if(data.fdata < _current_preset.SoundData.min_intensity)
            data.fdata = _current_preset.SoundData.min_intensity;

          _current_preset.SoundData.max_intensity = data.fdata;
        }
        else{
          if(data.fdata > _current_preset.SoundData.max_intensity)
            data.fdata = _current_preset.SoundData.max_intensity;
          _current_preset.SoundData.min_intensity = data.fdata;
        }
        
        _vis.MinMaxIntensity = new Vector2(_current_preset.SoundData.min_intensity, _current_preset.SoundData.max_intensity);
        break;
      }
    }
  }

  private int _getPresetDataSize(_presets_data pdata){
    return
      MaxPresetNameLength +
      
      // general data
      sizeof(int) +
      sizeof(float) +

      // static data
      sizeof(float) +

      // rgb data
      sizeof(float) +
      sizeof(float) +
      sizeof(bool) +

      // sound data
      sizeof(int) +
      sizeof(int) +
      sizeof(float) +
      sizeof(float) +

      // color data
      sizeof(ushort) +
      ((sizeof(float) + (sizeof(byte)*3)) * pdata.ColorDatas.Length) +

      // split data
      sizeof(ushort) +
      (
        (
          sizeof(int) +
          sizeof(int) +
          sizeof(float) +
          sizeof(float) +
          sizeof(bool) +
          sizeof(int)
        ) * pdata.SplitDatas.Count
      )
    ;
  }

  private void _sendPresetDataMCU(ushort idx, _presets_data pdata){
    int datalen = 
      sizeof(ushort) +
      _getPresetDataSize(pdata);

    Byte[] data = new byte[datalen];
    int dataiter = 0;

    // Preset index
    Array.Copy(BitConverter.GetBytes(idx), 0, data, dataiter, sizeof(ushort)); dataiter += sizeof(ushort);
    
    // Preset name
    for(int i = 0; i < pdata.PresetName.Length && i < MaxPresetNameLength; i++)
      data[dataiter+i] = (byte)pdata.PresetName[i];
    dataiter += MaxPresetNameLength;
  
    // general data
    Array.Copy(BitConverter.GetBytes(pdata.GeneralData.colorType), 0, data, dataiter, sizeof(int)); dataiter += sizeof(int);
    Array.Copy(BitConverter.GetBytes(pdata.GeneralData.brightness), 0, data, dataiter, sizeof(float)); dataiter += sizeof(float);

    // static data
    Array.Copy(BitConverter.GetBytes(pdata.StaticData.valueposx), 0, data, dataiter, sizeof(float)); dataiter += sizeof(float);

    // RGB data
    Array.Copy(BitConverter.GetBytes(pdata.RGBData.rgb_speed), 0, data, dataiter, sizeof(float)); dataiter += sizeof(float);
    Array.Copy(BitConverter.GetBytes(pdata.RGBData.rgb_window), 0, data, dataiter, sizeof(float)); dataiter += sizeof(float);
    Array.Copy(BitConverter.GetBytes(pdata.RGBData.in_union), 0, data, dataiter, sizeof(bool)); dataiter += sizeof(bool);

    // Sound data
    Array.Copy(BitConverter.GetBytes(pdata.SoundData.color_shifting), 0, data, dataiter, sizeof(int)); dataiter += sizeof(int);
    Array.Copy(BitConverter.GetBytes(pdata.SoundData.brightness_mode), 0, data, dataiter, sizeof(int)); dataiter += sizeof(int);
    Array.Copy(BitConverter.GetBytes(pdata.SoundData.max_intensity), 0, data, dataiter, sizeof(float)); dataiter += sizeof(float);
    Array.Copy(BitConverter.GetBytes(pdata.SoundData.min_intensity), 0, data, dataiter, sizeof(float)); dataiter += sizeof(float);

    // Color data
    Array.Copy(BitConverter.GetBytes((ushort)pdata.ColorDatas.Length), 0, data, dataiter, sizeof(ushort)); dataiter += sizeof(ushort);

    for(int i = 0; i < pdata.ColorDatas.Length; i++){
      var col = pdata.ColorDatas[i];
      Array.Copy(BitConverter.GetBytes(col.a), 0, data, dataiter, sizeof(float)); dataiter += sizeof(float);
      
      data[dataiter++] = (byte)col.r8;
      data[dataiter++] = (byte)col.g8;
      data[dataiter++] = (byte)col.b8;
    }

  
    // Sound data
    Array.Copy(BitConverter.GetBytes((ushort)pdata.SplitDatas.Count), 0, data, dataiter, sizeof(ushort)); dataiter += sizeof(ushort);

    for(int i = 0; i < pdata.SplitDatas.Count; i++){
      Array.Copy(BitConverter.GetBytes(pdata.SplitDatas[i].start_led), 0, data, dataiter, sizeof(int)); dataiter += sizeof(int);
      Array.Copy(BitConverter.GetBytes(pdata.SplitDatas[i].end_led), 0, data, dataiter, sizeof(int)); dataiter += sizeof(int);

      Array.Copy(BitConverter.GetBytes(pdata.SplitDatas[i].range_start), 0, data, dataiter, sizeof(float)); dataiter += sizeof(float);
      Array.Copy(BitConverter.GetBytes(pdata.SplitDatas[i].range_end), 0, data, dataiter, sizeof(float)); dataiter += sizeof(float);

      Array.Copy(BitConverter.GetBytes(pdata.SplitDatas[i].reversed), 0, data, dataiter, sizeof(bool)); dataiter += sizeof(bool);

      Array.Copy(BitConverter.GetBytes(pdata.SplitDatas[i].channel), 0, data, dataiter, sizeof(int)); dataiter += sizeof(int);
    }

    _sendMsg_forward(_mcu_forwardCodes.MCU_SETPRESET, data);
  }

  private void _askMCUInfo(){
    _sendMsg_forward(_mcu_forwardCodes.MCU_GETMCUINFO, new byte[]{});
  }

  private void _getPresetDataMCU(){
    _sendMsg_forward(_mcu_forwardCodes.MCU_ASKALLPRESET, new byte[]{});
  }

  private void _updatePresetDataMCU(){
    _sendMsg_forward(_mcu_forwardCodes.MCU_UPDATEPRESETS, BitConverter.GetBytes((ushort)_changed_presets.Count));
    _sendMsg_forward(_mcu_forwardCodes.MCU_USEPRESET, BitConverter.GetBytes((ushort)_presetIdx));

    for(int i = 0; i < _changed_presets.Count; i++)
      _sendPresetDataMCU((ushort)i, _changed_presets[i]);
  }

  private void _changeColor(_colorTypes type){
    _changeOption(type);
    _vismat.SetShaderParam("color_type", (int)type);
  }

  private void _changeOption(_colorTypes type){
    IEditInterface_Create.Autoload.RemoveAllChild(_vbcont_color);

    // setting up the option
    if(type == _colorTypes.Static || type == _colorTypes.Sound){
      IEditInterface_Create.EditInterfaceContent[] _mainContent_static = {
        new IEditInterface_Create.EditInterfaceContent{
          TitleName = "Value Position X",
          EditType = IEditInterface_Create.InterfaceType.Slider,
          Properties = new EditSlider.set_param{
            value = _current_preset.StaticData.valueposx
          },

          ID = (int)_editContent.ValuePosx
        }
      };

      
      _vismat.SetShaderParam("static_valueposx", _current_preset.StaticData.valueposx);
      IEditInterface_Create.Autoload.CreateAndAdd(_vbcont_color, this, "_on_optionChanged", _mainContent_static);
    }

    if(type == _colorTypes.RGB || type == _colorTypes.Sound){
      IEditInterface_Create.EditInterfaceContent[] _mainContent_rgb = {
        new IEditInterface_Create.EditInterfaceContent{
          TitleName = "Speed",
          EditType = IEditInterface_Create.InterfaceType.Text,
          Properties = new EditText.set_param{
            strdata = _current_preset.RGBData.rgb_speed.ToString()
          },

          ID = (int)_editContent.SpeedChange
        },

        new IEditInterface_Create.EditInterfaceContent{
          TitleName = "Color Portion (Window)",
          EditType = IEditInterface_Create.InterfaceType.Slider,
          Properties = new EditSlider.set_param{
            max_value = 2.0f,
            min_value = 0.01f,
            value = _current_preset.RGBData.rgb_window
          },

          ID = (int)_editContent.WindowSlide
        },

        new IEditInterface_Create.EditInterfaceContent{
          TitleName = "Changing color in union",
          EditType = IEditInterface_Create.InterfaceType.Check,
          Properties = new EditCheck.set_param{
            check = _current_preset.RGBData.in_union
          },

          ID = (int)_editContent.InUnion
        }
      };

      _vis.OffsetMultiplier = _current_preset.RGBData.rgb_speed;
      _vismat.SetShaderParam("rgb_window", _current_preset.RGBData.rgb_window);
      _vismat.SetShaderParam("rgb_inunion", _current_preset.RGBData.in_union);
      IEditInterface_Create.Autoload.CreateAndAdd(_vbcont_color, this, "_on_optionChanged", _mainContent_rgb);
    }

    if(type == _colorTypes.Sound){
      IEditInterface_Create.EditInterfaceContent[] _mainContent_sound = {
        new IEditInterface_Create.EditInterfaceContent{
          TitleName = "Color Shifting Mode",
          EditType = IEditInterface_Create.InterfaceType.Choice,
          Properties = new EditChoice.set_param{
            choices = new KeyValuePair<string, int>[]{
              new KeyValuePair<string, int>("Static", (int)_colorTypes.Static),
              new KeyValuePair<string, int>("RGB", (int)_colorTypes.RGB),
              new KeyValuePair<string, int>("Sound", (int)_colorTypes.Sound)
            },
            
            choiceID = _current_preset.SoundData.color_shifting
          },
          
          ID = (int)_editContent.ColorShifting
        },
        
        new IEditInterface_Create.EditInterfaceContent{
          TitleName = "Brightness Mode",
          EditType = IEditInterface_Create.InterfaceType.Choice,
          Properties = new EditChoice.set_param{
            choices = new KeyValuePair<string, int>[]{
              new KeyValuePair<string, int>("Static", (int)_brightnessMode.Static),
              new KeyValuePair<string, int>("Union", (int)_brightnessMode.Union),
              new KeyValuePair<string, int>("Individual", (int)_brightnessMode.Individual)
            },

            choiceID = _current_preset.SoundData.brightness_mode
          },

          ID = (int)_editContent.BrightnessMode
        },

        new IEditInterface_Create.EditInterfaceContent{
          TitleName = "Max Intensity",
          EditType = IEditInterface_Create.InterfaceType.Slider,
          Properties = new EditSlider.set_param{
            value = _current_preset.SoundData.max_intensity,
            max_value = max_intensity
          },

          ID = (int)_editContent.MaxIntensity
        },

        new IEditInterface_Create.EditInterfaceContent{
          TitleName = "Min Intensity",
          EditType = IEditInterface_Create.InterfaceType.Slider,
          Properties = new EditSlider.set_param{
            value = _current_preset.SoundData.min_intensity,
            max_value = max_intensity
          },

          ID = (int)_editContent.MinIntensity
        }
      };

      _vismat.SetShaderParam("color_shift", _current_preset.SoundData.color_shifting);
      _vismat.SetShaderParam("brightness_mode", _current_preset.SoundData.brightness_mode);
      _vis.MinMaxIntensity = new Vector2(_current_preset.SoundData.min_intensity, _current_preset.SoundData.max_intensity);
      IEditInterface_Create.Autoload.CreateAndAdd(_vbcont_color, this, "_on_optionChanged", _mainContent_sound);
    }
  }

  private void _onMsgGet_SounddataData(List<byte[]> datas){
    int datalen = BitConverter.ToInt32(datas[0], 0);
    float[] data = new float[datalen];
    for(int i = 0; i < datalen; i++)
      data[i] = BitConverter.ToSingle(datas[1], i*sizeof(float));

    _vis.UpdateVisualizer(ref data);
  }

  private void _onMsgGet_getCfgSg(List<byte[]> datas){
    if(datas[0].Length != sizeof(ushort))
      return;

    ushort code = BitConverter.ToUInt16(datas[0], 0);
    switch((_sg_configCodes)code){
      case _sg_configCodes.CFG_SG_INPUTDEVICESNAME:{
        if(datas[1].Length < sizeof(ushort))
          return;
        
        int offsetIndex = 0;

        ushort manydev = BitConverter.ToUInt16(datas[1], offsetIndex);
        offsetIndex += sizeof(ushort);

        string[] names = new string[manydev+1];
        names[0] = "Default";
        for(int i = 0; i < manydev && offsetIndex < datas[1].Length; i++){
          int strlen = BitConverter.ToInt32(datas[1], offsetIndex);
          offsetIndex += sizeof(int);

          char[] str = new char[strlen];
          Array.Copy(datas[1], offsetIndex, str, 0, strlen);
          offsetIndex += strlen;

          names[i+1] = new string(str);
        }

        _dropdown.SetChoiceName(VisualizerDropdown.ChoiceType.Device, names);
      }
      break;

      case _sg_configCodes.CFG_SG_INPUTDEVICEDATA:{
        int offsetIndex = 0;

        int currentIndex = BitConverter.ToInt32(datas[1], offsetIndex);
        offsetIndex += sizeof(int);

        int devnamelen = BitConverter.ToInt32(datas[1], offsetIndex);
        offsetIndex += sizeof(int);

        char[] devname = new char[devnamelen];
        Array.Copy(datas[1], offsetIndex, devname, 0, devnamelen);
        offsetIndex += devnamelen;

        ushort nchannel = BitConverter.ToUInt16(datas[1], offsetIndex);
        offsetIndex += sizeof(ushort);

        _dropdown.SetChoiceIndex(VisualizerDropdown.ChoiceType.Device, currentIndex+1);
        _dropdown.SetCurrentSoundDevData(new string(devname), nchannel);

        _vis.Channels = nchannel;
      }
      break;

      case _sg_configCodes.CFG_SG_CHANNELS:{
        _vis.Channels = BitConverter.ToUInt16(datas[1], 0);
      }
      break;

      case _sg_configCodes.CFG_SG_FLOW:{
        _dropdown.SetChoiceIndex(VisualizerDropdown.ChoiceType.Flow, BitConverter.ToInt16(datas[1], 0));
      }
      break;
    }
  }

  private void _sendMsg_forward(_mcu_forwardCodes code, Byte[] data){
    switch((_mcu_forwardCodes)code){
      case _mcu_forwardCodes.MCU_ASKALLPRESET:{
        _set_preset = true;
      }break;
    }

    lock(_sockListener){
      _sockListener.AppendData(BitConverter.GetBytes((ushort)_progCodes.FORWARD));

      Byte[] newdata = new Byte[sizeof(ushort)+data.Length];

      Array.Copy(BitConverter.GetBytes((ushort)code), 0, newdata, 0, sizeof(ushort));
      Array.Copy(data, 0, newdata, sizeof(short), data.Length);

      GD.Print("newdata len ", newdata.Length);
      _sockListener.AppendData(newdata);
    }
  }

  private void _onMsgGet_forward(List<Byte[]> datas){
    int dataiter = 0;
    ushort code = BitConverter.ToUInt16(datas[0], 0); dataiter += sizeof(ushort);
    GD.Print("code ", code);
    switch((_mcu_forwardCodes)code){
      case _mcu_forwardCodes.MCU_SETPRESET:{
        if(_set_preset){
          try{
            ushort preset_idx = BitConverter.ToUInt16(datas[0], dataiter); dataiter += sizeof(ushort);

            GD.Print("preset index: ", preset_idx);

            _presets_data pdata = new _presets_data();
            pdata.PresetName = "";
            for(int i = 0; i < MaxPresetNameLength && datas[0][dataiter+i] != 0; i++)
              pdata.PresetName += (char)datas[0][dataiter+i];
            dataiter += MaxPresetNameLength;
            
            GD.Print("preset name: ", pdata.PresetName);

            pdata.GeneralData.colorType = BitConverter.ToInt32(datas[0], dataiter); dataiter += sizeof(int);
            pdata.GeneralData.brightness = BitConverter.ToSingle(datas[0], dataiter); dataiter += sizeof(float);
            pdata.StaticData.valueposx = BitConverter.ToSingle(datas[0], dataiter); dataiter += sizeof(float);
            pdata.RGBData.rgb_speed = BitConverter.ToSingle(datas[0], dataiter); dataiter += sizeof(float);
            pdata.RGBData.rgb_window = BitConverter.ToSingle(datas[0], dataiter); dataiter += sizeof(float);
            pdata.RGBData.in_union = BitConverter.ToBoolean(datas[0], dataiter); dataiter += sizeof(bool);
            pdata.SoundData.color_shifting = BitConverter.ToInt32(datas[0], dataiter); dataiter += sizeof(int);
            pdata.SoundData.brightness_mode = BitConverter.ToInt32(datas[0], dataiter); dataiter += sizeof(int);
            pdata.SoundData.max_intensity = BitConverter.ToSingle(datas[0], dataiter); dataiter += sizeof(float);
            pdata.SoundData.min_intensity = BitConverter.ToSingle(datas[0], dataiter); dataiter += sizeof(float);

            // setting up color range
            ushort colorlen = BitConverter.ToUInt16(datas[0], dataiter); dataiter += sizeof(ushort);
            pdata.ColorDatas = new Color[colorlen];
            for(int i = 0; i < colorlen; i++){
              float range = BitConverter.ToSingle(datas[0], dataiter); dataiter += sizeof(float);

              byte data_r = datas[0][dataiter]; dataiter += sizeof(byte);
              byte data_g = datas[0][dataiter]; dataiter += sizeof(byte);
              byte data_b = datas[0][dataiter]; dataiter += sizeof(byte);

              Color col = Color.Color8(data_r, data_g, data_b);
              col.a = range;
              pdata.ColorDatas[i] = col;
            }

            // setting up split parts
            ushort partlen = BitConverter.ToUInt16(datas[0], dataiter); dataiter += sizeof(ushort);
            for(int i = 0; i < partlen; i++){
              _split_data partdata = new _split_data();
              partdata.start_led = BitConverter.ToInt32(datas[0], dataiter); dataiter += sizeof(int);
              partdata.end_led = BitConverter.ToInt32(datas[0], dataiter); dataiter += sizeof(int);
              partdata.range_start = BitConverter.ToSingle(datas[0], dataiter); dataiter += sizeof(float);
              partdata.range_end = BitConverter.ToSingle(datas[0], dataiter); dataiter += sizeof(float);
              partdata.reversed = BitConverter.ToBoolean(datas[0], dataiter); dataiter += sizeof(bool);
              partdata.channel = BitConverter.ToInt32(datas[0], dataiter); dataiter += sizeof(int);

              pdata.SplitDatas.Add(partdata);
            }

            GD.Print("colorlen ", colorlen, "  partlen ", partlen);
            if(_changed_presets[preset_idx] == null){
              GD.Print("s ", _set_manyPreset);
              _set_manyPreset--;
            }

            _changed_presets[preset_idx] = pdata;
            if(_set_manyPreset <= 0){
              GD.Print("Ready");
              _updateChoice_preset();
              _updateNewPreset_nosend();
              _changePreset(_presetIdx);
              _onEditorReadyToUse();
              _set_preset = false;
            }
          }
          catch(ArgumentOutOfRangeException e){
            GD.Print("Data send isn't sufficient enough. err: ", e.ToString());
          }
        }
      }break;

      case _mcu_forwardCodes.MCU_GETPRESETLEN:{
        if(_set_preset){
          ushort presetlen = BitConverter.ToUInt16(datas[0], dataiter); dataiter += sizeof(ushort);
          _set_manyPreset = presetlen;
          _changed_presets.RemoveRange(0, _changed_presets.Count);
          for(int i = 0; i < presetlen; i++)
            _changed_presets.Insert(i, null);
        }
      }break;

      case _mcu_forwardCodes.MCU_USEPRESET:{
        if(_set_preset)
          _presetIdx = (int)BitConverter.ToUInt16(datas[0], dataiter); dataiter += sizeof(ushort);
      }break;

      case _mcu_forwardCodes.MCU_GETMCUINFO:{
        ushort strsize = BitConverter.ToUInt16(datas[0], dataiter); dataiter += sizeof(ushort);

        string mcuname = "";
        for(int i = 0; i < strsize; i++)
          mcuname += (char)datas[0][dataiter+i];
        dataiter += strsize;
      
        ushort lednum = BitConverter.ToUInt16(datas[0], dataiter); dataiter += sizeof(ushort);
        _constManyLed = lednum;
        _manyLed = lednum;
      
        _dropdown.SetMCUInfo(mcuname, lednum);
      }break;
    }
  }

  private _sock_callbacks _params;
  private List<byte[]> _params_data = new List<byte[]>();
  private void _onMsgGet(ref byte[] bytes){
    if(_params.manyParam > 0){
      _params_data.Add(bytes);
      _params.manyParam--;
    }
    else{
      ushort code = BitConverter.ToUInt16(bytes, 0);
      if(_sock_callbackSet.ContainsKey(code)){
        _params = _sock_callbackSet[code];
      }
      else{
        GD.PrintErr("Code from socket isn't valid.\nCode ", code);
      }
    }

    if(_params.manyParam == 0){
      _params.cb(_params_data);
      _params_data.Clear();
    }
  }


  private void _onColorChanged(Color[] cols){
    _checkIfChanged();
    _current_preset.ColorDatas = cols;

    Image im = new Image();
    im.Create(cols.Length, 1, false, Image.Format.Rgba8);

    im.Lock();
    for(int i = 0; i < cols.Length; i++)
      im.SetPixel(i, 0, cols[i]);
    
    im.Unlock();
    
    ImageTexture imtex = new ImageTexture();
    imtex.CreateFromImage(im, 0);

    _vismat.SetShaderParam("coldata", imtex);
    _vismat.SetShaderParam("coldatalen", cols.Length);
  }

  private void _sendSGGetConfig(_sg_configCodes code){
    lock(_sockListener){
      _sockListener.AppendData(BitConverter.GetBytes((ushort)_progCodes.GET_CONFIG_SG));
      _sockListener.AppendData(BitConverter.GetBytes((ushort)code));
    }
  }

  private void _sendSGSetConfig(_sg_configCodes code, byte[] data){
    lock(_sockListener){
      _sockListener.AppendData(BitConverter.GetBytes((ushort)_progCodes.SET_CONFIG_SG));
      _sockListener.AppendData(BitConverter.GetBytes((ushort)code));
      _sockListener.AppendData(data);
    }
  }

  private void _onMsgGet_MCUConnState(List<byte[]> datas){
    if(datas[0].Length == sizeof(byte)){
      byte msgcode = datas[0][0];
      switch((_mcu_connState)msgcode){
        case _mcu_connState.MCUCONNSTATE_CONNECTED:{
          _dropdown.SetConnectedState(VisualizerDropdown.ConnectedState.Connected);
          _onConnectedToMCU();
          break;
        }

        case _mcu_connState.MCUCONNSTATE_DISCONNECTED:{
          _dropdown.SetConnectedState(VisualizerDropdown.ConnectedState.Disconnected);
          _onDisconnectedFromMCU();
          break;
        }

        case _mcu_connState.MCUCONNSTATE_RECONNECT:{
          _dropdown.SetConnectedState(VisualizerDropdown.ConnectedState.Reconnect);
          break;
        }
      }
    }
  }

  private void _onMsgGet_getMcuAddr(List<byte[]> datas){
    if(datas[0].Length == sizeof(uint) && datas[1].Length == sizeof(ushort)){
      uint _ip = BitConverter.ToUInt32(datas[0], 0);
      ushort port = BitConverter.ToUInt16(datas[1], 0);
      _pedit_IPPopup.IP = _ip;
      _pedit_IPPopup.Port = port;
    }
  }

  private void _onConnectedToSG(){
    _is_sgconnected = true;

    // getting soundget datas
    _sendSGGetConfig(_sg_configCodes.CFG_SG_INPUTDEVICESNAME);
    _sendSGGetConfig(_sg_configCodes.CFG_SG_INPUTDEVICEDATA);
    _sendSGGetConfig(_sg_configCodes.CFG_SG_FLOW);
    _dropdown.SetSGConnectedState(true);

    // getting mcu connected state
    _sockListener.AppendData(BitConverter.GetBytes((ushort)_progCodes.GETMCU_ADDRESS));
    _sockListener.AppendData(BitConverter.GetBytes((ushort)_progCodes.CONNECT_MCU_STATE));
    _sockListener.AppendData(new byte[]{(byte)_mcu_connState.MCUCONNSTATE_REQUESTSTATE});

    ErrorShowBlockAutoload.Autoload.CloseError();
    _sgtimer.Stop();
  }

  private void _onDisconnectedFromSG(){
    _is_sgconnected = false;

    _dropdown.SetSGConnectedState(false);
    _dropdown.SetConnectedState(VisualizerDropdown.ConnectedState.Disconnected);
    _dispSGError();
  }

  private void _onConnectedToMCU(){
    if(!_alreadyConnToMCU){
      _askMCUInfo();
      _getPresetDataMCU();
      
      _alreadyConnToMCU = true;
    }
  }

  private void _onDisconnectedFromMCU(){
    if(_alreadyConnToMCU){
      _onEditorNotReadyToUse();
      _alreadyConnToMCU = false;
    }
  }

  private void _doConnectMCU(bool doConn){
    // connecting
    lock(_sockListener){
      _sockListener.AppendData(BitConverter.GetBytes((ushort)_progCodes.CONNECT_MCU_STATE));
      if(doConn){
        _sockListener.AppendData(new byte[]{(byte)_mcu_connState.MCUCONNSTATE_CONNECTED});
        _dropdown.SetConnectedState(VisualizerDropdown.ConnectedState.Reconnect);
      }
      // disconnecting
      else
        _sockListener.AppendData(new byte[]{(byte)_mcu_connState.MCUCONNSTATE_DISCONNECTED});
    }
  }

  private void _onSetMCUAddressBtn(){
    _pedit_IPPopup.PopupCentered();
  }

  private void _setMCUAddress(uint ip, ushort port){
    // straight send to soundget tool
    lock(_sockListener){
      _sockListener.AppendData(BitConverter.GetBytes((ushort)_progCodes.SETMCU_ADDRESS));
      _sockListener.AppendData(BitConverter.GetBytes(ip));
      _sockListener.AppendData(BitConverter.GetBytes(port));
    }
  }

  private void _setupDropdown(){
    // just in case if it were hidden when editing
    _dropdown.Visible = true;
    _dropdown.Connect("on_connectbuttonpressed", this, "_doConnectMCU");
    _dropdown.Connect("on_setmcuaddrbuttonpressed", this, "_onSetMCUAddressBtn");
    _dropdown.Connect("on_changeindevice", this, "_onChangeDev");
    _dropdown.Connect("on_changeflow", this, "_onChangeFlow");
    _dropdown.Connect("on_sgbuttonpressed", this, "_SGButtonPressed");
    _dropdown.Connect("on_sandboxbuttonpressed", this, "_sandboxButtonPressed");
    _dropdown.Connect("on_editpanelchanged", this, "_on_editPanelChanged");
    _dropdown.SetSGConnectedState(false);
    _dropdown.SetSandboxState(false);

    _dropdown.SetChoiceName(VisualizerDropdown.ChoiceType.Flow, _sg_flowtypes_str);

    IEditInterface_Create.EditInterfaceContent[] _editCont = {
      new IEditInterface_Create.EditInterfaceContent{
        TitleName = "",
        EditType = IEditInterface_Create.InterfaceType.StaticText,
        Properties = new EditStaticText.set_param{},
        ID = -1
      },

      new IEditInterface_Create.EditInterfaceContent{
        TitleName = "Set WiFi Password(s)",
        EditType = IEditInterface_Create.InterfaceType.Button,
        Properties = new EditButton.set_param{},
        ID = (int)_vdropdownpanel_codes.PASSWORD
      }
    };

    _dropdown.SetPanelEdit(_editCont);
  }


  public override void _Ready(){
    _soundget_folderpath = OS.GetExecutablePath();
    int _separateidx = _soundget_folderpath.LastIndexOf('/');
    if(_separateidx < 0)
      _separateidx = _soundget_folderpath.LastIndexOf('\\');
    
    _separateidx++;
    _soundget_folderpath = _soundget_folderpath.Remove(_separateidx);


    ErrorShowAutoload.Autoload.AddToControl(this);

    _sock_callbackSet = new Dictionary<int, _sock_callbacks>();
    _sock_callbackSet.Add((int)_progCodes.SOUNDDATA_DATA, new _sock_callbacks{
      cb = _onMsgGet_SounddataData,
      manyParam = 2
    });
    _sock_callbackSet.Add((int)_progCodes.GET_CONFIG_SG, new _sock_callbacks{
      cb = _onMsgGet_getCfgSg,
      manyParam = 2
    });
    _sock_callbackSet.Add((int)_progCodes.FORWARD, new _sock_callbacks{
      cb = _onMsgGet_forward,
      manyParam = 1
    });
    _sock_callbackSet.Add((int)_progCodes.CONNECT_MCU_STATE, new _sock_callbacks{
      cb = _onMsgGet_MCUConnState,
      manyParam = 1
    });
    _sock_callbackSet.Add((int)_progCodes.GETMCU_ADDRESS, new _sock_callbacks{
      cb = _onMsgGet_getMcuAddr,
      manyParam = 2
    });

    _sockListener = new SocketListener(_sockport, BitConverter.GetBytes(_sockip));
    _sockListener.Connect("OnConnected", this, "_onConnectedToSG");
    _sockListener.Connect("OnDisconnected", this, "_onDisconnectedFromSG");
    _sockListener.CbOnSeperateMessageGet = _onMsgGet;
    _sockListener.StartListening();

    _hbcont_apply = GetNode<HBoxContainer>("1/1/1/1/1/6");
    _vbcont_main = GetNode<VBoxContainer>("1/1/1/1/1/3/1");
    _vbcont_color = GetNode<VBoxContainer>("1/1/1/1/1/5/1");
    _splitter = GetNode<Splitter>("1/2/2");
    _sliderColor = GetNode<SliderColor>("1/2/3/1");
    _vis = GetNode<Visualizer>("1/2/1");
    _dropdown = GetNode<VisualizerDropdown>("2");

    _setupDropdown();

    _splitter.MaxButtons = MaxSplits;
    _vismat = _vis.Material as ShaderMaterial;
    _vismat.SetShaderParam("use_splits", true);

    _hbcont_apply.Visible = false;

    GetNode<Button>("1/1/1/1/1/6/1").Connect("pressed", this, "_updateNewPreset");
    GetNode<Button>("1/1/1/1/1/6/2").Connect("pressed", this, "_cancelNewPreset");

    _presetChoice = GetNode<IEditInterface>("1/1/1/1/1/1/1/1/1");
    _presetButtonAdd = GetNode<IEditInterface>("1/1/1/1/1/1/1/1/2");
    _presetButtonRemove = GetNode<IEditInterface>("1/1/1/1/1/1/1/1/3");
    _presetNameEdit = GetNode<IEditInterface>("1/1/1/1/1/1/1/2");

    _presetButtonAdd.Title = "Add Preset";
    (_presetButtonAdd as Node).Connect("on_edited", this, "_addPreset");

    _presetButtonRemove.Title = "Remove Preset";
    (_presetButtonRemove as Node).Connect("on_edited", this, "_deletePreset");

    _presetChoice.Title = "Presets";
    (_presetChoice as Node).Connect("on_edited", this, "_changePreset");
    
    _presetNameEdit.Title = "Preset Name";
    _presetNameEdit.ID = (int)_editContent.PresetName;
    (_presetNameEdit as Node).Connect("on_edited", this, "_on_generalOptionChanged");

    // change when gets data from mcu
    _presets_list.Add(_presets_data.DefaultCopy());
    _addPreset();
    _changePreset(0);

    _splitter.Connect("on_buttonpressed", this, "_splitter_onPressed");
    _splitter.Connect("on_buttonadded", this, "_splitter_onAdded");
    _splitter.Connect("on_buttondeleted", this, "_splitter_onDeleted");
    _splitter.Connect("on_buttonadjusted", this, "_splitter_adjusted");

    _sliderColor.Connect("on_colorgradientchanged", this, "_onColorChanged");

    _pedit = _popupEdit.Instance<PopupEdit>();
    _pedit.Connect("on_optionchanged", this, "_popupedit_onoptchanged");
    AddChild(_pedit);

    _popupPassEdit = _popupEdit.Instance<PopupEdit>();
    _popupPassEdit.Connect("on_optionchanged", this, "_popuppassedit_onoptchanged");
    AddChild(_popupPassEdit);

    _pedit_IPPopup = _popupEdit_IPPort.Instance<PopupEdit_IPPort>();
    _pedit_IPPopup.Connect("on_optionchanged_ipport", this, "_setMCUAddress");
    _pedit_IPPopup.Port = _def_mcuport;
    AddChild(_pedit_IPPopup);

    _onEditorNotReadyToUse();

    _sgtimer = new Timer();
    AddChild(_sgtimer);
    _sgtimer.Connect("timeout", this, "_timer_dconnsg");
    _sgtimer.Autostart = false;
    _sgtimer.OneShot = true;
    _sgtimer.Start(_waitForSGConn);

    _mcu_passwordDatas.Add(new KeyValuePair<string, string>("test1", "1234"));
    _mcu_passwordDatas.Add(new KeyValuePair<string, string>("test2", "4534483756"));
  }
}