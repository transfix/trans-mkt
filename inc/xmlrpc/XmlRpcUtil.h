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

#ifndef _XMLRPCUTIL_H_
#define _XMLRPCUTIL_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//
#if defined(_MSC_VER)
# pragma warning(disable:4786)    // identifier was truncated in debug info
#endif

#ifndef MAKEDEPEND
# include <string>
#endif

#if defined(_MSC_VER)
# define snprintf	    _snprintf
# define vsnprintf    _vsnprintf
# define strcasecmp	  _stricmp
# define strncasecmp	_strnicmp
#elif defined(__BORLANDC__)
# define strcasecmp stricmp
# define strncasecmp strnicmp
#endif

namespace XmlRpc {

  //! Utilities for XML parsing, encoding, and decoding and message handlers.
  class XmlRpcUtil {
  public:
    // hokey xml parsing
    //! Returns contents between <tag> and </tag>, updates offset to char after </tag>
    static std::string parseTag(const char* tag, std::string const& xml, int* offset);

    //! Returns true if the tag is found and updates offset to the char after the tag
    static bool findTag(const char* tag, std::string const& xml, int* offset);

    //! Returns the next tag and updates offset to the char after the tag, or empty string
    //! if the next non-whitespace character is not '<'
    static std::string getNextTag(std::string const& xml, int* offset);

    //! Returns true if the tag is found at the specified offset (modulo any whitespace)
    //! and updates offset to the char after the tag
    static bool nextTagIs(const char* tag, std::string const& xml, int* offset);


    //! Convert raw text to encoded xml.
    static std::string xmlEncode(const std::string& raw);

    //! Convert encoded xml to raw text
    static std::string xmlDecode(const std::string& encoded);


    //! Dump messages somewhere
    static void log(int level, const char* fmt, ...);

    //! Dump error messages somewhere
    static void error(const char* fmt, ...);

  };
} // namespace XmlRpc

#endif // _XMLRPCUTIL_H_
