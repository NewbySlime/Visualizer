using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;


public class SocketListener: Godot.Object{
  [Godot.Signal]
  private delegate void OnConnected();
  
  [Godot.Signal]
  private delegate void OnDisconnected();

  private enum SocketListener_code{
    PINGMESSAGE = 0x1,
    SENDMESSAGE = 0x2,
    CLSOCKMESSAGE = 0x3,
    EOBULKMSG = 0x4
  }

  private IPAddress remoteAddress;
  private IPEndPoint endPoint;

  private Socket currentListener;
  private Socket currSock;

  private List<KeyValuePair<SocketListener_code, byte[]>> appendData
    = new List<KeyValuePair<SocketListener_code, byte[]>>();

  private Thread socketThread;
  private bool keepListening = false;

  private void dumpCb1(ref byte[] b){}


  public delegate void cbFunc1(ref byte[] b);
  public cbFunc1 CbOnSeperateMessageGet;


  private void _AppendData(SocketListener_code code, byte[] data){
    lock(appendData){
      appendData.Add(new KeyValuePair<SocketListener_code, byte[]>(code, data));
    }
  }

  private void _StartListening(){
    bool isLocked = false;
    keepListening = true;
    try{
      currentListener.Bind(endPoint);
      currentListener.Listen(1);
      Socket currSock = currentListener.Accept();
      EmitSignal("OnConnected");

      while(keepListening){
        //ArraySegment<byte> messagedata = new ArraySegment<byte>(new byte[sizeof(ushort)]);
        //await currSock.ReceiveAsync(messagedata, SocketFlags.None);
        byte[] messagedata = new byte[sizeof(ushort)];
        currSock.Receive(messagedata);
        ushort msgHead = BitConverter.ToUInt16(messagedata, 0);
        switch((SocketListener_code)msgHead){
          case SocketListener_code.PINGMESSAGE:{
            if(appendData.Count > 0){
              Monitor.Enter(appendData);
              isLocked = true;

              bool isSocketClosing = false;
              for(int i = 0; i < appendData.Count; i++){
                switch(appendData[i].Key){
                  case SocketListener_code.SENDMESSAGE:{
                    currSock.Send(BitConverter.GetBytes((ushort)SocketListener_code.SENDMESSAGE));
                    currSock.Send(BitConverter.GetBytes(appendData[i].Value.Length));
                    currSock.Send(appendData[i].Value);
                  }break;

                  case SocketListener_code.CLSOCKMESSAGE:{
                    currSock.Send(BitConverter.GetBytes((ushort)SocketListener_code.CLSOCKMESSAGE));
                    keepListening = false;
                    isSocketClosing = true;
                    i = appendData.Count;
                  }break;
                }
              }

              if(!isSocketClosing){
                appendData.RemoveRange(0, appendData.Count);
                currSock.Send(BitConverter.GetBytes((ushort)SocketListener_code.EOBULKMSG));
              }

              Monitor.Exit(appendData);
              isLocked = false;
            }
            else{
              currSock.Send(BitConverter.GetBytes((ushort)SocketListener_code.PINGMESSAGE));
            }
          }break;

          case SocketListener_code.SENDMESSAGE:{
            byte[] lengthData = new byte[sizeof(int)];
            currSock.Receive(lengthData);
            
            int length = BitConverter.ToInt32(lengthData, 0);
            byte[] actualMsgData = new byte[length];
            currSock.Receive(actualMsgData);
            CbOnSeperateMessageGet(ref actualMsgData);
          }break;

          case SocketListener_code.CLSOCKMESSAGE:{
            keepListening = false;
          }break;
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
    _AppendData(SocketListener_code.CLSOCKMESSAGE, null);
  }

  public void AppendData(byte[] data){
    _AppendData(SocketListener_code.SENDMESSAGE, data);
  }
}