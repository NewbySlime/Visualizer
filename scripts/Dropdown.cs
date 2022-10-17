using Godot;

public class Dropdown: Control{
  [Signal]
  private delegate void on_dropdown(bool isDropdown);

  [Export]
  private float DropAnimationTime, UpwardAnimationTime;
  [Export]
  private float[] BezierAnimData;
  [Export]
  private float BackgroundOpacity;
  [Export]
  private Color BackgroundColor;
  [Export]
  protected float OffsetY;

  private Button _button;
  private Control _panel;
  private VBoxContainer _vbcont;
  private TextureRect _background;

  private bool _isDropped = false;
  private bool _inAnimation = false;
  private bool _mouseInBg = false;
  private bool _lockpos = false;

  private Vector2 _targetPos, _initialPos;
  private float _animTime = 0.0f, _targetAnimTime = 0.0f;


  private void _mouseOnEnterBg(){
    _mouseInBg = true;
  }

  private void _mouseOnExitBg(){
    _mouseInBg = false;
  }

  private void _buttonOnPressed(){
    if(!_inAnimation && !_lockpos){
      _inAnimation = true;

      _isDropped = !_isDropped;
      _initialPos = _vbcont.RectPosition;
      _targetPos = new Vector2(0, _isDropped? 0: -_panel.RectSize.y + OffsetY);

      _animTime = 0.0f;
      _targetAnimTime = _isDropped? DropAnimationTime: UpwardAnimationTime;

      _background.Visible = true;
      _background.MouseFilter = MouseFilterEnum.Stop;

      (_button.Material as ShaderMaterial).SetShaderParam("flip_vertical", _isDropped);
      EmitSignal("on_dropdown", _isDropped);
    }

    _mouseInBg = false;
  }

  // when the panel's size is changed
  private void _onPanelChangedSize(){
    var newPos = new Vector2(0, _isDropped? 0: -_panel.RectSize.y + OffsetY);

    if(_inAnimation)
      _targetPos = newPos;
    else
      _vbcont.RectPosition = newPos;
  }

  public override void _Ready(){
    RectPosition = Vector2.Zero;

    _background = GetNode<TextureRect>("TextureRect");
    _vbcont = GetNode<VBoxContainer>("VBoxContainer");
    _button = GetNode<Button>("VBoxContainer/Button");
    _panel = GetNode<Control>("VBoxContainer/DropdownPanel");

    _vbcont.RectPosition = Vector2.Zero;
    _panel.RectMinSize = new Vector2(GetViewport().Size.x, _panel.RectMinSize.y);
    _background.RectMinSize = GetViewport().Size;
    _background.Visible = false;

    _background.Connect("mouse_entered", this, "_mouseOnEnterBg");
    _background.Connect("mouse_exited", this, "_mouseOnExitBg");

    _button.Connect("pressed", this, "_buttonOnPressed");
    _panel.Connect("resized", this, "_onPanelChangedSize");
    _background.Modulate = BackgroundColor;
    _onPanelChangedSize();
  }

  public override void _Process(float delta){
    if(_inAnimation){
      _animTime += delta;
      
      if(_animTime < _targetAnimTime){
        float val = AnimationFunction.Bezier1DFunc(_animTime/_targetAnimTime, BezierAnimData);
        _vbcont.RectPosition = (_targetPos-_initialPos)*val+_initialPos;

        var col = _background.Modulate;
        col.a = Mathf.Abs((_isDropped? 0: BackgroundOpacity) - val*BackgroundOpacity);
        _background.Modulate = col;
      }
      else{
        _inAnimation = false;
        _vbcont.RectPosition = _targetPos;

        _background.Visible = _isDropped;
        _background.MouseFilter = _isDropped? MouseFilterEnum.Stop: MouseFilterEnum.Ignore;
      }
    }
  }

  public override void _Input(InputEvent @event){
    if(_mouseInBg && @event is InputEventMouseButton){
      var eventMouse = @event as InputEventMouseButton;
      if(_mouseInBg && _isDropped && eventMouse.ButtonIndex == (int)ButtonList.Left && !eventMouse.Pressed){
        _mouseInBg = false;
        // reuse function in _buttonOnPressed
        _buttonOnPressed();
      }
    }
  }

  public void DoAnimation(bool drop){
    if(drop && _isDropped)
      return;
    
    if(!drop && !_isDropped)
      return;
    
    _buttonOnPressed();
  }
  
  public void LockPosition(bool b){
    _lockpos = b;
  }
}