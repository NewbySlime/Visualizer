using Godot;

public class Opt_EditSlider: Opt_Edit{
  private HSlider hSlider;


  private void OnSliderChanged(float value){
    EmitSignal("OnConfirmContent", value, this);
  }

  public override void _Ready(){
    base._Ready();

    hSlider = GetNode<HSlider>("HSlider");
    hSlider.Connect("value_changed", this, "OnSliderChanged");
  }

  public override void ChangeContent(object var){
    if(var is float)
      hSlider.Ratio = (float)var;
    else if(var is int)
      hSlider.Value = (int)var;
    else
      GD.PrintErr("Variable 'var' is supplied with not int or float for Opt_EditCheck.");
  }
}