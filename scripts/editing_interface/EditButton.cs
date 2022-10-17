using Godot;
using System;

public class EditButton: HBoxContainer, IEditInterface{
  public class set_param{
    // additional ID, ex. index
    public int id;
    public string changeOnHover = null;
  }

  public class get_param: Godot.Object{
    public int id;
  }

  [Signal]
  private delegate void on_edited(int id, int id2);

  private Button _button;
  private int _id = -1, _id2 = -1;
  private string _changeOnHover;
  private string _title;
  private bool _isMouseHover = false;
  
  public string Title{
    get{
      return _title;
    }

    set{
      _title = value;
      if(!_isMouseHover)
        _button.Text = value;
    }
  }

  public int ID{
    get{
      return _id;
    }

    set{
      _id = value;
    }
  }


  private void _onpressed(){
    EmitSignal("on_edited", _id, _id2);
  }

  private void _onMouseEntered(){
    _isMouseHover = true;
    if(_changeOnHover != null)
      _button.Text = _changeOnHover;
  }
  
  private void _onMouseExited(){
    _isMouseHover = false;
    _button.Text = _title;
  }

  public override void _Ready(){
    _button = GetNode<Button>("Button");
    _button.Connect("pressed", this, "_onpressed");
  }

  public void ChangeContent(object content){
    if(content is set_param){
      set_param param = (set_param)content;
      _id2 = param.id;
      _changeOnHover = param.changeOnHover;
    }
    else{
      throw new Exception(String.Format("Can't use a content of type {0}, in EditButton, which uses EditButton.set_param.", content.GetType().ToString()), new NotSupportedException());
    }
  }
}