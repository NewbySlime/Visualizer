using Godot;
using System;

public class EditText: HBoxContainer, IEditInterface{
  public class set_param{
    public string strdata = "";
    public bool fullEdit = false;
    public bool readOnly = false;
  }

  public class get_data: Godot.Object{
    public string strdata;
    public bool confirmed;
  }


  [Signal]
  private delegate void on_edited(int id);

  private int id = -1;
  private Label title;
  private TextEdit txtedit;

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


  private void _onedited(int _n = 0){
    get_data data = new get_data{
      strdata = txtedit.Text,
      confirmed = _n == -1
    };

    int _currcolumn = txtedit.CursorGetColumn();
    int _prevtextsize = txtedit.Text.Length;

    EmitSignal("on_edited", id, data);
    txtedit.Text = data.strdata;

    if(_currcolumn == 0 && _prevtextsize == 0)
      txtedit.CursorSetColumn(data.strdata.Length);
    else
      txtedit.CursorSetColumn(Mathf.Min(_currcolumn, data.strdata.Length));
  }

  private void _onstr_changed(){
    if(txtedit.Text.Length > 0 && txtedit.Text[txtedit.Text.Length-1] == '\n'){
      txtedit.Text = txtedit.Text.Remove(txtedit.Text.Length-1, 1);
      txtedit.ReleaseFocus();
      _onedited(-1);
    }
    else
      _onedited();
  }

  public override void _Ready(){
    title = GetNode<Label>("Label");
    txtedit = GetNode<TextEdit>("TextEdit");
    txtedit.Connect("text_changed", this, "_onstr_changed");
    txtedit.Connect("focus_exited", this, "_onedited", new Godot.Collections.Array{-1});
  }

  public void ChangeContent(object content){
    if(content is set_param){
      set_param param = (set_param)content;
      txtedit.Text = param.strdata;
      txtedit.Readonly = param.readOnly;

      if(param.fullEdit)
        RemoveChild(title);
      else if(title.GetParent() != this){
        AddChild(title);
        MoveChild(title, 0);
      }
    }
    else{
      throw new Exception(String.Format("Can't use a content of type {0}, in EditText, which uses EditText.set_param.", content.GetType().ToString()), new NotSupportedException());
    }
  }
}