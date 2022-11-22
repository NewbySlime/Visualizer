using Godot;
using System.Collections.Generic;

public class VisualizerDropdown: Dropdown{
  [Signal]
  private delegate void on_connectbuttonpressed(bool doConnect);
  [Signal]
  private delegate void on_setmcuaddrbuttonpressed();
  [Signal]
  private delegate void on_changeindevice(int idx);
  [Signal]
  private delegate void on_sgbuttonpressed();
  [Signal]
  private delegate void on_sandboxbuttonpressed();
  [Signal]
  private delegate void on_editpanelchanged(int idx);

  [Export]
  private Color DisconnectedTextColor, ConnectedTextColor, ReconnectTextColor;

  [Export]
  private StreamTexture DisconnectedImage, ConnectedImage, ReconnectImage;

  [Export]
  private string InfoTitle;
  [Export]
  private string DevOptionTitle = "Input devices";


  public enum OptionEnum{
    ChangePassword
  }

  public enum ConnectedState{
    Connected,
    Reconnect,
    Disconnected
  }


  private HBoxContainer _taskbar;
  private VBoxContainer _editPanel;

  private TextureProgress _batteryProgress;
  private Label _batteryLabel, _infoLabel, _soundLabel;
  private Button _buttonConnect, _buttonSet1, _buttonSGStart;

  private RichTextLabel _connectedLabel, _sgstateLabel, _sandboxstateLabel;
  private TextureRect _connectedTexture;

  private string _batteryLabel_template, _infoLabel_template, _soundLabel_template, _mcustateLabel_template, _sgstateLabel_template, _sandboxstateLabel_template;

  private IEditInterface _soundDevChoice;
  
  private ConnectedState currentState;

  
  private void _onButtonConnectPressed(){
    EmitSignal("on_connectbuttonpressed", currentState == ConnectedState.Disconnected? true: false);
  }

  private void _onButtonSet1Pressed(){
    EmitSignal("on_setmcuaddrbuttonpressed");
  }

  private void _onButtonSGStatePressed(){
    EmitSignal("on_sgbuttonpressed");
  }

  private void _onButtonSandboxStatePressed(){
    EmitSignal("on_sandboxbuttonpressed");
  }

  private void _onDeviceChanged(int id, object obj){
    EmitSignal("on_changeindevice", (obj as EditChoice.get_data).choiceID);
  }

  private void _onEditPanelChanged(int idx, object obj){
    EmitSignal("on_editpanelchanged", idx, obj);
  }

  public override void _Ready(){
    _taskbar = GetNode<HBoxContainer>("VBoxContainer/2/1");
    _taskbar.RectPosition = new Vector2(_taskbar.RectPosition.x, -_taskbar.RectSize.y);

    OffsetY = _taskbar.RectSize.y;

    base._Ready();

    GetNode<MarginContainer>("VBoxContainer/DropdownPanel/1").MarginBottom = _taskbar.RectSize.y;

    _batteryProgress = _taskbar.GetNode<TextureProgress>("1/1/1");
    _batteryLabel = _taskbar.GetNode<Label>("1/1/2");

    _connectedTexture = _taskbar.GetNode<TextureRect>("2/1/1");
    _connectedLabel = _taskbar.GetNode<RichTextLabel>("2/1/2");

    var panelNode = GetNode("VBoxContainer/DropdownPanel");

    _buttonConnect = panelNode.GetNode<Button>("1/1/2/4/1/1");
    _buttonSet1 = panelNode.GetNode<Button>("1/1/2/4/1/2");
    _infoLabel = panelNode.GetNode<Label>("1/1/2/3");

    _soundLabel = panelNode.GetNode<Label>("1/1/2/2");

    _soundDevChoice = panelNode.GetNode<IEditInterface>("1/1/2/1");
    _soundDevChoice.Title = DevOptionTitle;
    (_soundDevChoice as EditChoice).Connect("on_edited", this, "_onDeviceChanged");

    HBoxContainer _sgstate = GetNode<HBoxContainer>("VBoxContainer/2/2/1/1");
    HBoxContainer _sandboxstate = panelNode.GetNode<HBoxContainer>("1/1/1/3");

    _editPanel = panelNode.GetNode<VBoxContainer>("1/1/1/4/1");

    _buttonSGStart = _sgstate.GetNode<Button>("2");
    _sgstateLabel = _sgstate.GetNode<RichTextLabel>("1");
    _sandboxstateLabel = _sandboxstate.GetNode<RichTextLabel>("1");

    _batteryLabel_template = _batteryLabel.Text;
    _infoLabel_template = _infoLabel.Text;
    _soundLabel_template = _soundLabel.Text;
    _mcustateLabel_template = _connectedLabel.BbcodeText;
    _sgstateLabel_template = _sgstateLabel.BbcodeText;
    _sandboxstateLabel_template = _sandboxstateLabel.BbcodeText;

    _buttonConnect.Connect("pressed", this, "_onButtonConnectPressed");
    _buttonSet1.Connect("pressed", this, "_onButtonSet1Pressed");
    _buttonSGStart.Connect("pressed", this, "_onButtonSGStatePressed");
    _sandboxstate.GetNode("2").Connect("pressed", this, "_onButtonSandboxStatePressed");

    SetBatteryPrecent(-1);
    SetSGConnectedState(false);
    SetConnectedState(ConnectedState.Disconnected);
  }

  // if val is below 0, then the percent will be replaced by '-'
  public void SetBatteryPrecent(float val){
    _batteryProgress.Value = val;
    _batteryLabel.Text = string.Format(_batteryLabel_template, val >= 0? Mathf.RoundToInt(val).ToString(): "-");
  }

  public void SetConnectedState(ConnectedState state){
    string colhexstr = "";
    string statestr = "";

    _buttonConnect.Disabled = false;

    switch(state){
      case ConnectedState.Connected:
        _connectedTexture.Texture = ConnectedImage;
        colhexstr = ConnectedTextColor.ToHtml();
        statestr = "Connected";
        _buttonConnect.Text = "Disconnect";
        break;
      
      case ConnectedState.Reconnect:
        _connectedTexture.Texture = ReconnectImage;
        colhexstr = ReconnectTextColor.ToHtml();
        statestr = "Reconnecting";
        _buttonConnect.Text = "...";
        _buttonConnect.Disabled = true;
        _infoLabel.Text = "Trying to reconnect...\nPlease wait";
        break;

      case ConnectedState.Disconnected:
        _connectedTexture.Texture = DisconnectedImage;
        colhexstr = DisconnectedTextColor.ToHtml();
        statestr = "Disconnected";
        _buttonConnect.Text = "Connect";
        _infoLabel.Text = "Controller disconnected.";
        break;
    }

    _connectedLabel.BbcodeText = string.Format(
      _mcustateLabel_template,
      colhexstr,
      statestr
    );

    currentState = state;
  }
  
  public void SetSandboxState(bool state){
    string colhexstr = "";
    string statestr = "";

    if(state){
      colhexstr = ConnectedTextColor.ToHtml();
      statestr = "On";
    }
    else{
      colhexstr = DisconnectedTextColor.ToHtml();
      statestr = "Off";
    }

    _sandboxstateLabel.BbcodeText = string.Format(
      _sandboxstateLabel_template,
      colhexstr,
      statestr
    );
  }

  public void SetMCUInfo(string name, int ledNum){
    _infoLabel.Text = string.Format(_infoLabel_template, name, ledNum);
  }

  public void SetSGConnectedState(bool b){
    _soundLabel.Text = b? "Soundget tool connected.": "Soundget tool disconnected.";

    string colhexstr = "";
    string statestr = "";

    if(b){
      colhexstr = ConnectedTextColor.ToHtml();
      statestr = "Running";
      _buttonSGStart.Text = "Stop";
    }
    else{
      colhexstr = DisconnectedTextColor.ToHtml();
      statestr = "Stopped";
      _buttonSGStart.Text = "Start";
    }

    _sgstateLabel.BbcodeText = string.Format(
      _sgstateLabel_template,
      colhexstr,
      statestr
    );
  }

  private KeyValuePair<string, int>[] names = new KeyValuePair<string, int>[0];
  private int devidx = 0;
  public void SetSoundDevNames(string[] namearr){
    names = new KeyValuePair<string, int>[namearr.Length];
    for(int i = 0; i < namearr.Length; i++)
      names[i] = new KeyValuePair<string, int>(namearr[i], i);

    _soundDevChoice.ChangeContent(new EditChoice.set_param{
      choiceID = devidx,
      choices = names
    });
  }

  public void SetSoundDevIndex(int currid){
    devidx = currid;
    _soundDevChoice.ChangeContent(new EditChoice.set_param{
      choiceID = devidx,
      choices = names
    });
  }

  public void SetCurrentSoundDevData(string name, int channel){
    _soundLabel.Text = string.Format(_soundLabel_template, name, channel);
  }

  public void SetPanelEdit(IEditInterface_Create.EditInterfaceContent[] edit){
    IEditInterface_Create.Autoload.RemoveAllChild(_editPanel);
    IEditInterface_Create.Autoload.CreateAndAdd(_editPanel, this, "_onEditPanelChanged", edit);
  }
}