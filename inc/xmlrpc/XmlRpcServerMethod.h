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


#ifndef _XMLRPCSERVERMETHOD_H_
#define _XMLRPCSERVERMETHOD_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//
#if defined(_MSC_VER)
# pragma warning(disable:4786)    // identifier was truncated in debug info
#endif

#ifndef MAKEDEPEND
# include <string>
#endif

namespace XmlRpc {

  // Representation of a parameter or result value
  class XmlRpcValue;

  // The XmlRpcServer processes client requests to call RPCs
  class XmlRpcServer;

  //! Abstract class representing a single RPC method
  class XmlRpcServerMethod {
  public:
    //! Constructor
    XmlRpcServerMethod(std::string const& name, XmlRpcServer* server = 0);
    //! Destructor
    virtual ~XmlRpcServerMethod();

    //! Returns the name of the method
    std::string& name() { return _name; }

    //! Execute the method. Subclasses must provide a definition for this method.
    virtual void execute(XmlRpcValue& params, XmlRpcValue& result) = 0;

    //! Returns a help string for the method.
    //! Subclasses should define this method if introspection is being used.
    virtual std::string help() { return std::string(); }

  protected:
    std::string _name;
    XmlRpcServer* _server;
  };
} // namespace XmlRpc

#endif // _XMLRPCSERVERMETHOD_H_
