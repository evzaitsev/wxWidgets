/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/private/sockunix.h
// Purpose:     wxSocketImpl implementation for Unix systems
// Authors:     Guilhem Lavaux, Vadim Zeitlin
// Created:     April 1997
// RCS-ID:      $Id$
// Copyright:   (c) 1997 Guilhem Lavaux
//              (c) 2008 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIX_GSOCKUNX_H_
#define _WX_UNIX_GSOCKUNX_H_

#include <unistd.h>
#include <sys/ioctl.h>
#include "wx/private/fdiodispatcher.h"

class wxSocketImplUnix : public wxSocketImpl,
                         public wxFDIOHandler
{
public:
    wxSocketImplUnix(wxSocketBase& wxsocket)
        : wxSocketImpl(wxsocket)
    {
        m_fds[0] =
        m_fds[1] = -1;

        m_use_events = false;
        m_enabledCallbacks = 0;
    }

    virtual void Shutdown();
    virtual wxSocketImpl *WaitConnection(wxSocketBase& wxsocket);

    int Read(void *buffer, int size);
    int Write(const void *buffer, int size);
    //attach or detach from main loop
    void Notify(bool flag);

    // wxFDIOHandler methods
    virtual void OnReadWaiting();
    virtual void OnWriteWaiting();
    virtual void OnExceptionWaiting();

    // Unix-specific functions
    bool HasAnyEnabledCallbacks() const { return m_enabledCallbacks != 0; }
    void EnableCallback(wxFDIODispatcherEntryFlags flag)
        { m_enabledCallbacks |= flag; }
    void DisableCallback(wxFDIODispatcherEntryFlags flag)
        { m_enabledCallbacks &= ~flag; }
    int GetEnabledCallbacks() const { return m_enabledCallbacks; }

private:
    virtual wxSocketError DoHandleConnect(int ret);
    virtual void DoClose()
    {
        wxSocketManager * const manager = wxSocketManager::Get();
        if ( manager )
        {
            manager->Uninstall_Callback(this, wxSOCKET_INPUT);
            manager->Uninstall_Callback(this, wxSOCKET_OUTPUT);
        }

        close(m_fd);
    }

    virtual void UnblockAndRegisterWithEventLoop()
    {
        int trueArg = 1;
        ioctl(m_fd, FIONBIO, &trueArg);

        EnableEvents();
    }

    // enable or disable notifications for socket input/output events but only
    // if m_use_events is true; do nothing otherwise
    virtual void EnableEvents()
    {
        if ( m_use_events )
            DoEnableEvents(true);
    }

    void DisableEvents()
    {
        if ( m_use_events )
            DoEnableEvents(false);
    }

    // really enable or disable socket input/output events, regardless of
    // m_use_events value
    void DoEnableEvents(bool enable);


    // enable or disable events for the given event if m_use_events; do nothing
    // otherwise
    //
    // notice that these functions also update m_detected: EnableEvent() clears
    // the corresponding bit in it and DisableEvent() sets it
    void EnableEvent(wxSocketNotify event);
    void DisableEvent(wxSocketNotify event);

    int Recv_Stream(void *buffer, int size);
    int Recv_Dgram(void *buffer, int size);
    int Send_Stream(const void *buffer, int size);
    int Send_Dgram(const void *buffer, int size);


protected:
    // true if socket should fire events
    bool m_use_events;

    // descriptors for input and output event notification channels associated
    // with the socket
    int m_fds[2];

    // the events which are currently enabled for this socket, combination of
    // wxFDIO_INPUT and wxFDIO_OUTPUT values
    //
    // TODO: this overlaps with m_detected but the semantics of the latter are
    //       very unclear so I don't dare to remove it right now
    int m_enabledCallbacks;

private:
    // notify the associated wxSocket about a change in socket state and shut
    // down the socket if the event is wxSOCKET_LOST
    void OnStateChange(wxSocketNotify event);

    // give it access to our m_fds
    friend class wxSocketFDBasedManager;
};

// A version of wxSocketManager which uses FDs for socket IO
class wxSocketFDBasedManager : public wxSocketManager
{
public:
    // no special initialization/cleanup needed when using FDs
    virtual bool OnInit() { return true; }
    virtual void OnExit() { }

protected:
    // identifies either input or output direction
    //
    // NB: the values of this enum shouldn't change
    enum SocketDir
    {
        FD_INPUT,
        FD_OUTPUT
    };

    // get the FD index corresponding to the given wxSocketNotify
    SocketDir GetDirForEvent(wxSocketImpl *socket, wxSocketNotify event)
    {
        switch ( event )
        {
            default:
                wxFAIL_MSG( "unexpected socket event" );
                // fall through

            case wxSOCKET_LOST:
                // fall through

            case wxSOCKET_INPUT:
                return FD_INPUT;

            case wxSOCKET_OUTPUT:
                return FD_OUTPUT;

            case wxSOCKET_CONNECTION:
                // FIXME: explain this?
                return socket->m_server ? FD_INPUT : FD_OUTPUT;
        }
    }

    // access the FDs we store
    int& FD(wxSocketImplUnix *socket, SocketDir d)
    {
        return socket->m_fds[d];
    }
};

// Common base class for all ports using X11-like (and hence implemented in
// X11, Motif and GTK) AddInput() and RemoveInput() functions
class wxSocketInputBasedManager : public wxSocketFDBasedManager
{
public:
    virtual void Install_Callback(wxSocketImpl *socket_, wxSocketNotify event)
    {
        wxSocketImplUnix * const
            socket = static_cast<wxSocketImplUnix *>(socket_);

        wxCHECK_RET( socket->m_fd != -1,
                        "shouldn't be called on invalid socket" );

        const SocketDir d = GetDirForEvent(socket, event);

        int& fd = FD(socket, d);
        if ( fd != -1 )
            RemoveInput(fd);

        fd = AddInput(socket, socket->m_fd, d);
    }

    virtual void Uninstall_Callback(wxSocketImpl *socket_, wxSocketNotify event)
    {
        wxSocketImplUnix * const
            socket = static_cast<wxSocketImplUnix *>(socket_);

        const SocketDir d = GetDirForEvent(socket, event);

        int& fd = FD(socket, d);
        if ( fd != -1 )
        {
            RemoveInput(fd);
            fd = -1;
        }
    }

private:
    // these functions map directly to XtAdd/RemoveInput() or
    // gdk_input_add/remove()
    virtual int AddInput(wxFDIOHandler *handler, int fd, SocketDir d) = 0;
    virtual void RemoveInput(int fd) = 0;
};

#endif  /* _WX_UNIX_GSOCKUNX_H_ */
