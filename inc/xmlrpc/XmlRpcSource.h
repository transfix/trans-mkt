/******************************************************************************
				Copyright   

This code is developed within the Computational Visualization Center at The 
University of Texas at Austin.

This code has been made available to you under the auspices of a Lesser General 
Public License (LGPL) (http://www.ices.utexas.edu/cvc/software/license.html) 
and terms that you have agreed to.

Upon accepting the LGPL, we request you agree to acknowledge the use of use of 
the code that results in any published work, including scientific papers, 
films, and videotapes by citing the following references:

C. Bajaj, Z. Yu, M. Auer
Volumetric Feature Extraction and Visualization of Tomographic Molecular Imaging
Journal of Structural Biology, Volume 144, Issues 1-2, October 2003, Pages 
132-143.

If you desire to use this code for a profit venture, or if you do not wish to 
accept LGPL, but desire usage of this code, please contact Chandrajit Bajaj 
(bajaj@ices.utexas.edu) at the Computational Visualization Center at The 
University of Texas at Austin for a different license.
******************************************************************************/


#ifndef _XMLRPCSOURCE_H_
#define _XMLRPCSOURCE_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//
#if defined(_MSC_VER)
# pragma warning(disable:4786)    // identifier was truncated in debug info
#endif

namespace XmlRpc {

  //! An RPC source represents a file descriptor to monitor
  class XmlRpcSource {
  public:
    //! Constructor
    //!  @param fd The socket file descriptor to monitor.
    //!  @param deleteOnClose If true, the object deletes itself when close is called.
    XmlRpcSource(int fd = -1, bool deleteOnClose = false);

    //! Destructor
    virtual ~XmlRpcSource();

    //! Return the file descriptor being monitored.
    int getfd() const { return _fd; }
    //! Specify the file descriptor to monitor.
    void setfd(int fd) { _fd = fd; }

    //! Return whether the file descriptor should be kept open if it is no longer monitored.
    bool getKeepOpen() const { return _keepOpen; }
    //! Specify whether the file descriptor should be kept open if it is no longer monitored.
    void setKeepOpen(bool b=true) { _keepOpen = b; }

    //! Close the owned fd. If deleteOnClose was specified at construction, the object is deleted.
    virtual void close();

    //! Return true to continue monitoring this source
    virtual unsigned handleEvent(unsigned eventType) = 0;

  private:

    // Socket. This should really be a SOCKET (an alias for unsigned int*) on windows...
    int _fd;

    // In the server, a new source (XmlRpcServerConnection) is created
    // for each connected client. When each connection is closed, the
    // corresponding source object is deleted.
    bool _deleteOnClose;

    // In the client, keep connections open if you intend to make multiple calls.
    bool _keepOpen;
  };
} // namespace XmlRpc

#endif //_XMLRPCSOURCE_H_
