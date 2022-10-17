using System;

public static class AnimationFunction{
  public static float Bezier1DFunc(float val, float[] data){
    float[] newfdata = new float[data.Length-1];

    for(int i = 0; i < newfdata.Length; i++)
      newfdata[i] = (data[i+1]-data[i])*val+data[i];
    
    if(newfdata.Length == 1)
      return newfdata[0];
    else
      return Bezier1DFunc(val, newfdata);
  }
}