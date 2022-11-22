using Godot;
using System;

public class EditApplyCancel: HBoxContainer, IEditInterface{
  public class set_param{
    public HBoxContainer.AlignMode alignment;
  }

  public class get_param: Godot.Object{
    public Answer answer;
  }

  public enum Answer{
    APPLY,
    CANCEL
  }

  
  [Signal]
  private delegate void on_edited(int id);

  private int _id;
  
  public string Title{
    get{
      return "";
    }

    set{

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


  private void _on_buttonPressed(int id){
    get_param _p = new get_param{answer = (Answer)id};
    EmitSignal("on_edited", _id, _p);
  }

  public override void _Ready(){
    Button _btn1 = GetNode<Button>("1");
    _btn1.Text = "Apply";
    _btn1.Connect("pressed", this, "_on_buttonPressed", new Godot.Collections.Array{(int)Answer.APPLY});

    Button _btn2 = GetNode<Button>("2");
    _btn2.Text = "Cancel";
    _btn2.Connect("pressed", this, "_on_buttonPressed", new Godot.Collections.Array{(int)Answer.CANCEL});
  }

  public void ChangeContent(object content){
    if(content is set_param){
      var _p = content as set_param;
      Alignment = _p.alignment;
    }
    else{
      throw new Exception(String.Format("Can't use a content of type {0}, in EditStaticText, which uses EditApplyCancel.set_param.", content.GetType().ToString()), new NotSupportedException());
    }
  }
}