using Godot;
using System;

public class Visualizer: TextureRect{
  [Export]
  private int defaultUpdateFrequency = 30, sandboxUpdateFrequency = 3;
  [Export]
  private float decrementPerSec = 0.5f;
  [Export]
  // x member is the minimal
  // y member is the maximal
  private Vector2 minMaxIntensity;
  [Export]
  private int bandLength = 0;
  private int channelLength = 1;

  private Timer _updateSandboxTimer;

  private ShaderMaterial visShadMat;
  private Timer _updateTimer;
  private float _offset = 0.0f;
  private float _offset_mult = 0.0f;

  private float updateInterval = 0;
  private float[] maxBandIntensity = new float[0];
  private float maxBandIntensity_avg = 0;

  private float _time = 0f;
  private float _maxtime = 1000f;

  public int UpdateFrequency{
    set{
      updateInterval = 1f/value;
      _updateTimer.WaitTime = updateInterval;
    }
  }


  public int Channels{
    get{
      return channelLength;
    }
    
    set{
      if(channelLength != value){
        channelLength = value;
        visShadMat.SetShaderParam("channelCount", channelLength);
        visShadMat.SetShaderParam("arrayLength", channelLength*bandLength);
        Array.Resize<float>(ref maxBandIntensity, channelLength*bandLength);
      }
    }
  }

  public int Bands{
    get{
      return bandLength;
    }

    set{
      if(bandLength != value){
        bandLength = value;
        visShadMat.SetShaderParam("arrayLength", channelLength*bandLength);
        Array.Resize<float>(ref maxBandIntensity, channelLength*bandLength);
      }
    }
  }

  public Vector2 MinMaxIntensity{
    get{
      return minMaxIntensity;
    }

    set{
      if(minMaxIntensity != value){
        minMaxIntensity = value;
        visShadMat.SetShaderParam("max_intensity", minMaxIntensity.y);
        visShadMat.SetShaderParam("min_intensity", minMaxIntensity.x);
      }
    }
  }

  public float OffsetMultiplier{
    get{
      return _offset_mult;
    }

    set{
      _offset_mult = value;
    }
  }

  public bool SandboxMode{
    set{
      if(SandboxMode != value){
        if(value)
          _updateSandboxTimer.Start(1f/sandboxUpdateFrequency);
        else
          _updateSandboxTimer.Stop();
      }
    }

    get{
      return !_updateSandboxTimer.IsStopped();
    }
  }


  private void _onUpdateTimer(){
    _offset += updateInterval*_offset_mult;
    visShadMat.SetShaderParam("slide_offset", _offset);

    _UpdateVisualizer();
  }
  
  private void _onUpdateSandboxTimer(){
    float _avg = 0;
    for(int i = 0; i < maxBandIntensity.Length; i++){
      float _intensity = 2 * minMaxIntensity.y * Mathf.Sin(Mathf.Deg2Rad(90f*((float)i/maxBandIntensity.Length + _time)));
      if(_intensity > maxBandIntensity[i])
        maxBandIntensity[i] = _intensity;
      
      _avg += maxBandIntensity[i];
    }

    if(_avg > maxBandIntensity_avg)
      maxBandIntensity_avg = _avg;
  }

  private void _UpdateVisualizer(){
    if(maxBandIntensity.Length > 0){
      Image arrayData = new Image();
      arrayData.Create(channelLength * bandLength, 1, false, Image.Format.Rf);
      arrayData.Lock();

      float _updatediv = 10*updateInterval+1.0f;

      for(int idx = 0; idx < maxBandIntensity.Length; idx++){
        maxBandIntensity[idx] /= _updatediv;
        arrayData.SetPixel(idx, 0, new Color{r = maxBandIntensity[idx]});
      }

      arrayData.Unlock();
      ImageTexture _array = new ImageTexture();
      _array.CreateFromImage(arrayData, 0);
      visShadMat.SetShaderParam("floatArray", _array);


      maxBandIntensity_avg /= _updatediv;
      visShadMat.SetShaderParam("avg_intensity", maxBandIntensity_avg);
    }
  }


  public override void _Ready(){
    _updateSandboxTimer = new Timer();
    _updateSandboxTimer.OneShot = false;
    _updateSandboxTimer.Autostart = false;
    _updateSandboxTimer.Connect("timeout", this, "_onUpdateSandboxTimer");
    AddChild(_updateSandboxTimer);

    _updateTimer = new Timer();
    _updateTimer.OneShot = false;
    _updateTimer.Autostart = true;
    _updateTimer.Connect("timeout", this, "_onUpdateTimer");
    UpdateFrequency = defaultUpdateFrequency;
    AddChild(_updateTimer);
    visShadMat = Material as ShaderMaterial;

    Bands = bandLength;
    MinMaxIntensity = minMaxIntensity;
  }

  public override void _Process(float delta){
    _time += delta;
    if(_time > _maxtime)
      _time = 0f;
  }


  public void UpdateVisualizer(ref float[] fdata){
    if(channelLength > 0){
      int _currChannel = channelLength;
      int _currBand = bandLength;

      if(_currBand*_currChannel <= 0)
        return;

      float avg = 0;
      int datalenc = fdata.Length/_currChannel;

      float incr = 1.0f;
      int data_idx = 0;
      
      if(datalenc != _currBand)
        incr = (float)datalenc/_currBand;

      for(int i_c = 0; i_c < _currChannel; i_c++)
        for(float i_f = 0; i_f < datalenc && data_idx < maxBandIntensity.Length; i_f += incr){
          int idx = Mathf.FloorToInt(i_f);
          int nextidx = Mathf.FloorToInt(i_f+incr);

          float num = fdata[(idx*_currChannel)+i_c];
          if(nextidx < datalenc && (nextidx-idx) > 1)
            for(int i = idx+1; i < nextidx; i++)
              if(fdata[(i*_currChannel)+i_c] > num)
                num = fdata[(i*_currChannel)+i_c];
          
          if(num < maxBandIntensity[data_idx])
            num = maxBandIntensity[data_idx];
          
          avg += num;
          maxBandIntensity[data_idx] = num;
          data_idx++;
        }
      

      avg /= (_currBand*_currChannel);

      if(avg > maxBandIntensity_avg)
        maxBandIntensity_avg = avg;
    }
  }

  public void ResetOffset(){
    _offset = 0;
  }
}