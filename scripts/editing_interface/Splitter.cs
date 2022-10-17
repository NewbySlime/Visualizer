using Godot;
using System.Collections.Generic;

public class Splitter: HBoxContainer{
  [Signal]
  private delegate void on_buttonpressed(int index);
  [Signal]
  private delegate void on_buttonadded(int idx);
  [Signal]
  private delegate void on_buttondeleted(int idx);
  [Signal]
  private delegate void on_buttonadjusted(int idx1, int t1_1, int t1_2, bool _end);
  [Export]
  private PackedScene _button, _button_adjust;

  private HBoxContainer _splitter;
  private Button _RB, _LB;
  private int _manyButton = 0;
  private int _maxButtons = 4;
  private int _manyTicks = 1;
  private float _badj_fixedsize = 0.0f;

  private List<KeyValuePair<int, int>> _buttonIndexes = new List<KeyValuePair<int, int>>();

  private int _getTickByPosx(float x){
    return Mathf.RoundToInt(x/_splitter.RectSize.x*_manyTicks);
  }

  private float _getPosxByTick(int tick){
    return ((float)tick/_manyTicks)*_splitter.RectSize.x;
  }

  private int _b1_idx;
  private Button _b1, _b2, _b_adj;
  private void _onAdjuster_down(int idxbefore){
    _b1 = _splitter.GetChild<Button>(idxbefore*2);
    _b_adj = _splitter.GetChild<Button>(idxbefore*2+1);
    _b2 = _splitter.GetChild<Button>((idxbefore+1)*2);

    _b1_idx = idxbefore;

    GD.Print("pressed down");

    SetProcessInput(true);
  }

  private void _onButtonPressed(int idx){
    EmitSignal("on_buttonpressed", idx);
  }

  // idx = 1: left
  // idx = 2: right
  private void _onbuttonAdd(int dir){
    if(_manyButton < _maxButtons && _manyButton < _manyTicks){
      Button button = _button.Instance<Button>();
      Button buttonAdj = _button_adjust.Instance<Button>();
      int _increment = 1;
      int _idx = 0;
      int _newidx = 0;
      int _idx_adj = 0;
      switch(dir){
        // left
        case 1:{
          _splitter.AddChild(buttonAdj);
          _splitter.AddChild(button);
          _splitter.MoveChild(buttonAdj, 0);
          _splitter.MoveChild(button, 0);

          _buttonIndexes.Insert(0, new KeyValuePair<int, int>(0,0));

          _increment = 2;
          _idx = 0;
          _newidx = 0;
          _idx_adj = 0;
          break;
        }

        // right
        case 2:{
          _splitter.AddChild(buttonAdj);
          _splitter.AddChild(button);

          _buttonIndexes.Add(new KeyValuePair<int, int>(0,0));

          _increment = -2;
          _idx = _splitter.GetChildCount()-1;
          _newidx = _manyTicks;
          _idx_adj = _idx/2-1;
          break;
        }
      }

      int __idx = _idx/2;

      // use for just to adjust all the things
      float posxt1 = _getPosxByTick(1)-buttonAdj.RectSize.x;
      float _size = posxt1+buttonAdj.RectSize.x;
      for(; _idx >= 0 && _idx < _splitter.GetChildCount(); _idx += _increment){
        Button _b = _splitter.GetChild<Button>(_idx);
        if(_size > 0){
          float _xsize = Mathf.Max(posxt1, _b.RectSize.x);
          float _newxsize = Mathf.Max(posxt1, _xsize-_size);

          _size -= _xsize-_newxsize;
          _b.RectMinSize = new Vector2(_newxsize, _b.RectMinSize.y);

          int __newidx = _newidx+(_increment > 0? _getTickByPosx(_newxsize): -_getTickByPosx(_newxsize));
          _buttonIndexes[_idx/2] = new KeyValuePair<int, int>(
            _increment > 0? _newidx: __newidx, 
            _increment < 0? _newidx: __newidx
          );

          _newidx = __newidx;
        }

        if(_b.IsConnected("pressed", this, "_onButtonPressed"))
          _b.Disconnect("pressed", this, "_onButtonPressed");
        
        _b.Connect("pressed", this, "_onButtonPressed", new Godot.Collections.Array{_idx/2});

        if((_idx-1) > 0){
          Button _adj = _splitter.GetChild<Button>(_idx-1);
          if(_adj.IsConnected("button_down", this, "_onAdjuster_down"))
            _adj.Disconnect("button_down", this, "_onAdjuster_down");
          
          _adj.Connect("button_down", this, "_onAdjuster_down", new Godot.Collections.Array{_idx/2-1});
        }
      }

      _manyButton++;
      
      EmitSignal("on_buttonadded", __idx);
    }
    else
      ErrorShowAutoload.Autoload.DisplayError(string.Format("Cannot add split more than {0}.", Mathf.Min(_maxButtons, _manyTicks)));
  }


  public int MaxButtons{
    set{
      if(_maxButtons >= _manyButton)
        _maxButtons = value;
    }

    get{
      return _maxButtons;
    }
  }

  public int Ticks{
    set{
      if(value >= _manyButton){
        int _previousTick = _manyTicks;
        _manyTicks = value;

        // use indexes to get the size
        // if the button isn't perfectly set the size, run another loop but backwards
        bool _doBackwards = false;
        for(int i = 0; i < _manyButton; i++){
          Button _b = _splitter.GetChild<Button>(i*2);
          // BUG somehow the last element isn't edited
          var _bidxs = _buttonIndexes[i];

          int _newticksize = Mathf.Max(1, Mathf.RoundToInt((float)(_bidxs.Value-_bidxs.Key)/_previousTick*_manyTicks));
          
          _newticksize += _bidxs.Key;
          if(_newticksize > _manyTicks)
            _doBackwards = true;
          
          if((i+1) < _manyButton)
            _buttonIndexes[i+1] = new KeyValuePair<int, int>(_newticksize, _buttonIndexes[i+1].Value);
          else
            _newticksize = _manyTicks;

          _buttonIndexes[i] = new KeyValuePair<int, int>(_bidxs.Key, _newticksize);

          _b.RectMinSize = new Vector2(_getPosxByTick(_newticksize-_bidxs.Key)-_badj_fixedsize, 0);
        }

        if(_doBackwards && false){
          GD.Print("Do backwards");
          for(int i = _manyButton-1; i >= 0; i--){
            Button _b = _splitter.GetChild<Button>(i*2);
            var _bidxs = _buttonIndexes[i];

            int _newticksize = Mathf.Max(1, Mathf.RoundToInt((float)(_bidxs.Value-_bidxs.Key)/_previousTick*_manyTicks));

            _newticksize = _bidxs.Value-_newticksize;
            
            if((i-1) >= 0)
              _buttonIndexes[i-1] = new KeyValuePair<int, int>(_buttonIndexes[i-1].Key, _newticksize);
            else
              _newticksize = 0;
            
            _buttonIndexes[i] = new KeyValuePair<int, int>(_newticksize, _bidxs.Value);
            _b.RectMinSize = new Vector2(_getPosxByTick(_bidxs.Value-_newticksize)-_badj_fixedsize, 0);
          }
        }

        int _i = -1;
        foreach(var key in _buttonIndexes)
          EmitSignal("on_buttonadjusted", ++_i, key.Key, key.Value, (_i+1) < _manyButton? false: true);
      }
    }

    get{
      return _manyTicks;
    }
  }

  public int ManyButtons{
    get{
      return _manyButton;
    }
  }

  public override void _Ready(){
    _splitter = GetNode<HBoxContainer>("HBoxContainer");
    _LB = GetNode<Button>("LeftB");
    _RB = GetNode<Button>("RightB");

    // setting up size
    int sepSize = GetConstant("Separation");
    _splitter.RectMinSize = new Vector2(RectSize.x-(sepSize+_LB.RectSize.x)*3, 0);

    Button _b = _button.Instance<Button>();
    Button _tmp = _button_adjust.Instance<Button>();
    _badj_fixedsize = _tmp.RectSize.x;
    _splitter.AddChild(_b);
    _b.RectMinSize = new Vector2(_splitter.RectMinSize.x-_tmp.RectSize.x, _b.RectSize.y);
    _b.Connect("pressed", this, "_onButtonPressed", new Godot.Collections.Array{0});
    _tmp.QueueFree();

    _buttonIndexes.Add(new KeyValuePair<int, int>(0, _manyTicks));

    _manyButton++;

    _LB.Connect("pressed", this, "_onbuttonAdd", new Godot.Collections.Array{1});
    _RB.Connect("pressed", this, "_onbuttonAdd", new Godot.Collections.Array{2});

    SetProcessInput(false);
  }

  public override void _Input(InputEvent @event){
    if(@event is InputEventMouseMotion){
      float posx = Mathf.Clamp((@event as InputEventMouseMotion).GlobalPosition.x-_splitter.RectGlobalPosition.x, 0, _splitter.RectSize.x);

      int idxTick = _getTickByPosx(posx);
      if(idxTick <= _buttonIndexes[_b1_idx].Key)
        idxTick = _buttonIndexes[_b1_idx].Key+1;
      else if(idxTick >= _buttonIndexes[_b1_idx+1].Value)
        idxTick = _buttonIndexes[_b1_idx+1].Value-1;
      
      // just need one button to test, since the index is connected to next button
      if(idxTick != _buttonIndexes[_b1_idx].Value){
        var _idxs_b1 = _buttonIndexes[_b1_idx];
        var _idxs_b2 = _buttonIndexes[_b1_idx+1];

        float _b1len = _getPosxByTick(idxTick-_idxs_b1.Key)-_b_adj.RectSize.x;
        float _b2len = _getPosxByTick(_idxs_b2.Value-idxTick)-_b_adj.RectSize.x;
        
        _b1.RectMinSize = new Vector2(_b1len, _b1.RectMinSize.y);
        _b2.RectMinSize = new Vector2(_b2len, _b2.RectMinSize.y);

        _buttonIndexes[_b1_idx] = new KeyValuePair<int, int>(_idxs_b1.Key, idxTick);
        _buttonIndexes[_b1_idx+1] = new KeyValuePair<int, int>(idxTick, _idxs_b2.Value);

        EmitSignal("on_buttonadjusted", _b1_idx, _idxs_b1.Key, idxTick, false);
        EmitSignal("on_buttonadjusted", _b1_idx+1, idxTick, _idxs_b2.Value, true);

        for(int i = 0; i < _buttonIndexes.Count; i++)
          GD.PrintRaw(_buttonIndexes[i], " ");
        
        GD.PrintRaw('\n');
      }
    }
    else if(@event is InputEventMouseButton){
      InputEventMouseButton m = @event as InputEventMouseButton;

      if(m.ButtonIndex == (int)ButtonList.Left && !m.Pressed)
        SetProcessInput(false);
    }
  }

  public void ReEmitIndex(int idx){
    EmitSignal("on_buttonadded", idx);
    EmitSignal("on_buttonadjusted", idx, _buttonIndexes[idx].Key, _buttonIndexes[idx].Value, true);
  }

  public void AddIndex(int idx, int tick, int ticksize, bool realign = true, bool emitSignal = false){
    if(_manyButton < _maxButtons && _manyButton < _manyTicks){
      var button = _button.Instance<Button>();

      if(idx < _buttonIndexes.Count)
        _buttonIndexes[idx] = new KeyValuePair<int, int>(_buttonIndexes[idx].Key, tick);
      
      if((idx+1) < _buttonIndexes.Count)
        _buttonIndexes[idx+1] = new KeyValuePair<int, int>(tick+ticksize, _buttonIndexes[idx+1].Value);
      
      if(_manyButton > 0){
        var buttonAdj = _button_adjust.Instance<Button>();
        _splitter.AddChild(buttonAdj);

        if(idx < _manyButton)
          _splitter.MoveChild(buttonAdj, idx*2);
        
        buttonAdj.Connect("button_down", this, "_onAdjuster_down", new Godot.Collections.Array{idx-1});
      }

      _splitter.AddChild(button);
      if(idx < _manyButton)
        _splitter.MoveChild(button, idx*2);
      
      button.Connect("pressed", this, "_onButtonPressed", new Godot.Collections.Array{idx});

      _buttonIndexes.Insert(idx, new KeyValuePair<int, int>(tick, tick+ticksize));

      // reuse the realignment in the set_Ticks function
      if(realign)
        Ticks = _manyTicks;

      _manyButton++;
    }
    else
      ErrorShowAutoload.Autoload.DisplayError(string.Format("Cannot add split more than {0}.", Mathf.Min(_maxButtons, _manyTicks)));
  }

  // should add indices after this
  public void RemoveAllIndex(){
    _manyButton = 0;
    _buttonIndexes.Clear();

    for(int i = _splitter.GetChildCount()-1; i >= 0; i--){
      var _child = _splitter.GetChild(i);
      _splitter.RemoveChild(_child);
      _child.QueueFree();
    }
  }

  public void RemoveIndex(int idx){
    if(_manyButton > 1 && idx >= 0 && idx < _manyButton){
      Button _b = _splitter.GetChild<Button>(idx*2);
      Button _badj = null;

      if(idx < _manyButton-1)
        _badj = _splitter.GetChild<Button>(idx*2+1);
      else
        _badj = _splitter.GetChild<Button>(idx*2-1);
      
      int _sizeidxb = _buttonIndexes[idx].Value-_buttonIndexes[idx].Key;
      float _sizeb = _getPosxByTick(1);
      
      int _sizeidxb1 = idx == (_manyButton-1)? _sizeidxb: _sizeidxb/2;
      int _sizeidxb2 = idx == 0? _sizeidxb: Mathf.CeilToInt((float)_sizeidxb/2);
      float _sizeb1 = _sizeb*_sizeidxb1;
      float _sizeb2 = _sizeb*_sizeidxb2;

      // button 2
      if(idx < _manyButton-1){
        _splitter.GetChild<Button>((idx+1)*2).RectMinSize += new Vector2(_sizeb2, 0);
        var _bidx2 = _buttonIndexes[idx+1];
        _bidx2 = new KeyValuePair<int, int>(_bidx2.Key-_sizeidxb2, _bidx2.Value);
        
        _buttonIndexes[idx+1] = _bidx2;
        EmitSignal("on_buttonadjusted", idx+1, _bidx2.Key, _bidx2.Value, idx == 0? true: false);
      }

      // button 1
      if(idx > 0){
        _splitter.GetChild<Button>((idx-1)*2).RectMinSize += new Vector2(_sizeb1, 0);
        var _bidx1 = _buttonIndexes[idx-1];
        _bidx1 = new KeyValuePair<int, int>(_bidx1.Key, _bidx1.Value+_sizeidxb1); 
        
        _buttonIndexes[idx-1] = _bidx1;
        EmitSignal("on_buttonadjusted", idx-1, _bidx1.Key, _bidx1.Value, true);
      }

      _splitter.RemoveChild(_b);
      _splitter.RemoveChild(_badj);
      _buttonIndexes.RemoveAt(idx);
      
      _b.QueueFree();
      _badj.QueueFree();

      _manyButton--;

      for(int i = idx; i < _manyButton; i++){
        Button __b = _splitter.GetChild<Button>(i*2);
        __b.Disconnect("pressed", this, "_onButtonPressed");
        __b.Connect("pressed", this, "_onButtonPressed", new Godot.Collections.Array{i});

        if(i < _manyButton-1){
          Button __badj = _splitter.GetChild<Button>(i*2+1);
          __badj.Disconnect("button_down", this, "_onAdjuster_down");
          __badj.Connect("button_down", this, "_onAdjuster_down", new Godot.Collections.Array{i});
        }
      }

      EmitSignal("on_buttondeleted", idx);
    }
    else
      ErrorShowAutoload.Autoload.DisplayError("Cannot remove last split.");
  }

  public KeyValuePair<int, int> GetButtonBoundIndex(int buttonIdx){
    return _buttonIndexes[buttonIdx];
  }
}