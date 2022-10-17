using Godot;
using System;

public class EditStaticText: MarginContainer, IEditInterface{
  public class set_param{
    public int margin_top;
    public int margin_bottom;
    public int margin_left;
    public int margin_right;
    public Label.AlignEnum align;
  }


  [Signal]
  private delegate void on_edited(int id);

  private Label _text;

  public string Title{
    get{
      return _text.Text;
    }

    set{
      _text.Text = value;
    }
  }

  private int _dmp;
  public int ID{
    get{
      return _dmp;
    }

    set{
      _dmp = value;
    }
  }

  public override void _Ready(){
    _text = GetNode<Label>("Label");
  }

  public void ChangeContent(object content){
    if(content is set_param){
      set_param param = (set_param)content;
      MarginTop = param.margin_top;
      MarginBottom = param.margin_bottom;
      MarginLeft = param.margin_left;
      MarginRight = param.margin_right;
      _text.Align = param.align;
    }
    else{
      throw new Exception(String.Format("Can't use a content of type {0}, in EditStaticText, which uses EditStaticText.set_param.", content.GetType().ToString()), new NotSupportedException());
    }
  }
}