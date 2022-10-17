using Godot;
using System;
using System.Collections.Generic;

public class SliderColor: TextureButton{
  [Signal]
  private delegate void on_colorgradientchanged(Color[] col);

  [Export]
  private PackedScene colorPickerScene;
  private ShaderMaterial _shadmat;
  private Image _coldata = new Image();

  private List<DragColorPicker> ColorPickers = new List<DragColorPicker>();
  private List<float> ColorPickers_Range = new List<float>();

  private float _getRangeX(Vector2 mousepos){
    return Mathf.Clamp((mousepos.x-RectGlobalPosition.x)/RectSize.x, 0, 1);
  }

  private Color _getColBetween(float range){
    Color result = Colors.Black;

    if(ColorPickers.Count > 0){
      Color firstCol = Colors.Black, secondCol = Colors.Black;
      int index1 = -1, index2 = -1;
      for(int i = 0; i < ColorPickers_Range.Count; i++){
        if(ColorPickers_Range[i] < range)
          index1 = i;
        
        if(index2 < 0 && ColorPickers_Range[i] > range)
          index2 = i;
      }

      if(index1 >= 0)
        firstCol = ColorPickers[index1].Color;
      else
        return ColorPickers[0].Color;
      
      if(index2 >= 0)
        secondCol = ColorPickers[index2].Color;
      else
        return ColorPickers[ColorPickers.Count-1].Color;

      Vector3 vfirstcol = new Vector3(firstCol.r, firstCol.g, firstCol.b),
        vsecondcol = new Vector3(secondCol.r, secondCol.g, secondCol.b);
      
      Vector3 _diff = (vsecondcol-vfirstcol)*((range-ColorPickers_Range[index1])/(ColorPickers_Range[index2]-ColorPickers_Range[index1]));
      _diff += vfirstcol;

      result = Color.Color8(
        (byte)Mathf.RoundToInt(_diff.x*255),
        (byte)Mathf.RoundToInt(_diff.y*255),
        (byte)Mathf.RoundToInt(_diff.z*255),
        255
      );
    }

    return result;
  }

  private void _updateColor(){
    _coldata.Create(ColorPickers.Count, 1, false, Image.Format.Rgba8);
    _coldata.Lock();

    Color[] datacol = new Color[ColorPickers.Count];
    for(int i = 0; i < ColorPickers.Count; i++){
      Color _col = ColorPickers[i].Color;
      _col.a = ColorPickers_Range[i];

      datacol[i] = _col;
      _coldata.SetPixel(i, 0, _col);
    }

    _coldata.Unlock();

    ImageTexture array = new ImageTexture();
    array.CreateFromImage(_coldata, 0);

    _shadmat.SetShaderParam("coldata", array);
    _shadmat.SetShaderParam("coldatalen", ColorPickers.Count);
    EmitSignal("on_colorgradientchanged", datacol);
  }

  private void _colorPick_onchanged(int index, Color col){
    _updateColor();
  }

  // BUG the same bug still appears, somehow, by just quickly swapping it
  private void _colorPick_ondragged(int index, Vector2 _mousepos, DragColorPicker obj){
    float _range = _getRangeX(_mousepos);
    _mousepos.x = Mathf.Clamp(_mousepos.x-RectGlobalPosition.x, 0, RectSize.x);

    _mousepos.x -= obj.RectSize.x/2;
    _mousepos.y = RectSize.y;

    // then check the index (position)
    // if swap, then swap the pixel too
    int _newindex = -1;
    for(int idx = index; idx < ColorPickers_Range.Count && _range > ColorPickers_Range[idx]; idx++)
      _newindex = idx;
    
    for(int idx = index; idx >= 0 && _range < ColorPickers_Range[idx]; idx--)
      _newindex = idx;

    if(_newindex == index || _newindex == -1)
      ColorPickers_Range[index] = _range;
    else{
      DragColorPicker _col = ColorPickers[_newindex];
      float _f = ColorPickers_Range[_newindex];

      ColorPickers[_newindex] = ColorPickers[index];
      ColorPickers_Range[_newindex] = ColorPickers_Range[index];

      ColorPickers[index] = _col;
      ColorPickers_Range[index] = _f;

      for(int i = _newindex; i < ColorPickers.Count; i++)
        ColorPickers[i].ButtonIndex = i;
    }

    obj.RectPosition = _mousepos;
    _updateColor();
  }

  private void _colorPick_ondeleted(int index){
    if(ColorPickers.Count > 1){
      // delete the color

      ColorPickers[index].QueueFree();
      ColorPickers.RemoveAt(index);
      ColorPickers_Range.RemoveAt(index);

      // re-set all the index
      for(int i = index; i < ColorPickers.Count; i++)
        ColorPickers[i].ButtonIndex = i;

      _updateColor();
    }
  }

  private void _addcolor(float _range, Color _col, bool _updatecol){
    DragColorPicker cp = colorPickerScene.Instance<DragColorPicker>();
    cp.ChangeColor(_col);
    cp.ButtonIndex = ColorPickers.Count;
    AddChild(cp);
    ColorPickers.Add(cp);
    ColorPickers_Range.Add(_range);

    cp.Connect("on_dragged", this, "_colorPick_ondragged");
    cp.Connect("on_colorchanged", this, "_colorPick_onchanged");
    cp.Connect("on_deleted", this, "_colorPick_ondeleted");
    
    // don't forget to set the index (i think it can just use the previous function)
    _colorPick_ondragged(ColorPickers.Count-1, new Vector2(_range*RectSize.x+RectGlobalPosition.x, 0), cp);

    if(_updatecol)
      _updateColor();
  }

  private void _onpressed(){
    // x mouse placement in the button
    Vector2 _mousepos = GetGlobalMousePosition();
    float _range = _getRangeX(_mousepos);
    _addcolor(_range, _getColBetween(_range), true);
  }

  public override void _Ready(){
    Connect("pressed", this, "_onpressed");
    _shadmat = Material as ShaderMaterial;
  }

  public void SetColors(Color[] coldatas){
    ColorPickers.Clear();
    ColorPickers_Range.Clear();

    for(int i = GetChildCount()-1; i >= 0; i--){
      var _child = GetChild(i);
      RemoveChild(_child);
      _child.QueueFree();
    }

    foreach(var col in coldatas)
      _addcolor(col.a, new Color(col.r, col.g, col.b, 1), false);

    _updateColor();
  }
}