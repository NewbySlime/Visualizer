using Godot;

public class Opt_EditChoice: Opt_Edit{
  private OptionButton optButton;


  private void OnOptionPicked(int index){
    EmitSignal("OnConfirmContent", index, this);
  }

  public override void _Ready(){
    base._Ready();

    optButton = GetNode<OptionButton>("OptionButton");
    optButton.Connect("item_selected", this, "OnOptionPicked");
  }

  public override void ChangeContent(object var){
    if(var is string[]){
      string[] choices = var as string[];
      foreach(string str in choices)
        optButton.AddItem(str);
    }
    else
      GD.PrintErr("Variable 'var' is supplied with not a string[] for Opt_EditChoice.");
  }
}