using Godot;

public class Opt_EditCheck: Opt_Edit{
  private CheckBox checkBox;

  private void OnButtonToggled(bool toggle){
    EmitSignal("OnConfirmContent", toggle, this);
  }

  public override void _Ready(){
    base._Ready();

    checkBox = GetNode<CheckBox>("CheckBox");
    checkBox.Connect("toggled", this, "OnButtonToggled");
  }

  public override void ChangeContent(object var){
    if(var is bool)
      checkBox.Pressed = (bool)var;
    else
      GD.PrintErr("Variable 'var' is supplied with not a boolean for Opt_EditCheck.");
  }
}