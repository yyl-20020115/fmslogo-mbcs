// Copyright (C) 1995 by the Regents of the University of California
// Copyright (C) 1995 by George Mills
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "pch.h"
#ifndef USE_PRECOMPILED_HEADER
    #include "netwind.h"
    #include "logoeventqueue.h"

    #include "init.h"
    #include "messagebox.h"
    #include "graphics.h"
    #include "argumentutils.h"
    #include "stringprintednode.h"
    #include "logodata.h"
    #include "logorc.h"
    #include "lists.h"
    #include "eval.h"
    #include "logomath.h"
    #include "parse.h"
    #include "screenwindow.h"
    #include "debugheap.h"
    #include "localizedstrings.h"
#endif

/////////////////////////////////////////////////////////////////////////////////////
// Manifest Constants

const int MAX_PENDING_CONNECTS = 4;  // The backlog allowed for listen()

/////////////////////////////////////////////////////////////////////////////////////
// Global Variables

CClientNetworkConnection g_ClientConnection;
CServerNetworkConnection g_ServerConnection;

static int network_dns_sync = 0;

static bool network_is_started = false;

/////////////////////////////////////////////////////////////////////////////////////
// Helper Functions

// converts winsock errorcode to string
static
LPCSTR WSAGetLastErrorString(int error_arg)
{
    int error;

    if (error_arg != 0)
    {
        error = error_arg;
    }
    else
    {
        error = WSAGetLastError();
    }

    switch (error)
    {
    case WSAENAMETOOLONG:
        return LOCALIZED_ERROR_NETWORKNAMETOOLONG;
         
    case WSANOTINITIALISED:
        return LOCALIZED_ERROR_NETWORKNOTINITIALIZED;

    case WSASYSNOTREADY:
        return LOCALIZED_ERROR_NETWORKSYSTEMNOTREADY;

    case WSAVERNOTSUPPORTED:
        return LOCALIZED_ERROR_NETWORKVERSIONNOTSUPPORTED;

    case WSAESHUTDOWN:
        return LOCALIZED_ERROR_NETWORKSENDAFTERSHUTDOWN;

    case WSAEINTR:
        return LOCALIZED_ERROR_NETWORKINTERRUPTED;

    case WSAHOST_NOT_FOUND:
        return LOCALIZED_ERROR_NETWORKHOSTNOTFOUNDAUTH;

    case WSATRY_AGAIN:
        return LOCALIZED_ERROR_NETWORKHOSTNOTFOUNDNONAUTH;

    case WSANO_RECOVERY:
        return LOCALIZED_ERROR_NETWORKNOTRECOVERABLE;

    case WSANO_DATA:
        return LOCALIZED_ERROR_NETWORKNODATA;

    case WSAEBADF:
        return LOCALIZED_ERROR_NETWORKBADFILENUMBER;
           
    case WSAEWOULDBLOCK:
        return LOCALIZED_ERROR_NETWORKWOULDBLOCK;

    case WSAEINPROGRESS:
        return LOCALIZED_ERROR_NETWORKNOWINPROGRESS;

    case WSAEALREADY:
        return LOCALIZED_ERROR_NETWORKALREADYINPROGRESS;

    case WSAEFAULT:
        return LOCALIZED_ERROR_NETWORKBADADDRESS;

    case WSAEDESTADDRREQ:
        return LOCALIZED_ERROR_NETWORKNEEDDESTADDRESS;

    case WSAEMSGSIZE:
        return LOCALIZED_ERROR_NETWORKMESSAGETOOLONG;

    case WSAEPFNOSUPPORT:
        return LOCALIZED_ERROR_NETWORKPFNOTSUPPORTED;
           
    case WSAENOTEMPTY:
        return LOCALIZED_ERROR_NETWORKDIRNOTEMPTY;

    case WSAEPROCLIM:
        return LOCALIZED_ERROR_NETWORKAPPLIMITREACHED;

    case WSAEDQUOT:
        return LOCALIZED_ERROR_NETWORKDISKQUOTA;

    case WSAEINVAL:
        return LOCALIZED_ERROR_NETWORKINVALIDARGUMENTS;

    case WSAEMFILE:
        return LOCALIZED_ERROR_NETWORKTOOMANYFILES;

    case WSAEACCES:
        return LOCALIZED_ERROR_NETWORKACCESSDENIED;
         
    case WSAENOTSOCK:
        return LOCALIZED_ERROR_NETWORKNOTASOCKET;

    case WSAEADDRNOTAVAIL:
        return LOCALIZED_ERROR_NETWORKADDRNOTAVAILABLE;

    case WSAEADDRINUSE:
        return LOCALIZED_ERROR_NETWORKADDRINUSE;

    case WSAEAFNOSUPPORT:
        return LOCALIZED_ERROR_NETWORKAFNOTSUPPORTED;

    case WSAESOCKTNOSUPPORT:
        return LOCALIZED_ERROR_NETWORKTYPENOTSUPPORTED;
         
    case WSAEPROTONOSUPPORT:
        return LOCALIZED_ERROR_NETWORKPROTONOTSUPPORTED;

    case WSAENOBUFS:
        return LOCALIZED_ERROR_NETWORKNOBUFFER;

    case WSAETIMEDOUT:
        return LOCALIZED_ERROR_NETWORKTIMEDOUT;

    case WSAEISCONN:
        return LOCALIZED_ERROR_NETWORKALREADYCONNECTED;

    case WSAENOTCONN:
        return LOCALIZED_ERROR_NETWORKNOTCONNECTED;

    case WSAENOPROTOOPT:
        return LOCALIZED_ERROR_NETWORKBADPROTOOPT;

    case WSAECONNRESET:
        return LOCALIZED_ERROR_NETWORKCONNECTIONRESET;

    case WSAECONNABORTED:
        return LOCALIZED_ERROR_NETWORKCONNECTIONABORT;

    case WSAENETDOWN:
        return LOCALIZED_ERROR_NETWORKISDOWN;

    case WSAENETRESET:
        return LOCALIZED_ERROR_NETWORKRESET;

    case WSAECONNREFUSED:
        return LOCALIZED_ERROR_NETWORKCONNECTIONREFUSED;

    case WSAEHOSTDOWN:
        return LOCALIZED_ERROR_NETWORKHOSTISDOWN;
         
    case WSAEHOSTUNREACH:
        return LOCALIZED_ERROR_NETWORKHOSTISUNREACHABLE;
          
    case WSAEPROTOTYPE:
        return LOCALIZED_ERROR_NETWORKBADPROTOTYPE;
         
    case WSAEOPNOTSUPP:
        return LOCALIZED_ERROR_NETWORKBADOPNOTSUPPORTED;

    case WSAENETUNREACH:
        return LOCALIZED_ERROR_NETWORKUNREACHABLE;

    case WSAETOOMANYREFS:
        return LOCALIZED_ERROR_NETWORKTOOMANYREFS;

    default:
        {
            static char buffer[32];
            sprintf(buffer, LOCALIZED_UNKNOWN" %d", error);
            return buffer;
        }
    }
}

static
void
safe_free(
    char * & buffer
    )
{
    free(buffer);
    buffer = NULL;
}

/////////////////////////////////////////////////////////////////////////////////////
// private class CNetworkConnection::CCarryOverBuffer

CNetworkConnection::CCarryOverBuffer::CCarryOverBuffer() :
    m_Buffer(NULL),
    m_BufferSize(0),
    m_BytesOfData(0)
{
}

void
CNetworkConnection::CCarryOverBuffer::ReleaseBuffer()
{
    safe_free(m_Buffer);

    m_BytesOfData = 0;
    m_BufferSize  = 0;
}

void
CNetworkConnection::CCarryOverBuffer::Append(
    const char * AppendBuffer,
    int          AppendBufferLength
    )
{
    if (m_BufferSize < m_BytesOfData + AppendBufferLength + 1)
    {
        // there isn't enough room on the buffer.  Grow it.

        m_BufferSize += AppendBufferLength + 1; // leave room for NUL

        m_Buffer = (char*) realloc(m_Buffer, m_BufferSize);
    }

    memcpy(m_Buffer + m_BytesOfData, AppendBuffer, AppendBufferLength);

    m_BytesOfData += AppendBufferLength;

    // always keep m_Buffer NUL-terminated
    m_Buffer[m_BytesOfData] = '\0';
}

void
CNetworkConnection::CCarryOverBuffer::ShiftLeft(
    int ShiftAmount
    )
{
    assert(0 <= ShiftAmount);
    assert(ShiftAmount <= m_BytesOfData);

    // shift the buffer such that it begins at "begin"
    memmove(m_Buffer, m_Buffer + ShiftAmount, m_BytesOfData - ShiftAmount);
   
    m_BytesOfData -= ShiftAmount;
}

/////////////////////////////////////////////////////////////////////////////////////
// class CNetworkConnection

CNetworkConnection::CNetworkConnection() :
    m_Socket(INVALID_SOCKET),
    m_IsConnected(false),
    m_IsBusy(false),
    m_IsEnabled(false),
    m_OnReceiveReady(NULL),
    m_OnSendReady(NULL),
    m_ReceiveValue(NULL)
{
}

void
CNetworkConnection::SetLastPacketReceived(
    char * LastPacket
    )
{
    free(m_ReceiveValue);
    m_ReceiveValue = LastPacket;
}

NODE*
CNetworkConnection::GetLastPacketReceived() const
{
    if (m_ReceiveValue == NULL)
    {
        return NIL;
    }
    else
    {
        NODE* targ = make_strnode(m_ReceiveValue);
        NODE* val = parser(targ, false);
        return val;
    }
}

bool
CNetworkConnection::IsEnabled() const
{
    return m_IsEnabled;
}

void
CNetworkConnection::Disable()
{
    if (IsEnabled())
    {
        m_IsEnabled    = false;
        m_IsConnected  = false;
        m_IsBusy       = false;

        safe_free(m_ReceiveValue);

        closesocket(m_Socket);
        m_Socket = INVALID_SOCKET;
    }
}

void
CNetworkConnection::Enable(
    const char *    OnSendReady,
    const char *    OnReceiveReady
    )
{
    // copy the receive ready callback
    if (m_OnReceiveReady == NULL)
    {
        m_OnReceiveReady = (char *) malloc(MAX_BUFFER_SIZE);
    }
    strcpy(m_OnReceiveReady, OnReceiveReady);

    // copy the send ready callback
    if (m_OnSendReady == NULL)
    {
        m_OnSendReady = (char *) malloc(MAX_BUFFER_SIZE);
    }
    strcpy(m_OnSendReady, OnSendReady);

    // get sockets
    m_Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_Socket == INVALID_SOCKET)
    {
        ShowMessageAndStop("socket()", WSAGetLastErrorString(0));
    }
}

bool
CNetworkConnection::SendValue(
    const char * Data
    )
{
    if (m_IsConnected && !m_IsBusy)
    {
        // send the data
        int rval = send(m_Socket, Data, strlen(Data) + 1, 0);
        if (rval == SOCKET_ERROR)
        {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
            {
                ShowMessageAndStop("send(socket)", WSAGetLastErrorString(0));
                return false;
            }

            // Don't send anymore until we receive confirmation
            // that the remote side received the data.
            m_IsBusy = true;
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

void
CNetworkConnection::AsyncReceive(
    HWND                 WindowHandle,
    const char *         ErrorMessage
    )
{
    char buffer[MAX_PACKET_SIZE];
    memset(buffer, 0, MAX_PACKET_SIZE);

    // read the data from the buffer
    int status = recv(m_Socket, buffer, sizeof(buffer) - 1, 0);
    if (status == SOCKET_ERROR)
    {
        // if this would block, we just wait until we get called again
        if (WSAGetLastError() != WSAEWOULDBLOCK) 
        {
            ::MessageBox(
                WindowHandle,
                WSAGetLastErrorString(0),
                ErrorMessage,
                MB_OK);
            // err_logo(STOP_ERROR,NIL);
        }
    }
    else
    {
        // we received some data.

        // We have some data left from the last time we were called.
        // Append this buffer to the end of the network receive buffer. 

        // TODO: Don't Append the data to the carry-over buffer if there is none.
        //       We should be able to use buffer, instead.
        m_CarryOverData.Append(buffer, status);

        // now queue up a separate message for each packet
        int begin = 0;
        int end   = strlen(m_CarryOverData.m_Buffer);

        while (end < m_CarryOverData.m_BytesOfData)
        {
            callthing * callevent = callthing::CreateNetworkReceiveReadyEvent(
                this,
                m_OnReceiveReady,
                m_CarryOverData.m_Buffer + begin);

            calllists.insert(callevent);
            PostMessage(WindowHandle, WM_CHECKQUEUE, 0, 0);

            begin = end + 1;
            end   = begin + strlen(m_CarryOverData.m_Buffer + begin);
        }

        // shift the buffer such that it begins at "begin"
        m_CarryOverData.ShiftLeft(begin);
    }
}

void
CNetworkConnection::AsyncClose(
    HWND             WindowHandle
    )
{
    // send any data in the carry-over buffer upwards
    if (m_CarryOverData.m_BytesOfData != 0)
    {
        callthing *callevent = callthing::CreateNetworkReceiveReadyEvent(
            this,
            m_OnReceiveReady,
            m_CarryOverData.m_Buffer);

        calllists.insert(callevent);

        PostMessage(WindowHandle, WM_CHECKQUEUE, 0, 0);
    }

    m_CarryOverData.ReleaseBuffer();

    m_IsConnected = false;
}

void
CNetworkConnection::Shutdown()
{
    Disable();

    safe_free(m_OnReceiveReady);
    safe_free(m_OnSendReady);
    SetLastPacketReceived(NULL);
    m_CarryOverData.ReleaseBuffer();
}

void
CNetworkConnection::PostOnSendReadyEvent(
    HWND       WindowHandle
    )
{
    // we don't distinguish between all event types
    callthing *callevent = callthing::CreateNoYieldFunctionEvent(m_OnSendReady);

    calllists.insert(callevent);
    PostMessage(WindowHandle, WM_CHECKQUEUE, 0, 0);
}

CClientNetworkConnection::CClientNetworkConnection()
{
    memset(m_RemoteHostEntry.Buffer, 0x00, sizeof(m_RemoteHostEntry.Buffer));
}

void
CClientNetworkConnection::Enable(
    const char *    OnSendReady,
    const char *    OnReceiveReady,
    unsigned int    RemotePort,
    const char *    RemoteHostName
    )
{
    CNetworkConnection::Enable(OnSendReady, OnReceiveReady);
    if (NOT_THROWING)
    {
        if (network_dns_sync == 1)
        {
            PHOSTENT hostEntry = gethostbyname(RemoteHostName);
            if (hostEntry == NULL)
            {
                ShowMessageAndStop(
                    "gethostbyname(host)",
                    WSAGetLastErrorString(0));
                return;
            }

            // This HOSTENTRY structure is owned by WinSock and
            // could be changed.  Therefore, we copy the information
            // that we need before any other WinSock calls can be made.
            // Note that this may result in some dangling pointers, but
            // this is okay, since the pointers will be valid long enough
            // to initialize the SOCKADDR_IN struct, which is all we use
            // it for.
            m_RemoteHostEntry.Entry = *hostEntry;
            m_RemotePort            = RemotePort;
            m_IsEnabled             = true;

            // Set up the rest of the connection by invoking the logic that
            // should be invoked once m_HostEntry is filled in.
            SendMessage(GetMainWindow(), WM_NETWORK_CONNECTSENDFINISH, 0, 0);
        }
        else
        {
            // get address of remote machine
            HANDLE getHostByNameHandle = WSAAsyncGetHostByName(
                GetMainWindow(),
                WM_NETWORK_CONNECTSENDFINISH,
                RemoteHostName,
                m_RemoteHostEntry.Buffer,
                sizeof(m_RemoteHostEntry.Buffer));
            if (getHostByNameHandle == NULL)
            {
                ShowMessageAndStop(
                    "WSAAsyncGetHostByName()",
                    WSAGetLastErrorString(0));
                return;
            }

            m_RemotePort = RemotePort;
            m_IsEnabled  = true;

            // The rest of the connection will be set up when the GetHostByName
            // callback is invoked, after m_HostEntry is filled in.
        }
    }
}

// Call this to handle a WM_NETWORK_CONNECTSENDACK message.
int
CClientNetworkConnection::OnConnectSendAck(
    HWND        WindowHandle,
    LONG        LParam
    )
{
    if (WSAGETASYNCERROR(LParam) != 0)
    {
        ::MessageBox(
            WindowHandle,
            WSAGetLastErrorString(WSAGETASYNCERROR(LParam)),
            "WSAAsyncGetHostByNameCallBack()",
            MB_OK);
        // err_logo(STOP_ERROR,NIL);
        return 0;
    }

    if (!IsEnabled())
    {
        // The network client side has been shut down.
        // This message must have been delayed.
        return 0;
    }

    // update flags based on event type
    switch (WSAGETSELECTEVENT(LParam))
    {
    case FD_READ:
        AsyncReceive(WindowHandle, "recv(sendsock)");
        return 0;

    case FD_WRITE:
        // allow another frame to go out.
        m_IsBusy = false;
        break;

    case FD_CONNECT:
        // flag it's ok to start firing
        m_IsConnected = true;
        break;

    case FD_CLOSE:
        // done
        AsyncClose(WindowHandle);
        break;
    }

    // we don't distinguish between all event types
    PostOnSendReadyEvent(WindowHandle);
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
// CClientNetworkConnection class


// Call this to handle a WM_NETWORK_CONNECTSENDFINISH message.
int
CClientNetworkConnection::OnConnectSendFinish(
    HWND        WindowHandle,
    LONG        LParam
    )
{
    if (WSAGETASYNCERROR(LParam) != 0)
    {
        ::MessageBox(
            WindowHandle,
            WSAGetLastErrorString(WSAGETASYNCERROR(LParam)),
            "WSAAsyncGetHostByNameCallBack()",
            MB_OK);
        // err_logo(STOP_ERROR,NIL);
        return 0;
    }

    if (!IsEnabled())
    {
        // The client-side is not initialized.
        // This must be a delayed event coming in after shutdown.
        ::MessageBox(
            WindowHandle,
            LOCALIZED_ERROR_NETWORKSHUTDOWN,
            LOCALIZED_ERROR_NETWORK,
            MB_OK);
        return 0;
    }

    SOCKADDR_IN send_dest_sin = {0};
    send_dest_sin.sin_family = AF_INET;
    memcpy(
        &send_dest_sin.sin_addr,
        m_RemoteHostEntry.Entry.h_addr,
        m_RemoteHostEntry.Entry.h_length);
    send_dest_sin.sin_port = htons(m_RemotePort); // Convert to network ordering

    // watch for connect
    if (WSAAsyncSelect(
            m_Socket,
            WindowHandle,
            WM_NETWORK_CONNECTSENDACK,
            FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE) == SOCKET_ERROR)
    {
        ::MessageBox(
            WindowHandle,
            WSAGetLastErrorString(0),
            "WSAAsyncSelect(sendSock) FD_CONNECT",
            MB_OK);
        // err_logo(STOP_ERROR,NIL);
    }

    // let's try now
    int rval = connect(
        m_Socket,
        (PSOCKADDR) &send_dest_sin,
        sizeof send_dest_sin);
    if (rval == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSAEWOULDBLOCK)
        {
            ::MessageBox(
                WindowHandle,
                WSAGetLastErrorString(0),
                "connect(sendsock)",
                MB_OK);
            // err_logo(STOP_ERROR,NIL);
            return 0;
        }
    }

    // fire event that connection is made
    PostOnSendReadyEvent(WindowHandle);
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
// CServerNetworkConnection class

void
CServerNetworkConnection::Enable(
    const char *    OnSendReady,
    const char *    OnReceiveReady,
    unsigned int    ServerPort
    )
{
    CNetworkConnection::Enable(OnSendReady, OnReceiveReady);
    if (NOT_THROWING)
    {
        // Associate an address with the socket. (bind)
        SOCKADDR_IN socket_address = {0};
        socket_address.sin_family      = AF_INET;
        socket_address.sin_addr.s_addr = INADDR_ANY;
        socket_address.sin_port        = htons(ServerPort);

        int rval = bind(
            m_Socket,
            (struct sockaddr *) &socket_address,
            sizeof socket_address);
        if (rval == SOCKET_ERROR)
        {
            ShowMessageAndStop("bind(receivesock)", WSAGetLastErrorString(0));
            return;
        }

        // listen for connect
        if (listen(m_Socket, MAX_PENDING_CONNECTS) == SOCKET_ERROR)
        {
            ShowMessageAndStop("listen(receivesock)", WSAGetLastErrorString(0));
            return;
        }

        // watch for when connect happens
        m_IsEnabled = true;

        rval = WSAAsyncSelect(
            m_Socket,
            GetMainWindow(),
            WM_NETWORK_LISTENRECEIVEACK,
            FD_ACCEPT | FD_READ | FD_WRITE | FD_CLOSE);
        if (rval == SOCKET_ERROR)
        {
            ShowMessageAndStop(
                "WSAAsyncSelect(receivesock) FD_ACCEPT",
                WSAGetLastErrorString(0));
            return;
        }

        // queue this event
        PostOnSendReadyEvent(GetMainWindow());
    }
}

// Call this to handle a WM_NETWORK_LISTENRECEIVEACK message.
int
CServerNetworkConnection::OnListenReceiveAck(
    HWND        WindowHandle,
    LONG        LParam
    )
{
    if (WSAGETASYNCERROR(LParam) != 0)
    {
        ::MessageBox(
            WindowHandle,
            WSAGetLastErrorString(WSAGETASYNCERROR(LParam)),
            "WSAAsyncGetHostByNameCallBack()",
            MB_OK);
        // err_logo(STOP_ERROR,NIL);
        return 0;
    }

    if (!IsEnabled())
    {
        // The server-side is not initialized.
        // This must be a delayed event coming in after shutdown.
        return 0;
    }


    // based on event do the right thing
    switch (WSAGETSELECTEVENT(LParam))
    {
        SOCKADDR_IN acc_sin;       // Accept socket address - internet style
        int         acc_sin_len;   // Accept socket address length

    case FD_READ:
        AsyncReceive(WindowHandle, "recv(receivesock)");
        return 0;

    case FD_ACCEPT:
        acc_sin_len = sizeof acc_sin;

        m_Socket = accept(m_Socket, (struct sockaddr *) &acc_sin, &acc_sin_len);
        if (m_Socket == INVALID_SOCKET)
        {
            ::MessageBox(
                WindowHandle,
                WSAGetLastErrorString(0),
                "accept(receivesock)",
                MB_OK);
            // err_logo(STOP_ERROR,NIL);
            return 0;
        }
        m_IsConnected = true;
        break;

    case FD_CLOSE:
        AsyncClose(WindowHandle);
        break;

    case FD_WRITE:
        // allow another frame to go out.
        m_IsBusy = false;
        break;
    }

    // all other events just queue the event
    PostOnSendReadyEvent(WindowHandle);
    return 0;
}


// startup network
NODE *lnetstartup(NODE *args)
{
    // check if already started
    if (network_is_started)
    {
        ShowMessageAndStop(
            LOCALIZED_ERROR_NETWORK, 
            LOCALIZED_ERROR_NETWORKALREADYSTARTED);
        return Unbound;
    }

    if (args != NIL)
    {
        network_dns_sync = int_arg(args);
    }

    // tell winsock to wakeup
    WSADATA WSAData;
    if (WSAStartup(MAKEWORD(1,1), &WSAData) != 0)
    {
        ShowMessageAndStop("WSAStartup()", WSAGetLastErrorString(0));
        return Unbound;
    }

    network_is_started = true;
    return Unbound;
}

// put everything back to original state
NODE *lnetshutdown(NODE *)
{
    // cleanup receive
    g_ServerConnection.Shutdown();

    // cleanup send
    g_ClientConnection.Shutdown();

    // cleanup library
    if (network_is_started)
    {
        // There seems to be a race condition with WSAAsyncGetHostByName
        // in that WSACleanup() won't always cancel the callback.
        // As a result, it could still attempt to write into the
        // m_HostEntry.Entry field after it has been freed.
        //
        // This is not a problem for the current implementation, since it
        // statically allocates the memory and never frees it.
        // However, it could be a problem if g_ServerConnection and
        // g_ClientConnection were dynamically allocated.
        WSACleanup();
        network_is_started = false;
    }

    network_dns_sync = 0;
    return Unbound;
}

// enable receiving
NODE *lnetaccepton(NODE *args)
{
    // check sanity
    if (!network_is_started)
    {
        ShowMessageAndStop(
            LOCALIZED_ERROR_NETWORK, 
            LOCALIZED_ERROR_NETWORKNOTSTARTED);
        return Unbound;
    }

    if (g_ServerConnection.IsEnabled())
    {
        ShowMessageAndStop(
            LOCALIZED_ERROR_NETWORKRECEIVE, 
            LOCALIZED_ERROR_NETWORKALREADYON);
        return Unbound;
    }

    // get args (socket and callback)
    int port = getint(nonnegative_int_arg(args));

    char networksend[MAX_BUFFER_SIZE];
    cnv_strnode_string(networksend, cdr(args));

    char networkreceive[MAX_BUFFER_SIZE];
    cnv_strnode_string(networkreceive, cdr(cdr(args)));

    if (NOT_THROWING)
    {
        g_ServerConnection.Enable(networksend, networkreceive, port);
    }

    if (NOT_THROWING)
    {
        return Truex.GetNode();
    }

    return Unbound;
}

NODE *lnetacceptoff(NODE *)
{
    // tell handler not to do anything with messages for network receive
    if (g_ServerConnection.IsEnabled())
    {
        g_ServerConnection.Disable();
    }
    else
    {
        ShowMessageAndStop(
            LOCALIZED_ERROR_NETWORKRECEIVE, 
            LOCALIZED_ERROR_NETWORKALREADYOFF);
    }

    return Unbound;
}

NODE *lnetacceptreceivevalue(NODE *)
{
    // return current network value
    if (g_ServerConnection.IsEnabled())
    {
        return g_ServerConnection.GetLastPacketReceived();
    }

    return Unbound;
}

NODE *lnetconnectreceivevalue(NODE *)
{
    // return current network value
    if (g_ClientConnection.IsEnabled())
    {
        return g_ClientConnection.GetLastPacketReceived();
    }

    return Unbound;
}

NODE *lnetconnecton(NODE *args)
{
    // sanity check first
    if (!network_is_started)
    {
        ShowMessageAndStop(
            LOCALIZED_ERROR_NETWORK, 
            LOCALIZED_ERROR_NETWORKNOTSTARTED);
        return Unbound;
    }

    if (g_ClientConnection.IsEnabled())
    {
        ShowMessageAndStop(
            LOCALIZED_ERROR_NETWORKSEND, 
            LOCALIZED_ERROR_NETWORKALREADYON);
        return Unbound;
    }

    // get args (remotemachinename, socket, callback)
    CStringPrintedNode remotehostname(car(args));

    int remote_port = getint(nonnegative_int_arg(cdr(args)));

    char networksend[MAX_BUFFER_SIZE];
    cnv_strnode_string(networksend, cdr(cdr(args)));

    char networkreceive[MAX_BUFFER_SIZE];
    cnv_strnode_string(networkreceive, cdr(cdr(cdr(args))));

    if (NOT_THROWING)
    {
        g_ClientConnection.Enable(
            networksend,
            networkreceive,
            remote_port,
            remotehostname);
    }

    if (NOT_THROWING)
    {
        return Truex.GetNode();
    }

    return Unbound;
}

NODE *lnetconnectoff(NODE *)
{
    // tell handler not to do anything with messages for network send
    if (g_ClientConnection.IsEnabled())
    {
        g_ClientConnection.Disable();
    }
    else
    {
        ShowMessageAndStop(
            LOCALIZED_ERROR_NETWORKSEND, 
            LOCALIZED_ERROR_NETWORKALREADYOFF);
    }

    return Unbound;
}

NODE *lnetconnectsendvalue(NODE *args)
{
    // get args (data)
    CStringPrintedNode data(car(args));

    if (NOT_THROWING)
    {
        // try to send data
        bool isOk = g_ClientConnection.SendValue(data);
        return true_or_false(isOk);
    }

    return Unbound;
}

NODE *lnetacceptsendvalue(NODE *args)
{
    // get args (data)
    CStringPrintedNode data(car(args));

    if (NOT_THROWING)
    {
        // try to send data
        bool isOk = g_ServerConnection.SendValue(data);
        return true_or_false(isOk);
    }

    return Unbound;
}
