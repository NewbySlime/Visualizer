using Godot;

public class PopupEdit: Popup{
  [Signal]
  private delegate void on_optionchanged(int idPopup, int idOpt);

  private VBoxContainer _vbcont;
  private int _id = -1;

  private void _on_optionChanged(int id, object obj){
    EmitSignal("on_optionchanged", _id, id, obj, this);
  }

  public int ID{
    set{
      _id = value;
    }
  }

  public override void _Ready(){
    ScrollContainer _sc = GetNode<ScrollContainer>("ScrollContainer");
    _sc.RectMinSize = new Vector2(RectMinSize.x, RectMinSize.y);

    _vbcont = _sc.GetNode<VBoxContainer>("VBoxContainer");
    _vbcont.RectMinSize = _sc.RectMinSize;

    Panel _p = GetNode<Panel>("Panel");
    _p.RectMinSize = RectMinSize;
  }

  public void SetInterfaceEdit(IEditInterface_Create.EditInterfaceContent[] IE){
    IEditInterface_Create.Autoload.RemoveAllChild(_vbcont);
    IEditInterface_Create.Autoload.CreateAndAdd(_vbcont, this, "_on_optionChanged", IE);
  }

  public void Exit(){
    Visible = false;
  }
}