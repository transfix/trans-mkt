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


#include <xmlrpc/XmlRpcServerMethod.h>
#include <xmlrpc/XmlRpcServer.h>

namespace XmlRpc {


  XmlRpcServerMethod::XmlRpcServerMethod(std::string const& name, XmlRpcServer* server)
  {
    _name = name;
    _server = server;
    if (_server) _server->addMethod(this);
  }

  XmlRpcServerMethod::~XmlRpcServerMethod()
  {
    if (_server) _server->removeMethod(this);
  }


} // namespace XmlRpc
