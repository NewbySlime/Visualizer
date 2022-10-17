using Godot;


//  note: classes inheriting IEditInterface should have signal called on_edited
//    and in it should have first parameter of int (the id of the related)
public interface IEditInterface{
  string Title{get; set;}
  int ID{get; set;}
  void ChangeContent(object content);
}

public class IEditInterface_Create: AutoloadClass<IEditInterface_Create>{
  [Export]
  private PackedScene ECheck, EChoice, ESlider, EText, EStaticText, EButton;

  public enum InterfaceType{
    Check,
    Choice,
    Slider,
    Text,
    StaticText,
    Button
  }

  public struct EditInterfaceContent{
    public string TitleName;
    public InterfaceType EditType;
    public object Properties;
    public int ID;
  }

  public override void _Ready(){
    _setAutoload(this);
  }

  public void RemoveAllChild(Node parent){
    var childarray = parent.GetChildren();
    foreach(Node n in childarray){
      if(n is IEditInterface){
        parent.RemoveChild(n);
        n.QueueFree();
      }
    }
  }

  public void CreateAndAdd(Node parent, Node signalHandler, string callback, EditInterfaceContent[] content){
    for(int i = 0; i < content.Length; i++){
      IEditInterface Ie = null;

      switch(content[i].EditType){
        case InterfaceType.Check:
          Ie = ECheck.Instance<EditCheck>();
          break;
        
        case InterfaceType.Choice:
          Ie = EChoice.Instance<EditChoice>();
          break;
        
        case InterfaceType.Slider:
          Ie = ESlider.Instance<EditSlider>();
          break;
        
        case InterfaceType.Text:
          Ie = EText.Instance<EditText>();
          break;
        
        case InterfaceType.StaticText:
          Ie = EStaticText.Instance<EditStaticText>();
          break;
        
        case InterfaceType.Button:
          Ie = EButton.Instance<EditButton>();
          break;
      }

      Node n = Ie as Node;
      parent.AddChild(n);

      Ie.Title = content[i].TitleName;
      Ie.ID = content[i].ID;
      Ie.ChangeContent(content[i].Properties);

      n.Connect("on_edited", signalHandler, callback);
    }
  }
}