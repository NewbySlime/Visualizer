using Godot;

public class AutoloadClass<T>: Godot.Node{
  private static T _autoload;
  private static bool _isloaded = false;

  protected static void _setAutoload(T obj){
    if(!_isloaded){
      _isloaded = true;
      _autoload = obj;
    }
  }

  public static T Autoload{
    get{
      return _autoload;
    }
  }
}
