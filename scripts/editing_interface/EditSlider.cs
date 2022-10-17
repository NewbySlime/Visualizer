using Godot;
using System;

public class EditSlider: HBoxContainer, IEditInterface{
  public class set_param{
    public float min_value = 0.0f;
    public float max_value = 1.0f;
    public float step = 0.0f;
    public float value = 0.5f;
    public bool rounded = false;
  }

  public class get_data: Godot.Object{
    public float fdata;
  }

  [Signal]
  private delegate void on_edited(int id, float value);

  private int id = -1;
  private Label title;
  private HSlider slider;
  
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


  private void _onedited(double value){
    get_data data = new get_data();
    data.fdata = (float)value;
    EmitSignal("on_edited", id, data);

    slider.Value = data.fdata;
  }

  public override void _Ready(){
    title = GetNode<Label>("Label");
    slider = GetNode<HSlider>("HSlider");
    slider.Connect("value_changed", this, "_onedited");
  }

  public void ChangeContent(object content){
    if(content is set_param){
      set_param param = content as set_param;
      slider.MinValue = param.min_value;
      slider.MaxValue = param.max_value;
      slider.Step = param.step;
      slider.Value = param.value;
      slider.Rounded = param.rounded;
    }
    else{
      throw new Exception(String.Format("Can't use a content of type {0}, in EditSlider, which uses EditSlider.set_param.", content.GetType().ToString()), new NotSupportedException());
    }
  }
}