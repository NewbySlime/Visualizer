using Godot;
using System;
using System.Collections.Generic;

public class EditChoice: HBoxContainer, IEditInterface{
  public class set_param{
    public KeyValuePair<string, int>[] choices;
    public int choiceID;
  }

  public class get_data: Godot.Object{
    public int choiceID;
  }

  [Signal]
  private delegate void on_edited(int id, int idx);

  private int id = -1;
  private Label title;
  private OptionButton choices;
  private List<int> listID = new List<int>();
  private Dictionary<int, int> dictID = new Dictionary<int, int>();

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

  
  private void _onedited(int index){
    get_data data = new get_data{
      choiceID = listID[index]
    };

    EmitSignal("on_edited", id, data);
    choices.Selected = dictID[data.choiceID];
  }

  public override void _Ready(){
    title = GetNode<Label>("Label");
    choices = GetNode<OptionButton>("OptionButton");
    choices.Connect("item_selected", this, "_onedited");
  }

  public void ChangeContent(object content){
    if(content is set_param){
      set_param param = content as set_param;

      listID.Clear();
      dictID.Clear();
      choices.Clear();
      int i = 0;
      foreach(var pair in param.choices){
        listID.Add(pair.Value);
        dictID[pair.Value] = i;
        choices.AddItem(pair.Key, pair.Value);
        i++;
      }
      
      choices.Selected = param.choiceID;
    }
    else{
      throw new Exception(String.Format("Can't use a content of type {0}, in EditChoice, which uses EditChoice.set_param.", content.GetType().ToString()), new NotSupportedException());
    }
  }

  public void ChangeTextItem(int idx, string str){
    choices.SetItemText(idx, str);
  }
}