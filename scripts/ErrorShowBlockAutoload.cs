using Godot;

public class ErrorShowBlockAutoload: AutoloadClass<ErrorShowBlockAutoload>{
  [Export]
  private float time_animation, blur, blur_mult;
  [Export]
  private float[] BezierAnimData;

  private TextureRect _background;
  private Panel _windowbg;
  private MarginContainer _mcont;
  private Label _label;
  private HBoxContainer _buttoncont;

  // when not reversed, it will appear. vice versa
  private bool _doreverse = true;
  private bool _doanim = false;
  private float _currenttime;

  
  private void _onAnimDone(){
    if(_doreverse){
      IEditInterface_Create.Autoload.RemoveAllChild(_buttoncont);
      _background.Visible = false;
      _windowbg.Visible = false;
    }
  }

  public void _moveFront(){
    GetParent().MoveChild(this, GetParent().GetChildCount());
  }

  public override void _Ready(){
    _setAutoload(this);

    _background = GetNode<TextureRect>("1");
    _windowbg = GetNode<Panel>("2");
    _mcont = GetNode<MarginContainer>("2/1");
    _label = GetNode<Label>("2/1/1/1");
    _buttoncont = GetNode<HBoxContainer>("2/1/1/2");

    _background.RectMinSize = GetViewport().Size;
    _background.Visible = false;
    _windowbg.Visible = false;
  }

  public override void _Process(float delta){
    if(_doanim){
      _windowbg.RectMinSize = _mcont.RectSize;
      _windowbg.RectPosition = (GetViewport().Size-_windowbg.RectSize)/2;

      float range = Mathf.Abs((_doreverse? 1.0f: 0.0f)-_currenttime/time_animation);
      range = AnimationFunction.Bezier1DFunc(range, BezierAnimData);

      Color _windbgcol = _windowbg.Modulate;
      _windbgcol.a = range;
      _windowbg.Modulate = _windbgcol;

      (_background.Material as ShaderMaterial).SetShaderParam("blur", blur*range);
      (_background.Material as ShaderMaterial).SetShaderParam("blur_mult", blur_mult*range);

      _currenttime += delta;
      if(_currenttime >= time_animation){
        _onAnimDone();
        _doanim = false;
      }
    }
  }

  public void DisplayError(string message, IEditInterface_Create.EditInterfaceContent[] buttons, string signalfunc, Node signalhand){
    if(_doreverse){
      _moveFront();
      _label.Text = message;

      IEditInterface_Create.Autoload.RemoveAllChild(_buttoncont);
      IEditInterface_Create.Autoload.CreateAndAdd(_buttoncont, signalhand, signalfunc, buttons);

      _doreverse = false;
      _doanim = true;
      _currenttime = 0.0f;
      _windowbg.Visible = true;
      _background.Visible = true;
    }
  }

  public void CloseError(){
    if(!_doreverse){
      _doreverse = true;
      _doanim = true;
      _currenttime = 0.0f;
    }
  }
}