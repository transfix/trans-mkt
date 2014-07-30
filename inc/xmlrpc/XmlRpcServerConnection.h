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

#ifndef _XMLRPCSERVERCONNECTION_H_
#define _XMLRPCSERVERCONNECTION_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//
#if defined(_MSC_VER)
# pragma warning(disable:4786)    // identifier was truncated in debug info
#endif

#ifndef MAKEDEPEND
# include <string>
#endif

#include <xmlrpc/XmlRpcValue.h>
#include <xmlrpc/XmlRpcSource.h>

namespace XmlRpc {


  // The server waits for client connections and provides methods
  class XmlRpcServer;
  class XmlRpcServerMethod;

  //! A class to handle XML RPC requests from a particular client
  class XmlRpcServerConnection : public XmlRpcSource {
  public:
    // Static data
    static const char METHODNAME_TAG[];
    static const char PARAMS_TAG[];
    static const char PARAMS_ETAG[];
    static const char PARAM_TAG[];
    static const char PARAM_ETAG[];

    static const std::string SYSTEM_MULTICALL;
    static const std::string METHODNAME;
    static const std::string PARAMS;

    static const std::string FAULTCODE;
    static const std::string FAULTSTRING;

    //! Constructor
    XmlRpcServerConnection(int fd, XmlRpcServer* server, bool deleteOnClose = false);
    //! Destructor
    virtual ~XmlRpcServerConnection();

    // XmlRpcSource interface implementation
    //! Handle IO on the client connection socket.
    //!   @param eventType Type of IO event that occurred. @see XmlRpcDispatch::EventType.
    virtual unsigned handleEvent(unsigned eventType);

  protected:

    bool readHeader();
    bool readRequest();
    bool writeResponse();

    // Parses the request, runs the method, generates the response xml.
    virtual void executeRequest();

    // Parse the methodName and parameters from the request.
    std::string parseRequest(XmlRpcValue& params);

    // Execute a named method with the specified params.
    bool executeMethod(const std::string& methodName, XmlRpcValue& params, XmlRpcValue& result);

    // Execute multiple calls and return the results in an array.
    bool executeMulticall(const std::string& methodName, XmlRpcValue& params, XmlRpcValue& result);

    // Construct a response from the result XML.
    void generateResponse(std::string const& resultXml);
    void generateFaultResponse(std::string const& msg, int errorCode = -1);
    std::string generateHeader(std::string const& body);


    // The XmlRpc server that accepted this connection
    XmlRpcServer* _server;

    // Possible IO states for the connection
    enum ServerConnectionState { READ_HEADER, READ_REQUEST, WRITE_RESPONSE };
    ServerConnectionState _connectionState;

    // Request headers
    std::string _header;

    // Number of bytes expected in the request body (parsed from header)
    int _contentLength;

    // Request body
    std::string _request;

    // Response
    std::string _response;

    // Number of bytes of the response written so far
    int _bytesWritten;

    // Whether to keep the current client connection open for further requests
    bool _keepAlive;
  };
} // namespace XmlRpc

#endif // _XMLRPCSERVERCONNECTION_H_
