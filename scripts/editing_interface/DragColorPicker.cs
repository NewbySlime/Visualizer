using Godot;
using System;

public class DragColorPicker: ColorPickerButton{
  [Signal]
  private delegate void on_dragged(int index, Vector2 _mousepos, DragColorPicker obj);
  
  [Signal]
  private delegate void on_colorchanged(int index, Color col);

  [Signal]
  private delegate void on_deleted(int index);

  private bool _dragging = false, _mouseOnself = false;
  private int _buttonIndex = -1;


  private void _buttondown(){
    _dragging = true;
  }

  private void _buttonup(){
    _dragging = false;
  }

  private void _oncolorchanged(Color col){
    Color = col;
    EmitSignal("on_colorchanged", _buttonIndex, col);
  }

  private void _onself_mouseEntered(){
    _mouseOnself = true;
  }

  private void _onself_mouseExited(){
    _mouseOnself = false;
  }

  private void _deleteself(){
    EmitSignal("on_deleted", _buttonIndex);
  }


  public int ButtonIndex{
    get{
      return _buttonIndex;
    }
    
    set{
      _buttonIndex = value < 0? -1: value;
    }
  }

  public override void _Ready(){
    Connect("button_down", this, "_buttondown");
    Connect("button_up", this, "_buttonup");
    Connect("mouse_entered", this, "_onself_mouseEntered");
    Connect("mouse_exited", this, "_onself_mouseExited");
    Connect("color_changed", this, "_oncolorchanged");

    _oncolorchanged(Color);
  }

  public override void _Input(InputEvent @event){
    base._Input(@event);

    if(@event is InputEventMouseButton){
      InputEventMouseButton _mb = @event as InputEventMouseButton;
      if(_mouseOnself && _mb.ButtonIndex == (int)ButtonList.Right && !_mb.Pressed)
        _deleteself();
    }

    if(_dragging && @event is InputEventMouseMotion){
      Vector2 mousepos = (@event as InputEventMouseMotion).Position;
      EmitSignal("on_dragged", _buttonIndex, mousepos, this);
    }
  }

  public void ChangeColor(Color col){
    _oncolorchanged(col);
  }
}