using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

// TODO add a timeout to the socket

enum Socket_code{
  PINGMESSAGE = 0x1,
  SENDMESSAGE = 0x2,
  CLSOCKMESSAGE = 0x3,
  CHECKRATE = 0x5
}


public class SocketListener: Godot.Object{
  [Godot.Signal]
  private delegate void OnConnected();
  
  [Godot.Signal]
  private delegate void OnDisconnected();


  private IPAddress remoteAddress;
  private IPEndPoint endPoint;

  private Socket currentListener;
  private Socket currSock;

  private List<KeyValuePair<Socket_code, byte[]>> appendData
    = new List<KeyValuePair<Socket_code, byte[]>>();

  private Thread socketThread;
  private bool keepListening = false;
  private bool keepConnection = false;

  private void dumpCb1(ref byte[] b){}


  public delegate void cbFunc1(ref byte[] b);
  public cbFunc1 CbOnSeperateMessageGet;


  private void _AppendData(Socket_code code, byte[] data){
    if(keepConnection){
      lock(appendData){
        appendData.Add(new KeyValuePair<Socket_code, byte[]>(code, data));
      }
    }
  }

  private void _StartListening(){
    bool isLocked = false;
    keepListening = true;

    currentListener.Bind(endPoint);
    currentListener.Listen(1);
    while(keepListening){
      keepConnection = true;
      try{
        Socket currSock = currentListener.Accept();
        EmitSignal("OnConnected");

        while(keepConnection){
          //ArraySegment<byte> messagedata = new ArraySegment<byte>(new byte[sizeof(ushort)]);
          //await currSock.ReceiveAsync(messagedata, SocketFlags.None);
          byte[] messagedata = new byte[sizeof(ushort)];

          // BUG, might blocking
          currSock.Receive(messagedata);
          ushort msgHead = BitConverter.ToUInt16(messagedata, 0);
          bool _senddata = false;
          switch((Socket_code)msgHead){
            case Socket_code.PINGMESSAGE:{
              _senddata = true;
            }break;

            case Socket_code.SENDMESSAGE:{
              byte[] lengthData = new byte[sizeof(int)];
              currSock.Receive(lengthData);
              
              int length = BitConverter.ToInt32(lengthData, 0);
              byte[] actualMsgData = new byte[length];
              currSock.Receive(actualMsgData);
              CbOnSeperateMessageGet(ref actualMsgData);
            }break;

            case Socket_code.CLSOCKMESSAGE:{
              keepConnection = false;
            }break;
          }

          if(_senddata && appendData.Count > 0){
            Monitor.Enter(appendData);
            isLocked = true;

            bool isSocketClosing = false;
            for(int i = 0; i < appendData.Count; i++){
              switch(appendData[i].Key){
                case Socket_code.SENDMESSAGE:{
                  currSock.Send(BitConverter.GetBytes((ushort)Socket_code.SENDMESSAGE));
                  currSock.Send(BitConverter.GetBytes(appendData[i].Value.Length));
                  currSock.Send(appendData[i].Value);
                }break;

                case Socket_code.CLSOCKMESSAGE:{
                  currSock.Send(BitConverter.GetBytes((ushort)Socket_code.CLSOCKMESSAGE));
                  keepConnection = false;
                  isSocketClosing = true;
                  i = appendData.Count;
                }break;
              }
            }

            if(!isSocketClosing)
              appendData.RemoveRange(0, appendData.Count);
            

            Monitor.Exit(appendData);
            isLocked = false;
          }
          else{
            currSock.Send(BitConverter.GetBytes((ushort)Socket_code.PINGMESSAGE));
          }
        }

        currSock.Shutdown(SocketShutdown.Both);
      }
      catch(System.Exception e){
        if(keepListening)
          Godot.GD.PrintErr("Cannot use socket, errmsg:\n", e.ToString());

        if(isLocked)
          Monitor.Exit(appendData);
      }

      EmitSignal("OnDisconnected");
    }
  }


  public SocketListener(ushort port, byte[] ipAddress){
    CbOnSeperateMessageGet = dumpCb1;

    remoteAddress = new IPAddress(ipAddress);
    endPoint = new IPEndPoint(remoteAddress, port);
    currentListener = new Socket(remoteAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);
  }

  ~SocketListener(){
    StopListening();
  }

  public void StartListening(){
    if(keepListening)
      return;

    socketThread = new Thread(_StartListening);
    socketThread.Start();
  }

  public void StopListening(){
    if(!keepListening)
      return;
    
    keepListening = false;
    _AppendData(Socket_code.CLSOCKMESSAGE, null);
  }

  public void Disconnect(){
    keepConnection = false;
  }

  public void AppendData(byte[] data){
    _AppendData(Socket_code.SENDMESSAGE, data);
  }
}

/*
public class SocketHandlerAsync: Godot.Node{
  [Godot.Signal]
  private delegate void OnConnected();

  [Godot.Signal]
  private delegate void OnDisconnected();

  [Godot.Export]
  private float MaxTimeoutS = 1.0f;
  

  private const float DefaultWaitTimeS = 0.033f;
  private float WaitTimeS;

  private Task socketTask;

  private Queue<KeyValuePair<Socket_code, byte[]>> appendData = new Queue<KeyValuePair<Socket_code, byte[]>>();

  private void dumpCb1(ref byte[] b){}

  private bool _stillConnecting = false;

  
  public delegate void cbFunc1(ref byte[] b);
  public cbFunc1 CbOnSeperateMessageGet;

  private void _AppendData(Socket_code code, byte[] data){
    lock(appendData){
      appendData.Enqueue(new KeyValuePair<Socket_code, byte[]>(code, data));
    }
  }

  private void _CheckDataMessage(ushort msgcode, ref byte[] data, bool isSending){
    switch((Socket_code)msgcode){
      // do nothing
      // case Socket_code.PINGMESSAGE

      // handled outside this function
      // case Socket_code.EOBULKMESSAGE

      case Socket_code.SENDMESSAGE:
        if(!isSending)
          CbOnSeperateMessageGet(ref data);
        break;
      
      case Socket_code.CLSOCKMESSAGE:
        Disconnect();
        break;
      
      case Socket_code.CHECKRATE:
        int ratehz = BitConverter.ToInt32(data, 0);
        WaitTimeS = 1.0f/ratehz;
        break;
    }
  }

  private async Task<bool> _CheckForConnection(Socket socket, SelectMode mode){
    float time = 0.0f;
    while(_stillConnecting && time < MaxTimeoutS){
      if(socket.Poll(-1, mode))
        return true;

      await ToSignal(GetTree().CreateTimer(WaitTimeS), "timeout");
      time += WaitTimeS;
    }

    return false;
  }

  private async Task _StartConnect(ushort port, byte[] ipAddress){
    IPAddress remoteAddress = new IPAddress(ipAddress);
    IPEndPoint endPoint = new IPEndPoint(remoteAddress, port);
    Socket socket = new Socket(remoteAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);

    WaitTimeS = DefaultWaitTimeS;

    try{
      await socket.ConnectAsync(endPoint);
      if(!socket.Connected)
        throw new System.TimeoutException(string.Format("Cannot connect to {0}:{1}", remoteAddress.ToString(), port));
      
      EmitSignal("OnConnected");
      while(_stillConnecting){
        // sending data first for handler
        bool messageexist = false;
        for(int i = 0; appendData.Count > 0 || i < 1; i++){
          if(!(await _CheckForConnection(socket, SelectMode.SelectWrite)) && _stillConnecting)
            throw new System.TimeoutException("Error when sending message");
        
          ushort messagecode = (ushort)Socket_code.PINGMESSAGE;
          byte[] messagedata = new byte[0];

          if(appendData.Count > 0){
            var data = appendData.Dequeue();
            messagecode = (ushort)data.Key;
            messagedata = data.Value;
            messageexist = true;
          }

          int sent = socket.Send(BitConverter.GetBytes(messagecode));
          Godot.GD.Print("sent length ", sent);
          sent = socket.Send(BitConverter.GetBytes((int)messagedata.Length));
          Godot.GD.Print("sent length ", sent);
          if(messagedata.Length > 0)
            socket.Send(messagedata);

          _CheckDataMessage(messagecode, ref messagedata, true);

          if(appendData.Count == 0 && messageexist)
            socket.Send(BitConverter.GetBytes((ushort)Socket_code.EOBULKMSG));
        }


        // then reading data from the listener
        while(true){
          if(!(await _CheckForConnection(socket, SelectMode.SelectRead)) && _stillConnecting)
            throw new System.TimeoutException("Error when getting messagge");
            
          ushort messagecode = 0;
          int messagelength = 0;
          byte[] messagedata = new byte[0];

          {
            byte[] messagecode_bytes = new byte[sizeof(ushort)];
            byte[] messagelength_bytes = new byte[sizeof(int)];

            socket.Receive(messagecode_bytes);
            socket.Receive(messagelength_bytes);

            messagecode = BitConverter.ToUInt16(messagecode_bytes, 0);
            Godot.GD.Print("get msgcode ", messagecode);
            messagelength = BitConverter.ToInt32(messagelength_bytes, 0);
          }

          if(messagelength > 0){
            messagedata = new byte[messagelength];
            socket.Receive(messagedata);
          }

          _CheckDataMessage(messagecode, ref messagedata, false);

          if(messagecode == (int)Socket_code.EOBULKMSG || messagecode == (int)Socket_code.PINGMESSAGE)
            break;
        }

        await ToSignal(GetTree().CreateTimer(WaitTimeS), "timeout");
      }
    }
    catch(System.Exception e){
      Godot.GD.PrintErr("Cannot use socket, errmsg:\n", e.ToString());
    }

    if(appendData.Count > 0)
      appendData.Clear();
    socket.Close();
    EmitSignal("OnDisconnected");
  }

  public SocketHandlerAsync(){
    CbOnSeperateMessageGet = dumpCb1;
  }

  ~SocketHandlerAsync(){
    Disconnect(true);
  }

  public void StartConnecting(ushort port, byte[] ipAddress){
    if(!_stillConnecting){
      _stillConnecting = true;
      socketTask = _StartConnect(port, ipAddress);
    }
  }

  public void Disconnect(bool joinThread = false){
    if(_stillConnecting){
      _stillConnecting = false;
      if(joinThread)
        socketTask.Wait();
    }
  }

  public void AppendData(byte[] data){
    _AppendData(Socket_code.SENDMESSAGE, data);
  }
}
*/