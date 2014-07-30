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


#ifndef _XMLRPCDISPATCH_H_
#define _XMLRPCDISPATCH_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//
#if defined(_MSC_VER)
# pragma warning(disable:4786)    // identifier was truncated in debug info
#endif

#ifndef MAKEDEPEND
# include <list>
#endif

namespace XmlRpc {

  // An RPC source represents a file descriptor to monitor
  class XmlRpcSource;

  //! An object which monitors file descriptors for events and performs
  //! callbacks when interesting events happen.
  class XmlRpcDispatch {
  public:
    //! Constructor
    XmlRpcDispatch();
    ~XmlRpcDispatch();

    //! Values indicating the type of events a source is interested in
    enum EventType {
      ReadableEvent = 1,    //!< data available to read
      WritableEvent = 2,    //!< connected/data can be written without blocking
      Exception     = 4     //!< uh oh
    };
    
    //! Monitor this source for the event types specified by the event mask
    //! and call its event handler when any of the events occur.
    //!  @param source The source to monitor
    //!  @param eventMask Which event types to watch for. \see EventType
    void addSource(XmlRpcSource* source, unsigned eventMask);

    //! Stop monitoring this source.
    //!  @param source The source to stop monitoring
    void removeSource(XmlRpcSource* source);

    //! Modify the types of events to watch for on this source
    void setSourceEvents(XmlRpcSource* source, unsigned eventMask);


    //! Watch current set of sources and process events for the specified
    //! duration (in ms, -1 implies wait forever, or until exit is called)
    void work(double msTime);

    //! Exit from work routine
    void exit();

    //! Clear all sources from the monitored sources list. Sources are closed.
    void clear();

  protected:

    // helper
    double getTime();

    // A source to monitor and what to monitor it for
    struct MonitoredSource {
      MonitoredSource(XmlRpcSource* src, unsigned mask) : _src(src), _mask(mask) {}
      XmlRpcSource* getSource() const { return _src; }
      unsigned& getMask() { return _mask; }
      XmlRpcSource* _src;
      unsigned _mask;
    };

    // A list of sources to monitor
    typedef std::list< MonitoredSource > SourceList; 

    // Sources being monitored
    SourceList _sources;

    // When work should stop (-1 implies wait forever, or until exit is called)
    double _endTime;

    bool _doClear;
    bool _inWork;

  };
} // namespace XmlRpc

#endif  // _XMLRPCDISPATCH_H_
