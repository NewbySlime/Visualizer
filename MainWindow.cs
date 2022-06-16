using Godot;
using System;
using System.Collections.Generic;
using System.Threading.Tasks;



public enum LightingType{
  Music,
  RGB
}



public static class OptionContents{
  public static OptionDataCode[] GeneralOptions = {
    OptionDataCode.REFRESH_RATE
  };

  public static OptionDataCode[] MusicOptions = {

  };

  public static OptionDataCode[] RGBOptions = {

  };
}



public struct OptionsDataContent{
  ushort REFRESH_RATE;
}



public struct OptionDataType{
  public enum OptionType{
    Check,
    Text,
    Slider,
    Choice
  }

  public enum DataType{
    INT,
    FLOAT
  }

  public string name;
  public ushort dataCode;
  public OptionType type;

  // only available on choice option type
  public string[] choices;

  // only available on text option type
  public DataType dataType;
}



public enum MessageCode{
  VISDATA = 0x0000,
  STRDATA = 0x0001,
  TEST = 0x0002,
  FWDTOMC = 0x0003
}



public enum MCMessageCode{
  SETDATA = 0x0000,
  GETDATA = 0x0001
}



public enum OptionDataCode{
  REFRESH_RATE = 0x0000
}


// TODO test the communication between the option and the MainWindow class
public class MainWindow : Control{
  private static Dictionary<ushort, OptionDataType> __optionListDict = new Dictionary<ushort, OptionDataType>();

  private static ushort port = 3022;

  
  [Export]
  private PackedScene scene_Opt_EditCheck, scene_Opt_EditChoice, scene_Opt_EditSlider, scene_Opt_EditText;


  private Visualizer vis;
  private SocketListener socketListener;
  private VBoxContainer generalVBoxCont, additionalVBoxCont;

  private delegate void cbToMsgCode(ref object[] param);

  private Dictionary<ushort, KeyValuePair<ushort, cbToMsgCode>> messageCodeDict = new Dictionary<ushort, KeyValuePair<ushort, cbToMsgCode>>();
  private KeyValuePair<ushort, cbToMsgCode> currPairCode = new KeyValuePair<ushort, cbToMsgCode>();

  private bool waitForHeader = true;
  private int messageParamCount = 0;

  // this should contain only byte[]
  private object[] paramContainer = new object[0];

  private OptionsDataContent currentDataOptions;


  private static bool __isStaticSetup = false;
  private static void __SetupStaticClass(){
    if(!__isStaticSetup){
      __isStaticSetup = true;

      __optionListDict.Add((ushort)OptionDataCode.REFRESH_RATE, new OptionDataType{
        name = "Refresh Rate (Hz)",
        dataCode = (ushort)OptionDataCode.REFRESH_RATE,
        type = OptionDataType.OptionType.Text,
        dataType = OptionDataType.DataType.INT
      });
    }
  }


  private void OnMessageGet(ref byte[] msg){
    if(waitForHeader){
      ushort msgCode = BitConverter.ToUInt16(msg, 0);
      if(messageCodeDict.ContainsKey(msgCode)){
        currPairCode = messageCodeDict[msgCode];
        Array.Resize<object>(ref paramContainer, currPairCode.Key);
        messageParamCount = 0;
        waitForHeader = false;
      }
      else{
        GD.PrintErr(
          "Remote host sent wrong message code of (",
          BitConverter.ToString(BitConverter.GetBytes(msgCode)),
          ")."
        );
      }
    }
    else{
      paramContainer[messageParamCount++] = msg;
    }

    if(messageParamCount >= currPairCode.Key){
      currPairCode.Value(ref paramContainer);
      waitForHeader = true;
    }
  }

  // int dataLength | float[] fData
  private void OnUpdateVisualizer(ref object[] param){
    int dataLength = BitConverter.ToInt32(param[0] as byte[], 0);
    byte[] bdata = param[1] as byte[];
    float[] fdata = new float[dataLength];
    for(int i = 0; i < dataLength; i++){
      fdata[i] = BitConverter.ToSingle(bdata, i*sizeof(float));
      //GD.Print(fdata[i]);
    }
    
    vis.UpdateVisualizer(ref fdata);
  }

  // char[]
  private void OnStringGet(ref object[] param){
    byte[] msgarr = param[0] as byte[];
    string msgstr = "";
    foreach(char c in msgarr)
      msgstr += c;
  }

  //
  private void TestFunc(ref object[] param){
    socketListener.AppendData(BitConverter.GetBytes((ushort)MessageCode.TEST));
    socketListener.AppendData(BitConverter.GetBytes(124816));
  }

  // getting an option data from the microcontroller
  private object _GetOptionData(OptionDataType option){
    return null;
  }

  // putting all current options data from microcontroller then create the appropriate editor to a VBoxContainer
  private void _SetupOptions(VBoxContainer parent, ref OptionDataCode[] options){
    for(int i = 0; i < options.Length; i++){
      ushort code = (ushort)options[i];
      OptionDataType datType = __optionListDict[code];
      Opt_Edit optionEditor = null;
      switch(datType.type){
        case OptionDataType.OptionType.Check:
          optionEditor = scene_Opt_EditCheck.Instance<Opt_EditCheck>();
          break;

        case OptionDataType.OptionType.Text:
          optionEditor = scene_Opt_EditText.Instance<Opt_EditText>();
          break;
        
        case OptionDataType.OptionType.Slider:
          optionEditor = scene_Opt_EditSlider.Instance<Opt_EditSlider>();
          break;
        
        case OptionDataType.OptionType.Choice:
          optionEditor = scene_Opt_EditChoice.Instance<Opt_EditChoice>();
          break;
      }

      parent.AddChild(optionEditor);

      optionEditor.ChangeTitle(datType.name);
      optionEditor.ChangeContent(_GetOptionData(datType));
      optionEditor.Connect("OnConfirmContent", this, "OnGetOptionData", new Godot.Collections.Array{code});
    }
  }

  // called when getting a signal from the options
  private void OnGetOptionData(ushort dataCode, object dataObj, Opt_Edit editObj){

  }

  // called when user pressed the "apply" button
  private void OnApplySettings(){

  }


  public override void _Ready(){
    __SetupStaticClass();

    socketListener = new SocketListener(port, new byte[]{127,0,0,1});
    messageCodeDict.Add((ushort)MessageCode.VISDATA, new KeyValuePair<ushort, cbToMsgCode>(2, OnUpdateVisualizer));
    messageCodeDict.Add((ushort)MessageCode.STRDATA, new KeyValuePair<ushort, cbToMsgCode>(1, OnStringGet));
    messageCodeDict.Add((ushort)MessageCode.TEST, new KeyValuePair<ushort, cbToMsgCode>(0, TestFunc));

    socketListener.CbOnSeperateMessageGet = OnMessageGet;
    socketListener.StartListening();

    vis = GetNode<Visualizer>("TextureRect");
    vis.SetNumberOfBands(20);

    _SetupOptions(generalVBoxCont, ref OptionContents.GeneralOptions);
  }
}
