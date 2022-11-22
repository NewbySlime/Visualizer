using Godot;
using System;

public class PopupEdit_IPPort: PopupEdit{
  [Export]
  private string Title = "", Title_IPAddress = "", Title_Port = "";
  [Export]
  private int Margin_Top, Margin_Bottom, Margin_Left, Margin_Right;
  [Export]
  private Label.AlignEnum TextAlignment;
  [Signal]
  private delegate void on_optionchanged_ipport(uint ip, ushort port);

  private VBoxContainer _editCont;
  private HBoxContainer _btnCont;
  private uint _ip = 0;
  private ushort _port = 0;

  private uint _changedip;
  private ushort _changedport;

  public ushort Port{
    get{
      return _port;
    }

    set{
      _port = value;
    }
  }

  public uint IP{
    get{
      return _ip;
    }

    set{
      _ip = value;
    }
  }


  private void _onChange(int id, object obj){
    switch(id){
      // ip address
      case 1:{
        EditText.get_data _data = obj as EditText.get_data;
        string[] strs = _data.strdata.Split('.');

        byte[] intdata = BitConverter.GetBytes(_changedip);
        for(int i = 0; i < 4 && i < strs.Length; i++){
          try{
            int n = strs[i].ToInt();
            if(intdata[i] == 0 && intdata[i] != n){
              strs[i] = strs[i].Trim(new char[]{'0'});
              n = strs[i].ToInt();
            }
            
            if(n > 0xff)
              n = 0xff;

            intdata[i] = (byte)n;
          }
          catch{
            intdata[i] = 0;
          }
        }

        _data.strdata = string.Format("{0}.{1}.{2}.{3}", intdata[0], intdata[1], intdata[2], intdata[3]);

        _changedip = BitConverter.ToUInt32(intdata, 0);
        break;
      }

      // port
      case 2:{
        EditText.get_data _data = obj as EditText.get_data;

        if(_data.strdata.Length > 0){
          try{
            _changedport = (ushort)_data.strdata.ToInt();
          }
          catch{}
        }

        _data.strdata = _changedport.ToString();
        break;
      }

      // applying
      case 3:{
        _ip = _changedip;
        _port = _changedport;

        EmitSignal("on_optionchanged_ipport", _ip, _port);
        Hide();
        break;
      }

      // cancelling
      case 4:{
        Hide();
        break;
      }
    }
  }

  private void _onPopup(){
    IEditInterface_Create.Autoload.RemoveAllChild(_editCont);

    _changedip = _ip;
    _changedport = _port;

    byte[] _ipintdata = BitConverter.GetBytes(_ip);
    IEditInterface_Create.EditInterfaceContent[] contents = {
      new IEditInterface_Create.EditInterfaceContent{
        TitleName = Title,
        EditType = IEditInterface_Create.InterfaceType.StaticText,
        Properties = new EditStaticText.set_param{
          margin_top = Margin_Top,
          margin_bottom = Margin_Bottom,
          margin_left = Margin_Left,
          margin_right = Margin_Right,
          align = TextAlignment
        },

        ID = 0
      },

      new IEditInterface_Create.EditInterfaceContent{
        TitleName = Title_IPAddress,
        EditType = IEditInterface_Create.InterfaceType.Text,
        Properties = new EditText.set_param{
          strdata = string.Format("{0}.{1}.{2}.{3}", _ipintdata[0], _ipintdata[1], _ipintdata[2], _ipintdata[3])
        },

        ID = 1
      },

      new IEditInterface_Create.EditInterfaceContent{
        TitleName = Title_Port,
        EditType = IEditInterface_Create.InterfaceType.Text,
        Properties = new EditText.set_param{
          strdata = _port.ToString()
        },

        ID = 2
      }
    };

    IEditInterface_Create.Autoload.CreateAndAdd(_editCont, this, "_onChange", contents);


    // setting up the button container
    if(_btnCont == null){
      _btnCont = new HBoxContainer();
      _editCont.AddChild(_btnCont);
      _btnCont.Alignment = BoxContainer.AlignMode.Center;

      IEditInterface_Create.EditInterfaceContent[] contentsbtn = {
        new IEditInterface_Create.EditInterfaceContent{
          TitleName = "Apply",
          EditType = IEditInterface_Create.InterfaceType.Button,
          Properties = new EditButton.set_param{},
          ID = 3
        },

        new IEditInterface_Create.EditInterfaceContent{
          TitleName = "Cancel",
          EditType = IEditInterface_Create.InterfaceType.Button,
          Properties = new EditButton.set_param{},
          ID = 4
        }
      };

      IEditInterface_Create.Autoload.CreateAndAdd(_btnCont, this, "_onChange", contentsbtn);
    }

    _editCont.MoveChild(_btnCont, _editCont.GetChildCount()-1);
  }


  public override void _Ready(){
    base._Ready();

    _editCont = GetNode<VBoxContainer>("ScrollContainer/VBoxContainer");
    Connect("about_to_show", this, "_onPopup");
  }
}