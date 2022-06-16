using Godot;

public class Opt_EditText: Opt_Edit{
  private TextEdit textEdit;

  private void OnTextEdited(){
    int index = 0;
    if((index = textEdit.Text.Find('\n')) != -1){
      ChangeTextEdit(textEdit.Text.Remove(index, 1));
      textEdit.ReleaseFocus();
      EmitSignal("OnConfirmContent", textEdit.Text, this);
    }
  }

  public override void _Ready(){
    base._Ready();

    textEdit = GetNode<TextEdit>("TextEdit");
    textEdit.Connect("text_changed", this, "OnTextEdited");
  }
  
  public void ChangeTextEdit(string tedStr){
    textEdit.Text = tedStr;
  }

  public override void ChangeContent(object var){
    if(var is string)
      textEdit.Text = var as string;
    else
      GD.PrintErr("Variable 'var' is supplied with not a string for Opt_EditText.");  
  }
}