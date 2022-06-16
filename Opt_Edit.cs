using Godot;

// Opt_Edit class is just for getting data, for the alignment based on data type is handled outside the class
// TODO, if the object type for the signal isn't right, then should change
public class Opt_Edit: HBoxContainer{
  [Signal]
  protected delegate void OnConfirmContent(object content, Opt_Edit obj);

  protected Label labelText;


  public override void _Ready(){
    labelText = GetNode<Label>("Label");
  }

  public virtual void ChangeTitle(string titleStr){
    labelText.Text = titleStr;
  }

  public virtual void ChangeContent(object var){

  }
}