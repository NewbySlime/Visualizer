using Godot;
using System;

public class Visualizer: TextureRect{
  [Export]
  private int defaultUpdateFrequency = 30;
  [Export]
  private float decrementPerSec = 0.5f;
  [Export]
  // x member is the minimal
  // y member is the maximal
  private Vector2 maxMinIntensity;
  [Export]
  private int bandLength = 0;

  private ShaderMaterial visShadMat;

  private float updateInterval = 0;
  private float[] maxBandIntensity = new float[0];

  public int UpdateFrequency{
    set{
      updateInterval = 1f/value;
    }
  }


  private float f_max(ref float[] arr, int from, int to){
    float biggest = arr[from];
    for(int i = from+1; i < to && i < arr.Length; i++){
      if(arr[i] > biggest)
        biggest = arr[i];
    }

    return biggest;
  }


  public override void _Ready(){
    visShadMat = Material as ShaderMaterial;

    UpdateFrequency = defaultUpdateFrequency;
    SetNumberOfBands(bandLength);
    SetMaxMinIntensity(maxMinIntensity);
  }

  public void SetNumberOfBands(int num){
    bandLength = num;
    maxBandIntensity = new float[bandLength];
    visShadMat.SetShaderParam("arrayLength", bandLength);
  }

  public void SetMaxMinIntensity(Vector2 maxMin){
    maxMinIntensity = maxMin;
    visShadMat.SetShaderParam("dbMax", maxMin.y);
    visShadMat.SetShaderParam("dbMin", maxMin.x);
  }

  public void UpdateVisualizer(ref float[] fdata){
    Image arrayData = new Image();
    arrayData.Create(bandLength, 1, false, Image.Format.R8);
    arrayData.Lock();

    int i_data = 0;
    int incr = 1;
    
    if(fdata.Length > bandLength)
      incr = (int)Mathf.Ceil((float)fdata.Length/bandLength);
    
    for(int i = 0; i < fdata.Length; i += incr){
      float num = f_max(ref fdata, i, i+incr);
      if(num < maxMinIntensity.x)
        num = maxMinIntensity.x;
      else if(num > maxMinIntensity.y)
        num = maxMinIntensity.y;
      
      if(num > maxBandIntensity[i_data])
        maxBandIntensity[i_data] = num;
      
      if((maxBandIntensity[i_data] -= updateInterval*decrementPerSec) < 0f)
        maxBandIntensity[i_data] = 0f;
      
      arrayData.SetPixel(i_data, 0, Color.Color8(
        (byte)Mathf.Round(((float)(maxBandIntensity[i_data]-maxMinIntensity.x)/(maxMinIntensity.y-maxMinIntensity.x))*255),
        0,
        0,
        0
      ));

      i_data++;
    }

    for(; i_data < bandLength; i_data++){
      if((maxBandIntensity[i_data] -= updateInterval*decrementPerSec) < 0f)
        maxBandIntensity[i_data] = 0f;
      
      arrayData.SetPixel(i_data, 0, Color.Color8(
        (byte)Mathf.Round(((float)(maxBandIntensity[i_data]-maxMinIntensity.x)/(maxMinIntensity.y-maxMinIntensity.x))*255),
        0,
        0,
        0
      ));
    }

    arrayData.Unlock();
    ImageTexture array = new ImageTexture();
    array.CreateFromImage(arrayData);

    visShadMat.SetShaderParam("floatArray", array);
  }
}