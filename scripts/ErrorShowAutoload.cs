using Godot;

public class ErrorShowAutoload: AutoloadClass<ErrorShowAutoload>{
  [Export]
  private PackedScene _ErrorPopup;
  private AcceptDialog _dialog;

  public override void _Ready(){
    _setAutoload(this);
    _dialog = _ErrorPopup.Instance<AcceptDialog>();
  }

  public void DisplayError(string message){
    _dialog.WindowTitle = "Error";
    _dialog.DialogText = message;

    Vector2 windsize = GetViewport().Size;
    _dialog.RectPosition = (windsize-_dialog.RectSize)/2;
    _dialog.Popup_();
  }

  public void AddToControl(Node obj){
    obj.AddChild(_dialog);
  }
}