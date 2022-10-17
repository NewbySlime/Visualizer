using Godot;
using System;

public class EditCheck: HBoxContainer, IEditInterface{
  public class set_param{
    public bool check;
  }

  public class get_data: Godot.Object{
    public bool bdata;
  }

  [Signal]
  private delegate void on_edited(int id, bool check);

  private int id = -1;
  private Label title;
  private CheckBox checkBox;

  public string Title{
    get{
      return title.Text;
    }

    set{
      title.Text = value;
    }
  }

  public int ID{
    get{
      return id;
    }

    set{
      id = value;
    }
  }

  private void _onedited(bool check){
    get_data data = new get_data{
      bdata = check
    };

    EmitSignal("on_edited", id, data);
    checkBox.Pressed = data.bdata;
  }

  public override void _Ready(){
    title = GetNode<Label>("Label");
    checkBox = GetNode<CheckBox>("CheckBox");
    checkBox.Connect("toggled", this, "_onedited");
  }

  public void ChangeContent(object content){
    if(content is set_param){
      set_param param = content as set_param;
      checkBox.Pressed = param.check;
    }
    else{
      throw new Exception(String.Format("Can't use a content of type {0}, in EditCheck, which uses EditCheck.set_param.", content.GetType().ToString()), new NotSupportedException());
    }
  }
}